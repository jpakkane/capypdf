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

#include <pdfparser.hpp>
#include <strings.h>
#include <regex>
#include <string_view>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <variant>
#include <algorithm>

#include <fmt/core.h>

namespace {

const std::regex whitespace{R"(^\s)"};
const std::regex dictstart{R"(^<<)"};
const std::regex dictend{R"(^>>)"};
const std::regex arraystart{R"(^\[)"};
const std::regex arrayend{R"(^\])"};
const std::regex objname{R"(^(\d+)\s+(\d+)\s+obj)"};
const std::regex objref{R"((^\d+)\s+(\d+)\s+R)"};
const std::regex stringlit{R"(^/([a-zA-Z][a-zA-Z0-9]+))"};
const std::regex stringobj{R"(^\()"};
const std::regex endobj{R"(^endobj)"};
const std::regex number{R"(^-?\d+)"};
const std::regex real{R"(^(-?\d+\.\d+))"};
const std::regex hexstr{R"(^<([0-9a-fA-F]+)>)"};

} // namespace

const char *text =
    R"(3 0 obj << /key /value /otherkey [ 1 0 R ] /StringKey (a \(b(c)) /intkey 55 /realkey 9.34 /hexkey <03830A0b> >> endobj)";

int PdfLexer::lex_string(const char *t) {
    bool prev_was_backslash = false;
    int num_parens = 1;
    size_t myoff = 0;
    while(t[myoff] != '\0') {
        switch(t[myoff]) {

        case '\\':
            prev_was_backslash = true;
            break;

        case '(':
            if(prev_was_backslash) {

            } else {
                ++num_parens;
            }
            prev_was_backslash = false;
            break;

        case ')':
            if(prev_was_backslash) {

            } else {
                if(--num_parens == 0) {
                    return (int64_t)myoff + 1;
                }
            }
            prev_was_backslash = false;
            break;

        case '\0':
            return -1;

        default:
            prev_was_backslash = false;
            break;
        }
        ++myoff;
    }
    return -1;
}

PdfToken PdfLexer::next() {
    std::cmatch m;
    while(offset < text.size()) {
        if(std::regex_search(text.c_str() + offset, m, whitespace)) {
            offset += m.length();
            continue;
        }

        if(std::regex_search(text.c_str() + offset, m, objname)) {
            int64_t number = atoll(text.c_str() + offset + m.position(1));
            int64_t version = atoll(text.c_str() + offset + m.position(2));
            offset += m.length();
            return PdfTokenObjName(number, version);
        } else if(std::regex_search(text.c_str() + offset, m, dictstart)) {
            offset += m.length();
            return PdfTokenDictStart();
        } else if(std::regex_search(text.c_str() + offset, m, dictend)) {
            offset += m.length();
            return PdfTokenDictEnd();
        } else if(std::regex_search(text.c_str() + offset, m, arraystart)) {
            offset += m.length();
            return PdfTokenArrayStart{};
        } else if(std::regex_search(text.c_str() + offset, m, arrayend)) {
            offset += m.length();
            return PdfTokenArrayEnd{};
        } else if(std::regex_search(text.c_str() + offset, m, stringlit)) {
            std::string name(text.c_str() + offset + 1, m.length(1));
            offset += m.length();
            return PdfTokenStringLiteral{std::move(name)};
        } else if(std::regex_search(text.c_str() + offset, m, objref)) {
            int64_t onum = atoll(text.c_str() + offset);
            int64_t version = atoll(text.c_str() + offset + m.position(2));
            offset += m.length();
            return PdfTokenObjRef(onum, version);
        } else if(std::regex_search(text.c_str() + offset, m, stringobj)) {
            ++offset;
            auto advance = lex_string(text.c_str() + offset);
            if(advance < 0) {
                // std::cout << "\nParent string parsing failed.\n";
                return PdfTokenError{};
            }
            std::string temptext(text.c_str() + offset, advance);
            offset += advance;
            return PdfTokenString(std::move(temptext));
        } else if(std::regex_search(text.c_str() + offset, m, real)) {
            double value = strtod(text.c_str() + offset, nullptr);
            offset += m.length();
            return PdfTokenReal{value};
        } else if(std::regex_search(text.c_str() + offset, m, number)) {
            int64_t value = atoll(text.c_str() + offset);
            offset += m.length();
            return PdfTokenInteger{value};
        } else if(std::regex_search(text.c_str() + offset, m, endobj)) {
            offset += m.length();
            return PdfTokenEndObj{};
        } else if(std::regex_search(text.c_str() + offset, m, hexstr)) {
            std::string hexs(text.c_str() + offset + m.position(1), m.length(1));
            offset += m.length();
            return PdfTokenHexString{std::move(hexs)};
        } else {
            return PdfTokenError{};
        }
    }
    return PdfTokenFinished{};
}

std::optional<PdfObjectDefinition> PdfParser::parse() {
    pending = lex.next();
    auto root = expect<PdfTokenObjName>();
    if(!root) {
        return {};
    }
    objdef.number = root->number;
    objdef.version = root->version;
    auto robj = parse_value();
    if(!robj) {
        return {};
    }
    accept<PdfTokenEndObj>();
    if(!std::holds_alternative<PdfTokenFinished>(pending)) {
        return {};
    }
    objdef.root = std::move(*robj);
    return std::move(objdef);
}

