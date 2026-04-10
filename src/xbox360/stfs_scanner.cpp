#include "stfs_scanner.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <sstream>

// Windows needs special handling for raw physical drives
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#endif

namespace fs = std::filesystem;

// =====================================================================
// PLATFORM HELPERS
// =====================================================================

std::string getDefaultOutputDir() {
#ifdef _WIN32
    char* val = nullptr;
    size_t len = 0;
    _dupenv_s(&val, &len, "USERPROFILE");
    std::string home = (val ? val : "C:\\Users\\Public");
    free(val);
    return home + "\\SaveHarbor_Saves";
#else
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/SaveHarbor_Saves";
#endif
}

std::string getXeniaContentDir() {
#ifdef _WIN32
    char* val = nullptr;
    size_t len = 0;
    _dupenv_s(&val, &len, "APPDATA");
    std::string appdata = (val ? val : "C:\\Users\\Public");
    free(val);
    return appdata + "\\Xenia\\content";
#else
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/.config/Xenia/content";
#endif
}

// =====================================================================
// CROSS-PLATFORM DRIVE SIZE
// =====================================================================

static uint64_t getDriveSize(const std::string& drivePath) {
#ifdef _WIN32
    // on Windows, seekg/tellg doesn't work on physical drives
    // we must use DeviceIoControl instead
    HANDLE hDrive = CreateFileA(
        drivePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hDrive == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: cannot open drive (error code: "
                  << GetLastError() << ")" << std::endl;
        std::cerr << "Make sure you run as Administrator!" << std::endl;
        return 0;
    }

    GET_LENGTH_INFORMATION lengthInfo;
    DWORD bytesReturned = 0;
    if (!DeviceIoControl(
            hDrive,
            IOCTL_DISK_GET_LENGTH_INFO,
            NULL, 0,
            &lengthInfo, sizeof(lengthInfo),
            &bytesReturned,
            NULL)) {
        std::cerr << "Error: cannot get drive size (error code: "
                  << GetLastError() << ")" << std::endl;
        CloseHandle(hDrive);
        return 0;
    }

    CloseHandle(hDrive);
    return (uint64_t)lengthInfo.Length.QuadPart;
#else
    // on Linux seekg to end works fine
    std::ifstream f(drivePath, std::ios::binary);
    if (!f.is_open()) return 0;
    f.seekg(0, std::ios::end);
    uint64_t size = f.tellg();
    f.close();
    return size;
#endif
}

// =====================================================================
// BINARY HELPERS
// =====================================================================

static uint32_t readBE32(const uint8_t* buf, size_t offset) {
    return ((uint32_t)buf[offset]   << 24) |
           ((uint32_t)buf[offset+1] << 16) |
           ((uint32_t)buf[offset+2] <<  8) |
           ((uint32_t)buf[offset+3]);
}

static std::string utf16beToString(const uint8_t* buf, size_t maxBytes) {
    std::string result;
    for (size_t i = 0; i + 1 < maxBytes; i += 2) {
        uint16_t c = ((uint16_t)buf[i] << 8) | buf[i+1];
        if (c == 0) break;
        if (c >= 32 && c < 127) result += (char)c;
    }
    return result;
}

static std::string bytesToString(const uint8_t* buf, size_t maxBytes) {
    std::string result;
    for (size_t i = 0; i < maxBytes; i++) {
        if (buf[i] == 0) break;
        if (buf[i] >= 32 && buf[i] < 127) result += (char)buf[i];
    }
    return result;
}

// =====================================================================
// STFS VALIDATION
// =====================================================================

static bool isValidSTFS(const uint8_t* data, size_t size) {
    if (size < 0x500) return false;

    bool validMagic = (memcmp(data, "CON ", 4) == 0) ||
                      (memcmp(data, "LIVE", 4) == 0) ||
                      (memcmp(data, "PIRS", 4) == 0);
    if (!validMagic) return false;

    uint32_t titleId = readBE32(data, 0x360);
    if (titleId == 0) return false;

    uint32_t contentType = readBE32(data, 0x344);
    static const uint32_t validTypes[] = {
        0x00000001, 0x00000002, 0x00000003, 0x00000004,
        0x000D0000, 0x00090000, 0x00040000, 0x000B0000
    };
    bool validType = false;
    for (uint32_t t : validTypes)
        if (contentType == t) { validType = true; break; }
    if (!validType) return false;

    uint32_t version = readBE32(data, 0x364);
    if (version > 2) return false;

    return true;
}

// =====================================================================
// HEADER PARSING
// =====================================================================

static STFSSave parseHeader(const uint8_t* data, uint64_t offset) {
    STFSSave save;
    save.magic       = std::string((char*)data, 4);
    save.titleId     = readBE32(data, 0x360);

    auto it = KNOWN_TITLES.find(save.titleId);
    if (it != KNOWN_TITLES.end()) {
        save.titleName = it->second;
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "Unknown (0x%08X)", save.titleId);
        save.titleName = buf;
    }

    save.displayName = utf16beToString(data + 0x411, 0x80);
    save.gamertag    = bytesToString(data + 0x406, 16);
    save.date        = bytesToString(data + 0x020, 10);
    save.offset      = offset;
    return save;
}

