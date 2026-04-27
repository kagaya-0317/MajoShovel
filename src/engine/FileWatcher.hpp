#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace majo {

class FileWatcher {
public:
    void watchPath(std::filesystem::path path);
    void reset();
    bool poll(std::string& changedPath);

private:
    void scan(bool reportChanges, std::string& changedPath, bool& changed);

    std::vector<std::filesystem::path> roots_;
    std::unordered_map<std::string, std::filesystem::file_time_type> knownFiles_;
};

}
