# PDFgen

An experimental color managed PDF generator.

## Building

The program requires icc profile files for sRGB, sGray and CMYK.
Currently they are hardcoded in `pdfgen.cpp`, so you need to edit
those file strings before compiling.
