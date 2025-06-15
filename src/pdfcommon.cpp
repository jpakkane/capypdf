// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <pdfcommon.hpp>
#include <objectformatter.hpp>
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

const char *structure_type_names_data[CAPY_STRUCTURE_TYPE_NUM_ITEMS] = {
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

} // namespace

const pystd2025::Span<const char *> structure_type_names(structure_type_names_data,
                                                         sizeof(structure_type_names_data));

rvoe<PdfRectangle> PdfRectangle::construct(double l, double b, double r, double t) {
    if(r <= l) {
        RETERR(InvalidBBox);
    }
    if(t <= b) {
        RETERR(InvalidBBox);
    }
    return PdfRectangle(l, b, r, t);
}

void TransparencyGroupProperties::serialize(ObjectFormatter &fmt) const {
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/Group");
    fmt.add_token_pair("/S", "/Transparency");

    if(CS) {
        fmt.add_token("/CS");
        fmt.add_token(colorspace_names.at((int)CS.value()));
    }
    if(I) {
        fmt.add_token("/I");
        fmt.add_token(I.value() ? "true" : "false");
    }
    if(K) {
        fmt.add_token("/K");
        fmt.add_token(K.value() ? "true" : "false");
    }
    fmt.end_dict();
}

rvoe<asciistring> asciistring::from_cstr(const char *cstr) {
    return asciistring::from_view(pystd2025::CStringView(cstr));
}

rvoe<asciistring> asciistring::from_view(pystd2025::CStringView sv) {
    if(!is_ascii(sv)) {
        RETERR(NotASCII);
    }
    auto astr = asciistring(sv);
    if(astr.sv().find('\0') != (size_t)-1) {
        RETERR(EmbeddedNullInString);
    }
    return astr;
}

rvoe<u8string> u8string::from_cstr(const char *cstr) {
    if(!is_valid_utf8(cstr)) {
        RETERR(BadUtf8);
    }
    return u8string{cstr};
}

rvoe<u8string> u8string::from_view(pystd2025::CStringView sv) {
    if(!is_valid_utf8(sv)) {
        RETERR(BadUtf8);
    }
    auto ustr = u8string(sv);
    if(ustr.sv().find('\0') != (size_t)-1) {
        RETERR(EmbeddedNullInString);
    }
    return ustr;
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

RawData::RawData(pystd2025::CString input) : storage{std::move(input)} {};
RawData::RawData(pystd2025::Bytes input) : storage(std::move(input)) {}

RawData::RawData(pystd2025::CStringView input) : storage{pystd2025::CString(input)} {}

RawData::RawData(pystd2025::BytesView input) {
    pystd2025::Bytes data_copy(input.data(), input.size());
    storage = std::move(data_copy);
}

const char *RawData::data() const {
    if(auto *d = storage.get_if<pystd2025::CString>()) {
        return d->data();
    } else if(auto *d = storage.get_if<pystd2025::Bytes>()) {
        return (const char *)d->data();
    } else {
        std::abort();
    }
}

size_t RawData::size() const {
    if(auto *d = storage.get_if<pystd2025::CString>()) {
        return d->size();
    } else if(auto *d = storage.get_if<pystd2025::Bytes>()) {
        return d->size();
    } else {
        std::abort();
    }
}

pystd2025::CStringView RawData::sv() const { return pystd2025::CStringView{data(), size()}; }

pystd2025::BytesView RawData::span() const { return pystd2025::BytesView{data(), size()}; }

bool RawData::empty() const {
    if(auto *p = storage.get_if<pystd2025::CString>()) {
        return p->is_empty();
    } else if(auto *p = storage.get_if<pystd2025::Bytes>()) {
        return p->is_empty();
    } else {
        std::abort();
    }
}

void RawData::clear() {
    if(auto *p = storage.get_if<pystd2025::CString>()) {
        p->clear();
    } else if(auto *p = storage.get_if<pystd2025::Bytes>()) {
        p->clear();
    } else {
        std::abort();
    }
}

void RawData::assign(const char *buf, size_t bufsize) { storage = pystd2025::Bytes{buf, bufsize}; }

RawData &RawData::operator=(pystd2025::CString input) {
    storage = std::move(input);
    return *this;
}

RawData &RawData::operator=(pystd2025::Bytes input) {
    storage = std::move(input);
    return *this;
}

bool RawData::operator==(pystd2025::CStringView other) const { return sv() == other; }

bool RawData::operator==(pystd2025::BytesView other) const {
    pystd2025::CStringView other_sv{(const char *)other.data(), other.size()};
    return *this == other_sv;
}

} // namespace capypdf::internal
