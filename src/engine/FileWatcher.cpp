#include "engine/FileWatcher.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace majo {

namespace {

std::string pathToUtf8(const std::filesystem::path& path)
{
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
    const std::u8string encoded = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
#else
    return path.generic_u8string();
#endif
}

} // namespace

struct FileWatcher::Impl {
    std::vector<std::filesystem::path> roots;

#ifdef _WIN32
    using Clock = std::chrono::steady_clock;

    static constexpr auto ChangeDebounce = std::chrono::milliseconds(150);
    static constexpr std::size_t NotificationBufferBytes = 64 * 1024;

    struct PendingChange {
        std::string path;
        Clock::time_point readyAt{};
        bool createdSinceLastPoll = false;
    };

    struct WatchRegistration {
        std::filesystem::path outputRoot;
        std::filesystem::path directory;
        std::wstring fileNameFilter;
        HANDLE directoryHandle = INVALID_HANDLE_VALUE;
        HANDLE stopEvent = nullptr;
        HANDLE completionEvent = nullptr;
        bool recursive = true;
        std::thread worker;
    };

    std::vector<std::unique_ptr<WatchRegistration>> registrations;
    std::unordered_map<std::string, PendingChange> pendingChanges;
    std::mutex pendingMutex;
    std::atomic<bool> stopping = false;

    ~Impl()
    {
        stop();
    }

    void stop()
    {
        stopping.store(true, std::memory_order_release);
        for (const auto& registration : registrations) {
            if (registration->stopEvent != nullptr) {
                SetEvent(registration->stopEvent);
            }
        }
        for (const auto& registration : registrations) {
            if (registration->worker.joinable()) {
                registration->worker.join();
            }
            if (registration->directoryHandle != INVALID_HANDLE_VALUE) {
                CloseHandle(registration->directoryHandle);
                registration->directoryHandle = INVALID_HANDLE_VALUE;
            }
            if (registration->stopEvent != nullptr) {
                CloseHandle(registration->stopEvent);
                registration->stopEvent = nullptr;
            }
            if (registration->completionEvent != nullptr) {
                CloseHandle(registration->completionEvent);
                registration->completionEvent = nullptr;
            }
        }
        registrations.clear();
    }

    static bool fileNamesEqual(std::wstring_view left, std::wstring_view right)
    {
        return CompareStringOrdinal(
                   left.data(),
                   static_cast<int>(left.size()),
                   right.data(),
                   static_cast<int>(right.size()),
                   TRUE) == CSTR_EQUAL;
    }

    void discardPendingChange(const std::filesystem::path& path)
    {
        const std::string normalized = pathToUtf8(path.lexically_normal());
        std::lock_guard<std::mutex> lock(pendingMutex);
        pendingChanges.erase(normalized);
    }

    void queueChange(const std::filesystem::path& path, DWORD action)
    {
        const std::string normalized = pathToUtf8(path.lexically_normal());
        const auto readyAt = Clock::now() + ChangeDebounce;
        std::lock_guard<std::mutex> lock(pendingMutex);

        auto existing = pendingChanges.find(normalized);
        if (action == FILE_ACTION_REMOVED &&
            existing != pendingChanges.end() &&
            existing->second.createdSinceLastPoll) {
            pendingChanges.erase(existing);
            return;
        }

        const bool created = action == FILE_ACTION_ADDED || action == FILE_ACTION_RENAMED_NEW_NAME;
        if (existing == pendingChanges.end()) {
            pendingChanges.emplace(normalized, PendingChange{normalized, readyAt, created});
            return;
        }

        existing->second.readyAt = readyAt;
        existing->second.createdSinceLastPoll = existing->second.createdSinceLastPoll || created;
    }

    void handleNotification(
        const WatchRegistration& registration,
        std::wstring_view relativeName,
        DWORD action)
    {
        if (!registration.fileNameFilter.empty() &&
            !fileNamesEqual(relativeName, registration.fileNameFilter)) {
            return;
        }

        const std::filesystem::path changedPath = registration.fileNameFilter.empty()
            ? registration.outputRoot / std::filesystem::path(relativeName)
            : registration.outputRoot;
        if (action == FILE_ACTION_RENAMED_OLD_NAME) {
            discardPendingChange(changedPath);
            return;
        }
        queueChange(changedPath, action);
    }

