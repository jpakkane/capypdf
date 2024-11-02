// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <pdfcommon.hpp>
#include <utils.hpp>
#include <cassert>

namespace capypdf::internal {

namespace {

struct UtfDecodeStep {
    uint32_t byte1_data_mask;
    uint32_t num_subsequent_bytes;
};

uint32_t unpack_one(const unsigned char *valid_utf8, const UtfDecodeStep &par) {
    const uint32_t byte1 = uint32_t(*valid_utf8);
    const uint32_t subsequent_data_mask = 0b111111;
    const uint32_t subsequent_num_data_bits = 6;

    uint32_t unpacked = byte1 & par.byte1_data_mask;
    for(uint32_t i = 0; i < par.num_subsequent_bytes; ++i) {
        unpacked <<= subsequent_num_data_bits;
        const uint32_t subsequent = uint32_t((unsigned char)valid_utf8[1 + i]);
        assert((unpacked & subsequent_data_mask) == 0);
        unpacked |= subsequent & subsequent_data_mask;
    }

    return unpacked;
}

} // namespace

const std::array<const char *, (int)CAPY_STRUCTURE_TYPE_NUM_ITEMS> structure_type_names{
    // clang-format off
    "Document",
    "DocumentFragment",
    "Part",
    "Sect",
    "Div",
    "Aside",
    "NonStruct",
    "P",
    "H",
    "H1",
    "H2",
    "H3",
    "H4",
    "H5",
    "H6",
    "H7",
    "Title",
    "FENote",
    "Sub",
    "Lbl",
    "Span",
    "Em",
    "Strong",
    "Link",
    "Annot",
    "Form",
    "Ruby",
    "RB",
    "RT",
    "RP",
    "Warichu",
    "WT",
    "WP",
    "L",
    "LI",
    "LBody",
    "Table",
    "TR",
    "TH",
    "TD",
    "THead",
    "TBody",
    "TFoot",
    "Caption",
    "Figure",
    "Formula",
    "Artifact",
    // clang-format on
};

void TransparencyGroupProperties::serialize(std::back_insert_iterator<std::string> &app,
                                            const char *indent) const {
    std::format_to(app, "<<\n{}  /Type /Group\n{}  /S /Transparency\n", indent, indent);
    if(CS) {
        std::format_to(app, "{}  /CS {}\n", indent, colorspace_names.at((int)CS.value()));
    }
    if(I) {
        std::format_to(app, "{}  /I {}\n", indent, I.value() ? "true" : "false");
    }
    if(K) {
        std::format_to(app, "{}  /K {}\n", indent, K.value() ? "true" : "false");
    }
    std::format_to(app, "{}>>\n", indent);
}

rvoe<asciistring> asciistring::from_cstr(const char *cstr) {
    if(!is_ascii(cstr)) {
        RETERR(NotASCII);
    }
    return asciistring(cstr);
}

rvoe<u8string> u8string::from_cstr(const char *cstr) {
    if(!is_valid_utf8(cstr)) {
        RETERR(BadUtf8);
    }
    return u8string(cstr);
}

CodepointIterator::CharInfo CodepointIterator::extract_one_codepoint(const unsigned char *buf) {
    UtfDecodeStep par;
    // clang-format off
    const uint32_t twobyte_header_mask    = 0b11100000;
    const uint32_t twobyte_header_value   = 0b11000000;
    const uint32_t threebyte_header_mask  = 0b11110000;
    const uint32_t threebyte_header_value = 0b11100000;
    const uint32_t fourbyte_header_mask   = 0b11111000;
    const uint32_t fourbyte_header_value  = 0b11110000;
    // clang-format on
    const uint32_t code = uint32_t((unsigned char)buf[0]);
    if(code < 0x80) {
        return CharInfo{code, 1};
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
        std::abort();
    }
    return CharInfo{unpack_one(buf, par), 1 + par.num_subsequent_bytes};
}

} // namespace capypdf::internal
