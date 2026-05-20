#include "engine/Audio.hpp"

#include "engine/Log.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace majo {

namespace {

constexpr SDL_AudioFormat OutputFormat = SDL_AUDIO_F32;
constexpr int MinSampleRate = 8000;
constexpr int MaxSampleRate = 192000;
constexpr int MinChannels = 1;
constexpr int MaxChannels = 2;

std::string trimAscii(std::string text)
{
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    text.erase(text.begin(), std::find_if_not(text.begin(), text.end(), isSpace));
    text.erase(std::find_if_not(text.rbegin(), text.rend(), isSpace).base(), text.end());
    return text;
}

std::string lowerAscii(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

void stripUtf8Bom(std::string& text)
{
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
}

std::vector<std::string> splitTabs(const std::string& line)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t tab = line.find('\t', start);
        if (tab == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    return fields;
}

bool parseFloat(std::string_view text, float& value)
{
    const std::string trimmed = trimAscii(std::string(text));
    if (trimmed.empty()) {
        return false;
    }
    const char* begin = trimmed.data();
    const char* end = begin + trimmed.size();
    float parsed = 0.0f;
    const std::from_chars_result result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(parsed)) {
        return false;
    }
    value = parsed;
    return true;
}

float parseFloatOr(std::string_view text, float fallback)
{
    float value = fallback;
    return parseFloat(text, value) ? value : fallback;
}

bool parseBoolOr(std::string_view text, bool fallback)
{
    const std::string value = lowerAscii(trimAscii(std::string(text)));
    if (value.empty()) {
        return fallback;
    }
    if (value == "1" || value == "true" || value == "yes" || value == "on" || value == "loop") {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off" || value == "none") {
        return false;
    }
    return fallback;
}

std::optional<AudioCueType> parseCueType(std::string_view text)
{
    const std::string value = lowerAscii(trimAscii(std::string(text)));
    if (value == "bgm" || value == "music") {
        return AudioCueType::Bgm;
    }
    if (value == "se" || value == "sfx" || value == "sound") {
        return AudioCueType::Se;
    }
    return std::nullopt;
}

bool looksLikeAssetRootPath(const std::filesystem::path& path)
{
    const std::string generic = lowerAscii(path.generic_string());
    return generic.rfind("assets/", 0) == 0 || generic == "assets";
}

std::filesystem::path resolveAudioPath(
    const std::filesystem::path& manifestPath,
    const std::filesystem::path& clipPath)
{
    if (clipPath.is_absolute() || looksLikeAssetRootPath(clipPath)) {
        return clipPath;
    }
    return manifestPath.parent_path() / clipPath;
}

std::string pathForLog(const std::filesystem::path& path)
{
    return path.generic_string();
}

float clampVolume(float volume)
{
    return std::clamp(volume, 0.0f, 1.0f);
}

std::size_t framesFromSampleCount(std::size_t sampleCount, int channels)
{
    if (channels <= 0) {
        return 0;
    }
    return sampleCount / static_cast<std::size_t>(channels);
}

} // namespace

class AudioEngine::Impl {
public:
    ~Impl()
    {
        shutdown();
    }

    bool initialize(const AudioEngineSettings& requestedSettings)
    {
        shutdown();
        settings_ = requestedSettings;
        settings_.sampleRate = std::clamp(settings_.sampleRate, MinSampleRate, MaxSampleRate);
        settings_.channels = std::clamp(settings_.channels, MinChannels, MaxChannels);
        settings_.maxSeVoices = std::max(1, settings_.maxSeVoices);

        if (!settings_.enabled) {
            setLastError("Audio disabled by settings");
            return false;
        }

        const SDL_AudioSpec spec{
            .format = OutputFormat,
            .channels = settings_.channels,
            .freq = settings_.sampleRate,
        };

        SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
            &spec,
            &AudioEngine::Impl::audioCallback,
            this);
        if (stream == nullptr) {
            setLastError(std::string("SDL_OpenAudioDeviceStream failed: ") + SDL_GetError());
            return false;
        }
        if (!SDL_ResumeAudioStreamDevice(stream)) {
            const std::string error = std::string("SDL_ResumeAudioStreamDevice failed: ") + SDL_GetError();
            SDL_DestroyAudioStream(stream);
            setLastError(error);
            return false;
        }

