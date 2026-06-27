#pragma once

#include <filesystem>
#include <string>

std::string GetHomeDirectory();
// Directory containing the running executable (for locating bundled data/ regardless of CWD).
std::filesystem::path GetExecutableDirectory();
class Paths
{
public:
    static Paths& Get();

    std::filesystem::path GetBakDirectoryPath() const;
    std::string GetBakDirectory() const;

    std::filesystem::path GetModDirectoryPath() const;
    std::string GetModDirectory() const;

    void SetBakDirectory(std::string path);
    void SetModDirectory(std::string path);
private:
    Paths();

    std::filesystem::path mBakDirectoryPath;
    std::filesystem::path mModDirectoryPath;
};


