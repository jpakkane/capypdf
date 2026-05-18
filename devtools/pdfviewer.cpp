// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <pdfparser.hpp>
#include <gtk/gtk.h>
#include <sys/mman.h>
#include <zlib.h>
#include <cassert>
#include <filesystem>
#include <optional>
#include <vector>

enum { OBJNUM_COLUMN, OFFSET_COLUMN, STREAM_SIZE_COLUMN, TYPE_COLUMN, N_OBJ_COLUMNS };

namespace {

std::string inflate(std::string_view compressed) {
    std::string inflated;
    const int CHUNK = 1024 * 1024;
    int ret;
    unsigned have;
    z_stream strm;
    const unsigned char *current = (const unsigned char *)compressed.data();
    std::unique_ptr<unsigned char[]> out(new unsigned char[CHUNK]);

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if(ret != Z_OK) {
        printf("Could not init zlib.");
        return "";
    }

    /* decompress until deflate stream ends or end of file */
    strm.avail_in = compressed.size();
    strm.next_in = const_cast<unsigned char *>(current); // zlib header is const-broken
    do {
        if(strm.total_in >= compressed.size()) {
            break;
        }

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out.get();
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR); /* state not clobbered */
            switch(ret) {
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                printf("Zlib error: %s\n", strm.msg);
                return "";
            }
            have = CHUNK - strm.avail_out;

            inflated += std::string_view((const char *)out.get(), have);
        } while(strm.avail_out == 0);
        /* done when inflate() says it's done */
    } while(ret != Z_STREAM_END);
    /*
        if(Z_STREAM_END != Z_OK) {
            throw std::runtime_error("Decompression failed.");
        }
    */
    return inflated;
}

const char appname[] = "PDF browser";

const int xref_entry_size = 20;
const int free_generation = 65535;

struct MMapCloser {
    void *addr;
    size_t length;
    ~MMapCloser() { munmap(addr, length); }
};

struct BinaryData {
    std::string dict;
    std::string stream;
};

struct ObjectData {
    int32_t obj_generation;
    std::string dict;
    std::string stream;
};

struct UnpackedXRef {
    int32_t obj_generation;
    size_t offset;
    int32_t generation;
};

struct PackedXRef {
    int32_t stream_object;
    int32_t index_in_stream;
};

struct ObjStmIndex {
    int32_t object_number;
    int64_t offset;
};

typedef std::variant<UnpackedXRef, PackedXRef> XRef;

struct App {
    GtkApplication *app;
    std::filesystem::path ifile;
    GtkWindow *win;
    GtkTreeView *objectview;
    GtkTreeStore *objectstore;
    GtkTextView *obj_text;
    GtkTextView *stream_text;
    std::vector<ObjectData> objects;
};

std::string detect_type(std::string_view odict) {
    const char active_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    auto typeloc = odict.find("/Type ");
    if(typeloc != std::string::npos) {
        auto typeend = odict.find_first_not_of(active_chars, typeloc + 7);
        return std::string{odict.substr(typeloc + 7, typeend - typeloc - 7)};
    }
    typeloc = odict.find("/Type");
    if(typeloc != std::string::npos) {
        std::string bob{odict};
        auto typeview = odict.substr(typeloc + 6);
        auto typeend = typeview.find_first_not_of(active_chars);
        return std::string{typeview.substr(0, typeend)};
    }
    return "";
}

void reload_object_view(App &a) {
    gtk_tree_store_clear(a.objectstore);
    GtkTreeIter iter;
    size_t i = 0;
    for(const auto &object : a.objects) {
        auto type = detect_type(object.dict);
        gtk_tree_store_append(a.objectstore, &iter, nullptr);
        gtk_tree_store_set(a.objectstore,
                           &iter,
                           OBJNUM_COLUMN,
                           (int)i,
                           OFFSET_COLUMN,
                           0, //(int64_t)object.offset,
                           STREAM_SIZE_COLUMN,
                           (int)object.stream.size(),
                           TYPE_COLUMN,
                           type.c_str(),
                           -1);
        ++i;
    }
}

