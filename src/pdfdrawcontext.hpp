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
#include <errorhandling.hpp>
#include <pdfcolorconverter.hpp>
#include <pdfdocument.hpp>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
#include <optional>
#include <span>
#include <stack>

template<> struct std::hash<capypdf::FontSubset> {
    size_t operator()(capypdf::FontSubset const &s) const noexcept {
        const size_t x = (size_t)s.fid.id;
        const size_t y = s.subset_id;
        return (x << 32) + y;
    }
};

namespace capypdf {

class PdfDrawContext;

// Scope based q/Q pairing.
struct GstatePopper {
    PdfDrawContext *ctx;
    explicit GstatePopper(PdfDrawContext *ctx) : ctx(ctx) {}

    GstatePopper() = delete;
    GstatePopper(const GstatePopper &) = delete;

    ~GstatePopper();
};

struct SerializedBasicContext {
    std::string dict;
    std::string commands;
};

struct SerializedXObject {
    std::string dict;
    std::string stream;
};

typedef std::variant<SerializedBasicContext, SerializedXObject> DCSerialization;

struct PdfGlyph {
    uint32_t codepoint;
    double x, y;
};

enum class DrawStateType {
    MarkedContent,
    SaveState,
    Text,
};

class PdfDrawContext {

public:
    PdfDrawContext(PdfDocument *g,
                   PdfColorConverter *cm,
                   CAPYPDF_Draw_Context_Type dtype,
                   double w = -1,
                   double h = -1);
    ~PdfDrawContext();
    DCSerialization serialize(const TransparencyGroupExtra *trinfo = nullptr);

    PdfDrawContext() = delete;
    PdfDrawContext(const PdfDrawContext &) = delete;

    GstatePopper push_gstate();

    // All methods that begin with cmd_ map directly to the PDF primitive with the same name.

    // They are in the same order as in Annex A of the PDF spec.
    ErrorCode cmd_b();
    ErrorCode cmd_B();
    ErrorCode cmd_bstar();
    ErrorCode cmd_Bstar();
    ErrorCode cmd_BDC(CapyPDF_StructureItemId sid);
    ErrorCode cmd_BDC(CapyPDF_OptionalContentGroupId id);
    ErrorCode cmd_BMC(std::string_view tag);
    ErrorCode cmd_c(double x1, double y1, double x2, double y2, double x3, double y3);
    ErrorCode cmd_cm(double m1, double m2, double m3, double m4, double m5, double m6);
    ErrorCode cmd_CS(std::string_view cspace_name);
    ErrorCode cmd_cs(std::string_view cspace_name);
    ErrorCode cmd_d(double *dash_array, size_t dash_array_length, double phase);
    ErrorCode cmd_Do(CapyPDF_FormXObjectId fxoid);
    ErrorCode cmd_Do(CapyPDF_TransparencyGroupId trid);
    ErrorCode cmd_EMC();
    ErrorCode cmd_f();
    // ErrorCode cmd_F(); PDF spec says this is obsolete.
    ErrorCode cmd_fstar();
    ErrorCode cmd_G(double gray);
    ErrorCode cmd_g(double gray);
    ErrorCode cmd_gs(GstateId id);
    ErrorCode cmd_h();
    ErrorCode cmd_i(double flatness);
    ErrorCode cmd_j(CAPYPDF_Line_Join join_style);
    ErrorCode cmd_J(CAPYPDF_Line_Cap cap_style);
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
    ErrorCode cmd_ri(CapyPDF_Rendering_Intent ri);
    ErrorCode cmd_s();
    ErrorCode cmd_S();
    ErrorCode cmd_SCN(double value);
    ErrorCode cmd_scn(double value);
    ErrorCode cmd_sh(ShadingId shid);
    ErrorCode cmd_Tr(CapyPDF_Text_Mode mode);
    ErrorCode cmd_v(double x2, double y2, double x3, double y3);
    ErrorCode cmd_w(double w);
    ErrorCode cmd_W();
    ErrorCode cmd_Wstar();
    ErrorCode cmd_y(double x1, double y1, double x3, double y3);

    ErrorCode set_stroke_color(const Color &c) { return set_color(c, true); }
    ErrorCode set_nonstroke_color(const Color &c) { return set_color(c, false); }

    ErrorCode set_color(const Color &c, bool stroke);
    ErrorCode set_color(const DeviceRGBColor &c, bool stroke);
    ErrorCode set_color(const DeviceGrayColor &c, bool stroke);
    ErrorCode set_color(const DeviceCMYKColor &c, bool stroke);
    ErrorCode set_color(const LabColor &c, bool stroke);
    ErrorCode set_color(const ICCColor &icc, bool stroke);
    ErrorCode set_color(PatternId id, bool stroke);
    ErrorCode set_color(const SeparationColor &color, bool stroke);

