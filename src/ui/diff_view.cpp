#include "diff_view.hpp"
#include "../git_engine.hpp"
#include "common.hpp"
#include "../utils/diff_tool.hpp"
#include <iostream>

namespace {
    struct DiffFileCols : public Gtk::TreeModel::ColumnRecord {
        Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> icon;
        Gtk::TreeModelColumn<Glib::ustring>            filename;
        Gtk::TreeModelColumn<Glib::ustring>            stats;   // e.g. "+5 -3"
        Gtk::TreeModelColumn<std::string>             full_path;

        DiffFileCols() {
            add(icon); add(filename); add(stats); add(full_path);
        }
    };
    DiffFileCols D_COLS;
}

// ─── DiffView::Impl ────────────────────────────────────────────────────────────

struct DiffView::Impl {
    Gtk::Paned paned_;                        // file list top / diff bottom
    Gtk::Paned diff_paned_;                  // side-by-side old / new
    Gtk::TreeView file_view_;
    Glib::RefPtr<Gtk::TreeStore> file_model_;
    Gtk::TextView old_view_;
    Gtk::TextView new_view_;
    Gtk::Toolbar  toolbar_;
    Gtk::Button   revert_btn_;
    Gtk::Button   external_btn_;
    sigc::signal<void(const std::string&)> signal_file_revert_;
    std::vector<DiffFile> current_files_;

    Impl() : paned_(Gtk::ORIENTATION_VERTICAL),
             diff_paned_(Gtk::ORIENTATION_HORIZONTAL) {
        // ── File list (top) ────────────────────────────────────────────────
        file_model_ = Gtk::TreeStore::create(D_COLS);
        file_view_.set_model(file_model_);
        file_view_.set_headers_visible(true);

        auto* icon_c = Gtk::manage(new Gtk::CellRendererPixbuf());
        auto* name_c = Gtk::manage(new Gtk::CellRendererText());
        auto* stat_c = Gtk::manage(new Gtk::CellRendererText());
        stat_c->property_foreground() = "#6b7280";

        file_view_.append_column("", *icon_c);
        file_view_.append_column("File", *name_c);
        file_view_.append_column("+/-", *stat_c);
        file_view_.get_column(0)->set_fixed_width(24);
        file_view_.get_column(1)->set_expand(true);
        file_view_.get_column(1)->add_attribute(name_c->property_text(), D_COLS.filename);
        file_view_.get_column(2)->add_attribute(stat_c->property_text(), D_COLS.stats);

        file_view_.get_selection()->signal_changed().connect(
            sigc::mem_fun(*this, &Impl::on_file_selected));

        // ── Toolbar ─────────────────────────────────────────────────────────
        revert_btn_.set_label("Revert");
        external_btn_.set_label("External Diff");
        toolbar_.add(revert_btn_);
        toolbar_.add(external_btn_);

        revert_btn_.signal_clicked().connect([&] {
            auto sel = file_view_.get_selection()->get_selected();
            if (sel)
                signal_file_revert_.emit(std::string((*sel)[D_COLS.full_path]));
        });

        external_btn_.signal_clicked().connect([&] {
            auto sel = file_view_.get_selection()->get_selected();
            if (sel) {
                std::string p = (*sel)[D_COLS.full_path];
                DiffToolLauncher launcher;
                launcher.set_tool("git difftool", {});
                launcher.launch(p, false);
            }
        });

        // ── Diff panes (side-by-side) ───────────────────────────────────────
        old_view_.set_editable(false);
        new_view_.set_editable(false);
        old_view_.set_wrap_mode(Gtk::WRAP_NONE);
        new_view_.set_wrap_mode(Gtk::WRAP_NONE);
        ui::set_monospace(old_view_);
        ui::set_monospace(new_view_);

        auto* old_sw = Gtk::manage(new Gtk::ScrolledWindow());
        auto* new_sw = Gtk::manage(new Gtk::ScrolledWindow());
        old_sw->add(old_view_);
        new_sw->add(new_view_);

        diff_paned_.add1(*old_sw);
        diff_paned_.add2(*new_sw);
        diff_paned_.set_position(400);

        // ── Layout ───────────────────────────────────────────────────────────
        paned_.pack1(file_view_, false, false);
        paned_.pack2(diff_paned_, true, false);
        paned_.set_position(180);
    }

