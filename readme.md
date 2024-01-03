<p align="center">
<img src="images/capylogo_web.png">
</p>

# CapyPDF

This is a library for generating PDF files. It aims to be very low level.
It does not have its own document model, it merely exposes PDF primitives
directly.

## Features

- Fully color managed using [LittleCMS 2](https://littlecms.com/)
- All fonts are embedded as subsets
- Not implemented in C
- Provides a plain C API for easy integration into scripting languages
- Ships with a `ctypes` Python binding
- Minimal dependencies

## Things the library does not do

- Reading PDF files
- Modifying PDF files
- Supporting any other backend than PDF
- Parsing any vector data files like SVG
- Data conversions in general (apart from colorspaces)
- Supporting PDF versions earlier than 1.7

## API stability guarantees

Until 1.0 there is no guarantee of any kind. Anything can be changed.
However we try not to change things without a good reason. Once 1.0
happens, we aim to provide the following:

- The plain C interface is both API and ABI stable
- The internal C++ implementation "API" is not stable in any way
- Only C symbols are exported so in order to get to the C++ API you
  have to change build settings. If you do that, there is no stability
  guarantee.
- The Python API shall be stable as well

## Status

The basic functionality is there but it's not even close to feature
complete.
