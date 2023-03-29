#!/bin/sh

mkdir fuzzgen
clang++-15 -DA4FUZZING -std=c++20 -O2 -g -fsanitize=fuzzer,address -Isrc src/ft_subsetter.cpp src/fontfuzz.cpp -o fuzzgen/fontfuzz `pkg-config --cflags --libs freetype2`
