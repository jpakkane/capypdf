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

struct App {
    GtkApplication *app;
    std::filesystem::path ifile;
    GtkWindow *win;
    GtkTreeView *objectview;
    GtkTreeStore *objectstore;
};

enum { OBJNUM_COLUMN, DICT_COLUMN, STREAM_COLUMN, N_OBJ_COLUMNS };

namespace {

struct MMapCloser {
    void *addr;
    size_t length;
    ~MMapCloser() { munmap(addr, length); }
};

void parse_pdf(App &a, std::string_view data) {
    if(data.find("%PDF-1.") != 0) {
        printf("Not a valid PDF file.\n");
        return;
    }
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
    parse_pdf(a, data);
}

void activate(GtkApplication *, gpointer user_data) {
    auto *app = static_cast<App *>(user_data);
    app->win = GTK_WINDOW(gtk_application_window_new(app->app));
    gtk_window_set_title(app->win, "PDF browser");
    gtk_window_set_default_size(app->win, 800, 480);

    app->objectstore = gtk_tree_store_new(N_OBJ_COLUMNS, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);
    app->objectview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->objectstore)));
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(
        "Object number", r, "text", OBJNUM_COLUMN, nullptr);
    gtk_tree_view_append_column(app->objectview, c);
    gtk_window_set_child(GTK_WINDOW(app->win), GTK_WIDGET(app->objectview));
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
