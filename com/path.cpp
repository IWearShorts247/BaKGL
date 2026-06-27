#include "com/path.hpp"
#include "com/logger.hpp"

#if defined(_WIN32)
#include <windows.h>
#endif

std::filesystem::path GetExecutableDirectory()
{
    std::error_code ec{};
#if defined(_WIN32)
    char buf[MAX_PATH];
    const auto len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH)
    {
        return std::filesystem::path{std::string{buf, len}}.parent_path();
    }
#else
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec)
    {
        return exe.parent_path();
    }
#endif
    return std::filesystem::current_path(ec);
}

std::string GetHomeDirectory()
{
#if defined(_WIN32)
    constexpr auto homeVar = "APPDATA";
#else
    constexpr auto homeVar = "HOME";
#endif
    auto* home = getenv(homeVar);

    if (home == nullptr)
    {
        throw std::runtime_error("No " + std::string{homeVar} + " environment variable set.");
    }

    return std::string{home};
}

Paths& Paths::Get()
{
    static Paths paths{};
    return paths;
}

std::filesystem::path Paths::GetBakDirectoryPath() const
{
    return mBakDirectoryPath;
}

std::string Paths::GetBakDirectory() const
{
    return GetBakDirectoryPath().string();
}

std::filesystem::path Paths::GetModDirectoryPath() const
{
    return mModDirectoryPath;
}

std::string Paths::GetModDirectory() const
{
    return GetModDirectoryPath().string();
}

void Paths::SetBakDirectory(std::string path)
{
    Logging::LogInfo(__FUNCTION__) << "Overriding BAK path to: " << path << "\n";
    mBakDirectoryPath = path;
}

void Paths::SetModDirectory(std::string path)
{
    Logging::LogInfo(__FUNCTION__) << "Overriding Mod path to: " << path << "\n";
    mModDirectoryPath = path;
}

Paths::Paths()
:
    mBakDirectoryPath{std::filesystem::path{GetHomeDirectory()} / "bak"},
    mModDirectoryPath{""}
{}
