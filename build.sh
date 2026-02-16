#!/bin/sh

cd build/

cmake --build . --config Release

cd ..

./build/pbr_viewer
