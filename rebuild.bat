@echo off

cd build

cmake ..

cmake --build . --config Release

cd ..

REM Single-config generators (Ninja) drop the exe in build\, multi-config
REM (Visual Studio) in build\Release\. Run whichever is actually there.
if exist build\pbr_viewer.exe (build\pbr_viewer.exe) else (build\Release\pbr_viewer.exe)