    void set_all_stroke_color();
    ErrorCode draw_image(CapyPDF_ImageId obj_num);
    void scale(double xscale, double yscale);
    void translate(double xtran, double ytran);
    void rotate(double angle);
    ErrorCode render_utf8_text(
        std::string_view text, CapyPDF_FontId fid, double pointsize, double x, double y);
    ErrorCode render_text(const PdfText &textobj);
    void render_raw_glyph(uint32_t glyph, CapyPDF_FontId fid, double pointsize, double x, double y);
    ErrorCode
    render_glyphs(const std::vector<PdfGlyph> &glyphs, CapyPDF_FontId fid, double pointsize);
    ErrorCode render_pdfdoc_text_builtin(const char *pdfdoc_encoded_text,
                                         CapyPDF_Builtin_Fonts font_id,
                                         double pointsize,
                                         double x,
                                         double y);

    void draw_unit_circle();
    void draw_unit_box();

    void clear();

    ErrorCode add_form_widget(CapyPDF_FormWidgetId widget);
    ErrorCode annotate(CapyPDF_AnnotationId annotation);

    CAPYPDF_Draw_Context_Type draw_context_type() const { return context_type; }
    PdfDocument &get_doc() { return *doc; }

    std::string build_resource_dict();
    std::string_view get_command_stream() { return commands; }

    void set_form_xobject_size(double w, double h);
    double get_form_xobj_w() const { return form_xobj_w; }
    double get_form_xobj_h() const { return form_xobj_h; }

    int32_t marked_content_depth() const { return marked_depth; }

    const std::unordered_set<CapyPDF_FormWidgetId> &get_form_usage() const { return used_widgets; }
    const std::unordered_set<CapyPDF_AnnotationId> &get_annotation_usage() const {
        return used_annotations;
    }
    const std::unordered_set<CapyPDF_StructureItemId> &get_structure_usage() const {
        return used_structures;
    }

    const std::optional<Transition> &get_transition() const { return transition; }

    const std::vector<SubPageNavigation> &get_subpage_navigation() const { return sub_navigations; }

    bool has_unclosed_state() const { return !dstates.empty(); }

    rvoe<NoReturnValue> set_transition(const Transition &tr);

    rvoe<NoReturnValue> add_simple_navigation(std::span<const CapyPDF_OptionalContentGroupId> navs,
                                              const std::optional<Transition> &tr);

private:
    rvoe<NoReturnValue> serialize_charsequence(const std::vector<CharItem> &charseq,
                                               std::string &serialisation,
                                               CapyPDF_FontId &current_font,
                                               int32_t &current_subset,
                                               double &current_pointsize);
    ErrorCode utf8_to_kerned_chars(std::string_view utf8_text,
                                   std::vector<CharItem> &charseq,
                                   CapyPDF_FontId fid);

    void indent(DrawStateType dtype) {
        dstates.push(dtype);
        ind += "  ";
    }

    rvoe<NoReturnValue> dedent(DrawStateType dtype) {
        if(dstates.empty()) {
            RETERR(DrawStateEndMismatch);
        }
        if(dstates.top() != dtype) {
            RETERR(DrawStateEndMismatch);
        }
        if(ind.size() < 2) {
            std::abort();
        }
        dstates.pop();
        ind.pop_back();
        ind.pop_back();
        return NoReturnValue{};
    }

    PdfDocument *doc;
    PdfColorConverter *cm;
    CAPYPDF_Draw_Context_Type context_type;
    std::string commands;
    std::back_insert_iterator<std::string> cmd_appender;
    std::unordered_set<int32_t> used_images;
    std::unordered_set<FontSubset> used_subset_fonts;
    std::unordered_set<int32_t> used_fonts;
    std::unordered_set<int32_t> used_colorspaces;
    std::unordered_set<int32_t> used_gstates;
    std::unordered_set<int32_t> used_shadings;
    std::unordered_set<int32_t> used_patterns;
    std::unordered_set<int32_t> used_form_xobjects;
    std::unordered_set<CapyPDF_FormWidgetId> used_widgets;
    std::unordered_set<CapyPDF_AnnotationId> used_annotations;
    std::unordered_set<CapyPDF_StructureItemId> used_structures;
    std::unordered_set<CapyPDF_OptionalContentGroupId> used_ocgs;
    std::unordered_set<CapyPDF_TransparencyGroupId> used_trgroups;
    std::vector<SubPageNavigation> sub_navigations;

    std::stack<DrawStateType> dstates;
    std::optional<Transition> transition;
    // Reminder: If you add stuff  here, also add them to .clear().
    bool is_finalized = false;
    bool uses_all_colorspace = false;
    double form_xobj_w = -1;
    double form_xobj_h = -1;
    int32_t marked_depth = 0;
    std::string ind;
};

struct ColorPatternBuilder {
    PdfDrawContext pctx;
    double w, h;
};

} // namespace capypdf
