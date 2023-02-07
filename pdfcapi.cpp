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
#include <pdfpage.hpp>

PdfOptions *pdf_options_create() { return reinterpret_cast<PdfOptions *>(new PdfGenerationData()); }

void pdf_options_destroy(PdfOptions *opt) { delete reinterpret_cast<PdfGenerationData *>(opt); }

PdfGenerator *pdf_generator_create(const char *filename, PdfOptions *options) {
    return reinterpret_cast<PdfGenerator *>(
        new PdfGen(filename, *reinterpret_cast<PdfGenerationData *>(options)));
}

void pdf_generator_destroy(PdfGenerator *generator) {
    delete reinterpret_cast<PdfGen *>(generator);
}

PdfPage2 *pdf_generator_new_page(PdfGenerator *gen_c) {
    auto *g = reinterpret_cast<PdfGen *>(gen_c);
    return reinterpret_cast<PdfPage2 *>(g->new_page_capihack());
}

void pdf_page_destroy(PdfPage2 *p) { delete reinterpret_cast<PdfPage *>(p); }
