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
                     PdfTokenError,
                     PdfTokenFinished>
    PdfToken;

class PdfLexer {
public:
    explicit PdfLexer(const char *t) : text(t), offset(0) {}

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
    PdfValueElement root;
};

class PdfParser {
public:
    explicit PdfParser(const char *t) : lex(t) {}

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
