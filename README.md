# SaveHarbor Xbox360 data extractor

<p align="center">
  <img src="assets/Logo.png" alt="Save Harbor Logo" width="400">
</p>

SaveHarbor is a tool written in c++, it aims to the detection of save game files of an xbox360, finding them and making them extractable so you can play them with an emulator (Ex. Xenia), it's still in early development since I'm making this alone, once i finsih with the xbox360 i might try to develop a program for the ps3 too.

## Installing (Linux)

You can install SaveHarbor by first cloning the repo:
~~~bash
git clone https://github.com/DPFschermo/SaveHarbor.git
cd SaveHarbor
mkdir build && cd build
sudo cmake
make
~~~

## Usage (Linux)
Using SaveHarbor is pretty simple:
~~~bash
cd build
./saveharbor /path/to/xboxhdd (Ex. /dev/sda)
~~~

## Installing necessary components(Windows)
To install SaveHarbor on windows you need some other apps:
- Cmake for windows [Here](https://cmake.org/download/)
- Visual studio community [Here](https://visualstudio.microsoft.com/it/vs/community/), once you enter vs community find your version and click modify, then under the Workloads tab, find "Desktop development with C++", check that box and click modify at the bottom right to download the necessary components
- Git for windows

## Installing SaveHarbor (Windows)
Once you've installed what you need open up git CMD and navigate to the directory where you want to clone SaveHarbor:
~~~bash
C:\Users\DPFschermo\Onedrive\Desktop> git clone https://github.com/DPFschermo/SaveHarbor.git
~~~

Now that you've cloned the repository, close git CMD and open Developer Command Prompt for VS as an administrator and go to the SaveHarbor directory, once you're there type:
~~~bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
~~~
Congratulations you've installed SaveHarbor on windows!

## Usage (Windows)
Using SaveHarbor on windows is as simple as using it on linux, go into the Release directory:
~~~bash
C:\Users\DPFschermo\Onedrive\Desktop\SaveHarbor\build> cd Release
~~~
once you're there simply run SaveHarbor:
~~~bash
saveharbor.exe \\.\PhysicalDrive6 
~~~
(you can find the number of the xbox drive with Windows Disk Management (right click Start → Disk Management) and look which disk number your Xbox drive is.)
## Status
Xbox 360: In development
Ps3: Planned

## WARNING
Again, I repeat, SaveHarbor is still in early development, so the scan might take a while and not all games are detected, i'm still unsure if the extracted files are playable, if you encounter bugs please contact me