        {
            std::scoped_lock lock(mutex_);
            stream_ = stream;
            initialized_ = true;
            lastError_.clear();
            voices_.clear();
            playClockSeconds_ = 0.0;
            missingCueWarnings_.clear();
        }
        logInfo("Audio initialized");
        return true;
    }

    void shutdown()
    {
        SDL_AudioStream* stream = nullptr;
        {
            std::scoped_lock lock(mutex_);
            stream = stream_;
            stream_ = nullptr;
            initialized_ = false;
            voices_.clear();
            cues_.clear();
            currentBgmId_.clear();
            missingCueWarnings_.clear();
            seCooldownUntil_.clear();
        }
        if (stream != nullptr) {
            SDL_DestroyAudioStream(stream);
        }
    }

    void update(float dt)
    {
        if (dt <= 0.0f) {
            return;
        }
        std::scoped_lock lock(mutex_);
        playClockSeconds_ += static_cast<double>(dt);
    }

    bool available() const
    {
        std::scoped_lock lock(mutex_);
        return initialized_ && stream_ != nullptr;
    }

    std::string lastError() const
    {
        std::scoped_lock lock(mutex_);
        return lastError_;
    }

    bool loadManifest(const std::filesystem::path& path)
    {
        manifestPath_ = path;
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            std::scoped_lock lock(mutex_);
            cues_.clear();
            logInfo("Audio manifest not found: " + pathForLog(path));
            return true;
        }

        std::unordered_map<std::string, Cue> loadedCues;
        std::string line;
        int lineNumber = 0;
        int loadedCount = 0;
        while (std::getline(file, line)) {
            ++lineNumber;
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (lineNumber == 1) {
                stripUtf8Bom(line);
            }

            std::string trimmed = trimAscii(line);
            if (trimmed.empty() || trimmed[0] == '#') {
                continue;
            }

            std::vector<std::string> fields = splitTabs(line);
            if (!fields.empty() && lowerAscii(trimAscii(fields[0])) == "id") {
                continue;
            }
            if (fields.size() < 3) {
                logWarning("Audio manifest row skipped: " + pathForLog(path) + ":" + std::to_string(lineNumber));
                continue;
            }

            std::string id = trimAscii(fields[0]);
            const std::optional<AudioCueType> type = parseCueType(fields[1]);
            const std::string clipPathText = trimAscii(fields[2]);
            if (id.empty() || !type || clipPathText.empty()) {
                logWarning("Audio manifest row skipped: " + pathForLog(path) + ":" + std::to_string(lineNumber));
                continue;
            }

            AudioCueOptions options;
            options.volume = clampVolume(fields.size() >= 4 ? parseFloatOr(fields[3], 1.0f) : 1.0f);
            options.loop = fields.size() >= 5 ? parseBoolOr(fields[4], *type == AudioCueType::Bgm) : (*type == AudioCueType::Bgm);
            const float cooldownMs = fields.size() >= 6 ? std::max(0.0f, parseFloatOr(fields[5], 0.0f)) : 0.0f;
            options.cooldownSeconds = cooldownMs / 1000.0f;

            Cue cue;
            if (!loadCueData(id, *type, resolveAudioPath(path, clipPathText), options, cue)) {
                continue;
            }
            loadedCues[id] = std::move(cue);
            ++loadedCount;
        }

        {
            std::scoped_lock lock(mutex_);
            cues_ = std::move(loadedCues);
            missingCueWarnings_.clear();
        }
        logInfo("Audio manifest loaded: " + pathForLog(path) + " cues=" + std::to_string(loadedCount));
        return true;
    }

    bool reloadManifest()
    {
        if (manifestPath_.empty()) {
            return true;
        }
        return loadManifest(manifestPath_);
    }

    void clearCues()
    {
        std::scoped_lock lock(mutex_);
        cues_.clear();
        missingCueWarnings_.clear();
    }

    bool loadCue(std::string id, AudioCueType type, const std::filesystem::path& path, AudioCueOptions options)
    {
        Cue cue;
        if (!loadCueData(id, type, path, options, cue)) {
            return false;
        }
        std::scoped_lock lock(mutex_);
        cues_[cue.id] = std::move(cue);
        missingCueWarnings_.erase(id);
        return true;
    }

    void playBgm(std::string_view id, float fadeSeconds, bool restart)
    {
        const std::string key(id);
        std::scoped_lock lock(mutex_);
        const Cue* cue = findCueLocked(key, AudioCueType::Bgm);
        if (cue == nullptr) {
            return;
        }
        if (!restart && currentBgmId_ == key && hasActiveBgmLocked(key)) {
            return;
        }

        const int fadeFrames = fadeFramesForSeconds(fadeSeconds);
        if (fadeFrames <= 0) {
            eraseBgmVoicesLocked();
        } else {
            fadeOutBgmVoicesLocked(fadeFrames);
        }

        Voice voice;
        voice.id = cue->id;
        voice.type = AudioCueType::Bgm;
        voice.sound = cue->sound;
        voice.loop = cue->options.loop;
        voice.baseVolume = clampVolume(cue->options.volume);
        voice.currentVolume = fadeFrames > 0 ? 0.0f : voice.baseVolume;
        voice.targetVolume = voice.baseVolume;
        voice.fadeRemainingFrames = fadeFrames;
        voice.fadeDeltaPerFrame = fadeFrames > 0 ? voice.targetVolume / static_cast<float>(fadeFrames) : 0.0f;
        voices_.push_back(std::move(voice));
        currentBgmId_ = key;
    }

    void stopBgm(float fadeSeconds)
    {
        std::scoped_lock lock(mutex_);
        const int fadeFrames = fadeFramesForSeconds(fadeSeconds);
        if (fadeFrames <= 0) {
            eraseBgmVoicesLocked();
        } else {
            fadeOutBgmVoicesLocked(fadeFrames);
        }
        currentBgmId_.clear();
    }

    void playSe(std::string_view id, float volumeScale)
    {
        if (volumeScale <= 0.0f) {
            return;
        }

        const std::string key(id);
        std::scoped_lock lock(mutex_);
        const Cue* cue = findCueLocked(key, AudioCueType::Se);
        if (cue == nullptr) {
            return;
        }

        const double cooldownUntil = seCooldownUntil_[key];
        if (playClockSeconds_ < cooldownUntil) {
            return;
        }
        if (cue->options.cooldownSeconds > 0.0f) {
            seCooldownUntil_[key] = playClockSeconds_ + static_cast<double>(cue->options.cooldownSeconds);
        }

        trimSeVoiceCountLocked(settings_.maxSeVoices - 1);

        Voice voice;
        voice.id = cue->id;
        voice.type = AudioCueType::Se;
        voice.sound = cue->sound;
        voice.loop = false;
        voice.baseVolume = clampVolume(cue->options.volume * volumeScale);
        voice.currentVolume = voice.baseVolume;
        voice.targetVolume = voice.baseVolume;
        voices_.push_back(std::move(voice));
    }

    void stopAll()
    {
        std::scoped_lock lock(mutex_);
        voices_.clear();
        currentBgmId_.clear();
    }

    void setMasterVolume(float volume)
    {
        std::scoped_lock lock(mutex_);
        masterVolume_ = clampVolume(volume);
    }

    void setBgmVolume(float volume)
    {
        std::scoped_lock lock(mutex_);
        bgmVolume_ = clampVolume(volume);
    }

    void setSeVolume(float volume)
    {
        std::scoped_lock lock(mutex_);
        seVolume_ = clampVolume(volume);
    }

    float masterVolume() const
    {
        std::scoped_lock lock(mutex_);
        return masterVolume_;
    }

    float bgmVolume() const
    {
        std::scoped_lock lock(mutex_);
        return bgmVolume_;
    }

    float seVolume() const
    {
        std::scoped_lock lock(mutex_);
        return seVolume_;
    }

