/*
 * Copyright 2022-2023 Jussi Pakkanen
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

#include <utils.hpp>
#include <zlib.h>
#include <cassert>
#include <cstring>
#include <iconv.h>
#include <sys/time.h>

#include <fmt/core.h>
#include <memory>

namespace A4PDF {

std::expected<std::string, ErrorCode> flate_compress(std::string_view data) {
    std::string compressed;
    const int CHUNK = 1024 * 1024;
    std::string buf;
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    auto ret = deflateInit(&strm, Z_BEST_COMPRESSION);
    if(ret != Z_OK) {
        return std::unexpected(ErrorCode::CompressionFailure);
    }
    std::unique_ptr<z_stream, int (*)(z_stream *)> zcloser(&strm, deflateEnd);
    strm.avail_in = data.size();
    strm.next_in = (Bytef *)(data.data()); // Very unsafe.

    do {
        buf.resize(CHUNK);
        strm.avail_out = CHUNK;
        strm.next_out = (Bytef *)buf.data();
        ret = deflate(&strm, Z_FINISH); /* no bad return value */
        assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
        int write_size = CHUNK - strm.avail_out;
        assert(write_size <= (int)buf.size());
        buf.resize(write_size);
        compressed += buf;
    } while(strm.avail_out == 0);
    if(strm.avail_in != 0) { /* all input will be used */
        return std::unexpected(ErrorCode::CompressionFailure);
    }
    /* done when last data in file processed */
    if(ret != Z_STREAM_END) {
        std::unexpected(ErrorCode::CompressionFailure);
    }

    return std::move(compressed);
}

std::expected<std::string, ErrorCode> load_file(const char *fname) {
    FILE *f = fopen(fname, "rb");
    if(!f) {
        perror(nullptr);
        return std::unexpected(ErrorCode::CouldNotOpenFile);
    }
    std::unique_ptr<FILE, int (*)(FILE *)> fcloser(f, fclose);
    return load_file(f);
}

std::expected<std::string, ErrorCode> load_file(FILE *f) {
    fseek(f, 0, SEEK_END);
    auto fsize = (size_t)ftell(f);
    std::string contents(fsize, '\0');
    fseek(f, 0, SEEK_SET);
    if(fread(contents.data(), 1, fsize, f) != fsize) {
        fclose(f);
        perror(nullptr);
        return std::unexpected(ErrorCode::FileReadError);
    }
    return contents;
}

std::expected<std::string, ErrorCode> utf8_to_pdfmetastr(std::string_view input) {
    // For now put everything into UTF-16 bracketstrings.
    std::string encoded = "<FEFF";

    std::string u16buf(input.length() * 4 + 10, '\0');
    errno = 0;
    auto cd = iconv_open("UTF-16BE", "UTF-8"); // PDF 1.7 spec 7.9.2.2
    if(errno != 0) {
        throw std::runtime_error(strerror(errno));
    }
    auto in_ptr = (char *)input.data();
    auto out_ptr = (char *)u16buf.data();
    auto in_bytes = input.length();
    auto out_bytes = u16buf.length();
    auto iconv_result = iconv(cd, &in_ptr, &in_bytes, &out_ptr, &out_bytes);
    iconv_close(cd);
    if(errno != 0) {
        perror(nullptr);
        return std::unexpected(ErrorCode::IconvError);
    }
    if(iconv_result == (size_t)-1) {
        perror(nullptr);
        return std::unexpected(ErrorCode::IconvError);
    }
    if(in_bytes != 0) {
        return std::unexpected(ErrorCode::BadUtf8);
    }

    auto bi = std::back_inserter(encoded);
    for(const char *i = u16buf.data(); i != out_ptr; ++i) {
        fmt::format_to(bi, "{:02X}", (unsigned char)(*i));
    }
    encoded += '>';
    return std::move(encoded);
}

std::string current_date_string() {
    const int bufsize = 128;
    char buf[bufsize];
    time_t timepoint;
    struct tm utctime;
    struct timespec tp;
    if(getenv("SOURCE_DATE_EPOCH")) {
        timepoint = strtol(getenv("SOURCE_DATE_EPOCH"), nullptr, 10);
    } else {
        if(clock_gettime(CLOCK_REALTIME, &tp) != 0) {
            throw std::runtime_error(strerror(errno));
        }
        timepoint = tp.tv_sec;
    }
    if(gmtime_r(&timepoint, &utctime) == nullptr) {
        throw std::runtime_error(strerror(errno));
    }
    strftime(buf, bufsize, "(D:%Y%m%d%H%M%SZ)", &utctime);
    return std::string(buf);
}

} // namespace A4PDF