std::optional<PdfValueElement> PdfParser::parse_value() {
    if(auto intval = accept<PdfTokenInteger>(); intval) {
        return intval->value;
    }
    if(auto realval = accept<PdfTokenReal>(); realval) {
        return realval->value;
    }
    if(auto refval = accept<PdfTokenObjRef>(); refval) {
        return PdfNodeObjRef{refval->objnum, refval->version};
    }
    if(auto strval = accept<PdfTokenString>(); strval) {
        return PdfNodeString{strval->text};
    }
    if(auto strval = accept<PdfTokenStringLiteral>(); strval) {
        return PdfNodeStringLiteral{strval->text};
    }
    if(auto strval = accept<PdfTokenHexString>(); strval) {
        return PdfNodeHexString{strval->text};
    }
    if(auto dictval = accept<PdfTokenDictStart>(); dictval) {
        auto dict_id = parse_dict();
        if(!dict_id) {
            return {};
        }
        return PdfNodeDict(*dict_id);
    }
    if(auto arrval = accept<PdfTokenArrayStart>(); arrval) {
        auto array_id = parse_array();
        if(!array_id) {
            return {};
        }
        return PdfNodeArray(*array_id);
    }
    if(accept<PdfTokenFinished>()) {
        return {};
    }
    return {};
}

std::optional<size_t> PdfParser::parse_dict() {
    PdfDict dict;
    while(true) {
        if(auto the_end = accept<PdfTokenDictEnd>(); the_end) {
            objdef.dicts.emplace_back(std::move(dict));
            return objdef.dicts.size() - 1;
        }
        auto k = expect<PdfTokenStringLiteral>();
        if(!k) {
            return {};
        }
        auto v = parse_value();
        if(!v) {
            return {};
        }
        dict[k->text] = std::move(*v);
    }
}

std::optional<size_t> PdfParser::parse_array() {
    PdfArray arr;
    while(true) {
        if(auto the_end = accept<PdfTokenArrayEnd>(); the_end) {
            objdef.arrays.emplace_back(std::move(arr));
            return objdef.arrays.size() - 1;
        }
        auto v = parse_value();
        if(!v) {
            return {};
        }
        arr.emplace_back(std::move(*v));
    }
}

class PrettyPrinter {
public:
    explicit PrettyPrinter(PdfObjectDefinition p) : def{p}, app{std::back_inserter(output)} {}

    std::string prettyprint() {
        fmt::format_to(app, "{}obj {} {}\n", indent, def.number, def.version);
        print_value(def.root);
        return std::move(output);
    }

private:
    void print_array(const PdfArray &a) {
        for(const auto &i : a) {
            print_value(i);
        }
    }

    void print_dict(const PdfDict &d) {
        std::vector<std::string> keys;
        for(const auto &[key, value] : d) {
            keys.push_back(key);
        }

        std::sort(keys.begin(), keys.end(), [](const std::string &s1, const std::string &s2) {
            return strcasecmp(s1.c_str(), s2.c_str()) < 0;
        });
        for(const auto &key : keys) {
            fmt::format_to(app, "{}/{} ", indent, key);
            print_value(d.at(key), false);
        }
    }

    void print_value(const PdfValueElement &e, bool with_indent = true) {
        const char *ind = with_indent ? indent.c_str() : "";
        if(std::holds_alternative<int64_t>(e)) {
            const auto &v = std::get<int64_t>(e);
            fmt::format_to(app, "{}{}\n", ind, v);
        } else if(std::holds_alternative<double>(e)) {
            const auto &v = std::get<double>(e);
            fmt::format_to(app, "{}{}\n", ind, v);
        } else if(std::holds_alternative<PdfNodeArray>(e)) {
            const auto &v = std::get<PdfNodeArray>(e);
            fmt::format_to(app, "{}[\n", ind);
            indent += "  ";
            print_array(def.arrays[v.i]);
            indent.pop_back();
            indent.pop_back();
            fmt::format_to(app, "{}]\n", indent);
        } else if(std::holds_alternative<PdfNodeDict>(e)) {
            const auto &v = std::get<PdfNodeDict>(e);
            fmt::format_to(app, "{}<<\n", ind);
            indent += "  ";
            print_dict(def.dicts[v.i]);
            indent.pop_back();
            indent.pop_back();
            fmt::format_to(app, "{}>>\n", indent);
        } else if(std::holds_alternative<PdfNodeObjRef>(e)) {
            const auto &v = std::get<PdfNodeObjRef>(e);
            fmt::format_to(app, "{}{} {} R\n", ind, v.obj, v.version);
        } else if(std::holds_alternative<PdfNodeString>(e)) {
            const auto &v = std::get<PdfNodeString>(e);
            fmt::format_to(app, "{}({})\n", ind, v.value);
        } else if(std::holds_alternative<PdfNodeStringLiteral>(e)) {
            const auto &v = std::get<PdfNodeStringLiteral>(e);
            fmt::format_to(app, "{}/{}\n", ind, v.value);
        } else if(std::holds_alternative<PdfNodeHexString>(e)) {
            const auto &v = std::get<PdfNodeHexString>(e);
            fmt::format_to(app, "{}<{}>\n", ind, v.value);
        }
    }

    PdfObjectDefinition def;
    std::string indent;
    std::string output;
    std::back_insert_iterator<std::string> app;
};

int main() {
    // PdfLexer plex(text);
    // PdfToken t = plex.next();
    PdfParser p(text);
    auto result = p.parse();
    if(result) {
        PrettyPrinter pp(*result);
        auto output = pp.prettyprint();
        printf("%s", output.c_str());
    } else {
        fprintf(stderr, "Parse fail.\n");
    }
    return 0;
}
