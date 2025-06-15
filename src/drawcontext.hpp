// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#pragma once

#include <pdfcommon.hpp>
#include <pdftext.hpp>
#include <errorhandling.hpp>
#include <colorconverter.hpp>
#include <document.hpp>
#include <commandstreamformatter.hpp>
#include <vector>

template<typename Hasher> struct pystd2025::HashFeeder<Hasher, capypdf::internal::FontSubset> {
    void operator()(Hasher &h, const capypdf::internal::FontSubset &sset) noexcept {
        h.feed_hash(sset.fid);
        h.feed_hash(sset.subset_id);
    }
};

namespace capypdf::internal {

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
    pystd2025::CString resource_dict;
    pystd2025::CString command_stream;
};

struct SerializedXObject {
    ObjectFormatter dict;
    pystd2025::CString command_stream;
};

typedef pystd2025::Variant<SerializedBasicContext, SerializedXObject> DCSerialization;

struct PdfGlyph {
    uint32_t codepoint;
    double x, y;
};

class PdfDrawContext {

public:
    PdfDrawContext(PdfDocument *g,
                   PdfColorConverter *cm,
                   CapyPDF_Draw_Context_Type dtype,
                   const PdfRectangle &area);
    ~PdfDrawContext();
    rvoe<DCSerialization> serialize();

    PdfDrawContext() = delete;
    PdfDrawContext(const PdfDrawContext &) = delete;

    GstatePopper push_gstate();

    // All methods that begin with cmd_ map directly to the PDF primitive with the same name.

    // They are in the same order as in Annex A of the PDF spec.
    rvoe<NoReturnValue> cmd_b();
    rvoe<NoReturnValue> cmd_B();
    rvoe<NoReturnValue> cmd_bstar();
    rvoe<NoReturnValue> cmd_Bstar();
    rvoe<NoReturnValue> cmd_BDC(const asciistring &name,
                                pystd2025::Optional<CapyPDF_StructureItemId> sid,
                                const BDCTags *attributes);
    rvoe<NoReturnValue> cmd_BDC(CapyPDF_StructureItemId sid, const BDCTags *attributes);
    rvoe<NoReturnValue> cmd_BDC(CapyPDF_OptionalContentGroupId id);
    rvoe<NoReturnValue> cmd_BMC(pystd2025::CStringView tag);
    rvoe<NoReturnValue> cmd_c(double x1, double y1, double x2, double y2, double x3, double y3);
    rvoe<NoReturnValue> cmd_cm(double m1, double m2, double m3, double m4, double m5, double m6);
    rvoe<NoReturnValue> cmd_CS(pystd2025::CStringView cspace_name);
    rvoe<NoReturnValue> cmd_CS(pystd2025::CString &cspace_name) {
        return cmd_CS(pystd2025::CStringView(cspace_name.data(), cspace_name.size()));
    }
    rvoe<NoReturnValue> cmd_cs(pystd2025::CStringView cspace_name);
    rvoe<NoReturnValue> cmd_cs(pystd2025::CString &cspace_name) {
        return cmd_cs(pystd2025::CStringView(cspace_name.data(), cspace_name.size()));
    }
    rvoe<NoReturnValue> cmd_d(double *dash_array, size_t dash_array_length, double phase);
    rvoe<NoReturnValue> cmd_Do(CapyPDF_FormXObjectId fxoid);
    rvoe<NoReturnValue> cmd_Do(CapyPDF_TransparencyGroupId trid);
    rvoe<NoReturnValue> cmd_Do(CapyPDF_ImageId obj_num);
    rvoe<NoReturnValue> cmd_EMC();
    rvoe<NoReturnValue> cmd_f();
    // rvoe<NoReturnValue> cmd_F(); PDF spec says this is obsolete.
    rvoe<NoReturnValue> cmd_fstar();
    rvoe<NoReturnValue> cmd_G(LimitDouble gray);
    rvoe<NoReturnValue> cmd_g(LimitDouble gray);
    rvoe<NoReturnValue> cmd_gs(CapyPDF_GraphicsStateId id);
    rvoe<NoReturnValue> cmd_h();
    rvoe<NoReturnValue> cmd_i(double flatness);
    rvoe<NoReturnValue> cmd_j(CapyPDF_Line_Join join_style);
    rvoe<NoReturnValue> cmd_J(CapyPDF_Line_Cap cap_style);
    rvoe<NoReturnValue> cmd_K(LimitDouble c, LimitDouble m, LimitDouble y, LimitDouble k);
    rvoe<NoReturnValue> cmd_k(LimitDouble c, LimitDouble m, LimitDouble y, LimitDouble k);
    rvoe<NoReturnValue> cmd_l(double x, double y);
    rvoe<NoReturnValue> cmd_m(double x, double y);
    rvoe<NoReturnValue> cmd_M(double miterlimit);
    rvoe<NoReturnValue> cmd_n();
    rvoe<NoReturnValue> cmd_q(); // Save
    rvoe<NoReturnValue> cmd_Q(); // Restore
    rvoe<NoReturnValue> cmd_re(double x, double y, double w, double h);
    rvoe<NoReturnValue> cmd_RG(LimitDouble r, LimitDouble g, LimitDouble b);
    rvoe<NoReturnValue> cmd_rg(LimitDouble r, LimitDouble g, LimitDouble b);
    rvoe<NoReturnValue> cmd_ri(CapyPDF_Rendering_Intent ri);
    rvoe<NoReturnValue> cmd_s();
    rvoe<NoReturnValue> cmd_S();
    rvoe<NoReturnValue> cmd_SCN(double value);
    rvoe<NoReturnValue> cmd_scn(double value);
    rvoe<NoReturnValue> cmd_sh(CapyPDF_ShadingId shid);
    rvoe<NoReturnValue> cmd_Tr(CapyPDF_Text_Mode mode);
    rvoe<NoReturnValue> cmd_v(double x2, double y2, double x3, double y3);
    rvoe<NoReturnValue> cmd_w(double w);
    rvoe<NoReturnValue> cmd_W();
    rvoe<NoReturnValue> cmd_Wstar();
    rvoe<NoReturnValue> cmd_y(double x1, double y1, double x3, double y3);

