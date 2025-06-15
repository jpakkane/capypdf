// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <utils.hpp>
#include <objectformatter.hpp>
#include <zlib.h>
#include <cassert>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace capypdf::internal {

namespace {

const char *transition_names_data[12] = {
    "/Split",
    "/Blinds",
    "/Box",
    "/Wipe",
    "/Dissolve",
    "/Glitter",
    "/R",
    "/Fly",
    "/Push",
    "/Cover",
    "/Uncover",
    "/Fade",
};

const pystd2025::Span<const char *> transition_names(transition_names_data,
                                                     sizeof(transition_names_data));

struct UtfDecodeStep {
    uint32_t byte1_data_mask;
    uint32_t num_subsequent_bytes;
};

bool is_valid_uf8_character(pystd2025::CStringView input, size_t cur, const UtfDecodeStep &par) {
    const uint32_t byte1 = uint32_t((unsigned char)input[cur]);
    const uint32_t subsequent_header_mask = 0b011000000;
    const uint32_t subsequent_header_value = 0b10000000;
    const uint32_t subsequent_data_mask = 0b111111;
    const uint32_t subsequent_num_data_bits = 6;

    if(cur + par.num_subsequent_bytes >= input.size()) {
        return false;
    }
    uint32_t unpacked = byte1 & par.byte1_data_mask;
    for(uint32_t i = 0; i < par.num_subsequent_bytes; ++i) {
        unpacked <<= subsequent_num_data_bits;
        const uint32_t subsequent = uint32_t((unsigned char)input[cur + 1 + i]);
        if((subsequent & subsequent_header_mask) != subsequent_header_value) {
            return false;
        }
        assert((unpacked & subsequent_data_mask) == 0);
        unpacked |= subsequent & subsequent_data_mask;
    }

    return true;
}

void append_glyph_to_utf16be(uint32_t glyph, pystd2025::Vector<uint16_t> &u16buf) {
    if(glyph < 0x10000) {
        u16buf.push_back((uint16_t)glyph);
    } else {
        const auto reduced = glyph - 0x10000;
        const auto high_surrogate = (reduced >> 10) + 0xD800;
        const auto low_surrogate = (reduced & 0b1111111111) + 0xDC00;
        u16buf.push_back((uint16_t)high_surrogate);
        u16buf.push_back((uint16_t)low_surrogate);
    }
}

bool needs_quoting(const unsigned char c) {
    if(c < '!' || c > '~') {
        return true;
    }
    if(c == '#' || c == '(' || c == ')' || c == ' ' || c == '/') {
        return true;
    }
    return false;
}

template<typename T> rvoe<T> do_file_load(FILE *f) {
    if(fseek(f, 0, SEEK_END) != 0) {
        perror(nullptr);
        RETERR(FileReadError);
    }
    auto fsize = ftell(f);
    if(fsize < 0) {
        perror(nullptr);
        RETERR(FileReadError);
    }
    T contents;
    contents.resize(fsize);
    if(fseek(f, 0, SEEK_SET) != 0) {
        perror(nullptr);
        RETERR(FileReadError);
    }
    size_t rc;
    if constexpr(pystd2025::is_same_v<T, pystd2025::CString>) {
        // FIXME, check for embedded nulls.
        rc = fread(contents.mutable_data(), 1, fsize, f);
    } else {
        rc = fread(contents.data(), 1, fsize, f);
    }
    if(rc != (size_t)fsize) {
        perror(nullptr);
        RETERR(FileReadError);
    }
    return contents;
}

struct DeflateCloser {
    static void del(z_stream *zs) {
        if(zs) {
            auto rc = deflateEnd(zs);
            if(rc != Z_OK) {
                fprintf(stderr, "Zlib error when closing: %s\n", zs->msg);
            }
        }
    }
};

} // namespace

rvoe<pystd2025::Bytes> flate_compress(pystd2025::CStringView data) {
    pystd2025::Bytes compressed;
    const int CHUNK = 1024 * 1024;
    pystd2025::Bytes buf;
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    auto ret = deflateInit(&strm, Z_BEST_COMPRESSION);
    if(ret != Z_OK) {
        RETERR(CompressionFailure);
    }
    pystd2025::unique_ptr<z_stream, DeflateCloser> zcloser(&strm);
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
        RETERR(CompressionFailure);
    }
    /* done when last data in file processed */
    if(ret != Z_STREAM_END) {
        RETERR(CompressionFailure);
    }

    return compressed;
}

rvoe<pystd2025::Bytes> flate_compress(pystd2025::BytesView data) {
    pystd2025::CStringView sv((const char *)data.data(), data.size());
    return flate_compress(sv);
}

