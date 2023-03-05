/*
 * Copyright 2023 Jussi Pakkanen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <tiffio.h>
#include <cstdio>
#include <vector>

int main(int argc, char **argv) {
    if(argc != 2) {
        printf("%s <tiff file>\n", argv[0]);
    }
    TIFF *tif = TIFFOpen(argv[1], "rb");
    if(!tif) {
        return 1;
    }
    uint32_t w{}, h{}, d{};
    uint16_t inkset{}, bitspersample{}, samplesperpixel{}, sampleformat{}, photometric{},
        planarconf{}, extrasamples{};
    uint32_t icc_count{};
    void *icc_data{};
    if(TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w) != 1) {
        return 1;
    }
    if(TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h) != 1) {
        return 1;
    }

    if(TIFFGetField(tif, TIFFTAG_IMAGEDEPTH, &d) != 1) {
        printf("Fail depth\n");
    }

    if(TIFFGetField(tif, TIFFTAG_INKSET, &inkset) != 1) {
        printf("Fail inkset\n");
    }

    if(TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitspersample) != 1) {
        printf("Fail bitspersample\n");
    }

    if(TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel) != 1) {
        printf("Fail samplesperpixel\n");
    }

    if(TIFFGetField(tif, TIFFTAG_SAMPLEFORMAT, &sampleformat) != 1) {
        printf("Fail sampleformat\n");
    }

    if(TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric) != 1) {
        printf("Fail photometric\n");
    }

    if(TIFFGetField(tif, TIFFTAG_EXTRASAMPLES, &extrasamples) != 1) {
        printf("Fail extrasamples\n");
    }

    if(TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarconf) != 1) {
        // Needs to be PLANARCONFIG_CONTIG.
        printf("Fail planarconf\n");
    }

    if(TIFFGetField(tif, TIFFTAG_ICCPROFILE, &icc_count, &icc_data) != 1) {
        printf("Fail icc\n");
    }

    const auto scanlinesize = TIFFScanlineSize64(tif);
    std::vector<char> line(scanlinesize, 0);
    for(uint32_t row = 0; row < h; ++row) {
        if(TIFFReadScanline(tif, line.data(), row) != 1) {
            printf("Fail in decoding.\n");
            return 1;
        }
    }
    printf("W: %d\nH: %d\nD: %d\n", w, h, d);
    printf("Inkset: %d\n", inkset);
    printf("Bitspersample: %d\n", bitspersample);
    printf("Samplesperpixel: %d\n", samplesperpixel);
    printf("Sampleformat: %d\n", sampleformat);
    printf("Extrasamples: %d\n", extrasamples);
    printf("Photometric: %d\n", photometric);
    printf("Planarconfig: %d\n", planarconf);
    printf("Icccount: %d\n", icc_count);
    TIFFClose(tif);
    return 0;
}
