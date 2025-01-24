// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#pragma once

#include <string>
#include <variant>
#include <optional>
#include <vector>
#include <unordered_map>

struct PdfTokenArrayStart {};

struct PdfTokenArrayEnd {};

struct PdfTokenDictStart {};

struct PdfTokenDictEnd {};

struct PdfTokenString {
    explicit PdfTokenString(std::string s) : text(std::move(s)) {}
    std::string text;
};

struct PdfTokenStringLiteral {
    explicit PdfTokenStringLiteral(std::string s) : text(std::move(s)) {}
    std::string text;
};

struct PdfTokenObjName {
    PdfTokenObjName(int number_, int version_) : number(number_), version(version_) {}
    int number;
    int version;
};

struct PdfTokenHexString {
    explicit PdfTokenHexString(std::string s) : text(std::move(s)) {}
    std::string text;
};

struct PdfTokenObjRef {
    PdfTokenObjRef(int o, int v) {
        objnum = o;
        version = v;
    }
    int objnum, version;
};

struct PdfTokenInteger {
    int64_t value;
};

struct PdfTokenReal {
    double value;
};

struct PdfTokenBoolean {
    bool value;
};

struct PdfStreamData {
    std::string stream;
};

struct PdfTokenEndObj {};

struct PdfTokenFinished {};

struct PdfTokenError {};

typedef std::variant<PdfTokenDictStart,
                     PdfTokenDictEnd,
                     PdfTokenArrayStart,
                     PdfTokenArrayEnd,
                     PdfTokenString,
                     PdfTokenStringLiteral,
                     PdfTokenObjName,
                     PdfTokenObjRef,
                     PdfTokenEndObj,
                     PdfTokenHexString,
                     PdfTokenInteger,
                     PdfTokenReal,
                     PdfTokenBoolean,
                     PdfTokenError,
                     PdfStreamData,
                     PdfTokenFinished>
    PdfToken;

class PdfLexer {
public:
    explicit PdfLexer(const char *t) : text(t), offset(0) {}
    explicit PdfLexer(std::string_view t) : text(t), offset(0) {}

    int lex_string(const char *t);
    PdfToken next();

private:
    std::string text;
    size_t offset;
};

struct PdfNodeArray {
    size_t i;
};
struct PdfNodeDict {
    size_t i;
};
struct PdfNodeObjRef {
    int64_t obj;
    int64_t version;
};
struct PdfNodeString {
    std::string value;
};
struct PdfNodeStringLiteral {
    std::string value;
}; // Without leading slash.

struct PdfNodeHexString {
    std::string value;
};

typedef std::variant<int64_t,
                     double,
                     bool,
                     PdfNodeArray,
                     PdfNodeDict,
                     PdfNodeObjRef,
                     PdfNodeString,
                     PdfNodeStringLiteral,
                     PdfNodeHexString>
    PdfValueElement;

typedef std::vector<PdfValueElement> PdfArray;
typedef std::unordered_map<std::string, PdfValueElement> PdfDict;

struct PdfObjectDefinition {
    int64_t number = -1;
    int64_t version = -1;
    std::vector<PdfArray> arrays;
    std::vector<PdfDict> dicts;
    std::string stream;
    PdfValueElement root;
};

class PdfParser {
public:
    explicit PdfParser(const char *t) : lex(t) {}
    explicit PdfParser(const std::string_view t) : lex(t) {}

    std::optional<PdfObjectDefinition> parse();

private:
    std::optional<PdfValueElement> parse_value();

    std::optional<size_t> parse_dict();
    std::optional<size_t> parse_array();

    template<typename T> std::optional<T> accept() {
        if(!std::holds_alternative<T>(pending)) {
            return {};
        }
        auto retval = std::move(pending);
        pending = lex.next();
        return std::move(std::get<T>(retval));
    }

    template<typename T> std::optional<T> expect() {
        if(!std::holds_alternative<T>(pending)) {
            return {};
        }
        auto retval = std::move(pending);
        pending = lex.next();
        return std::move(std::get<T>(retval));
    }

    PdfLexer lex;
    PdfToken pending;
    PdfObjectDefinition objdef;
};

class PrettyPrinter {
public:
    explicit PrettyPrinter(PdfObjectDefinition p) : def{p}, app{std::back_inserter(output)} {}

    std::string prettyprint();

private:
    void print_array(const PdfArray &a);

    void print_dict(const PdfDict &d);

    void print_value(const PdfValueElement &e, bool with_indent = true);

    PdfObjectDefinition def;
    std::string indent;
    std::string output;
    std::back_insert_iterator<std::string> app;
};
