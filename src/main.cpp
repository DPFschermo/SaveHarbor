#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include "stfs_scanner.h"

void printBanner() {
    std::cout << "=================================" << std::endl;
    std::cout << "  SaveHarbor v0.4 - Xbox 360"     << std::endl;
    std::cout << "  Save File Extractor"             << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << std::endl;
}

void printTable(const std::vector<STFSSave>& saves) {
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
        std::string tag  = s.gamertag.length()   > 12
            ? s.gamertag.substr(0, 12)   : s.gamertag;
        std::string date = s.date.length()        > 10
            ? s.date.substr(0, 10)        : s.date;

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

// ask user if they want raw or xenia extraction
// returns true = xenia, false = raw
bool askExtractionMode() {
    std::cout << "\nHow would you like to extract?" << std::endl;
    std::cout << "  1 - Raw file (use with any emulator or backup)" << std::endl;
    std::cout << "  2 - Direct to Xenia content folder"             << std::endl;
    std::cout << "\nYour choice (1/2): ";

    std::string choice;
    std::cin >> choice;
    return (choice == "2");
}

void doExtract(const std::string& drivePath,
               const STFSSave& save,
               bool xeniaMode) {
    if (xeniaMode) {
        std::string xeniaDir = getXeniaContentDir();
        std::cout << "\nXenia content dir: " << xeniaDir << std::endl;
        extractSaveForXenia(drivePath, save, xeniaDir);
    } else {
        std::string outputDir = getDefaultOutputDir();
        extractSave(drivePath, save, outputDir);
    }
}

int main(int argc, char* argv[]) {
    printBanner();

    if (argc < 2) {
        std::cout << "Usage:" << std::endl;
#ifdef _WIN32
        std::cout << "  Run as Administrator!" << std::endl;
        std::cout << "  saveharbor.exe \\\\.\\PhysicalDrive1" << std::endl;
        std::cout << "  (use PhysicalDrive0, 1, 2... for your drive)" << std::endl;
#else
        std::cout << "  sudo ./saveharbor /dev/sda" << std::endl;
        std::cout << "  sudo ./saveharbor /dev/sdb  (if sda is your main disk)"
                  << std::endl;
#endif
        return 1;
    }

    std::string drivePath = argv[1];
    std::cout << "Scanning: " << drivePath << std::endl;
    std::cout << "This may take a few minutes..." << std::endl << std::endl;

    std::vector<STFSSave> saves = scanDriveForSaves(drivePath);

    if (saves.empty()) {
        std::cout << "No valid save files found." << std::endl;
        return 0;
    }

    std::cout << "\nFound " << saves.size() << " save files!" << std::endl;
    printTable(saves);

    // ask extraction mode once upfront
    bool xeniaMode = askExtractionMode();

    if (xeniaMode) {
        std::cout << "\nSaves will go to: " << getXeniaContentDir() << std::endl;
    } else {
        std::cout << "\nSaves will go to: " << getDefaultOutputDir() << std::endl;
    }

    std::cout << "\nOptions:"                                    << std::endl;
    std::cout << "  Enter a number to extract that save"        << std::endl;
    std::cout << "  Enter 'all' to extract everything"          << std::endl;
    std::cout << "  Enter 'm' to switch extraction mode"        << std::endl;
    std::cout << "  Enter 'q' to quit"                          << std::endl;

    while (true) {
        std::cout << "\n[Mode: "
                  << (xeniaMode ? "Xenia" : "Raw file")
                  << "] Your choice: ";

        std::string choice;
        std::cin >> choice;

        if (choice == "q" || choice == "Q") {
            std::cout << "Goodbye!" << std::endl;
            break;

        } else if (choice == "m" || choice == "M") {
            xeniaMode = !xeniaMode;
            std::cout << "Switched to: "
                      << (xeniaMode ? "Xenia mode" : "Raw file mode")
                      << std::endl;

        } else if (choice == "all" || choice == "ALL") {
            std::cout << "\nExtracting all " << saves.size()
                      << " saves..." << std::endl;
            int success = 0;
            for (const auto& save : saves) {
                if (xeniaMode) {
                    if (extractSaveForXenia(drivePath, save,
                                            getXeniaContentDir())) success++;
                } else {
                    if (extractSave(drivePath, save,
                                    getDefaultOutputDir())) success++;
                }
            }
            std::cout << "\nDone! " << success << "/"
                      << saves.size() << " saves extracted." << std::endl;
            break;

        } else {
            try {
                int idx = std::stoi(choice) - 1;
                if (idx >= 0 && idx < (int)saves.size()) {
                    doExtract(drivePath, saves[idx], xeniaMode);
                } else {
                    std::cout << "Enter a number between 1 and "
                              << saves.size() << std::endl;
                }
            } catch (...) {
                std::cout << "Invalid input. Enter a number, "
                             "'all', 'm', or 'q'" << std::endl;
            }
        }
    }

    return 0;
}
