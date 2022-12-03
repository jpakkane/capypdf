/*
 * Copyright 2022 Jussi Pakkanen
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

#include <pdfgen.hpp>
#include <imageops.hpp>
#include <cstring>
#include <cerrno>
#include <lcms2.h>
#include <stdexcept>
#include <array>
#include <fmt/core.h>

namespace {

const char PDF_header[] = "%PDF-1.7\n\xe5\xf6\xc4\xd6\n";

const std::array<const char *, 9> font_names{
    "Times-Roman",
    "Helvetica",
    "Courier",
    "Times-Roman-Bold",
    "Helvetica-Bold",
    "Courier-Bold",
    "Times-Italic",
    "Helvetica-Oblique",
    "Courier-Oblique",
};

const char default_srgb_profile[] = "/usr/share/color/icc/ghostscript/srgb.icc";
const char default_gray_profile[] = "/usr/share/color/icc/ghostscript/sgray.icc";
const char default_cmyk_profile[] =
    "/home/jpakkane/Downloads/temp/Adobe ICC Profiles (end-user)/CMYK/UncoatedFOGRA29.icc";

} // namespace

LcmsHolder::~LcmsHolder() { deallocate(); }

void LcmsHolder::deallocate() {
    if(h) {
        cmsCloseProfile(h);
    }
    h = nullptr;
}

PdfGen::PdfGen(const char *ofname, const PdfGenerationData &d) : opts{d} {
    ofile = fopen(ofname, "wb");
    if(!ofile) {
        throw std::runtime_error(strerror(errno));
    }
    write_header();
    write_info();
    load_profiles();
}

PdfGen::~PdfGen() {
    try {
        close_file();
    } catch(const std::exception &e) {
        fprintf(stderr, "%s", e.what());
        fclose(ofile);
        return;
    }

    if(fflush(ofile) != 0) {
        perror("Flushing file contents failed");
    }
    if(fclose(ofile) != 0) {
        perror("Closing output file failed");
    }
}

ICCInfo PdfGen::load_icc_profile(const char *fname) {
    std::string contents = load_file(fname);
    cmsHPROFILE h = cmsOpenProfileFromMem(contents.data(), contents.size());
    if(!h) {
        throw std::runtime_error(std::string("Could not open color profile ") + fname);
    }
    std::string compressed = flate_compress(contents);
    std::string buf;
    const auto num_channels = (int32_t)cmsChannelsOf(cmsGetColorSpace(h));
    fmt::format_to(std::back_inserter(buf),
                   R"(<<
  /Filter /FlateDecode
  /Length {}
  /N {}
>>
stream
)",
                   compressed.size(),
                   num_channels);
    buf += compressed;
    buf += "\nendstream\n";
    return ICCInfo{add_object(buf), num_channels, LcmsHolder(h)};
}

void PdfGen::load_profiles() {
    icc_handles.reserve(10);
    icc_handles.emplace_back(load_icc_profile(default_srgb_profile));
    if(icc_handles.back().num_channels != 3) {
        throw std::runtime_error("RGB profile does not have 3 color channels.");
    }
    icc_handles.emplace_back(load_icc_profile(default_gray_profile));
    if(icc_handles.back().num_channels != 1) {
        throw std::runtime_error("Gray profile does not have 1 color channel.");
    }
    icc_handles.emplace_back(load_icc_profile(default_cmyk_profile));
    if(icc_handles.back().num_channels != 4) {
        throw std::runtime_error("CMYK profile does not have 4 color channels.");
    }
}

void PdfGen::write_bytes(const char *buf, size_t buf_size) {
    if(fwrite(buf, 1, buf_size, ofile) != buf_size) {
        throw std::runtime_error(strerror(errno));
    }
}

void PdfGen::write_header() { write_bytes(PDF_header, strlen(PDF_header)); }

void PdfGen::write_info() {
    std::string obj_data{"<<\n"};
    if(!opts.title.empty()) {
        obj_data += "  /Title (";
        obj_data += opts.title; // FIXME, do escaping.
        obj_data += ")\n";
    }
    if(!opts.author.empty()) {
        obj_data += "  /Author (";
        obj_data += opts.author; // FIXME, here too.
        obj_data += ")\n";
    }
    obj_data += "  /Producer (PDF Testbed generator)\n>>\n";
    add_object(obj_data);
}

void PdfGen::close_file() {
    write_pages();
    write_catalog();
    const int64_t xref_offset = ftell(ofile);
    write_cross_reference_table();
    write_trailer(xref_offset);
}

void PdfGen::write_pages() {
    const auto pages_obj_num = (int32_t)(object_offsets.size() + pages.size() + 1);

    std::string buf;

    std::vector<int32_t> page_objects;
    for(const auto &i : pages) {
        buf.clear();
        fmt::format_to(std::back_inserter(buf),
                       R"(<<
  /Type /Page
  /Parent {} 0 R
  /MediaBox [ {} {} {} {} ]
  /Contents {} 0 R
  /Resources {} 0 R
>>
)",
                       pages_obj_num,
                       opts.mediabox.x,
                       opts.mediabox.y,
                       opts.mediabox.w,
                       opts.mediabox.h,
                       i.commands_obj_num,
                       i.resource_obj_num);

        page_objects.push_back(add_object(buf));
    }
    buf = R"(<<
  /Type /Pages
  /Kids [
)";
    for(const auto &i : page_objects) {
        fmt::format_to(std::back_inserter(buf), "    {} 0 R\n", i);
    }
    fmt::format_to(std::back_inserter(buf), "  ]\n  /Count {}\n>>\n", page_objects.size());
    const auto actual_number = add_object(buf);
    if(actual_number != pages_obj_num) {
        throw std::runtime_error("Buggy McBugFace!");
    }
}

void PdfGen::write_catalog() {
    const int32_t pages_obj_num = (int32_t)object_offsets.size();
    std::string buf;
    fmt::format_to(std::back_inserter(buf),
                   R"(<<
  /Type /Catalog
  /Pages {} 0 R
>>
)",
                   pages_obj_num);
    add_object(buf);
}

void PdfGen::write_cross_reference_table() {
    std::string buf;
    fmt::format_to(
        std::back_inserter(buf),
        R"(xref
0 {}
0000000000 65535 f{}
)",
        object_offsets.size() + 1,
        " "); // Qt Creator removes whitespace at the end of lines but it is significant here.
    for(const auto &i : object_offsets) {
        fmt::format_to(std::back_inserter(buf), "{:010} 00000 n \n", i);
    }
    write_bytes(buf);
}

void PdfGen::write_trailer(int64_t xref_offset) {
    const int32_t info = 1;                     // Info object is the first printed.
    const int32_t root = object_offsets.size(); // Root object is the last one printed.
    std::string buf;
    fmt::format_to(std::back_inserter(buf),
                   R"(trailer
<<
  /Size {}
  /Root {} 0 R
  /Info {} 0 R
>>
startxref
{}
%%EOF
)",
                   object_offsets.size() + 1,
                   root,
                   info,
                   xref_offset);
    write_bytes(buf);
}

PdfPage PdfGen::new_page() { return PdfPage(this); }

void PdfGen::add_page(std::string_view resource_data, std::string_view page_data) {
    const auto resource_num = add_object(resource_data);
    const auto page_num = add_object(page_data);
    pages.emplace_back(PageOffsets{resource_num, page_num});
}

int32_t PdfGen::add_object(std::string_view object_data) {
    auto object_num = (int32_t)object_offsets.size() + 1;
    object_offsets.push_back(ftell(ofile));
    const int bufsize = 128;
    char buf[bufsize];
    snprintf(buf, bufsize, "%d 0 obj\n", object_num);
    write_bytes(buf);
    write_bytes(object_data);
    write_bytes("endobj\n");
    return object_num;
}

ImageId PdfGen::load_image(const char *fname) {
    auto image = load_image_file(fname);
    std::string buf;
    int32_t smask_id = -1;
    if(image.alpha) {
        auto compressed = flate_compress(*image.alpha);
        fmt::format_to(std::back_inserter(buf),
                       R"(<<
  /Type /XObject
  /Subtype /Image
  /Width {}
  /Height {}
  /ColorSpace /DeviceGray
  /BitsPerComponent 8
  /Length {}
  /Filter /FlateDecode
>>
stream
)",
                       image.w,
                       image.h,
                       compressed.size());
        buf += compressed;
        buf += "\nendstream\n";
        smask_id = add_object(buf);
        buf.clear();
    }
    switch(opts.output_colorspace) {
    case PDF_DEVICE_RGB: {
        const auto compressed = flate_compress(image.pixels);
        fmt::format_to(std::back_inserter(buf),
                       R"(<<
  /Type /XObject
  /Subtype /Image
  /ColorSpace [/ICCBased {} 0 R]
  /Width {}
  /Height {}
  /BitsPerComponent 8
  /Length {}
  /Filter /FlateDecode
)",
                       icc_handles[0].obj_num,
                       image.w,
                       image.h,
                       compressed.size());
        if(smask_id >= 0) {
            fmt::format_to(std::back_inserter(buf), "/SMask {} 0 R\n", smask_id);
        }
        buf += ">>\nstream\n";
        buf += compressed;
        buf += "\nendstream\n";
        auto im_id = add_object(buf);
        image_info.emplace_back(ImageInfo{{image.w, image.h}, im_id});
        return ImageId{(int32_t)image_info.size() - 1};
    }
    case PDF_DEVICE_GRAY: {
        const int32_t num_pixels = (int32_t)image.pixels.size() / 3;
        std::string converted_pixels(num_pixels, '\0');
        auto transform = cmsCreateTransform(icc_handles[0].lcms.h,
                                            TYPE_RGB_8,
                                            icc_handles[1].lcms.h,
                                            TYPE_GRAY_8,
                                            INTENT_RELATIVE_COLORIMETRIC,
                                            0);
        cmsDoTransform(transform, image.pixels.data(), converted_pixels.data(), num_pixels);
        cmsDeleteTransform(transform);
        const auto compressed = flate_compress(converted_pixels);
        fmt::format_to(std::back_inserter(buf),
                       R"(<<
  /Type /XObject
  /Subtype /Image
  /ColorSpace [/ICCBased {} 0 R]
  /Width {}
  /Height {}
  /BitsPerComponent 8
  /Length {}
  /Filter /FlateDecode
)",
                       icc_handles[1].obj_num, // FIXME, maybe this should be DeviceGray?
                       image.w,
                       image.h,
                       compressed.size());
        if(smask_id >= 0) {
            fmt::format_to(std::back_inserter(buf), "/SMask {} 0 R\n", smask_id);
        }
        buf += ">>\nstream\n";
        buf += compressed;
        buf += "\nendstream\n";
        auto im_id = add_object(buf);
        image_info.emplace_back(ImageInfo{{image.w, image.h}, im_id});
        return ImageId{(int32_t)image_info.size() - 1};
    }
    case PDF_DEVICE_CMYK: {
        throw std::runtime_error("Not implemented.\n");
    }
    }
    throw std::runtime_error("Unreachable.");
}

FontId PdfGen::get_builtin_font_id(BuiltinFonts font) {
    auto it = builtin_fonts.find(font);
    if(it != builtin_fonts.end()) {
        return it->second;
    }
    std::string font_dict;
    fmt::format_to(std::back_inserter(font_dict),
                   R"(<<
  /Type /Font
  /Subtype /Type1
  /BaseFont /{}
>>
)",
                   font_names[font]);
    font_objects.push_back(add_object(font_dict));
    auto fontid = FontId{(int32_t)font_objects.size() - 1};
    builtin_fonts[font] = fontid;
    return fontid;
}
