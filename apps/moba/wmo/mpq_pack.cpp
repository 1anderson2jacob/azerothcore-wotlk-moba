// Build a 3.3.5a client patch archive from a directory tree.
//
// Links pywowlib's already-built StormLib static lib -- the same build that
// produces the storm module every other tool here imports, so this adds no
// setup step beyond what the README's toolchain section already requires.

#include <StormLib.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

// StormLib rounds the hash table up to a power of two regardless; doing it here
// means the count asked for is the count granted.
static DWORD RoundUpPow2(DWORD n)
{
    DWORD v = 16;
    while (v < n)
        v <<= 1;
    return v;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr,
                "usage: mpq_pack OUT.MPQ SRCDIR [SUBDIR ...]\n"
                "\n"
                "Packs SRCDIR -- or only the named SUBDIRs beneath it -- into a fresh\n"
                "MPQ v1 archive. Archive names are paths relative to SRCDIR with '/'\n"
                "turned into '\\'. Files are added locale-neutral and zlib-compressed,\n"
                "matching how Blizzard's own locale patches store DBCs.\n");
        return 2;
    }

    fs::path const out = argv[1];
    fs::path const src = argv[2];

    std::vector<fs::path> roots;
    for (int i = 3; i < argc; ++i)
        roots.push_back(src / argv[i]);
    if (roots.empty())
        roots.push_back(src);

    std::vector<std::pair<fs::path, std::string>> files;
    for (auto const& root : roots)
    {
        if (!fs::exists(root))
        {
            fprintf(stderr, "no such path: %s\n", root.c_str());
            return 1;
        }

        for (auto const& entry : fs::recursive_directory_iterator(root))
        {
            if (!entry.is_regular_file())
                continue;

            std::string name = fs::relative(entry.path(), src).generic_string();
            if (name.empty() || name[0] == '.' || name.find("/.") != std::string::npos)
                continue;   // .DS_Store and friends

            std::replace(name.begin(), name.end(), '/', '\\');
            files.emplace_back(entry.path(), std::move(name));
        }
    }

    if (files.empty())
    {
        fprintf(stderr, "nothing to pack under %s\n", src.c_str());
        return 1;
    }

    std::sort(files.begin(), files.end(),
              [](auto const& a, auto const& b) { return a.second < b.second; });

    std::error_code ec;
    if (fs::exists(out))
    {
        printf("replacing existing %s (%ju bytes)\n",
               out.c_str(), static_cast<std::uintmax_t>(fs::file_size(out, ec)));
        fs::remove(out, ec);
        if (ec)
        {
            fprintf(stderr, "cannot remove %s: %s\n", out.c_str(), ec.message().c_str());
            return 1;
        }
    }

    HANDLE mpq = nullptr;
    DWORD const maxFiles = RoundUpPow2(static_cast<DWORD>(files.size()) + 2);
    if (!SFileCreateArchive(out.c_str(),
                            MPQ_CREATE_ARCHIVE_V1 | MPQ_CREATE_LISTFILE,
                            maxFiles, &mpq))
    {
        fprintf(stderr, "SFileCreateArchive failed on %s (%u)\n", out.c_str(), GetLastError());
        return 1;
    }

    std::uintmax_t bytesIn = 0;
    for (auto const& [local, name] : files)
    {
        if (!SFileAddFileEx(mpq, local.c_str(), name.c_str(),
                            MPQ_FILE_COMPRESS | MPQ_FILE_REPLACEEXISTING,
                            MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_NEXT_SAME))
        {
            fprintf(stderr, "failed to add %s (%u)\n", name.c_str(), GetLastError());
            SFileCloseArchive(mpq);
            return 1;
        }

        bytesIn += fs::file_size(local, ec);
        printf("  %s\n", name.c_str());
    }

    SFileCloseArchive(mpq);

    printf("\n%zu files, %ju bytes -> %ju bytes  %s\n",
           files.size(), bytesIn,
           static_cast<std::uintmax_t>(fs::file_size(out, ec)), out.c_str());
    return 0;
}