BinaryData load_binary_data(std::string_view data) {
    BinaryData bd;

    const auto obj_start = data.find(" obj\n");
    if(obj_start >= 10) { // Not correct
        printf("Invalid offset to object.\n");
        std::abort();
    }
    // Not reliable but so what.
    const auto stream_pos = data.find("stream\n");
    const auto endstream_pos = data.find("endstream\n");
    const auto endobj_pos = data.find("endobj\n");

    if(endobj_pos == std::string_view::npos) {
        printf("End of object marker missing.\n");
        std::abort();
    }
    if(stream_pos != std::string_view::npos) {
        if(endstream_pos == std::string_view::npos) {
            printf("Malformed stream.\n");
            std::abort();
        }
        if(stream_pos < endobj_pos) {
            bd.dict = data.substr(0, stream_pos);
            bd.stream = data.substr(stream_pos + 7, endstream_pos - stream_pos - 7);
        } else {
            bd.dict = data.substr(0, endobj_pos);
        }
    } else {
        bd.dict = data.substr(0, endobj_pos);
    }
    return bd;
}

struct XRefStreamEntry {
    size_t f1;
    size_t f2;
    size_t f3;
};

size_t fuf(const char *data, int size) {
    if(size == 1) {
        return (unsigned char)data[0];
    }
    if(size == 2) {
        return std::byteswap(((uint16_t *)(data))[0]);
    }
    if(size == 4) {
        return std::byteswap(((uint32_t *)(data))[0]);
    }
    if(size == 8) {
        return std::byteswap(((uint64_t *)(data))[0]);
    }
    std::abort();
}

XRefStreamEntry unpack_entry(const char *data, const std::array<int, 3> &W) {
    XRefStreamEntry r;
    r.f1 = fuf(data, W[0]);
    data += W[0];
    r.f2 = fuf(data, W[1]);
    data += W[1];
    r.f3 = fuf(data, W[2]);
    data += W[2];
    return r;
}

std::optional<std::vector<XRef>> parse_xref_stream(std::string_view data,
                                                   const std::array<int, 3> &W) {
    std::vector<XRef> refs;
    const auto entry_size = W[0] + W[1] + W[2];
    assert(entry_size > 0);
    assert(data.size() % entry_size == 0);
    const auto num_xrefs = data.size() / entry_size;
    for(size_t i = 0; i < num_xrefs; ++i) {
        const char *current = data.data() + i * entry_size;
        auto raw_entry = unpack_entry(current, W);
        switch(raw_entry.f1) {
        case 0:
            assert(raw_entry.f2 == 0);
            break;
        case 1:
            refs.emplace_back(UnpackedXRef{(int32_t)raw_entry.f3, raw_entry.f2, {}});
            break;
        case 2:
            refs.emplace_back(PackedXRef{raw_entry.f2, raw_entry.f3});
            break;
        default:
            std::abort();
        }
    }
    return refs;
}

