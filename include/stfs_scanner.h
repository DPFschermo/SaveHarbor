#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <map>

// =====================================================================
// KNOWN XBOX 360 TITLE IDs
// =====================================================================
static const std::map<uint32_t, std::string> KNOWN_TITLES = {

    // --- Assassin's Creed ---
    {0x415608C6, "Assassins Creed 1"},
    {0x454109C6, "Assassins Creed 1 (EU)"},
    {0x41560817, "Assassins Creed 2"},
    {0x415608CB, "Assassins Creed Brotherhood"},
    {0x41560911, "Assassins Creed 3"},
    {0x5553085D, "Assassins Creed (DLC)"},
    {0x55530879, "Assassins Creed Revelations"},
    {0x555308AE, "Assassins Creed (Uplay)"},

    // --- Call of Duty ---
    {0x41560904, "COD Ghosts"},
    {0x415608FC, "COD Ghosts"},
    {0x415608C3, "COD Black Ops 2"},
    {0x41560A28, "COD Black Ops 2"},
    {0x415605F0, "COD Black Ops"},
    {0x415605CA, "COD MW2"},
    {0x4B5607E4, "COD MW3"},
    {0x415688E6, "COD MW3"},

    // --- GTA ---
    {0x545407E6, "GTA IV"},
    {0x545407F2, "GTA V"},
    {0x545408A7, "GTA V"},

    // --- Halo ---
    {0x4D5307EA, "Halo 3"},
    {0x4D5307E6, "Halo 3 ODST"},
    {0x4D5307EB, "Halo ODST"},
    {0x4D53084D, "Halo 4"},
    {0x4D530919, "Halo 4"},
    {0x4D5308CE, "Halo Waypoint"},

    // --- FIFA ---
    {0x4E4D07FC, "FIFA"},
    {0x45410967, "FIFA 12"},
    {0x45410998, "FIFA 13"},
    {0x4E4D0998, "FIFA 13"},
    {0x454109C3, "FIFA 14"},
    {0x4E4D09C3, "FIFA 14"},

    // --- Red Dead Redemption ---
    {0x555308B7, "Red Dead Redemption"},
    {0x55530833, "Red Dead Redemption GOTY"},
    {0x5454082B, "Red Dead Redemption (DLC)"},

    // --- Other games ---
    {0x4C4107D7, "Dark Souls"},
    {0x425607D4, "Batman Arkham City"},
    {0x53438807, "Batman Arkham Asylum"},
    {0x545107F6, "Minecraft"},
    {0x555308C2, "Borderlands 2"},
    {0x4D5308D6, "Fable III"},
    {0x4D5389C2, "Fable III"},
    {0x4D530910, "Forza Motorsport 4"},
    {0x4D53882C, "Forza Motorsport 2"},
    {0x4D538861, "Forza Motorsport 3"},
    {0x5454086B, "Max Payne 3"},
    {0x55530810, "Far Cry 3"},
    {0x53510802, "Tomb Raider"},
    {0x53510811, "Sleeping Dogs"},
    {0x55530879, "Assassins Creed Revelations"},
    {0x454107F6, "Medal of Honor"},
    {0x434D0819, "DiRT 2"},
    {0x545107FC, "Saints Row"},
    {0x545407F2, "GTA V"},
    {0x4D530A1B, "Xbox Live Events"},
    {0x494707E4, "Race Pro"},
    {0x534307D4, "Tomb Raider Legend"},
    {0x544307D5, "Ninja Gaiden"},
    {0x545107D5, "Juiced 2"},
    {0x4B4E081D, "Pro Evolution Soccer"},
    {0x5848085B, "Kinect"},
    {0x584111F7, "Portal 2"},
    {0x45410866, "The Godfather"},
    {0x45410950, "Battlefield 3"},
    {0x4D530919, "Halo 4"},

    // --- Xbox Live Arcade ---
    {0x584107D1, "Hexic HD"},
    {0x58410834, "Pinball FX"},
    {0x584108A4, "Super Street Fighter II Turbo HD"},
    {0x584108B7, "Castle Crashers"},
    {0x58410864, "Sonic The Hedgehog"},
    {0x53438802, "Battlestations Pacific"},

    // --- Apps ---
    {0x423607D1, "YouTube App"},
    {0x584807E1, "Facebook App"},
    {0x413707D1, "Eurosport App"},
    {0x58480880, "Internet Explorer"},
    {0x415607E6, "Variety App"},
};

// one save file entry
struct STFSSave {
    std::string magic;        // CON, LIVE, or PIRS
    uint32_t    titleId;
    std::string titleName;
    std::string displayName;
    std::string gamertag;
    std::string date;
    uint64_t    offset;
};

// scan drive for all STFS containers
std::vector<STFSSave> scanDriveForSaves(const std::string& drivePath);

// extract raw save file to output directory
bool extractSave(const std::string& drivePath,
                 const STFSSave& save,
                 const std::string& outputDir);

// extract save directly into Xenia's content folder
bool extractSaveForXenia(const std::string& drivePath,
                         const STFSSave& save,
                         const std::string& xeniaContentDir);

// get the default save output directory (cross-platform)
std::string getDefaultOutputDir();

// get the default Xenia content directory (cross-platform)
std::string getXeniaContentDir();
