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

rvoe<std::string> utf8_to_pdfmetastr(const u8string &input);

bool is_valid_utf8(std::string_view input);

std::string current_date_string();

std::string pdfstring_quote(std::string_view raw_string);

bool is_ascii(std::string_view text);

std::string create_trailer_id();

} // namespace capypdf
