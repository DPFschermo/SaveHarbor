#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include "stfs_scanner.h"
#include "xtaf_parser.h"

// Xbox 360 data partition offset (confirmed from our analysis)
const uint64_t XBOX360_PARTITION = 4496818176ULL;

void printBanner() {
    std::cout << "=================================" << std::endl;
    std::cout << "  SaveHarbor v0.5 - Xbox 360"     << std::endl;
    std::cout << "  Save & Game Extractor"           << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << std::endl;
}

void printSaveTable(const std::vector<STFSSave>& saves) {
    std::cout << std::endl;
    std::cout << std::string(72, '=') << std::endl;
    std::cout << std::left
              << std::setw(5)  << "#"
              << std::setw(32) << "Game"
              << std::setw(14) << "Gamertag"
              << std::setw(12) << "Date"
              << std::setw(6)  << "Type"
              << std::endl;
    std::cout << std::string(72, '=') << std::endl;

    for (size_t i = 0; i < saves.size(); i++) {
        const auto& s = saves[i];
        std::string name = s.titleName.length() > 30
            ? s.titleName.substr(0, 30) : s.titleName;
        std::string tag  = s.gamertag.length() > 12
            ? s.gamertag.substr(0, 12) : s.gamertag;
        std::string date = s.date.length() > 10
            ? s.date.substr(0, 10) : s.date;

        std::cout << std::left
                  << std::setw(5)  << (i + 1)
                  << std::setw(32) << name
                  << std::setw(14) << tag
                  << std::setw(12) << date
                  << std::setw(6)  << s.magic
                  << std::endl;
    }
    std::cout << std::string(72, '=') << std::endl;
}

void printGameTable(const std::vector<XTAFGame>& games) {
    if (games.empty()) {
        std::cout << "\nNo installed games found in filesystem." << std::endl;
        std::cout << "Games may be stored in GOD format (will show in save scan)"
                  << std::endl;
        return;
    }

    std::cout << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << std::left
              << std::setw(5)  << "#"
              << std::setw(12) << "Title ID"
              << std::setw(30) << "Path"
              << std::setw(10) << "Size"
              << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    for (size_t i = 0; i < games.size(); i++) {
        const auto& g = games[i];
        std::string path = g.xexPath.length() > 28
            ? "..." + g.xexPath.substr(g.xexPath.length()-25) : g.xexPath;

        std::cout << std::left
                  << std::setw(5)  << (i + 1)
                  << std::setw(12) << g.titleId
                  << std::setw(30) << path
                  << std::setw(10) << (g.xexSize / (1024*1024))
                  << " MB" << std::endl;
    }
    std::cout << std::string(60, '=') << std::endl;
}

bool askExtractionMode() {
    std::cout << "\nExtraction mode:" << std::endl;
    std::cout << "  1 - Raw file (backup, use with any emulator)" << std::endl;
    std::cout << "  2 - Direct to Xenia content folder"           << std::endl;
    std::cout << "\nChoice (1/2): ";
    std::string choice;
    std::cin >> choice;
    return (choice == "2");
}

void doExtractSave(const std::string& drivePath,
                   const STFSSave& save,
                   bool xeniaMode) {
    if (xeniaMode) {
        extractSaveForXenia(drivePath, save, getXeniaContentDir());
    } else {
        extractSave(drivePath, save, getDefaultOutputDir());
    }
}