private:
    struct SoundData {
        std::vector<float> samples;
        int channels = 2;
        int sampleRate = 48000;
        std::size_t frames = 0;
    };

    struct Cue {
        std::string id;
        AudioCueType type = AudioCueType::Se;
        std::filesystem::path path;
        AudioCueOptions options;
        std::shared_ptr<const SoundData> sound;
    };

    struct Voice {
        std::string id;
        AudioCueType type = AudioCueType::Se;
        std::shared_ptr<const SoundData> sound;
        std::size_t frameIndex = 0;
        bool loop = false;
        bool stoppingAfterFade = false;
        float baseVolume = 1.0f;
        float currentVolume = 1.0f;
        float targetVolume = 1.0f;
        float fadeDeltaPerFrame = 0.0f;
        int fadeRemainingFrames = 0;
        bool finished = false;
    };

    static void SDLCALL audioCallback(void* userdata, SDL_AudioStream* stream, int additionalAmount, int)
    {
        auto* self = static_cast<AudioEngine::Impl*>(userdata);
        if (self == nullptr || stream == nullptr || additionalAmount <= 0) {
            return;
        }
        self->queueAudio(stream, additionalAmount);
    }

    void queueAudio(SDL_AudioStream* stream, int requestedBytes)
    {
        const int frameSize = settings_.channels * static_cast<int>(sizeof(float));
        if (frameSize <= 0) {
            return;
        }
        const int frames = requestedBytes / frameSize;
        if (frames <= 0) {
            return;
        }

        std::vector<float> buffer(static_cast<std::size_t>(frames * settings_.channels), 0.0f);
        {
            std::scoped_lock lock(mutex_);
            mixLocked(buffer.data(), frames);
        }
        (void)SDL_PutAudioStreamData(stream, buffer.data(), frames * frameSize);
    }

    void mixLocked(float* out, int frames)
    {
        if (out == nullptr || frames <= 0) {
            return;
        }

        for (int frame = 0; frame < frames; ++frame) {
            for (Voice& voice : voices_) {
                if (voice.finished || !voice.sound || voice.sound->frames == 0) {
                    voice.finished = true;
                    continue;
                }

                if (voice.frameIndex >= voice.sound->frames) {
                    if (voice.loop) {
                        voice.frameIndex = 0;
                    } else {
                        voice.finished = true;
                        continue;
                    }
                }

                const float busVolume = voice.type == AudioCueType::Bgm ? bgmVolume_ : seVolume_;
                const float voiceVolume = masterVolume_ * busVolume * voice.currentVolume;
                const std::size_t sourceBase = voice.frameIndex * static_cast<std::size_t>(settings_.channels);
                const std::size_t outputBase = static_cast<std::size_t>(frame * settings_.channels);
                for (int channel = 0; channel < settings_.channels; ++channel) {
                    out[outputBase + static_cast<std::size_t>(channel)] +=
                        voice.sound->samples[sourceBase + static_cast<std::size_t>(channel)] * voiceVolume;
                }

                ++voice.frameIndex;
                if (voice.frameIndex >= voice.sound->frames && !voice.loop) {
                    voice.finished = true;
                }
                stepFade(voice);
            }
        }

        const std::size_t sampleCount = static_cast<std::size_t>(frames * settings_.channels);
        for (std::size_t i = 0; i < sampleCount; ++i) {
            out[i] = std::clamp(out[i], -1.0f, 1.0f);
        }

        voices_.erase(
            std::remove_if(voices_.begin(), voices_.end(), [](const Voice& voice) {
                return voice.finished;
            }),
            voices_.end());
    }

    void stepFade(Voice& voice) const
    {
        if (voice.fadeRemainingFrames <= 0) {
            return;
        }

        voice.currentVolume += voice.fadeDeltaPerFrame;
        --voice.fadeRemainingFrames;
        if (voice.fadeRemainingFrames <= 0) {
            voice.currentVolume = voice.targetVolume;
            voice.fadeDeltaPerFrame = 0.0f;
            if (voice.stoppingAfterFade) {
                voice.finished = true;
            }
        }
    }

    bool loadCueData(
        const std::string& id,
        AudioCueType type,
        const std::filesystem::path& path,
        AudioCueOptions options,
        Cue& outCue)
    {
        if (id.empty()) {
            setLastError("Audio cue id is empty");
            return false;
        }

        SDL_AudioSpec sourceSpec{};
        Uint8* sourceBytes = nullptr;
        Uint32 sourceLength = 0;
        if (!SDL_LoadWAV(path.string().c_str(), &sourceSpec, &sourceBytes, &sourceLength)) {
            setLastError(std::string("SDL_LoadWAV failed: ") + pathForLog(path) + ": " + SDL_GetError());
            logWarning(lastError());
            return false;
        }

        const SDL_AudioSpec outputSpec{
            .format = OutputFormat,
            .channels = settings_.channels,
            .freq = settings_.sampleRate,
        };

        Uint8* convertedBytes = nullptr;
        int convertedLength = 0;
        const bool converted = SDL_ConvertAudioSamples(
            &sourceSpec,
            sourceBytes,
            static_cast<int>(std::min<Uint32>(sourceLength, static_cast<Uint32>(std::numeric_limits<int>::max()))),
            &outputSpec,
            &convertedBytes,
            &convertedLength);
        SDL_free(sourceBytes);

        if (!converted) {
            setLastError(std::string("SDL_ConvertAudioSamples failed: ") + pathForLog(path) + ": " + SDL_GetError());
            logWarning(lastError());
            return false;
        }
        if (convertedBytes == nullptr || convertedLength <= 0) {
            if (convertedBytes != nullptr) {
                SDL_free(convertedBytes);
            }
            setLastError("Audio clip converted to empty buffer: " + pathForLog(path));
            logWarning(lastError());
            return false;
        }

        auto sound = std::make_shared<SoundData>();
        sound->channels = outputSpec.channels;
        sound->sampleRate = outputSpec.freq;
        const std::size_t sampleCount = static_cast<std::size_t>(convertedLength) / sizeof(float);
        sound->samples.resize(sampleCount);
        std::memcpy(sound->samples.data(), convertedBytes, sampleCount * sizeof(float));
        SDL_free(convertedBytes);
        sound->frames = framesFromSampleCount(sound->samples.size(), sound->channels);
        if (sound->frames == 0) {
            setLastError("Audio clip has no frames: " + pathForLog(path));
            logWarning(lastError());
            return false;
        }

        outCue.id = id;
        outCue.type = type;
        outCue.path = path;
        outCue.options = options;
        outCue.options.volume = clampVolume(outCue.options.volume);
        outCue.options.cooldownSeconds = std::max(0.0f, outCue.options.cooldownSeconds);
        outCue.sound = std::move(sound);
        return true;
    }

    const Cue* findCueLocked(const std::string& id, AudioCueType expectedType)
    {
        const auto it = cues_.find(id);
        if (it == cues_.end()) {
            warnMissingCueLocked(id);
            return nullptr;
        }
        if (it->second.type != expectedType) {
            warnMissingCueLocked(id + " type mismatch");
            return nullptr;
        }
        if (!it->second.sound) {
            warnMissingCueLocked(id + " missing data");
            return nullptr;
        }
        return &it->second;
    }

    void warnMissingCueLocked(const std::string& message)
    {
        if (missingCueWarnings_.insert(message).second) {
            logWarning("Audio cue unavailable: " + message);
        }
    }

    int fadeFramesForSeconds(float seconds) const
    {
        if (seconds <= 0.0f) {
            return 0;
        }
        const double frames = static_cast<double>(seconds) * static_cast<double>(settings_.sampleRate);
        return static_cast<int>(std::clamp(frames, 1.0, static_cast<double>(std::numeric_limits<int>::max())));
    }

    bool hasActiveBgmLocked(const std::string& id) const
    {
        return std::any_of(voices_.begin(), voices_.end(), [&id](const Voice& voice) {
            return voice.type == AudioCueType::Bgm && voice.id == id && !voice.finished;
        });
    }

    void eraseBgmVoicesLocked()
    {
        voices_.erase(
            std::remove_if(voices_.begin(), voices_.end(), [](const Voice& voice) {
                return voice.type == AudioCueType::Bgm;
            }),
            voices_.end());
    }

    void fadeOutBgmVoicesLocked(int fadeFrames)
    {
        for (Voice& voice : voices_) {
            if (voice.type != AudioCueType::Bgm) {
                continue;
            }
            voice.targetVolume = 0.0f;
            voice.fadeRemainingFrames = std::max(1, fadeFrames);
            voice.fadeDeltaPerFrame = -voice.currentVolume / static_cast<float>(voice.fadeRemainingFrames);
            voice.stoppingAfterFade = true;
        }
    }

    void trimSeVoiceCountLocked(int maxSeVoices)
    {
        int seCount = 0;
        for (const Voice& voice : voices_) {
            if (voice.type == AudioCueType::Se) {
                ++seCount;
            }
        }
        while (seCount > maxSeVoices) {
            const auto it = std::find_if(voices_.begin(), voices_.end(), [](const Voice& voice) {
                return voice.type == AudioCueType::Se;
            });
            if (it == voices_.end()) {
                return;
            }
            voices_.erase(it);
            --seCount;
        }
    }

    void setLastError(std::string error)
    {
        std::scoped_lock lock(mutex_);
        lastError_ = std::move(error);
    }

    mutable std::mutex mutex_;
    SDL_AudioStream* stream_ = nullptr;
    AudioEngineSettings settings_{};
    bool initialized_ = false;
    std::string lastError_;
    std::filesystem::path manifestPath_;
    std::unordered_map<std::string, Cue> cues_;
    std::vector<Voice> voices_;
    std::unordered_set<std::string> missingCueWarnings_;
    std::unordered_map<std::string, double> seCooldownUntil_;
    std::string currentBgmId_;
    double playClockSeconds_ = 0.0;
    float masterVolume_ = 1.0f;
    float bgmVolume_ = 1.0f;
    float seVolume_ = 1.0f;
};