    // Command serialization.
    rvoe<NoReturnValue> serialize_G(LimitDouble gray);
    rvoe<NoReturnValue> serialize_g(LimitDouble gray);
    rvoe<NoReturnValue> serialize_K(LimitDouble c, LimitDouble m, LimitDouble y, LimitDouble k);
    rvoe<NoReturnValue> serialize_k(LimitDouble c, LimitDouble m, LimitDouble y, LimitDouble k);
    rvoe<NoReturnValue> serialize_RG(LimitDouble r, LimitDouble g, LimitDouble b);
    rvoe<NoReturnValue> serialize_rg(LimitDouble r, LimitDouble g, LimitDouble b);

    // Color
    rvoe<NoReturnValue> set_stroke_color(const Color &c) { return set_color(c, true); }
    rvoe<NoReturnValue> set_nonstroke_color(const Color &c) { return set_color(c, false); }

    rvoe<NoReturnValue> set_color(const Color &c, bool stroke);
    rvoe<NoReturnValue> set_color(const DeviceRGBColor &c, bool stroke);
    rvoe<NoReturnValue> set_color(const DeviceGrayColor &c, bool stroke);
    rvoe<NoReturnValue> set_color(const DeviceCMYKColor &c, bool stroke);
    rvoe<NoReturnValue> convert_to_output_cs_and_set_color(const DeviceRGBColor &c, bool stroke);
    rvoe<NoReturnValue> convert_to_output_cs_and_set_color(const DeviceGrayColor &c, bool stroke);
    rvoe<NoReturnValue> convert_to_output_cs_and_set_color(const DeviceCMYKColor &c, bool stroke);
    rvoe<NoReturnValue> set_color(const LabColor &c, bool stroke);
    rvoe<NoReturnValue> set_color(const ICCColor &icc, bool stroke);
    rvoe<NoReturnValue> set_color(CapyPDF_PatternId id, bool stroke);
    rvoe<NoReturnValue> set_color(const SeparationColor &color, bool stroke);

