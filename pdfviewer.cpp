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

#include <gtk/gtk.h>
#include <sys/mman.h>
#include <filesystem>
#include <optional>
#include <vector>

enum { OBJNUM_COLUMN, OFFSET_COLUMN, N_OBJ_COLUMNS };

namespace {

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

struct XrefEntry {
    int32_t obj_generation;
    size_t offset;
    BinaryData bd;
};

struct App {
    GtkApplication *app;
    std::filesystem::path ifile;
    GtkWindow *win;
    GtkTreeView *objectview;
    GtkTreeStore *objectstore;
    GtkTextView *obj_text;
    GtkTextView *stream_text;
    std::vector<XrefEntry> objects;
};

void reload_object_view(App &a) {
    gtk_tree_store_clear(a.objectstore);
    GtkTreeIter iter;
    size_t i = 0;
    for(const auto &object : a.objects) {
        gtk_tree_store_append(a.objectstore, &iter, nullptr);
        gtk_tree_store_set(
            a.objectstore, &iter, OBJNUM_COLUMN, (int)i, OFFSET_COLUMN, (int64_t)object.offset, -1);
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

std::optional<std::vector<XrefEntry>> parse_xreftable(std::string_view xref) {
    std::vector<XrefEntry> refs;
    if(xref.find("xref\n") != 0) {
        printf("Xref table is not valid.\n");
        return {};
    }
    const char *data_in = xref.data() + 5;
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
    data_in = tmp + 1;
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
        refs.push_back(XrefEntry{(int32_t)obj_generation, size_t(obj_offset), {}});
        data_in += xref_entry_size;
    }
    return refs;
}

std::optional<std::vector<XrefEntry>> parse_pdf(App &a, std::string_view data) {
    if(data.find("%PDF-1.") != 0) {
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
    const auto trailer_off = data.rfind("trailer", i3);
    if(trailer_off == std::string::npos) {
        printf("Trailer dictionary is missing.\n");
        return {};
    }
    auto xref = data.substr(i3 + 1, i2 - i3 - 1);
    if(xref != "startxref") {
        printf("Cross reference table missing.\n");
        return {};
    }
    const auto xrefstart = strtol(data.data() + i2 + 1, nullptr, 10);
    if(xrefstart <= 0 || (size_t)xrefstart >= data.size()) {
        printf("Cross reference offset incorrect.\n");
        return {};
    }
    auto xreftable_maybe = parse_xreftable(data.substr(xrefstart));
    if(!xreftable_maybe) {
        return {};
    }
    auto &xreftable = *xreftable_maybe;
    for(auto &xref : xreftable) {
        if(xref.offset >= data.size()) {
            printf("Xref points past end of file.\n");
            return {};
        }
        if(xref.obj_generation == free_generation) {
            continue;
        }
        xref.bd = load_binary_data(data.substr(xref.offset));
    }
    return std::move(xreftable);
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
    auto new_entries_maybe = parse_pdf(a, data);
    if(new_entries_maybe) {
        a.objects = std::move(*new_entries_maybe);
        reload_object_view(a);
    }
}

void selection_changed_cb(GtkTreeSelection *selection, gpointer data) {
    App *a = static_cast<App *>(data);
    GtkTreeIter iter;
    GtkTreeModel *model;
    if(!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }
    int32_t index;
    gtk_tree_model_get(model, &iter, OBJNUM_COLUMN, &index, -1);
    auto buf = gtk_text_view_get_buffer(a->obj_text);
    gtk_text_buffer_set_text(
        buf, a->objects[index].bd.dict.c_str(), a->objects[index].bd.dict.size());
}

void build_gui(App &a) {
    a.win = GTK_WINDOW(gtk_application_window_new(a.app));
    gtk_window_set_title(a.win, "PDF browser");
    gtk_window_set_default_size(a.win, 1024, 800);

    a.objectstore = gtk_tree_store_new(N_OBJ_COLUMNS, G_TYPE_INT, G_TYPE_INT64);
    a.objectview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(a.objectstore)));
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(
        "Object number", r, "text", OBJNUM_COLUMN, nullptr);
    gtk_tree_view_append_column(a.objectview, c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes("Offset", r, "text", OFFSET_COLUMN, nullptr);
    gtk_tree_view_append_column(a.objectview, c);
    auto select = gtk_tree_view_get_selection(a.objectview);
    gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
    g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(selection_changed_cb), &a);

    GtkGrid *grid = GTK_GRID(gtk_grid_new());
    auto *scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(scroll, 400, -1);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(a.objectview));
    gtk_grid_attach(grid, scroll, 0, 0, 1, 1);
    a.obj_text = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_widget_set_vexpand(GTK_WIDGET(a.obj_text), 1);
    gtk_widget_set_hexpand(GTK_WIDGET(a.obj_text), 1);
    gtk_grid_attach(grid, GTK_WIDGET(a.obj_text), 1, 0, 1, 1);
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
