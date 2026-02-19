@echo off

cd build

cmake --build . --config Release

cd ..

build\Release\pbr_viewer.exe