std::optional<std::vector<XRef>> parse_xreftable(std::string_view xref) {
    std::vector<XRef> refs;
    const char *data_in;
    if(xref.find("xref\n") == 0) {
        data_in = xref.data() + 5;
    } else if(xref.find("xref\r\n") == 0) {
        data_in = xref.data() + 6;
    } else {
        PdfParser p(xref);
        auto obj = p.parse();
        if(!obj) {
            printf("Xref table is not valid.\n");
            return {};
        }
        auto inflated = inflate(obj->stream);
        std::array<int, 3> W;
        bool was_found = false;
        for(const auto &d : obj->dicts) {
            const auto f = d.find("W");
            if(f != d.end()) {
                const auto &w_entry = f->second;
                const auto &w_array_index = std::get<PdfNodeArray>(w_entry);
                const auto &w_array = obj->arrays[w_array_index.i];
                W[0] = std::get<int64_t>(w_array[0]);
                W[1] = std::get<int64_t>(w_array[1]);
                W[2] = std::get<int64_t>(w_array[2]);
                was_found = true;
                break;
            }
        }
        if(!was_found) {
            printf("XRef object is malformed.\n");
            return {};
        }
        return parse_xref_stream(inflated, W);
        std::abort();
    }
    char *tmp;
    const long first_obj = strtol(data_in, &tmp, 10);
    if(first_obj != 0) {
        printf("Xref entries must start with object 0.\n");
        return {};
    }
    data_in = tmp;
    const long num_objects = strtol(data_in, &tmp, 10);
    if(num_objects <= 0) {
        printf("Invalid number of entries in xref table.");
        return {};
    }
    data_in = tmp;
    if(*data_in == '\r') {
        ++data_in;
    }
    ++data_in;
    refs.reserve(num_objects);
    for(long i = 0; i < num_objects; ++i) {
        auto obj_offset = strtol(data_in, nullptr, 10);
        auto obj_generation = strtol(data_in + 11, nullptr, 10);
        auto type = data_in[17];
        switch(type) {
        case 'n': {
            if(obj_generation != 0) {
                printf("Can not handle multi-generation PDF files.\n");
                return {};
            }
        } break;
        case 'f': {
            if(obj_generation != free_generation) {
                printf("Can not handle multipart indexes yet.\n");
                return {};
            }
        } break;
        default: {
            printf("Cross reference entry type is invalid.\n");
            return {};
        }
        }
        refs.push_back(UnpackedXRef{size_t(obj_offset), (int32_t)obj_generation, {}});
        data_in += xref_entry_size;
    }
    return refs;
}

std::vector<ObjStmIndex> get_objstm_offsets(std::string_view stream, int num_entries) {
    const char *loc = stream.data();
    std::vector<ObjStmIndex> offsets;
    offsets.reserve(num_entries);
    for(int i = 0; i < num_entries; ++i) {
        char *end;
        int32_t id = strtol(loc, &end, 10);
        loc = end;
        int64_t offset = strtol(loc, &end, 10);
        loc = end;
        offsets.emplace_back(id, offset);
    }
    return offsets;
}

struct Between {
    size_t from;
    size_t length;
};

Between get_objstm_offset(const std::vector<ObjStmIndex> &indices, size_t object_index) {
    Between res;
    res.from = indices[object_index].offset;
    res.length = object_index + 1 == indices.size() ? (size_t)-1
                                                    : (indices[object_index + 1].offset - res.from);
    return res;
}

