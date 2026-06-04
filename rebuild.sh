#!/bin/bash

cd build

cmake ..

cmake --build . --config Release

cd ..

./build/pbr_viewer
