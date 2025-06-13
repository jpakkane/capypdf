// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#pragma once

#include <document.hpp>

namespace capypdf::internal {

class PdfWriter {
public:
    explicit PdfWriter(PdfDocument &doc);
    rvoe<NoReturnValue> write_to_file(const pystd2025::Path &ofilename);

private:
    rvoe<NoReturnValue> write_to_file(FILE *output_file);
    rvoe<NoReturnValue> write_to_file_impl();

    rvoe<NoReturnValue> write_bytes(const char *buf,
                                    size_t buf_size = (size_t)-1); // With error checking.
    rvoe<NoReturnValue> write_bytes(pystd2025::CStringView view) {
        return write_bytes(view.data(), view.size());
    }
    rvoe<NoReturnValue> write_bytes(const pystd2025::CString &str) {
        return write_bytes(str.data(), str.size());
    }

    rvoe<std::vector<uint64_t>> write_objects();

    rvoe<NoReturnValue> write_header();
    rvoe<NoReturnValue> write_cross_reference_table(const std::vector<uint64_t> &object_offsets);
    rvoe<NoReturnValue> write_trailer(int64_t xref_offset);
    rvoe<NoReturnValue> write_finished_object(int32_t object_number,
                                              pystd2025::CStringView dict_data,
                                              pystd2025::BytesView stream_data);
    rvoe<NoReturnValue> write_subset_font_data(int32_t object_num,
                                               const DelayedSubsetFontData &ssfont);
    rvoe<NoReturnValue> write_subset_font_descriptor(int32_t object_num,
                                                     const TtfFont &font,
                                                     int32_t font_data_obj,
                                                     int32_t subset_number);
    rvoe<NoReturnValue> write_subset_cmap(int32_t object_num, const FontThingy &font);
    rvoe<NoReturnValue>
    write_subset_font(int32_t object_num, const FontThingy &font, int32_t tounicode_obj);
    rvoe<NoReturnValue>
    write_cid_dict(int32_t object_num, CapyPDF_FontId fid, int32_t font_descriptor_obj);
    rvoe<NoReturnValue> write_pages_root();
    rvoe<NoReturnValue> write_delayed_page(const DelayedPage &p);
    rvoe<NoReturnValue> write_checkbox_widget(int obj_num,
                                              const DelayedCheckboxWidgetAnnotation &checkbox);
    rvoe<NoReturnValue> write_annotation(int obj_num, const DelayedAnnotation &annotation);
    rvoe<NoReturnValue> write_delayed_structure_item(int obj_num, const DelayedStructItem &p);

    bool add_info_key_to_trailer() const;

    PdfDocument &doc;
    // This is for convenience so that the file argument does not need to be passed around all the
    // time.
    FILE *ofile = nullptr;
};

} // namespace capypdf::internal
