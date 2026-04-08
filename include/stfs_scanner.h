#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <map>

// known Xbox 360 title IDs
static const std::map<uint32_t, std::string> KNOWN_TITLES = {
    {0x415608C6, "Assassins Creed 1"},
    {0x454109C6, "Assassins Creed 1 (EU)"},
    {0x41560817, "Assassins Creed 2"},
    {0x415608CB, "Assassins Creed Brotherhood"},
    {0x41560911, "Assassins Creed 3"},
    {0x41560904, "COD Ghosts"},
    {0x545407E6, "GTA IV"},
    {0x545407F2, "GTA V"},
    {0x545408A7, "GTA V"},
    {0x4D5307EA, "Halo 3"},
    {0x4D53084D, "Halo 4"},
    {0x4D530919, "Halo 4"},
    {0x4D5307EB, "Halo ODST"},
    {0x555308B7, "Red Dead Redemption"},
    {0x55530833, "Red Dead Redemption GOTY"},
    {0x555308C2, "Borderlands 2"},
    {0x4B5607E4, "Call of Duty MW3"},
    {0x4E4D07FC, "FIFA"},
    {0x4E4D0998, "FIFA 13"},
    {0x4E4D09C3, "FIFA 14"},
    {0x4C4107D7, "Dark Souls"},
    {0x425607D4, "Batman Arkham City"},
    {0x545107F6, "Minecraft"},
    {0x41560A28, "COD Black Ops 2"},
    {0x415605F0, "COD Black Ops"},
    {0x415605CA, "COD MW2"},
    {0x45410998, "FIFA 13"},
    {0x454109C3, "FIFA 14"},
    {0x45410967, "FIFA 12"},
    {0x45410950, "Battlefield"},
};

// one save file entry
struct STFSSave {
    std::string magic;        // CON, LIVE, or PIRS
    uint32_t    titleId;
    std::string titleName;
    std::string displayName;
    std::string gamertag;
    std::string date;
    uint64_t    offset;       // byte offset on drive
};

// scan drive for all STFS containers
std::vector<STFSSave> scanDriveForSaves(const std::string& drivePath);

// extract one save to output directory
bool extractSave(const std::string& drivePath,
                 const STFSSave& save,
                 const std::string& outputDir);
