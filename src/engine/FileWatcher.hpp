#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace majo {

class FileWatcher {
public:
    FileWatcher();
    ~FileWatcher();

    FileWatcher(FileWatcher&&) noexcept;
    FileWatcher& operator=(FileWatcher&&) noexcept;

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    void watchPath(std::filesystem::path path);
    void reset();
    bool poll(std::string& changedPath);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
