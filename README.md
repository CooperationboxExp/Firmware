# Firmware

## Publication

Repository: Zenodo  
Publication date: 2024-11-18  
DOI: [10.5281/zenodo.14178677](https://www.doi.org/10.5281/zenodo.14178677)  

## Quick Upload instructions

### Clone

- Clone the repository with:

      git clone --recursive https://github.com/CooperationboxExp/Firmware.git

### Settings

Follow the SETUP GUIDE in the `include/settings.h` settings file.

### Linux/Mac

- Make sure you are in the `Firmware` Directory and run:
           sh install_arduino_nano.sh

### Windows

- Make sure you have a recent version of Python installed. You can either get it directly from the Microsoft Store or download it from [here](https://www.python.org/downloads/).

- Make sure you are in the `Firmware` Directory and run:

      .\install_arduino_nano.bat

- If the installation throws an error related to `Long Path Support` first remove the `Firmware\PlatformIO\install` directory and then open a command prompt as administrator and run:

       reg add "HKLM\SYSTEM\CurrentControlSet\Control\FileSystem" /v LongPathsEnabled /t REG_DWORD /d 1