    void set_all_stroke_color();
    void scale(double xscale, double yscale);
    void translate(double xtran, double ytran);
    void rotate(double angle);
    rvoe<NoReturnValue>
    render_text(const u8string &text, CapyPDF_FontId fid, double pointsize, double x, double y);
    rvoe<NoReturnValue> render_text(const PdfText &textobj);
    rvoe<NoReturnValue>
    render_glyphs(const pystd2025::Vector<PdfGlyph> &glyphs, CapyPDF_FontId fid, double pointsize);
    rvoe<NoReturnValue> render_pdfdoc_text_builtin(const char *pdfdoc_encoded_text,
                                                   CapyPDF_Builtin_Fonts font_id,
                                                   double pointsize,
                                                   double x,
                                                   double y);

    void draw_unit_circle();
    void draw_unit_box();

    void clear();

    rvoe<NoReturnValue> add_form_widget(CapyPDF_FormWidgetId widget);
    rvoe<NoReturnValue> annotate(CapyPDF_AnnotationId annotation);

    CapyPDF_Draw_Context_Type draw_context_type() const { return context_type; }
    PdfDocument &get_doc() { return *doc; }

    void build_resource_dict(ObjectFormatter &fmt);

    double get_w() const { return bbox.x2 - bbox.x1; }
    double get_h() const { return bbox.y2 - bbox.y1; }

    int32_t marked_content_depth() const { return cmds.marked_content_depth(); }

    const pystd2025::HashSet<CapyPDF_FormWidgetId> &get_form_usage() const { return used_widgets; }
    const pystd2025::HashSet<CapyPDF_AnnotationId> &get_annotation_usage() const {
        return used_annotations;
    }
    const pystd2025::Vector<CapyPDF_StructureItemId> &get_structure_usage() const {
        return used_structures;
    }

    const pystd2025::Optional<Transition> &get_transition() const { return transition; }

    const pystd2025::Vector<SubPageNavigation> &get_subpage_navigation() const {
        return sub_navigations;
    }

    bool has_unclosed_state() const { return cmds.has_unclosed_state(); }

    rvoe<NoReturnValue> set_transition(const Transition &tr);

    rvoe<NoReturnValue>
    add_simple_navigation(pystd2025::Span<const CapyPDF_OptionalContentGroupId> navs,
                          const pystd2025::Optional<Transition> &tr);

    const PageProperties &get_custom_props() const { return custom_props; }

    rvoe<NoReturnValue> set_custom_page_properties(const PageProperties &new_props);

    rvoe<NoReturnValue> set_transparency_properties(const TransparencyGroupProperties &props);

    rvoe<NoReturnValue> set_group_matrix(const PdfMatrix &mat);

private:
    rvoe<NoReturnValue> serialize_charsequence(const TextEvents &charseq,
                                               CommandStreamFormatter &serialisation,
                                               CapyPDF_FontId &current_font);

    rvoe<int32_t> add_bcd_structure(CapyPDF_StructureItemId sid);

    rvoe<NoReturnValue> validate_text_contents(const PdfText &text);

    PdfDocument *doc;
    PdfColorConverter *cm;
    CapyPDF_Draw_Context_Type context_type;
    pystd2025::HashSet<int32_t> used_images;
    pystd2025::HashSet<FontSubset> used_subset_fonts;
    pystd2025::HashSet<int32_t> used_fonts;
    pystd2025::HashSet<int32_t> used_colorspaces;
    pystd2025::HashSet<int32_t> used_gstates;
    pystd2025::HashSet<int32_t> used_shadings;
    pystd2025::HashSet<int32_t> used_patterns;
    pystd2025::HashSet<int32_t> used_form_xobjects;
    pystd2025::HashSet<CapyPDF_FormWidgetId> used_widgets;
    pystd2025::HashSet<CapyPDF_AnnotationId> used_annotations;
    pystd2025::Vector<CapyPDF_StructureItemId>
        used_structures; // A vector because numbering is relevant.
    pystd2025::HashSet<CapyPDF_OptionalContentGroupId> used_ocgs;
    pystd2025::HashSet<CapyPDF_TransparencyGroupId> used_trgroups;
    pystd2025::Vector<SubPageNavigation> sub_navigations;

    // Not a std::stack because we need to access all entries.
    pystd2025::Optional<Transition> transition;

    PageProperties custom_props;
    // Reminder: If you add stuff here, also add them to .clear().
    bool is_finalized = false;
    bool uses_all_colorspace = false;
    PdfRectangle bbox;
    pystd2025::Optional<PdfMatrix> group_matrix;
    CommandStreamFormatter cmds;
};

} // namespace capypdf::internal
