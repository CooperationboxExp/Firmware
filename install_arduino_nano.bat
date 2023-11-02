@echo off

set PLATFORMIO_CORE_DIR=PlatformIO\install

if not exist PlatformIO\install\ (
  python PlatformIO/platformio-core-installer/get-platformio.py
)

PlatformIO\install\penv\Scripts\platformio.exe run -e nanoatmega328
PlatformIO\install\penv\Scripts\platformio.exe run -e nanoatmega328 -t upload