// =====================================================================
// CROSS-PLATFORM FILE READER
// On Windows we use HANDLE directly for physical drives
// On Linux we use ifstream
// =====================================================================

#ifdef _WIN32

// Windows drive reader using HANDLE
class DriveReader {
public:
    HANDLE hDrive = INVALID_HANDLE_VALUE;

    bool open(const std::string& path) {
        hDrive = CreateFileA(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, 0, NULL
        );
        return hDrive != INVALID_HANDLE_VALUE;
    }

    bool seek(uint64_t offset) {
        LARGE_INTEGER li;
        li.QuadPart = (LONGLONG)offset;
        return SetFilePointerEx(hDrive, li, NULL, FILE_BEGIN) != 0;
    }

    size_t read(uint8_t* buf, size_t size) {
        // Windows requires reads on physical drives to be
        // aligned to 512-byte sector boundaries
        // round up size to nearest 512
        size_t aligned = ((size + 511) / 512) * 512;
        std::vector<uint8_t> tmp(aligned);
        DWORD bytesRead = 0;
        ReadFile(hDrive, tmp.data(), (DWORD)aligned, &bytesRead, NULL);
        size_t actual = (std::min)((size_t)bytesRead, size);
        memcpy(buf, tmp.data(), actual);
        return actual;
    }

    void close() {
        if (hDrive != INVALID_HANDLE_VALUE) {
            CloseHandle(hDrive);
            hDrive = INVALID_HANDLE_VALUE;
        }
    }

    bool isOpen() { return hDrive != INVALID_HANDLE_VALUE; }
};

#else

// Linux drive reader using ifstream
class DriveReader {
public:
    std::ifstream f;

    bool open(const std::string& path) {
        f.open(path, std::ios::binary);
        return f.is_open();
    }

    bool seek(uint64_t offset) {
        f.seekg(offset);
        return f.good();
    }

    size_t read(uint8_t* buf, size_t size) {
        f.read((char*)buf, size);
        return f.gcount();
    }

    void close() { f.close(); }
    bool isOpen() { return f.is_open(); }
};

#endif

// =====================================================================
// DRIVE SCANNER
// =====================================================================

std::vector<STFSSave> scanDriveForSaves(const std::string& drivePath) {
    std::vector<STFSSave> saves;

    // get drive size first
    uint64_t driveSize = getDriveSize(drivePath);
    if (driveSize == 0) {
        std::cerr << "Error: could not determine drive size." << std::endl;
        return saves;
    }

    std::cout << "Drive size: "
              << (driveSize / (1024ULL*1024*1024)) << " GB" << std::endl;

    DriveReader drive;
    if (!drive.open(drivePath)) {
        std::cerr << "Error: cannot open " << drivePath << std::endl;
#ifdef _WIN32
        std::cerr << "Make sure you run as Administrator!" << std::endl;
#else
        std::cerr << "Try running with sudo!" << std::endl;
#endif
        return saves;
    }

    const size_t CHUNK    = 1024 * 1024;
    const size_t HDR_SIZE = 0x500;
    uint64_t     offset   = 0;

    std::vector<uint8_t> chunk(CHUNK + HDR_SIZE);
    std::vector<uint8_t> header(HDR_SIZE);

    const uint8_t* MAGICS[] = {
        (uint8_t*)"CON ",
        (uint8_t*)"LIVE",
        (uint8_t*)"PIRS"
    };

    while (offset < driveSize) {
        double pct   = (double)offset / driveSize * 100.0;
        double gbDone = offset    / (1024.0*1024.0*1024.0);
        double gbTot  = driveSize / (1024.0*1024.0*1024.0);
        int    bars   = (int)(pct / 5);

        std::cout << "\r[";
        for (int i = 0; i < 20; i++)
            std::cout << (i < bars ? "#" : "-");
        std::cout << "] "
                  << std::fixed << std::setprecision(1) << pct << "% | "
                  << gbDone << "/" << gbTot << " GB | "
                  << "Found: " << saves.size()
                  << std::flush;

        drive.seek(offset);
        size_t bytesRead = drive.read(chunk.data(), CHUNK + HDR_SIZE);
        if (bytesRead == 0) break;

        for (const uint8_t* magic : MAGICS) {
            size_t pos = 0;
            while (pos + 4 <= bytesRead) {
                size_t found = std::string::npos;
                for (size_t i = pos; i + 4 <= bytesRead; i++) {
                    if (memcmp(chunk.data() + i, magic, 4) == 0) {
                        found = i;
                        break;
                    }
                }
                if (found == std::string::npos) break;

                uint64_t absOffset = offset + found;

                drive.seek(absOffset);
                drive.read(header.data(), HDR_SIZE);

                if (isValidSTFS(header.data(), HDR_SIZE)) {
                    bool duplicate = false;
                    for (const auto& s : saves)
                        if (s.offset == absOffset) { duplicate = true; break; }
                    if (!duplicate)
                        saves.push_back(parseHeader(header.data(), absOffset));
                }
                pos = found + 1;
            }
        }

        offset += CHUNK;
    }

    double gbTot = driveSize / (1024.0*1024.0*1024.0);
    std::cout << "\r[####################] 100.0% | "
              << std::fixed << std::setprecision(1)
              << gbTot << "/" << gbTot << " GB | "
              << "Found: " << saves.size() << std::endl;

    drive.close();
    return saves;
}

