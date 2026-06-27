#include "bak/file/util.hpp"

#include "com/logger.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace BAK::File {

unsigned GetStreamSize(std::ifstream& ifs)
{
    ifs.ignore( std::numeric_limits<std::streamsize>::max() );
    std::streamsize length = ifs.gcount();
    ifs.clear();
    ifs.seekg( 0, std::ios_base::beg );
    return static_cast<unsigned>(length);
}

FileBuffer CreateFileBuffer(const std::string& fileName)
{
    Logging::LogInfo(__FUNCTION__) << "Opening: " << fileName << std::endl;
    // NB: read via stdio (fopen) rather than std::ifstream. On the mingw-w64 Windows
    // build, libstdc++ basic_filebuf::open() fails for perfectly valid paths (it errors
    // with ENOENT even though the file exists and fopen() on the identical path in the
    // same process succeeds — a global C-locale change by a dependency breaks filebuf's
    // narrow->wide path conversion). fopen is unaffected and works on every platform.
    std::FILE* in = std::fopen(fileName.c_str(), "rb");
    if (in == nullptr)
    {
        const int openErrno = errno;
        std::stringstream ss{};
        ss << __FILE__ << ":" << __LINE__ << " " << __FUNCTION__ << " OpenError! ["
           << fileName << "] errno=" << openErrno << " (" << std::strerror(openErrno) << ")";
        Logging::LogFatal("FileBuffer") << ss.str() << std::endl;
        throw std::runtime_error(ss.str());
    }

    std::fseek(in, 0, SEEK_END);
    const long size = std::ftell(in);
    std::fseek(in, 0, SEEK_SET);

    FileBuffer fb{static_cast<unsigned>(size < 0 ? 0 : size)};
    fb.Load(in);
    std::fclose(in);
    return fb;
}

}
