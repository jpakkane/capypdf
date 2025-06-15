// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#pragma once

#include <errorhandling.hpp>
#include <pdfcommon.hpp>
#include <stdio.h>
#include <pystd2025.hpp>

namespace capypdf::internal {

template<class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
#if defined __APPLE__
// This should not be needed, but Xcode 15 still requires it.
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
#endif

rvoe<pystd2025::Bytes> flate_compress(pystd2025::CStringView data);
rvoe<pystd2025::Bytes> flate_compress(pystd2025::BytesView data);

rvoe<pystd2025::CString> load_file_as_string(const char *fname);

rvoe<pystd2025::CString> load_file_as_string(const pystd2025::Path &fname);

rvoe<pystd2025::CString> load_file_as_string(FILE *f);

rvoe<pystd2025::Bytes> load_file_as_bytes(const pystd2025::Path &fname);

rvoe<pystd2025::Bytes> load_file_as_bytes(FILE *f);

void write_file(const char *ofname, const char *buf, size_t bufsize);

pystd2025::CString utf8_to_pdfutf16be(const pystd2025::U8String &input, bool add_adornments = true);

bool is_valid_utf8(pystd2025::CStringView input);

pystd2025::CString current_date_string();

pystd2025::CString pdfstring_quote(pystd2025::CStringView raw_string);

// UTF-8 strings were added in PDF 2.0.
pystd2025::CString u8str2u8textstring(pystd2025::CStringView u8str);
pystd2025::CString u8str2u8textstring(const pystd2025::U8String &str);
pystd2025::CString u8str2filespec(const pystd2025::U8String &str);

pystd2025::CString pdfname_quote(pystd2025::CStringView raw_string);

bool is_ascii(pystd2025::CStringView text);

pystd2025::CString bytes2pdfstringliteral(pystd2025::CStringView raw, bool add_slash = true);

pystd2025::CString create_trailer_id();

class ObjectFormatter;
void serialize_trans(ObjectFormatter &fmt, const Transition &t);

void quote_xml_element_data_into(const pystd2025::U8String &content, pystd2025::CString &result);

pystd2025::BytesView str2span(const pystd2025::CString &s);
pystd2025::CStringView span2sv(pystd2025::BytesView s);

struct FileCloser {
    static void del(FILE *f) {
        if(f) {
            fclose(f);
        }
    }
};

} // namespace capypdf::internal