std::optional<std::vector<ObjectData>> parse_pdf(std::string_view data) {
    if(data.find("%PDF-") != 0) {
        printf("Not a valid PDF file.\n");
        return {};
    }
    const auto i1 = data.rfind('\n', data.size() - 2);
    const auto i2 = data.rfind('\n', i1 - 2);
    const auto i3 = data.rfind('\n', i2 - 2);
    if(i3 == std::string_view::npos) {
        printf("Very invalid PDF file.\n");
        return {};
    }
    /*
     * The trailer is not actually needed for now and some files don't have it.
    const auto trailer_off = data.rfind("trailer", i3);
    if(trailer_off == std::string::npos) {
        printf("Trailer dictionary is missing.\n");
        return {};
    }
    */
    auto xrefloc = data.find("startxref", i3);
    if(xrefloc == std::string::npos) {
        printf("Cross reference table missing.\n");
        return {};
    }
    auto xrefnumberstart = data.find_first_not_of("\n\r", xrefloc + strlen("startxref"));
    if(xrefnumberstart == std::string::npos) {
        printf("Malformed xref.\n");
        return {};
    }
    const auto xrefstart = strtol(data.data() + xrefnumberstart, nullptr, 10);
    if(xrefstart <= 0 || (size_t)xrefstart >= data.size()) {
        printf("Cross reference offset incorrect.\n");
        return {};
    }
    std::vector<ObjectData> objs;
    objs.reserve(1024);
    auto xreftable_maybe = parse_xreftable(data.substr(xrefstart));
    if(!xreftable_maybe) {
        return {};
    }
    auto &xreftable = *xreftable_maybe;
    ObjectData *objstm = nullptr;
    for(auto &xref : xreftable) {
        if(auto *uxref = std::get_if<UnpackedXRef>(&xref)) {
            if(uxref->offset >= data.size()) {
                printf("Xref points past end of file.\n");
                return {};
            }
            if(uxref->obj_generation == free_generation) {
                continue;
            }
            auto bd = load_binary_data(data.substr(uxref->offset));
            objs.emplace_back(uxref->generation, std::move(bd.dict), std::move(bd.stream));
            if(objs.back().dict.find("/ObjStm") != std::string::npos) {
                objstm = &objs.back();
            }
        } else if(auto *cxref = std::get_if<PackedXRef>(&xref)) {
            objs.push_back(ObjectData{});
        } else {
            printf("Skibidi.\n");
            std::abort();
        }
    }

    if(objstm) {
        // Unpack compressed objects.
        std::string poop = objstm->dict;
        poop += "endobj\n";
        PdfParser pp(poop);
        auto tmp = pp.parse();
        auto &objstm_dict = tmp.value();
        auto &top_dict = objstm_dict.dicts[0];
        const auto first_offset = std::get<int64_t>(top_dict["First"]);
        const auto N = std::get<int64_t>(top_dict["N"]);
        const auto unpacked = inflate(objstm->stream);
        const auto offsets = get_objstm_offsets(unpacked, N);
        const auto stream_data = std::string_view(unpacked).substr(first_offset);

        for(size_t i = 0; i < xreftable.size(); ++i) {
            const auto &xref = xreftable[i];
            if(auto *cxref = std::get_if<PackedXRef>(&xref)) {
                const auto obj_offset = get_objstm_offset(offsets, cxref->index_in_stream);
                std::string parsed_string = std::to_string(i + 1);
                parsed_string += " 0 obj\n";
                parsed_string += stream_data.substr(obj_offset.from, obj_offset.length);
                parsed_string += parsed_string.back() == '\n' ? "endobj\n" : "\nendobj\n";
                const auto bd = load_binary_data(parsed_string);
                assert(bd.stream.empty());
                objs[i] = ObjectData{0, bd.dict, bd.stream};
            }
        }
    }

    return std::move(objs);
}

void load_file(App &a, const std::filesystem::path &ifile) {
    FILE *f = fopen(ifile.c_str(), "r");
    if(!f) {
        printf("Could not open file %s: %s\n", ifile.c_str(), strerror(errno));
        return;
    }
    std::unique_ptr<FILE, int (*)(FILE *)> fcloser(f, fclose);
    fseek(f, 0, SEEK_END);
    auto file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *addr = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fileno(f), 0);
    if(!addr) {
        printf("MMap failed: %s\n", strerror(errno));
        return;
    }
    MMapCloser mc{addr, (size_t)file_size};
    std::string_view data{(const char *)addr, (size_t)file_size};
    auto new_entries_maybe = parse_pdf(data);
    if(new_entries_maybe) {
        a.objects = std::move(*new_entries_maybe);
        reload_object_view(a);
        std::string title{appname};
        title += " - ";
        title += ifile.filename().c_str();
        gtk_window_set_title(a.win, title.c_str());
    }
}

void write_file(std::filesystem::path ofile, std::string_view data) {
    FILE *f = fopen(ofile.c_str(), "w");
    if(!f) {
        printf("Could not open file %s: %s.\n", ofile.c_str(), strerror(errno));
        return;
    }
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
}

