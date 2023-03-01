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
    void finalize();

    PdfDrawContext() = delete;
    PdfDrawContext(const PdfDrawContext &) = delete;

    GstatePopper push_gstate();

    // All methods that begin with cmd_ map directly to the PDF primitive with the same name.

    // They are in the same order as in Annex A of the PDF spec.
    void cmd_B();
    void cmd_Bstar();
    void cmd_c(double x1, double y1, double x2, double y2, double x3, double y3);
    void cmd_cm(double m1, double m2, double m3, double m4, double m5, double m6);
    void cmd_CS(std::string_view cspace_name);
    void cmd_cs(std::string_view cspace_name);
    ErrorCode cmd_f();
    void cmd_G(double gray);
    void cmd_g(double gray);
    void cmd_gs(GstateId id);
    void cmd_h();
    ErrorCode cmd_j(A4PDF_Line_Join join_style);
    ErrorCode cmd_J(A4PDF_Line_Cap cap_style);
    void cmd_K(double c, double m, double y, double k);
    void cmd_k(double c, double m, double y, double k);
    void cmd_l(double x, double y);
    void cmd_m(double x, double y);
    void cmd_n();
    void cmd_q(); // Save
    void cmd_Q(); // Restore
    ErrorCode cmd_re(double x, double y, double w, double h);
    void cmd_RG(double r, double g, double b);
    void cmd_rg(double r, double g, double b);
    void cmd_s();
    void cmd_S();
    void cmd_SCN(double value);
    void cmd_scn(double value);
    void cmd_sh(ShadingId shid);
    void cmd_Tr(A4PDF_Text_Rendering_Mode mode);
    void cmd_W();
    void cmd_Wstar();
    ErrorCode cmd_w(double w);

    void set_stroke_color(const DeviceRGBColor &c);
    void set_nonstroke_color(const DeviceRGBColor &c);
    void set_stroke_color(LabId lid, const LabColor &c);
    void set_stroke_color(IccColorId icc_id, const double *values, int32_t num_values);
    void set_nonstroke_color(LabId lid, const LabColor &c);
    void set_nonstroke_color(const DeviceGrayColor &c);
    void set_nonstroke_color(PatternId id);
    void set_nonstroke_color(IccColorId icc_id, const double *values, int32_t num_values);
    void set_separation_stroke_color(SeparationId id, LimitDouble value);
    void set_separation_nonstroke_color(SeparationId id, LimitDouble value);
    void set_all_stroke_color();
    void draw_image(ImageId obj_num);
    void scale(double xscale, double yscale);
    void translate(double xtran, double ytran);
    void rotate(double angle);
    void
    render_utf8_text(std::string_view text, A4PDF_FontId fid, double pointsize, double x, double y);
    void render_raw_glyph(uint32_t glyph, A4PDF_FontId fid, double pointsize, double x, double y);
    void render_glyphs(const std::vector<PdfGlyph> &glyphs, A4PDF_FontId fid, double pointsize);
    void render_ascii_text_builtin(
        const char *ascii_text, A4PDF_Builtin_Fonts font_id, double pointsize, double x, double y);

    void draw_unit_circle();
    void draw_unit_box();

    void clear();

    A4PDF_Draw_Context_Type draw_context_type() const { return context_type; }
    PdfDocument &get_doc() { return *doc; }

    std::string build_resource_dict();
    std::string_view get_command_stream() { return commands; }

private:
    void setup_initial_cs();

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
