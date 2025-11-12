// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#pragma once

import std;

#include <document.hpp>

namespace capypdf::internal {

struct ObjectOffset {
    bool store_compressed = false;
    uint64_t offset; // Either in file or in the compressed object stream
};

class PdfWriter {
public:
    explicit PdfWriter(PdfDocument &doc);
    rvoe<NoReturnValue> write_to_file(const char *ofilename);

private:
    rvoe<NoReturnValue> write_to_file(FILE *output_file);
    rvoe<NoReturnValue> write_to_file_impl();

    rvoe<NoReturnValue> write_bytes(const char *buf,
                                    size_t buf_size); // With error checking.
    rvoe<NoReturnValue> write_bytes(std::string_view view) {
        return write_bytes(view.data(), view.size());
    }
    rvoe<NoReturnValue> write_bytes(std::vector<std::byte> &bytes) {
        return write_bytes((char *)bytes.data(), bytes.size());
    }

    rvoe<std::vector<ObjectOffset>> write_objects();

    rvoe<NoReturnValue> write_header();
    rvoe<NoReturnValue> write_cross_reference_table(const std::vector<ObjectOffset> &final_offsets);
    rvoe<NoReturnValue> write_main_objstm(const std::vector<ObjectOffset> &final_offsets);
    rvoe<NoReturnValue> write_cross_reference_stream(const std::vector<ObjectOffset> &final_offsets,
                                                     uint64_t objstm_offset);
    rvoe<NoReturnValue> write_oldstyle_trailer(int64_t xref_offset);
    rvoe<NoReturnValue> write_newstyle_trailer(int64_t xref_offset);
    rvoe<NoReturnValue> write_finished_object(int32_t object_number,
                                              std::string_view dict_data,
                                              std::span<std::byte> stream_data);
    rvoe<NoReturnValue> write_finished_object_to_objstm(int32_t object_number,
                                                        std::string_view dict_data);
    rvoe<NoReturnValue> write_subset_font_data(int32_t object_num,
                                               const DelayedSubsetFontData &ssfont);
    rvoe<NoReturnValue> write_subset_font_descriptor(int32_t object_num,
                                                     const TtfFont &font,
                                                     int32_t font_data_obj,
                                                     int32_t subset_number);
    rvoe<NoReturnValue> write_subset_cmap(int32_t object_num, const FontThingy &font);
    rvoe<NoReturnValue> write_subset_font(int32_t object_num,
                                          const FontThingy &font,
                                          int32_t tounicode_obj,
                                          int32_t subset_id);
    rvoe<NoReturnValue> write_cid_dict(int32_t object_num,
                                       CapyPDF_FontId fid,
                                       int32_t font_descriptor_obj,
                                       int32_t subset_id);
    rvoe<NoReturnValue> write_pages_root();
    rvoe<NoReturnValue> write_delayed_page(const DelayedPage &p);
    rvoe<NoReturnValue> write_checkbox_widget(int obj_num,
                                              const DelayedCheckboxWidgetAnnotation &checkbox);
    rvoe<NoReturnValue> write_annotation(int obj_num, const DelayedAnnotation &annotation);
    rvoe<NoReturnValue> write_delayed_structure_item(int obj_num, const DelayedStructItem &p);

    bool add_info_key_to_trailer() const;

    PdfDocument &doc;
    bool use_xref = true;
    uint32_t compressed_object_number = (uint32_t)-1;
    std::string objstm_stream;
    std::vector<ObjectOffset> object_offsets;

    // This is for convenience so that the file argument does not need to be passed around all the
    // time.
    FILE *ofile = nullptr;
};

} // namespace capypdf::internal