rvoe<pystd2025::CString> load_file_as_string(const char *fname) {
    FILE *f = fopen(fname, "rb");
    if(!f) {
        perror(nullptr);
        RETERR(CouldNotOpenFile);
    }
    pystd2025::unique_ptr<FILE, FileCloser> fcloser(f);
    return load_file_as_string(f);
}

rvoe<pystd2025::CString> load_file_as_string(const pystd2025::Path &fname) {
    if(!fname.is_file()) {
        RETERR(FileDoesNotExist);
    }

    return load_file_as_string(fname.c_str());
}

rvoe<pystd2025::CString> load_file_as_string(FILE *f) {
    return do_file_load<pystd2025::CString>(f);
}

rvoe<pystd2025::Bytes> load_file_as_bytes(const pystd2025::Path &fname) {
    FILE *f = fopen(fname.c_str(), "rb");
    if(!f) {
        perror(nullptr);
        RETERR(CouldNotOpenFile);
    }
    pystd2025::unique_ptr<FILE, FileCloser> fcloser(f);
    return load_file_as_bytes(f);
}

rvoe<pystd2025::Bytes> load_file_as_bytes(FILE *f) { return do_file_load<pystd2025::Bytes>(f); }

void write_file(const char *ofname, const char *buf, size_t bufsize) {
    FILE *f = fopen(ofname, "wb");
    if(!f) {
        abort();
    }
    if(fwrite(buf, 1, bufsize, f) != bufsize) {
        abort();
    }
    fclose(f);
}

pystd2025::CString utf8_to_pdfutf16be(const u8string &input, bool add_adornments) {
    pystd2025::CString encoded(add_adornments ? "<FEFF" : ""); // PDF 2.0 spec, 7.9.2.2.1

    pystd2025::Vector<uint16_t> u16buf;
    for(const auto codepoint : input) {
        u16buf.clear();
        append_glyph_to_utf16be(codepoint, u16buf);
        for(const auto &u16 : u16buf) {
            const auto *lower_byte = (const unsigned char *)&u16;
            const auto *upper_byte = lower_byte + 1;
            pystd2025::format_append(
                encoded, "%02X%02X", (uint32_t)*upper_byte, (uint32_t)*lower_byte);
        }
    }
    if(add_adornments) {
        encoded += '>';
    }
    return encoded;
}

bool is_valid_utf8(pystd2025::CStringView input) {
    UtfDecodeStep par;
    // clang-format off
    const uint32_t twobyte_header_mask    = 0b11100000;
    const uint32_t twobyte_header_value   = 0b11000000;
    const uint32_t threebyte_header_mask  = 0b11110000;
    const uint32_t threebyte_header_value = 0b11100000;
    const uint32_t fourbyte_header_mask   = 0b11111000;
    const uint32_t fourbyte_header_value  = 0b11110000;
    // clang-format on
    for(size_t i = 0; i < input.size(); ++i) {
        const uint32_t code = uint32_t((unsigned char)input[i]);
        if(code < 0x80) {
            continue;
        } else if((code & twobyte_header_mask) == twobyte_header_value) {
            par.byte1_data_mask = 0b11111;
            par.num_subsequent_bytes = 1;
        } else if((code & threebyte_header_mask) == threebyte_header_value) {
            par.byte1_data_mask = 0b1111;
            par.num_subsequent_bytes = 2;
        } else if((code & fourbyte_header_mask) == fourbyte_header_value) {
            par.byte1_data_mask = 0b111;
            par.num_subsequent_bytes = 3;
        } else {
            return false;
        }
        if(!is_valid_uf8_character(input, i, par)) {
            return false;
        }
        i += par.num_subsequent_bytes;
    }
    return true;
}

