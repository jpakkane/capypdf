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

#include <pdfcapi.h>
#include <pdfgen.hpp>
#include <pdfpagebuilder.hpp>

PdfOptions *pdf_options_create() { return reinterpret_cast<PdfOptions *>(new PdfGenerationData()); }

void pdf_options_destroy(PdfOptions *opt) { delete reinterpret_cast<PdfGenerationData *>(opt); }

int32_t pdf_options_set_title(PdfOptions *opt, const char *utf8_title) {
    reinterpret_cast<PdfGenerationData *>(opt)->title = utf8_title;
    return 0;
}

PdfGenerator *pdf_generator_create(const char *filename, const PdfOptions *options) {
    auto opts = reinterpret_cast<const PdfGenerationData *>(options);
    return reinterpret_cast<PdfGenerator *>(new PdfGen(filename, *opts));
}

void pdf_generator_destroy(PdfGenerator *generator) {
    delete reinterpret_cast<PdfGen *>(generator);
}

void pdf_generator_new_page(PdfGenerator *gen_c) { reinterpret_cast<PdfGen *>(gen_c)->new_page(); }

const char *pdf_error_message(int32_t error_code) {
    if(error_code == 0)
        return "No error";
    return "Error messages not implemented yet";
}
