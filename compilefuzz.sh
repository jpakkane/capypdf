#!/bin/sh

mkdir capyfuzz
CXX=clang++-16 CPP_FLAGS=-DCAPYFUZZING ~/workspace/meson/meson.py setup --buildtype=debugoptimized -Db_sanitizer=address,fuzzer capyfuzz && ninja -C capyfuzz
