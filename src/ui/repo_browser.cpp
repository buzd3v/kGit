#include "repo_browser.hpp"
#include "../git_engine.hpp"
#include "common.hpp"
#include <iostream>

namespace {
    struct TreeCols : public Gtk::TreeModel::ColumnRecord {
        Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> icon;
        Gtk::TreeModelColumn<Glib::ustring>             name;
        Gtk::TreeModelColumn<Glib::ustring>             size;
        Gtk::TreeModelColumn<std::string>              full_path;
        Gtk::TreeModelColumn<bool>                     is_dir;

        TreeCols() {
            add(icon); add(name); add(size); add(full_path); add(is_dir);
        }
    };
    TreeCols T_COLS;
}

// ─── RepoBrowser::Impl ─────────────────────────────────────────────────────────

struct RepoBrowser::Impl {
    GitEngine& engine_;

    Gtk::ComboBoxText ref_combo_;
    Gtk::TreeView    tree_view_;
    Gtk::Entry       commit_entry_;
    Gtk::Button      blob_btn_;
    Gtk::Button      refresh_btn_;

    Glib::RefPtr<Gtk::TreeStore> model_;
    sigc::signal<void(const std::string& path, const std::string& content)> signal_open_file_;

    Impl(GitEngine& engine) : engine_(engine) {
        model_ = Gtk::TreeStore::create(T_COLS);
        tree_view_.set_model(model_);
        tree_view_.set_headers_visible(false);

        auto* icon_c = Gtk::manage(new Gtk::CellRendererPixbuf());
        auto* text_c = Gtk::manage(new Gtk::CellRendererText());
        tree_view_.append_column("", *icon_c);
        tree_view_.append_column("Name", *text_c);
        tree_view_.get_column(1)->set_expand(true);
        tree_view_.get_column(1)->add_attribute(text_c->property_text(), T_COLS.name);
        tree_view_.get_column(0)->add_attribute(icon_c->property_pixbuf(), T_COLS.icon);

        tree_view_.signal_row_activated().connect(
            [&](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*) {
                if (path.size() == 0) return;
                auto row = *model_->get_iter(path);
                if (row.get_value(T_COLS.is_dir)) {
                    // Already expanded via populate
                } else {
                    load_file(std::string(row.get_value(T_COLS.full_path)));
                }
            }
        );

        refresh_btn_.set_label("Refresh");
        blob_btn_.set_label("View File");

        blob_btn_.signal_clicked().connect([&] {
            auto sel = tree_view_.get_selection()->get_selected();
            if (!sel) return;
            load_file(std::string((*sel).get_value(T_COLS.full_path)));
        });

        ref_combo_.append("HEAD");
        ref_combo_.set_active(0);
        ref_combo_.signal_changed().connect([&] {
            std::string ref = ref_combo_.get_active_text();
            if (!ref.empty()) load_tree(ref);
        });

        commit_entry_.set_placeholder_text("Commit hash…");
        commit_entry_.signal_activate().connect([&] {
            std::string ref = commit_entry_.get_text();
            if (!ref.empty()) load_tree(ref);
        });

        ref_combo_.append("HEAD");
        ref_combo_.set_active(0);
    }

    void populate(const std::string& ref) {
        if (!engine_.repo()) return;
        model_->clear();
        auto result = engine_.tree_ls(ref);
        if (!result.ok()) return;

        for (const auto& entry : result.value) {
            std::string path = entry;
            auto slash = path.find('/');
            std::string name = slash == std::string::npos ? path : path.substr(0, slash);
            bool is_dir = (slash != std::string::npos);

            Gtk::TreeRow row = *model_->append();
            row.set_value(T_COLS.name,      Glib::ustring(name));
            row.set_value(T_COLS.icon,      is_dir ? icons::folder_icon() : icons::file_icon(name));
            row.set_value(T_COLS.is_dir,    is_dir);
            row.set_value(T_COLS.full_path,  path);
            row.set_value(T_COLS.size,       Glib::ustring(""));
        }
        tree_view_.expand_all();
    }

    void load_tree(const std::string& ref) {
        if (!engine_.repo()) return;
        model_->clear();
        auto result = engine_.tree_ls(ref);
        if (!result.ok()) return;

        for (const auto& entry : result.value) {
            std::string path = entry;
            auto slash = path.find('/');
            std::string name = slash == std::string::npos ? path : path.substr(0, slash);
            bool is_dir = (slash != std::string::npos);

            Gtk::TreeRow row = *model_->append();
            row.set_value(T_COLS.name,      Glib::ustring(name));
            row.set_value(T_COLS.icon,      is_dir ? icons::folder_icon() : icons::file_icon(name));
            row.set_value(T_COLS.is_dir,    is_dir);
            row.set_value(T_COLS.full_path,  path);
            row.set_value(T_COLS.size,       Glib::ustring(""));
        }
        tree_view_.expand_all();
    }

    void load_file(const std::string& path) {
        std::string ref = ref_combo_.get_active_text();
        if (ref.empty()) ref = "HEAD";
        auto result = engine_.blob_content(ref, path);
        if (result.ok()) {
            signal_open_file_.emit(path, result.value);
        }
    }
};

// ─── RepoBrowser ───────────────────────────────────────────────────────────────

RepoBrowser::RepoBrowser(GitEngine& engine)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 4)
    , impl_(std::make_unique<Impl>(engine))
{
    set_hexpand(true);
    set_vexpand(true);
    set_border_width(4);

    auto* toolbar = Gtk::manage(new Gtk::Toolbar());
    toolbar->set_toolbar_style(Gtk::TOOLBAR_BOTH_HORIZ);
    toolbar->add(impl_->refresh_btn_);
    toolbar->add(impl_->blob_btn_);

    auto* top_bar = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6));
    top_bar->pack_start(*Gtk::manage(new Gtk::Label("Ref:")), false, false, 0);
    top_bar->pack_start(impl_->ref_combo_, true, true, 0);
    top_bar->pack_start(*Gtk::manage(new Gtk::Label("Hash:")), false, false, 0);
    top_bar->pack_start(impl_->commit_entry_, true, true, 0);

    pack_start(*toolbar, false, false, 0);
    pack_start(*top_bar, false, false, 0);
    pack_start(impl_->tree_view_, true, true, 0);

    impl_->populate("HEAD");
}

RepoBrowser::~RepoBrowser() = default;

sigc::signal<void(const std::string&, const std::string&)>& RepoBrowser::signal_open_file() {
    return impl_->signal_open_file_;
}
