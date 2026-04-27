#include "engine/FileWatcher.hpp"

#include <system_error>

namespace majo {

void FileWatcher::watchPath(std::filesystem::path path)
{
    roots_.push_back(std::move(path));
}

void FileWatcher::reset()
{
    knownFiles_.clear();
    std::string unusedPath;
    bool unusedChanged = false;
    scan(false, unusedPath, unusedChanged);
}

bool FileWatcher::poll(std::string& changedPath)
{
    bool changed = false;
    scan(true, changedPath, changed);
    return changed;
}

void FileWatcher::scan(bool reportChanges, std::string& changedPath, bool& changed)
{
    std::unordered_map<std::string, std::filesystem::file_time_type> current;
    for (const auto& root : roots_) {
        std::error_code ec;
        if (!std::filesystem::exists(root, ec)) {
            continue;
        }

        auto record = [&](const std::filesystem::path& filePath) {
            std::error_code timeEc;
            const auto writeTime = std::filesystem::last_write_time(filePath, timeEc);
            if (timeEc) {
                return;
            }
            const std::string key = filePath.lexically_normal().string();
            current[key] = writeTime;
            if (!reportChanges || changed) {
                return;
            }
            auto known = knownFiles_.find(key);
            if (known == knownFiles_.end() || known->second != writeTime) {
                changed = true;
                changedPath = key;
            }
        };

        if (std::filesystem::is_regular_file(root, ec)) {
            record(root);
            continue;
        }

        std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec);
        std::filesystem::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            if (it->is_regular_file(ec)) {
                record(it->path());
            }
        }
    }

    if (reportChanges && !changed) {
        for (const auto& [path, _] : knownFiles_) {
            if (current.find(path) == current.end()) {
                changed = true;
                changedPath = path;
                break;
            }
        }
    }
    knownFiles_ = std::move(current);
}

}