int main(int argc, char* argv[]) {
    printBanner();

    if (argc < 2) {
        std::cout << "Usage:" << std::endl;
#ifdef _WIN32
        std::cout << "  Run as Administrator!" << std::endl;
        std::cout << "  saveharbor.exe \\\\.\\PhysicalDrive1" << std::endl;
#else
        std::cout << "  sudo ./saveharbor /dev/sda" << std::endl;
#endif
        return 1;
    }

    std::string drivePath = argv[1];

    // main menu
    while (true) {
        std::cout << "\nWhat would you like to do?" << std::endl;
        std::cout << "  1 - Scan for save files"          << std::endl;
        std::cout << "  2 - Scan for installed games"     << std::endl;
        std::cout << "  q - Quit"                         << std::endl;
        std::cout << "\nChoice: ";

        std::string choice;
        std::cin >> choice;

        if (choice == "q" || choice == "Q") {
            std::cout << "Goodbye!" << std::endl;
            break;

        // ── SAVE SCAN ──────────────────────────────────────────
        } else if (choice == "1") {

            std::cout << "\nScanning: " << drivePath << std::endl;
            std::cout << "This may take a while on an HDD..."
                      << std::endl << std::endl;

            std::vector<STFSSave> saves = scanDriveForSaves(drivePath);

            if (saves.empty()) {
                std::cout << "No save files found." << std::endl;
                continue;
            }

            std::cout << "\nFound " << saves.size()
                      << " save files!" << std::endl;
            printSaveTable(saves);

            bool xeniaMode = askExtractionMode();
            std::cout << "\nSaves will go to: "
                      << (xeniaMode ? getXeniaContentDir()
                                    : getDefaultOutputDir())
                      << std::endl;

            std::cout << "\nEnter number, 'all', 'm' (switch mode), or 'b' (back)"
                      << std::endl;

            while (true) {
                std::cout << "\n[" << (xeniaMode ? "Xenia" : "Raw")
                          << "] Choice: ";
                std::string c;
                std::cin >> c;

                if (c == "b" || c == "B") break;
                else if (c == "m" || c == "M") {
                    xeniaMode = !xeniaMode;
                    std::cout << "Switched to: "
                              << (xeniaMode ? "Xenia" : "Raw")
                              << std::endl;
                } else if (c == "all" || c == "ALL") {
                    int ok = 0;
                    for (const auto& s : saves) {
                        if (doExtractSave(drivePath, s, xeniaMode), true) ok++;
                    }
                    std::cout << "\nDone! " << ok << " saves extracted."
                              << std::endl;
                } else {
                    try {
                        int idx = std::stoi(c) - 1;
                        if (idx >= 0 && idx < (int)saves.size()) {
                            doExtractSave(drivePath, saves[idx], xeniaMode);
                        } else {
                            std::cout << "Enter 1-" << saves.size()
                                      << std::endl;
                        }
                    } catch (...) {
                        std::cout << "Invalid input." << std::endl;
                    }
                }
            }

        // ── GAME SCAN ──────────────────────────────────────────
        } else if (choice == "2") {

            std::cout << "\nScanning Xbox 360 filesystem for installed games..."
                      << std::endl;

            std::vector<XTAFGame> games =
                scanXTAFForGames(drivePath, XBOX360_PARTITION);

            printGameTable(games);

            if (games.empty()) continue;

            std::cout << "\nEnter number to extract game, or 'b' to go back: ";

            while (true) {
                std::string c;
                std::cin >> c;

                if (c == "b" || c == "B") break;

                try {
                    int idx = std::stoi(c) - 1;
                    if (idx >= 0 && idx < (int)games.size()) {
                        const auto& game = games[idx];

#ifdef _WIN32
                        std::string sep = "\\";
#else
                        std::string sep = "/";
#endif
                        // output dir: SaveHarbor_Saves/Games/TitleID/
                        std::string outDir = getDefaultOutputDir() +
                                             sep + "Games" +
                                             sep + game.titleId;

                        extractGameFiles(drivePath, game, outDir);

                        std::cout << "\nTo play in Xenia:" << std::endl;
                        std::cout << "  File → Open → navigate to: "
                                  << outDir << sep << "default.xex"
                                  << std::endl;
                    } else {
                        std::cout << "Enter 1-" << games.size() << std::endl;
                    }
                } catch (...) {
                    std::cout << "Invalid input." << std::endl;
                }

                std::cout << "Choice: ";
            }
        }
    }

    return 0;
}