// =====================================================================
// RAW EXTRACTION
// =====================================================================

bool extractSave(const std::string& drivePath,
                 const STFSSave& save,
                 const std::string& outputDir) {

    fs::create_directories(outputDir);

    std::string safeName = save.titleName;
    for (char& c : safeName)
        if (!isalnum((unsigned char)c) && c != '_' && c != '-') c = '_';

    char offsetBuf[32];
    snprintf(offsetBuf, sizeof(offsetBuf), "_%llx.stfs",
             (unsigned long long)save.offset);

    std::string sep = 
#ifdef _WIN32
        "\\";
#else
        "/";
#endif

    std::string outputPath = outputDir + sep + safeName + offsetBuf;

    std::cout << "\nExtracting: " << save.titleName << std::endl;
    std::cout << "Gamertag:   " << save.gamertag    << std::endl;
    std::cout << "Date:       " << save.date         << std::endl;
    std::cout << "To:         " << outputPath        << std::endl;

    DriveReader drive;
    if (!drive.open(drivePath)) {
        std::cerr << "Error: cannot open drive!" << std::endl;
        return false;
    }

    drive.seek(save.offset);
    const size_t EXTRACT_SIZE = 4 * 1024 * 1024;
    std::vector<uint8_t> data(EXTRACT_SIZE);
    size_t bytesRead = drive.read(data.data(), EXTRACT_SIZE);
    drive.close();

    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Error: cannot write to " << outputPath << std::endl;
        return false;
    }
    out.write((char*)data.data(), bytesRead);
    out.close();

    std::cout << "Done! (" << bytesRead / 1024 << "KB extracted)" << std::endl;
    return true;
}

// =====================================================================
// XENIA EXTRACTION
// =====================================================================

bool extractSaveForXenia(const std::string& drivePath,
                         const STFSSave& save,
                         const std::string& xeniaContentDir) {
    char titleIdHex[16];
    snprintf(titleIdHex, sizeof(titleIdHex), "%08X", save.titleId);

    std::string sep =
#ifdef _WIN32
        "\\";
#else
        "/";
#endif

    std::string profileDir = xeniaContentDir + sep +
                             "0000000000000000" + sep +
                             titleIdHex + sep +
                             "00000001";

    fs::create_directories(profileDir);

    std::string outputPath = profileDir + sep + titleIdHex;

    std::cout << "\nExtracting for Xenia: " << save.titleName << std::endl;
    std::cout << "Title ID:   0x" << titleIdHex               << std::endl;
    std::cout << "Gamertag:   " << save.gamertag               << std::endl;
    std::cout << "To:         " << outputPath                  << std::endl;

    DriveReader drive;
    if (!drive.open(drivePath)) {
        std::cerr << "Error: cannot open drive!" << std::endl;
        return false;
    }

    drive.seek(save.offset);
    const size_t EXTRACT_SIZE = 4 * 1024 * 1024;
    std::vector<uint8_t> data(EXTRACT_SIZE);
    size_t bytesRead = drive.read(data.data(), EXTRACT_SIZE);
    drive.close();

    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Error: cannot write to " << outputPath << std::endl;
        return false;
    }
    out.write((char*)data.data(), bytesRead);
    out.close();

    std::cout << "Done! Launch the game in Xenia and your save should appear."
              << std::endl;
    return true;
}
