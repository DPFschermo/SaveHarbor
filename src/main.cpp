#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <limits>
#include "stfs_scanner.h"

// default output directory
const std::string OUTPUT_DIR =
    std::string(getenv("HOME")) + "/SaveHarbor_Saves";

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
        // truncate long strings for display
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

int main(int argc, char* argv[]) {
    std::cout << "=================================" << std::endl;
    std::cout << "  SaveHarbor v0.3 - Xbox 360"     << std::endl;
    std::cout << "  Save File Extractor"             << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << std::endl;

    if (argc < 2) {
        std::cout << "Usage: sudo ./saveharbor <drive>" << std::endl;
        std::cout << "Example: sudo ./saveharbor /dev/sda" << std::endl;
        return 1;
    }

    std::string drivePath = argv[1];
    std::cout << "Scanning: " << drivePath << std::endl;
    std::cout << "This may take a few minutes..." << std::endl << std::endl;

    // scan the drive
    std::vector<STFSSave> saves = scanDriveForSaves(drivePath);

    if (saves.empty()) {
        std::cout << "No valid save files found." << std::endl;
        return 0;
    }

    std::cout << "\nFound " << saves.size() << " save files!" << std::endl;

    // print results table
    printTable(saves);

    std::cout << "\nSaves will go to: " << OUTPUT_DIR << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  Enter a number to extract that save" << std::endl;
    std::cout << "  Enter 'all' to extract everything"   << std::endl;
    std::cout << "  Enter 'q' to quit"                   << std::endl;

    while (true) {
        std::cout << "\nYour choice: ";
        std::string choice;
        std::cin >> choice;

        if (choice == "q" || choice == "Q") {
            std::cout << "Goodbye!" << std::endl;
            break;

        } else if (choice == "all" || choice == "ALL") {
            std::cout << "\nExtracting all " << saves.size() << " saves..." << std::endl;
            int success = 0;
            for (const auto& save : saves) {
                if (extractSave(drivePath, save, OUTPUT_DIR)) success++;
            }
            std::cout << "\nDone! " << success << "/" << saves.size()
                      << " saves extracted to " << OUTPUT_DIR << std::endl;
            break;

        } else {
            // try to parse as number
            try {
                int idx = std::stoi(choice) - 1;
                if (idx >= 0 && idx < (int)saves.size()) {
                    extractSave(drivePath, saves[idx], OUTPUT_DIR);
                    std::cout << "\nTo use with Xenia:" << std::endl;
                    std::cout << "  Copy the .stfs file to ~/.config/Xenia/content/"
                              << std::endl;
                } else {
                    std::cout << "Enter a number between 1 and "
                              << saves.size() << std::endl;
                }
            } catch (...) {
                std::cout << "Invalid input. Enter a number, 'all', or 'q'" << std::endl;
            }
        }
    }

    return 0;
}
