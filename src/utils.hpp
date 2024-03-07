// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#pragma once

#include <errorhandling.hpp>
#include <pdfcommon.hpp>
#include <string>
#include <expected>
#include <cstdio>
#include <string_view>
#include <filesystem>
#include <vector>

namespace capypdf {

template<class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
#if defined __APPLE__
// This should not be needed, but Xcode 15 still requires it.
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
#endif

rvoe<std::string> flate_compress(std::string_view data);

rvoe<std::string> load_file(const char *fname);

rvoe<std::string> load_file(const std::filesystem::path &fname);

rvoe<std::string> load_file(FILE *f);

void write_file(const char *ofname, const char *buf, size_t bufsize);

std::string utf8_to_pdfmetastr(const u8string &input);

bool is_valid_utf8(std::string_view input);

std::string current_date_string();

std::string pdfstring_quote(std::string_view raw_string);

bool is_ascii(std::string_view text);

std::string bytes2pdfstringliteral(std::string_view raw, bool add_slash = true);

std::string create_trailer_id();

void serialize_trans(std::back_insert_iterator<std::string> buf_append,
                     const Transition &t,
                     std::string_view indent);

} // namespace capypdf
