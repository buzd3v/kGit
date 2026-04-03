#include "status_view.hpp"
#include "../git_engine.hpp"
#include "common.hpp"
#include <git2.h>
#include <iostream>

namespace {
    struct StatusCols : public Gtk::TreeModel::ColumnRecord {
        Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> icon;
        Gtk::TreeModelColumn<Glib::ustring>             status_label;
        Gtk::TreeModelColumn<Glib::ustring>             path;
        Gtk::TreeModelColumn<bool>                      is_staged;
        Gtk::TreeModelColumn<int>                      change_type;

        StatusCols() {
            add(icon); add(status_label); add(path); add(is_staged); add(change_type);
        }
    };
    StatusCols COLS;
}

// ─── StatusView::Impl ─────────────────────────────────────────────────────────

struct StatusView::Impl {
    Gtk::TreeView            tree_view_;
    Glib::RefPtr<Gtk::TreeStore> model_;
    Gtk::Menu                context_menu_;

    Gtk::TreeRow             staged_header_;
    Gtk::TreeRow             unstaged_header_;

    sigc::signal<void(const std::string&, bool)> signal_file_toggled_;

    Impl() {
        tree_view_.set_headers_visible(false);
        model_ = Gtk::TreeStore::create(COLS);
        tree_view_.set_model(model_);

        // Icon column
        auto* icon_cell = Gtk::manage(new Gtk::CellRendererPixbuf());
        icon_cell->property_xalign() = 0.0f;
        tree_view_.append_column("", *icon_cell);
        tree_view_.get_column(0)->add_attribute(icon_cell->property_pixbuf(), COLS.icon);

        // File path column
        auto* text_cell = Gtk::manage(new Gtk::CellRendererText());
        tree_view_.append_column("File", *text_cell);
        tree_view_.get_column(1)->set_expand(true);
        tree_view_.get_column(1)->add_attribute(text_cell->property_text(), COLS.path);

        // ── Section headers ───────────────────────────────────────────────────
        Gtk::TreeRow staged_row   = *model_->append();
        Gtk::TreeRow unstaged_row = *model_->append();

        staged_row[COLS.icon]         = icons::status_icon("modified");
        staged_row[COLS.status_label] = "Staged";
        staged_row[COLS.path]          = "";
        staged_row[COLS.is_staged]     = true;

        unstaged_row[COLS.icon]         = icons::status_icon("untracked");
        unstaged_row[COLS.status_label] = "Unstaged";
        unstaged_row[COLS.path]         = "";
        unstaged_row[COLS.is_staged]    = false;

        staged_header_   = staged_row;
        unstaged_header_ = unstaged_row;

        // ── Context menu ─────────────────────────────────────────────────────
        auto* stage_mi  = Gtk::manage(new Gtk::MenuItem("Stage / Unstage"));
        auto* revert_mi = Gtk::manage(new Gtk::MenuItem("Revert changes"));
        auto* delete_mi = Gtk::manage(new Gtk::MenuItem("Delete (untracked)"));

        context_menu_.append(*stage_mi);
        context_menu_.append(*revert_mi);
        context_menu_.append(*delete_mi);
        context_menu_.show_all();

        tree_view_.signal_button_press_event().connect(
            [&, stage_mi, revert_mi, delete_mi](GdkEventButton* ev) -> bool {
                if (ev->button != 3) return false;
                Gtk::TreeModel::Path path;
                Gtk::TreeViewColumn* col;
                int cx = static_cast<int>(ev->x), cy = static_cast<int>(ev->y);
                if (!tree_view_.get_path_at_pos(cx, cy, path, col, cx, cy))
                    return false;

                auto row = *model_->get_iter(path);
                bool is_header = !(row->parent());
                delete_mi->set_sensitive(!is_header);
                context_menu_.popup(ev->button, ev->time);
                return true;
            }, false
        );

        stage_mi->signal_activate().connect([&] {
            auto sel = tree_view_.get_selection()->get_selected();
            if (!sel) return;
            auto row = *sel;
            bool is_staged = row.get_value(COLS.is_staged);
            Glib::ustring p = row.get_value(COLS.path);
            if (p.empty()) return;
            signal_file_toggled_.emit(std::string(p), !is_staged);
        });

        revert_mi->signal_activate().connect([&] {
            auto sel = tree_view_.get_selection()->get_selected();
            if (!sel) return;
            auto row = *sel;
            bool is_header = !(row->parent());
            if (is_header) {
                for (auto& child : row.children())
                    signal_file_toggled_.emit(std::string(child.get_value(COLS.path)), false);
            } else {
                signal_file_toggled_.emit(std::string(row.get_value(COLS.path)), false);
            }
        });

        delete_mi->signal_activate().connect([&] {
            auto sel = tree_view_.get_selection()->get_selected();
            if (!sel) return;
            auto p = std::string((*sel).get_value(COLS.path));
            signal_file_toggled_.emit(p, false);
        });

        tree_view_.signal_row_activated().connect(
            [&](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*) {
                if (path.size() < 2) return;
                auto row = *model_->get_iter(path);
                bool staged = (row.get_value(COLS.status_label) == "Staged");
                Glib::ustring p = row.get_value(COLS.path);
                if (!p.empty())
                    signal_file_toggled_.emit(std::string(p), !staged);
            }
        );
    }
};

// ─── StatusView ────────────────────────────────────────────────────────────────

StatusView::StatusView()
    : impl_(std::make_unique<Impl>())
{
    set_hexpand(true);
    set_vexpand(true);
    add(impl_->tree_view_);
    impl_->tree_view_.expand_all();
}

void StatusView::set_status(const std::vector<RepoStatus>& statuses) {
    // Remove all child rows under section headers.
    for (auto& header : impl_->model_->children()) {
        for (auto& child : header.children())
            impl_->model_->erase(child);
    }

    for (const auto& s : statuses) {
        Gtk::TreeRow parent = s.staged ? impl_->staged_header_ : impl_->unstaged_header_;
        Gtk::TreeRow row = *impl_->model_->append(parent.children());
        int gt = static_cast<int>(s.change);
        row.set_value(COLS.icon,         icons::status_icon(icons::change_to_string(gt)));
        row.set_value(COLS.status_label, s.staged ? Glib::ustring("Staged") : Glib::ustring("Unstaged"));
        row.set_value(COLS.path,         Glib::ustring(s.path));
        row.set_value(COLS.is_staged,    s.staged);
        row.set_value(COLS.change_type,  gt);
    }

    impl_->tree_view_.expand_all();
}

void StatusView::clear() {
    set_status({});
}

sigc::signal<void(const std::string&, bool)>& StatusView::signal_file_toggled() {
    return impl_->signal_file_toggled_;
}
