#!/bin/sh

mkdir capyfuzz
CXX=clang++-16 ~/workspace/meson/meson.py setup --buildtype=debugoptimized -Db_sanitizer=address,fuzzer -Dfuzzing=true capyfuzz && ninja -C capyfuzz
