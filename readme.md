<p align="center">
<img src="testdata/images/capylogo_web.png">
</p>

# CapyPDF

CapyPDF is a low level library for generating PDF files. It does not
have its own document model, it merely exposes PDF primitives
directly.

## Features

- Aims to eventually support all functionality in PDF, including
  accessibility features
- Deprecated PDF features are not supported by default, but support
  may be added for important, widely used features
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
- Supporting any other output format than PDF
- Parsing any vector data, eg SVG
- Data conversions in general (apart from colorspaces)

## API stability guarantees

Until 1.0 there is no guarantee of any kind. Anything can be changed.
However we try not to change things without a good reason. Once 1.0
happens, we aim to provide the following:

- The plain C interface is both API and ABI stable
- The Python API shall be stable as well
- Nothing else is stable

## PDF validity

The library shall always generate PDFs that are syntactically valid.
Any deviation is a bug that should be reported.

The output is _not_ guaranteed to be semantically valid. PDF has
structural validity requirements that a plain PDF generation library
can not guarantee to hold. That work needs to be done by the
generating application. CapyPDF does have some semantic checks, such
as not permitting RGB images in PDF/X3 documents, but they are
implemented on a best effort basis.

## Status

The basic functionality is there and the library can be used to
generate fairly complex documents. The APIs are not stable yet,
however they are not expected to change much any more.

## AI policy

The use of any and all AI tools for this project is prohibited. Issues
and pull requests created using AI will not be looked at, but instead
closed immediately upon detection.