    void on_file_selected() {
        auto sel = file_view_.get_selection()->get_selected();
        if (!sel) return;
        std::string path = (*sel)[D_COLS.full_path];

        auto tag_table = Gtk::TextTagTable::create();
        auto green  = Gtk::TextTag::create(); green->property_foreground()  = "#22c55e";
        auto red    = Gtk::TextTag::create(); red->property_foreground()    = "#ef4444";
        auto hunk   = Gtk::TextTag::create(); hunk->property_foreground()   = "#60a5fa";
        tag_table->add(green); tag_table->add(red); tag_table->add(hunk);

        auto old_buf = Gtk::TextBuffer::create(tag_table);
        auto new_buf = Gtk::TextBuffer::create(tag_table);
        auto end_old = old_buf->end();
        auto end_new = new_buf->end();

        for (const auto& df : current_files_) {
            std::string match_path = df.new_path.empty() ? df.old_path : df.new_path;
            if (match_path != path) continue;
            for (const auto& h : df.hunks) {
                old_buf->insert_with_tag(end_old, h.header + "\n", hunk);
                new_buf->insert_with_tag(end_new, h.header + "\n", hunk);
                for (const auto& line : h.lines) {
                    if (line.rfind("+", 0) == 0) {
                        new_buf->insert_with_tag(end_new, line + "\n", green);
                    } else if (line.rfind("-", 0) == 0) {
                        old_buf->insert_with_tag(end_old, line + "\n", red);
                    } else if (line.rfind("@@", 0) == 0) {
                        old_buf->insert_with_tag(end_old, line + "\n", hunk);
                        new_buf->insert_with_tag(end_new, line + "\n", hunk);
                    } else if (line.size() > 1) {
                        old_buf->insert(end_old, " " + line.substr(1) + "\n");
                        new_buf->insert(end_new, " " + line.substr(1) + "\n");
                    }
                }
            }
            break;
        }

        old_view_.set_buffer(old_buf);
        new_view_.set_buffer(new_buf);
    }
};

// ─── DiffView ─────────────────────────────────────────────────────────────────

DiffView::DiffView()
    : impl_(std::make_unique<Impl>())
{
    set_hexpand(true);
    set_vexpand(true);
    pack_start(impl_->toolbar_, false, false, 0);
    pack_start(impl_->paned_, true, true, 0);
}

void DiffView::set_diff(const std::vector<DiffFile>& files) {
    impl_->current_files_ = files;
    impl_->file_model_->clear();

    for (const auto& df : files) {
        std::string name = df.new_path.empty() ? df.old_path : df.new_path;
        Gtk::TreeRow row = *impl_->file_model_->append();
        row[D_COLS.icon]      = icons::status_icon(name.empty() ? "deleted" : "modified");
        row[D_COLS.filename]  = name;
        row[D_COLS.stats]     = "+" + std::to_string(df.additions)
                               + " -" + std::to_string(df.deletions);
        row[D_COLS.full_path] = name;
    }

    // Auto-select first file.
    if (!files.empty()) {
        auto children = impl_->file_model_->children();
        if (!children.empty())
            impl_->file_view_.get_selection()->select(children[0]);
    }
}

void DiffView::clear() {
    impl_->file_model_->clear();
    impl_->old_view_.get_buffer()->set_text("");
    impl_->new_view_.get_buffer()->set_text("");
}

sigc::signal<void(const std::string&)>& DiffView::signal_file_revert() {
    return impl_->signal_file_revert_;
}