    void watchDirectory(WatchRegistration& registration)
    {
        alignas(FILE_NOTIFY_INFORMATION) std::array<std::byte, NotificationBufferBytes> buffer{};
        constexpr DWORD NotifyFilter =
            FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_LAST_WRITE |
            FILE_NOTIFY_CHANGE_SIZE |
            FILE_NOTIFY_CHANGE_CREATION;

        while (!stopping.load(std::memory_order_acquire)) {
            ResetEvent(registration.completionEvent);
            OVERLAPPED overlapped{};
            overlapped.hEvent = registration.completionEvent;
            DWORD bytesReturned = 0;
            const BOOL succeeded = ReadDirectoryChangesW(
                registration.directoryHandle,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                registration.recursive ? TRUE : FALSE,
                NotifyFilter,
                nullptr,
                &overlapped,
                nullptr);
            const DWORD readError = succeeded ? ERROR_SUCCESS : GetLastError();
            if (!succeeded && readError != ERROR_IO_PENDING) {
                const DWORD error = readError;
                if (stopping.load(std::memory_order_acquire) || error == ERROR_OPERATION_ABORTED) {
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            const std::array<HANDLE, 2> waitHandles{
                registration.stopEvent,
                registration.completionEvent,
            };
            const DWORD waitResult = WaitForMultipleObjects(
                static_cast<DWORD>(waitHandles.size()),
                waitHandles.data(),
                FALSE,
                INFINITE);
            if (waitResult == WAIT_OBJECT_0) {
                CancelIoEx(registration.directoryHandle, &overlapped);
                GetOverlappedResult(registration.directoryHandle, &overlapped, &bytesReturned, TRUE);
                return;
            }
            if (waitResult != WAIT_OBJECT_0 + 1) {
                CancelIoEx(registration.directoryHandle, &overlapped);
                GetOverlappedResult(registration.directoryHandle, &overlapped, &bytesReturned, TRUE);
                if (stopping.load(std::memory_order_acquire)) {
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            if (!GetOverlappedResult(registration.directoryHandle, &overlapped, &bytesReturned, FALSE)) {
                const DWORD error = GetLastError();
                if (stopping.load(std::memory_order_acquire) || error == ERROR_OPERATION_ABORTED) {
                    return;
                }
                continue;
            }
            if (bytesReturned == 0) {
                queueChange(registration.outputRoot, FILE_ACTION_MODIFIED);
                continue;
            }

            std::size_t offset = 0;
            while (offset + sizeof(FILE_NOTIFY_INFORMATION) <= bytesReturned) {
                const auto* notification = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer.data() + offset);
                const std::wstring_view relativeName(
                    notification->FileName,
                    notification->FileNameLength / sizeof(wchar_t));
                handleNotification(registration, relativeName, notification->Action);
                if (notification->NextEntryOffset == 0) {
                    break;
                }
                offset += notification->NextEntryOffset;
            }
        }
    }

    void start()
    {
        stop();
        {
            std::lock_guard<std::mutex> lock(pendingMutex);
            pendingChanges.clear();
        }
        stopping.store(false, std::memory_order_release);

        for (const std::filesystem::path& root : roots) {
            std::error_code error;
            const std::filesystem::path absoluteRoot = std::filesystem::absolute(root, error);
            if (error) {
                continue;
            }

            auto registration = std::make_unique<WatchRegistration>();
            registration->outputRoot = root.lexically_normal();
            if (std::filesystem::is_regular_file(absoluteRoot, error) && !error) {
                registration->directory = absoluteRoot.parent_path();
                registration->fileNameFilter = absoluteRoot.filename().wstring();
                registration->recursive = false;
            } else if (std::filesystem::is_directory(absoluteRoot, error) && !error) {
                registration->directory = absoluteRoot;
            } else {
                continue;
            }

            registration->directoryHandle = CreateFileW(
                registration->directory.c_str(),
                FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                nullptr);
            if (registration->directoryHandle == INVALID_HANDLE_VALUE) {
                continue;
            }
            registration->stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            registration->completionEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (registration->stopEvent == nullptr || registration->completionEvent == nullptr) {
                if (registration->stopEvent != nullptr) {
                    CloseHandle(registration->stopEvent);
                }
                if (registration->completionEvent != nullptr) {
                    CloseHandle(registration->completionEvent);
                }
                CloseHandle(registration->directoryHandle);
                continue;
            }

            registrations.push_back(std::move(registration));
            WatchRegistration* started = registrations.back().get();
            started->worker = std::thread([this, started]() {
                watchDirectory(*started);
            });
        }
    }

    bool poll(std::string& changedPath)
    {
        const auto now = Clock::now();
        std::lock_guard<std::mutex> lock(pendingMutex);
        auto ready = pendingChanges.end();
        for (auto it = pendingChanges.begin(); it != pendingChanges.end(); ++it) {
            if (it->second.readyAt > now) {
                continue;
            }
            if (ready == pendingChanges.end() || it->second.readyAt < ready->second.readyAt) {
                ready = it;
            }
        }
        if (ready == pendingChanges.end()) {
            return false;
        }

        changedPath = std::move(ready->second.path);
        pendingChanges.erase(ready);
        return true;
    }
#else
    std::unordered_map<std::string, std::filesystem::file_time_type> knownFiles;

    void scan(bool reportChanges, std::string& changedPath, bool& changed)
    {
        std::unordered_map<std::string, std::filesystem::file_time_type> current;
        for (const std::filesystem::path& root : roots) {
            std::error_code error;
            if (!std::filesystem::exists(root, error)) {
                continue;
            }

            auto record = [&](const std::filesystem::path& filePath) {
                std::error_code timeError;
                const auto writeTime = std::filesystem::last_write_time(filePath, timeError);
                if (timeError) {
                    return;
                }
                const std::string key = pathToUtf8(filePath.lexically_normal());
                current[key] = writeTime;
                if (!reportChanges || changed) {
                    return;
                }
                const auto known = knownFiles.find(key);
                if (known == knownFiles.end() || known->second != writeTime) {
                    changed = true;
                    changedPath = key;
                }
            };

            if (std::filesystem::is_regular_file(root, error)) {
                record(root);
                continue;
            }

            std::filesystem::recursive_directory_iterator it(
                root,
                std::filesystem::directory_options::skip_permission_denied,
                error);
            const std::filesystem::recursive_directory_iterator end;
            for (; !error && it != end; it.increment(error)) {
                if (it->is_regular_file(error)) {
                    record(it->path());
                }
            }
        }

        if (reportChanges && !changed) {
            for (const auto& [path, _] : knownFiles) {
                if (!current.contains(path)) {
                    changed = true;
                    changedPath = path;
                    break;
                }
            }
        }
        knownFiles = std::move(current);
    }

    void start()
    {
        knownFiles.clear();
        std::string unusedPath;
        bool unusedChanged = false;
        scan(false, unusedPath, unusedChanged);
    }

    bool poll(std::string& changedPath)
    {
        bool changed = false;
        scan(true, changedPath, changed);
        return changed;
    }
#endif
};

FileWatcher::FileWatcher()
    : impl_(std::make_unique<Impl>())
{
}

FileWatcher::~FileWatcher() = default;
FileWatcher::FileWatcher(FileWatcher&&) noexcept = default;
FileWatcher& FileWatcher::operator=(FileWatcher&&) noexcept = default;

void FileWatcher::watchPath(std::filesystem::path path)
{
    impl_->roots.push_back(std::move(path));
}

void FileWatcher::reset()
{
    impl_->start();
}

bool FileWatcher::poll(std::string& changedPath)
{
    return impl_->poll(changedPath);
}

} // namespace majo
