#!/bin/bash


CXX=g++
CXXFLAGS="-Wall -Werror -std=c++17 -O2"

TARGET="directory_daemon"

SOURCES=("main.cpp" "daemon.cpp")

echo "Cleaning previous build..."
rm -f *.o $TARGET

echo "Compiling source files..."
for source in "${SOURCES[@]}"; do
    if [ ! -f "$source" ]; then
        echo "Error: Source file $source not found!"
        exit 1
    fi

    $CXX $CXXFLAGS -c "$source" -o "${source%.cpp}.o"
    if [ $? -ne 0 ]; then
        echo "Error: Compilation failed for $source"
        exit 1
    fi
done

echo "Linking object files..."
$CXX $CXXFLAGS *.o -o $TARGET
if [ $? -ne 0 ]; then
    echo "Error: Linking failed"
    exit 1
fi

echo "Cleaning intermediate files..."
rm -f *.o

chmod +x $TARGET

echo "Build successful! Binary: $TARGET"