AudioEngine::AudioEngine()
    : impl_(std::make_unique<Impl>())
{
}

AudioEngine::~AudioEngine() = default;

AudioEngine::AudioEngine(AudioEngine&&) noexcept = default;

AudioEngine& AudioEngine::operator=(AudioEngine&&) noexcept = default;

bool AudioEngine::initialize(const AudioEngineSettings& settings)
{
    return impl_->initialize(settings);
}

void AudioEngine::shutdown()
{
    impl_->shutdown();
}

void AudioEngine::update(float dt)
{
    impl_->update(dt);
}

bool AudioEngine::available() const
{
    return impl_->available();
}

std::string AudioEngine::lastError() const
{
    return impl_->lastError();
}

bool AudioEngine::loadManifest(const std::filesystem::path& path)
{
    return impl_->loadManifest(path);
}

bool AudioEngine::reloadManifest()
{
    return impl_->reloadManifest();
}

void AudioEngine::clearCues()
{
    impl_->clearCues();
}

bool AudioEngine::loadCue(
    std::string id,
    AudioCueType type,
    const std::filesystem::path& path,
    AudioCueOptions options)
{
    return impl_->loadCue(std::move(id), type, path, options);
}

void AudioEngine::playBgm(std::string_view id, float fadeSeconds, bool restart)
{
    impl_->playBgm(id, fadeSeconds, restart);
}

void AudioEngine::stopBgm(float fadeSeconds)
{
    impl_->stopBgm(fadeSeconds);
}

void AudioEngine::playSe(std::string_view id, float volumeScale)
{
    impl_->playSe(id, volumeScale);
}

void AudioEngine::stopAll()
{
    impl_->stopAll();
}

void AudioEngine::setMasterVolume(float volume)
{
    impl_->setMasterVolume(volume);
}

void AudioEngine::setBgmVolume(float volume)
{
    impl_->setBgmVolume(volume);
}

void AudioEngine::setSeVolume(float volume)
{
    impl_->setSeVolume(volume);
}

float AudioEngine::masterVolume() const
{
    return impl_->masterVolume();
}

float AudioEngine::bgmVolume() const
{
    return impl_->bgmVolume();
}

float AudioEngine::seVolume() const
{
    return impl_->seVolume();
}

}
