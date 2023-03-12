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

#include <a4pdf.h>
#include <stdio.h>

int main() {
    A4PDF_EC rc;
    A4PDF_Generator *gen;
    A4PDF_Options *opt;

    if((rc = a4pdf_options_new(&opt)) != 0) {
        fprintf(stderr, "%s\n", a4pdf_error_message(rc));
        return 1;
    }

    if((rc = a4pdf_generator_new("dummy.pdf", opt, &gen)) != 0) {
        fprintf(stderr, "%s\n", a4pdf_error_message(rc));
        return 1;
    }

    if((rc = a4pdf_options_destroy(opt)) != 0) {
        fprintf(stderr, "%s\n", a4pdf_error_message(rc));
        return 1;
    }

    if((rc = a4pdf_generator_destroy(gen)) != 0) {
        fprintf(stderr, "%s\n", a4pdf_error_message(rc));
        return 1;
    }
    return 0;
}
