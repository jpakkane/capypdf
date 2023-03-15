/*
 * Copyright 2022 Jussi Pakkanen
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

#include <pdfcommon.hpp>
#include <pdftext.hpp>
#include <errors.hpp>
#include <pdfcolorconverter.hpp>
#include <pdfdocument.hpp>
#include <string>
#include <unordered_set>
#include <vector>
#include <optional>
#include <span>

template<> struct std::hash<A4PDF::FontSubset> {
    size_t operator()(A4PDF::FontSubset const &s) const noexcept {
        const size_t x = (size_t)s.fid.id;
        const size_t y = s.subset_id;
        return (x << 32) + y;
    }
};

namespace A4PDF {

class PdfDrawContext;

// Scope based q/Q pairing.
struct GstatePopper {
    PdfDrawContext *ctx;
    explicit GstatePopper(PdfDrawContext *ctx) : ctx(ctx) {}

    GstatePopper() = delete;
    GstatePopper(const GstatePopper &) = delete;

    ~GstatePopper();
};

struct SerializedContext {
    std::string dict;
    std::string commands;
};

struct PdfGlyph {
    uint32_t codepoint;
    double x, y;
};

class PdfDrawContext {

public:
    PdfDrawContext(PdfDocument *g, PdfColorConverter *cm, A4PDF_Draw_Context_Type dtype);
    ~PdfDrawContext();
    SerializedContext serialize();

    PdfDrawContext() = delete;
    PdfDrawContext(const PdfDrawContext &) = delete;

    GstatePopper push_gstate();

    // All methods that begin with cmd_ map directly to the PDF primitive with the same name.

    // They are in the same order as in Annex A of the PDF spec.
    ErrorCode cmd_b();
    ErrorCode cmd_B();
    ErrorCode cmd_bstar();
    ErrorCode cmd_Bstar();
    ErrorCode cmd_c(double x1, double y1, double x2, double y2, double x3, double y3);
    ErrorCode cmd_cm(double m1, double m2, double m3, double m4, double m5, double m6);
    ErrorCode cmd_CS(std::string_view cspace_name);
    ErrorCode cmd_cs(std::string_view cspace_name);
    ErrorCode cmd_d(double *dash_array, size_t dash_array_length, double phase);
    ErrorCode cmd_f();
    // ErrorCode cmd_F(); PDF spec says this is obsolete.
    ErrorCode cmd_fstar();
    ErrorCode cmd_G(double gray);
    ErrorCode cmd_g(double gray);
    ErrorCode cmd_gs(GstateId id);
    ErrorCode cmd_h();
    ErrorCode cmd_i(double flatness);
    ErrorCode cmd_j(A4PDF_Line_Join join_style);
    ErrorCode cmd_J(A4PDF_Line_Cap cap_style);
    ErrorCode cmd_K(double c, double m, double y, double k);
    ErrorCode cmd_k(double c, double m, double y, double k);
    ErrorCode cmd_l(double x, double y);
    ErrorCode cmd_m(double x, double y);
    ErrorCode cmd_M(double miterlimit);
    ErrorCode cmd_n();
    ErrorCode cmd_q(); // Save
    ErrorCode cmd_Q(); // Restore
    ErrorCode cmd_re(double x, double y, double w, double h);
    ErrorCode cmd_RG(double r, double g, double b);
    ErrorCode cmd_rg(double r, double g, double b);
    ErrorCode cmd_ri(A4PDF_Rendering_Intent ri);
    ErrorCode cmd_s();
    ErrorCode cmd_S();
    ErrorCode cmd_SCN(double value);
    ErrorCode cmd_scn(double value);
    ErrorCode cmd_sh(ShadingId shid);
    ErrorCode cmd_Tr(A4PDF_Text_Rendering_Mode mode);
    ErrorCode cmd_v(double x2, double y2, double x3, double y3);
    ErrorCode cmd_w(double w);
    ErrorCode cmd_W();
    ErrorCode cmd_Wstar();
    ErrorCode cmd_y(double x1, double y1, double x3, double y3);

    void set_stroke_color(const DeviceRGBColor &c);
    void set_nonstroke_color(const DeviceRGBColor &c);
    ErrorCode set_stroke_color(LabId lid, const LabColor &c);
    ErrorCode set_stroke_color(IccColorId icc_id, const double *values, int32_t num_values);
    ErrorCode set_nonstroke_color(LabId lid, const LabColor &c);
    void set_nonstroke_color(const DeviceGrayColor &c);
    void set_nonstroke_color(PatternId id);
    ErrorCode set_nonstroke_color(IccColorId icc_id, const double *values, int32_t num_values);
    ErrorCode set_separation_stroke_color(SeparationId id, LimitDouble value);
    ErrorCode set_separation_nonstroke_color(SeparationId id, LimitDouble value);
    void set_all_stroke_color();
    ErrorCode draw_image(A4PDF_ImageId obj_num);
    void scale(double xscale, double yscale);
    void translate(double xtran, double ytran);
    void rotate(double angle);
    ErrorCode
    render_utf8_text(std::string_view text, A4PDF_FontId fid, double pointsize, double x, double y);
    ErrorCode render_text(const PdfText &textobj);
    void render_raw_glyph(uint32_t glyph, A4PDF_FontId fid, double pointsize, double x, double y);
    ErrorCode
    render_glyphs(const std::vector<PdfGlyph> &glyphs, A4PDF_FontId fid, double pointsize);
    void render_pdfdoc_text_builtin(const char *pdfdoc_encoded_text,
                                    A4PDF_Builtin_Fonts font_id,
                                    double pointsize,
                                    double x,
                                    double y);

    void draw_unit_circle();
    void draw_unit_box();

    void clear();

    A4PDF_Draw_Context_Type draw_context_type() const { return context_type; }
    PdfDocument &get_doc() { return *doc; }

    std::string build_resource_dict();
    std::string_view get_command_stream() { return commands; }

private:
    PdfDocument *doc;
    PdfColorConverter *cm;
    A4PDF_Draw_Context_Type context_type;
    std::string commands;
    std::back_insert_iterator<std::string> cmd_appender;
    std::unordered_set<int32_t> used_images;
    std::unordered_set<FontSubset> used_subset_fonts;
    std::unordered_set<int32_t> used_fonts;
    std::unordered_set<int32_t> used_colorspaces;
    std::unordered_set<int32_t> used_gstates;
    std::unordered_set<int32_t> used_shadings;
    std::unordered_set<int32_t> used_patterns;
    bool is_finalized = false;
    bool uses_all_colorspace = false;
};

struct ColorPatternBuilder {
    PdfDrawContext pctx;
    double w, h;
};

} // namespace A4PDF