pystd2025::CString current_date_string() {
    const int bufsize = 128;
    char buf[bufsize];
    time_t timepoint;
    struct tm utctime;

    if(getenv("SOURCE_DATE_EPOCH")) {
        timepoint = strtol(getenv("SOURCE_DATE_EPOCH"), nullptr, 10);
    } else {
        timespec tspec;
        clock_gettime(CLOCK_REALTIME, &tspec);
        timepoint = tspec.tv_sec;
    }
#ifdef _WIN32
    if(gmtime_s(&utctime, &timepoint) != 0) {
#else
    if(gmtime_r(&timepoint, &utctime) == nullptr) {
#endif
        perror(nullptr);
        abort();
    }
    strftime(buf, bufsize, "(D:%Y%m%d%H%M%SZ)", &utctime);
    return pystd2025::CString(buf);
}

pystd2025::CString pdfstring_quote(pystd2025::CStringView raw_string) {
    pystd2025::CString result;
    result.reserve(raw_string.size() * 2 + 2);
    result.push_back('(');
    for(const char c : raw_string) {
        switch(c) {
        case '(':
        case ')':
        case '\\':
            result.push_back('\\');
            break;
        default:
            break;
        }
        result.push_back(c);
    }
    result.push_back(')');
    return result;
}

pystd2025::CString u8str2u8textstring(const u8string &str) { return u8str2u8textstring(str.sv()); }

// PDF 2.0 spec 7.9.2.2
pystd2025::CString u8str2u8textstring(pystd2025::CStringView u8string) {
    pystd2025::CString result;
    result.reserve(u8string.size() + 10);
    result.push_back('(');
    result.push_back(char(239));
    result.push_back(char(187));
    result.push_back(char(191));
    for(const char c : u8string) {
        switch(c) {
        case '(':
        case ')':
        case '\\':
            result.push_back('\\');
            break;
        default:
            break;
        }
        result.push_back(c);
    }
    result.push_back(')');
    return result;
}

pystd2025::CString u8str2filespec(const u8string &str) {
    pystd2025::CString result;
    const char escaped_slash[4] = {'\\', '\\', '/', 0};
    result.reserve(str.size() + 10);
    result.push_back('(');
    result.push_back(char(239));
    result.push_back(char(187));
    result.push_back(char(191));
    for(const char c : str.sv()) {
        switch(c) {
        case '(':
        case ')':
        case '\\':
            result.push_back('\\');
            break;
        case '/':
            result += escaped_slash;
            break;
        default:
            break;
        }
        result.push_back(c);
    }
    result.push_back(')');
    return result;
}

pystd2025::CString pdfname_quote(pystd2025::CStringView raw_string) {
    pystd2025::CString result;
    result.reserve(raw_string.size() + 10);
    for(const char c : raw_string) {
        switch(c) {
        case '/':
            result += "#2F";
            break;
        case '#':
            result += "#23";
            break;
        default:
            result += c;
        }
    }
    return result;
}

bool is_ascii(pystd2025::CStringView text) {
    for(const auto c : text) {
        auto ci = int32_t((unsigned char)c);
        if(ci > 127) {
            return false;
        }
    }
    return true;
}

// As in PDF 2.0 spec 7.3.5
pystd2025::CString bytes2pdfstringliteral(pystd2025::CStringView raw, bool add_slash) {
    pystd2025::CString result;
    char buf[10];
    result.reserve(raw.size() + 1);
    if(add_slash) {
        result.push_back('/');
    }
    for(const unsigned char c : raw) {
        if(needs_quoting(c)) {
            result += '#';
            if(snprintf(buf, 10, "%02x", (unsigned int)c) != 2) {
                abort();
            }
            result += buf;
        } else {
            result += c;
        }
    }
    return result;
}

pystd2025::CString create_trailer_id() {
    const int num_bytes = 16;
    pystd2025::CString msg;
    msg.reserve(num_bytes * 2 + 2);
    msg.reserve(num_bytes * 2 + 2);
    msg.push_back('<');

    for(int i = 0; i < num_bytes; ++i) {
        auto big_num = random();
        int randnum = (int)(big_num & 0xFF);
        pystd2025::format_append(msg, "%02X", randnum);
    }
    msg.push_back('>');
    return msg;
}

void serialize_trans(ObjectFormatter &fmt, const Transition &t) {
    fmt.add_token("/Trans");
    fmt.begin_dict();
    if(t.type) {
        fmt.add_token("/S");
        fmt.add_token(transition_names.at((int32_t)*t.type));
    }
    if(t.duration) {
        fmt.add_token("/D");
        fmt.add_token(*t.duration);
    }
    if(t.Dm) {
        fmt.add_token("/Dm");
        fmt.add_token(*t.Dm == CAPY_TR_DIM_H ? "/H" : "/V");
    }
    if(t.Di) {
        fmt.add_token("/Di");
        fmt.add_token(*t.Di);
    }
    if(t.M) {
        fmt.add_token("/M");
        fmt.add_token(*t.M == CAPY_TR_M_I ? "/I" : "/O");
    }
    if(t.SS) {
        fmt.add_token("/SS");
        fmt.add_token(*t.SS);
    }
    if(t.B) {
        fmt.add_token("/B");
        fmt.add_token(*t.B ? "true" : "false");
    }
    fmt.end_dict();
}

void quote_xml_element_data_into(const u8string &content, pystd2025::CString &result) {
    auto content_view = content.sv();
    for(char c : content_view) {
        switch(c) {
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '&':
            result += "&quot;";
            break;
        default:
            result += c;
            break;
        }
    }
}

pystd2025::BytesView str2span(const pystd2025::CString &s) {
    return pystd2025::BytesView(s.data(), s.size());
}

pystd2025::CStringView span2sv(pystd2025::BytesView s) {
    auto *ptr = (const char *)s.data();
    return pystd2025::CStringView(ptr, s.size());
}

} // namespace capypdf::internal
