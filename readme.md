<p align="center">
<img src="testdata/images/capylogo_web.png">
</p>

# CapyPDF

CapyPDF is a low level library for generating PDF files. It does not
have its own document model, it merely exposes PDF primitives
directly.

## Features

- Aims to support all functionality in PDF (eventually), including
  accessibility features
- Reads PNG, JPEG and TIFF files
- Fully color managed using [LittleCMS 2](https://littlecms.com/)
- Not implemented in C
- Provides a plain C API for easy integration into scripting languages
- Ships with a `ctypes` Python binding and a C++ wrapper header
- Minimal dependencies
- Creates PDF 2.0 unless chosen PDF type (X, A, etc) requires a
  specific older version

## Things the library does not do

- Reading PDF files
- Modifying PDF files
- Cryptographic operations (i.e. document signing)
- Supporting any other backend than PDF
- Parsing any vector data files like SVG
- Data conversions in general (apart from colorspaces)

## API stability guarantees

Until 1.0 there is no guarantee of any kind. Anything can be changed.
However we try not to change things without a good reason. Once 1.0
happens, we aim to provide the following:

- The plain C interface is both API and ABI stable
- Only C symbols are exported so you alter build settings to get at
  the internals, there is no stability guarantee
- The Python API shall be stable as well

## PDF validity

The library shall always generate PDFs that are syntactically valid.
Any deviation is a bug that should be reported.

The output is _not_ guaranteed to be semantically valid. PDF has
certain requirements for valid documents that can not be checked in
a plain PDF generation library. This work needs to be done by the
generating application. CapyPDF does have some semantic checks, such
as not permitting RGB images in PDF/X3 documents, but they are
implemented on a best effort basis.

## Status

The basic functionality is there and the library can be used to
generate fairly complex documents. The APIs are not stable yet,
however they are not expected to change much any more.
