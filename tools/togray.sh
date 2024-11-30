#!/bin/sh

# Converts a PDF to grayscale.

gs \
   -sDEVICE=pdfwrite \
   -sProcessColorModel=DeviceGray \
   -sColorConversionStrategy=Gray \
   -dOverrideICC \
   -o gs_gray_converted.pdf \
   -f $1
