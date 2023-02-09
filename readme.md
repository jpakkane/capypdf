# A4PDF

This is a library for generating PDF files. It aims to be very low level.
It does not have its own document model, it merely exposes PDF primitives
directly.

## Features

- Fully color managed using [LittleCMS 2](https://littlecms.com/)
- All fonts are embedded as subsets
- Not implemented in C
- Provides a plain C API for easy integration into scripting languages
- Minimal dependencies

## Non-features

- Reading PDF files
- Modifying PDF files
- Supporting any other backend than PDF
- Parsing any vector data files like SVG
- Supporting PDF versions earlier than 1.7

## Status

The basic functionality is there but it's not even close to feature
complete.