void save_stream(App &a, const std::filesystem::path &ofile) {
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(a.objectview);
    if(!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }
    int32_t index;
    gtk_tree_model_get(model, &iter, OBJNUM_COLUMN, &index, -1);
    if(index < 0 || (size_t)index >= a.objects.size()) {
        printf("Invalid selection.\n");
        return;
    }
    const auto &outobj = a.objects[index];
    if(outobj.stream.empty()) {
        printf("Object stream is empty");
        return;
    }
    if(outobj.dict.find("/FlateDecode") == std::string::npos) {
        write_file(ofile, outobj.stream);
    } else {
        auto inflated = inflate(outobj.stream);
        write_file(ofile, inflated);
    }
}

void selection_changed_cb(GtkTreeSelection *selection, gpointer data) {
    App *a = static_cast<App *>(data);
    if(a->objects.empty()) {
        return;
    }
    GtkTreeIter iter;
    GtkTreeModel *model;
    if(!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }
    int32_t index;
    gtk_tree_model_get(model, &iter, OBJNUM_COLUMN, &index, -1);

    auto buf = gtk_text_view_get_buffer(a->obj_text);
    assert(index >= 0);
    assert((size_t)index < a->objects.size());
    const auto &raw_dict = a->objects[index].dict;
    std::string cleaned_dict;
    PdfParser pparser(raw_dict);
    auto parseout = pparser.parse();
    if(parseout) {
        PrettyPrinter pp(*parseout);
        cleaned_dict = pp.prettyprint();
    } else {
        cleaned_dict = raw_dict;
    }
    for(auto &c : cleaned_dict) {
        if(int(c) > 127 || int(c) <= 0) {
            c = '?';
        }
    }
    gtk_text_buffer_set_text(buf, cleaned_dict.c_str(), cleaned_dict.size());

    buf = gtk_text_view_get_buffer(a->stream_text);
    const auto &streamtext = a->objects[index].stream;
    if(a->objects[index].dict.find("FlateDecode") != std::string::npos) {
        auto inflated = inflate(streamtext);
        gtk_text_buffer_set_text(buf, inflated.c_str(), inflated.length());
    } else {
        gtk_text_buffer_set_text(buf, streamtext.c_str(), streamtext.length());
    }
}

void file_open_cb(GtkDialog *dialog, int response, gpointer data) {
    App *a = static_cast<App *>(data);
    if(response == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        g_autoptr(GFile) file = gtk_file_chooser_get_file(chooser);
        std::filesystem::path infile(g_file_get_path(file));
        load_file(*a, infile);
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
}

void load_cb(GtkButton *, gpointer data) {
    App *a = static_cast<App *>(data);
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;

    dialog = gtk_file_chooser_dialog_new("Open File",
                                         a->win,
                                         action,
                                         "_Cancel",
                                         GTK_RESPONSE_CANCEL,
                                         "_Open",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);
    gtk_window_present(GTK_WINDOW(dialog));

    g_signal_connect(dialog, "response", G_CALLBACK(file_open_cb), data);
}

void stream_save_cb(GtkDialog *dialog, int response, gpointer data) {
    App *a = static_cast<App *>(data);
    if(response == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        g_autoptr(GFile) file = gtk_file_chooser_get_file(chooser);
        std::filesystem::path infile(g_file_get_path(file));
        gtk_window_destroy(GTK_WINDOW(dialog));
        save_stream(*a, infile); // Might show a dialog.
    } else {

        gtk_window_destroy(GTK_WINDOW(dialog));
    }
}

void file_save_cb(GtkButton *, gpointer data) {
    App *a = static_cast<App *>(data);
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;

    dialog = gtk_file_chooser_dialog_new("Save stream",
                                         a->win,
                                         action,
                                         "_Cancel",
                                         GTK_RESPONSE_CANCEL,
                                         "_Save",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);
    gtk_window_present(GTK_WINDOW(dialog));

    g_signal_connect(dialog, "response", G_CALLBACK(stream_save_cb), data);
}

void quit_cb(GtkButton *, gpointer) { g_main_loop_quit(nullptr); }

void build_gui(App &a) {
    a.win = GTK_WINDOW(gtk_application_window_new(a.app));
    gtk_window_set_title(a.win, appname);
    gtk_window_set_default_size(a.win, 1600, 800);

    // I could not decipher how to create a menumodel, ergo this.
    auto *bbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    auto *b = gtk_button_new_with_label("Load");
    g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(load_cb), &a);
    gtk_box_append(GTK_BOX(bbox), b);
    b = gtk_button_new_with_label("Save stream");
    g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(file_save_cb), &a);
    gtk_box_append(GTK_BOX(bbox), b);
    b = gtk_button_new_with_label("Quit");
    g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(quit_cb), &a);
    gtk_box_append(GTK_BOX(bbox), b);

    a.objectstore =
        gtk_tree_store_new(N_OBJ_COLUMNS, G_TYPE_INT, G_TYPE_INT64, G_TYPE_INT, G_TYPE_STRING);
    a.objectview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(a.objectstore)));
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *c =
        gtk_tree_view_column_new_with_attributes("Number", r, "text", OBJNUM_COLUMN, nullptr);
    gtk_tree_view_append_column(a.objectview, c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes("Type", r, "text", TYPE_COLUMN, nullptr);
    gtk_tree_view_append_column(a.objectview, c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes("Offset", r, "text", OFFSET_COLUMN, nullptr);
    gtk_tree_view_append_column(a.objectview, c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes(
        "Stream size", r, "text", STREAM_SIZE_COLUMN, nullptr);
    gtk_tree_view_append_column(a.objectview, c);

    auto select = gtk_tree_view_get_selection(a.objectview);
    gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
    g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(selection_changed_cb), &a);

    GtkNotebook *note = GTK_NOTEBOOK(gtk_notebook_new());
    GtkGrid *grid = GTK_GRID(gtk_grid_new());
    gtk_grid_attach(grid, bbox, 0, 0, 2, 1);
    auto *scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(scroll, 600, -1);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(a.objectview));
    gtk_grid_attach(grid, scroll, 0, 1, 1, 1);
    a.obj_text = GTK_TEXT_VIEW(gtk_text_view_new());
    a.stream_text = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_widget_set_vexpand(GTK_WIDGET(a.obj_text), 1);
    gtk_widget_set_hexpand(GTK_WIDGET(a.obj_text), 1);
    gtk_widget_set_vexpand(GTK_WIDGET(a.stream_text), 1);
    gtk_widget_set_hexpand(GTK_WIDGET(a.stream_text), 1);
    scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, 1);
    gtk_widget_set_hexpand(scroll, 1);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(a.obj_text));
    gtk_notebook_append_page(note, scroll, gtk_label_new("Dictionary"));
    scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, 1);
    gtk_widget_set_hexpand(scroll, 1);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(a.stream_text));
    gtk_notebook_append_page(note, scroll, gtk_label_new("Stream"));
    gtk_grid_attach(grid, GTK_WIDGET(note), 1, 1, 1, 1);

    gtk_window_set_child(GTK_WINDOW(a.win), GTK_WIDGET(grid));
}

void activate(GtkApplication *, gpointer user_data) {
    auto *app = static_cast<App *>(user_data);
    build_gui(*app);
    load_file(*app, app->ifile);
    gtk_window_present(GTK_WINDOW(app->win));
}

} // namespace

int main(int argc, char **argv) {
    App app;
    app.ifile = "title.pdf";
    app.app = gtk_application_new("io.github.jpakkane.pdfviewer", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app.app, "activate", G_CALLBACK(activate), static_cast<gpointer>(&app));
    int status = g_application_run(G_APPLICATION(app.app), argc, argv);
    g_object_unref(app.app);
    return status;
}
