#include "branch_view.hpp"
#include "../git_engine.hpp"
#include "common.hpp"
#include <iostream>

namespace {
    struct BranchCols : public Gtk::TreeModel::ColumnRecord {
        Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> icon;
        Gtk::TreeModelColumn<Glib::ustring>             name;
        Gtk::TreeModelColumn<bool>                    is_current;
        Gtk::TreeModelColumn<bool>                    is_remote;
        Gtk::TreeModelColumn<std::string>             full_name;

        BranchCols() {
            add(icon); add(name); add(is_current); add(is_remote); add(full_name);
        }
    };
    BranchCols B_COLS;

    struct TagCols : public Gtk::TreeModel::ColumnRecord {
        Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> icon;
        Gtk::TreeModelColumn<Glib::ustring>             name;
        Gtk::TreeModelColumn<std::string>             target;

        TagCols() {
            add(icon); add(name); add(target);
        }
    };
    TagCols T_COLS;
}

// ─── BranchView::Impl ─────────────────────────────────────────────────────────

struct BranchView::Impl {
    BranchView& parent_;
    Glib::RefPtr<Gtk::ListStore> local_model_;
    Glib::RefPtr<Gtk::ListStore> remote_model_;
    Glib::RefPtr<Gtk::ListStore> tag_model_;

    Impl(BranchView& parent)
        : parent_(parent)
    {
        local_model_  = Gtk::ListStore::create(B_COLS);
        remote_model_ = Gtk::ListStore::create(B_COLS);
        tag_model_    = Gtk::ListStore::create(T_COLS);

        // ── Local branches ─────────────────────────────────────────────────
        parent_.local_view_.set_model(local_model_);
        parent_.local_view_.set_headers_visible(false);
        auto* licon = Gtk::manage(new Gtk::CellRendererPixbuf());
        auto* ltxt  = Gtk::manage(new Gtk::CellRendererText());
        parent_.local_view_.append_column("", *licon);
        parent_.local_view_.append_column("Local Branches", *ltxt);
        parent_.local_view_.get_column(1)->set_expand(true);
        parent_.local_view_.get_column(1)->add_attribute(ltxt->property_text(), B_COLS.name);
        parent_.local_view_.get_column(1)->add_attribute(ltxt->property_weight(), B_COLS.is_current);
        parent_.local_view_.get_column(0)->add_attribute(licon->property_pixbuf(), B_COLS.icon);

        // ── Remote branches ─────────────────────────────────────────────────
        parent_.remote_view_.set_model(remote_model_);
        parent_.remote_view_.set_headers_visible(false);
        auto* ricon = Gtk::manage(new Gtk::CellRendererPixbuf());
        auto* rtxt  = Gtk::manage(new Gtk::CellRendererText());
        parent_.remote_view_.append_column("", *ricon);
        parent_.remote_view_.append_column("Remote Branches", *rtxt);
        parent_.remote_view_.get_column(1)->set_expand(true);
        parent_.remote_view_.get_column(1)->add_attribute(rtxt->property_text(), B_COLS.name);

        // ── Tags ───────────────────────────────────────────────────────────
        parent_.tag_view_.set_model(tag_model_);
        parent_.tag_view_.set_headers_visible(false);
        auto* ticon = Gtk::manage(new Gtk::CellRendererPixbuf());
        auto* ttxt  = Gtk::manage(new Gtk::CellRendererText());
        parent_.tag_view_.append_column("", *ticon);
        parent_.tag_view_.append_column("Tags", *ttxt);
        parent_.tag_view_.get_column(1)->set_expand(true);
        parent_.tag_view_.get_column(1)->add_attribute(ttxt->property_text(), T_COLS.name);

        // ── Button wiring ───────────────────────────────────────────────────
        parent_.checkout_btn_.signal_clicked().connect([&] {
            auto sel = parent_.local_view_.get_selection()->get_selected();
            if (!sel) sel = parent_.remote_view_.get_selection()->get_selected();
            if (!sel) return;
            std::string name = std::string((*sel)[B_COLS.full_name]);
            parent_.signal_checkout_.emit(name);
        });

        parent_.delete_btn_.signal_clicked().connect([&] {
            auto lit = parent_.local_view_.get_selection()->get_selected();
            if (lit) {
                if ((*lit)[B_COLS.is_current]) return;
                parent_.signal_delete_branch_.emit(std::string((*lit)[B_COLS.full_name]));
                return;
            }
            auto rit = parent_.remote_view_.get_selection()->get_selected();
            if (rit) {
                parent_.signal_delete_branch_.emit(std::string((*rit)[B_COLS.full_name]));
                return;
            }
            auto tit = parent_.tag_view_.get_selection()->get_selected();
            if (tit) {
                parent_.signal_delete_tag_.emit(std::string((*tit)[T_COLS.target]));
            }
        });

        parent_.new_tag_btn_.signal_clicked().connect([&] {
            parent_.signal_delete_tag_.emit("__new_tag__");
        });
    }
};

// ─── BranchView ────────────────────────────────────────────────────────────────

BranchView::BranchView()
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6)
    , impl_(std::make_unique<Impl>(*this))
{
    set_hexpand(true);
    set_vexpand(true);

    auto* local_lbl  = Gtk::make_managed<Gtk::Label>();
    local_lbl->set_markup("<b>Local Branches</b>");
    local_lbl->set_xalign(0);
    auto* remote_lbl = Gtk::make_managed<Gtk::Label>();
    remote_lbl->set_markup("<b>Remote Branches</b>");
    remote_lbl->set_xalign(0);
    auto* tag_lbl    = Gtk::make_managed<Gtk::Label>();
    tag_lbl->set_markup("<b>Tags</b>");
    tag_lbl->set_xalign(0);

    auto* local_sw   = Gtk::make_managed<Gtk::ScrolledWindow>();
    auto* remote_sw  = Gtk::make_managed<Gtk::ScrolledWindow>();
    auto* tag_sw     = Gtk::make_managed<Gtk::ScrolledWindow>();
    local_sw->add(local_view_);
    remote_sw->add(remote_view_);
    tag_sw->add(tag_view_);

    auto* btn_bar = Gtk::make_managed<Gtk::ButtonBox>(Gtk::ORIENTATION_HORIZONTAL);
    checkout_btn_.set_label("Checkout");
    delete_btn_.set_label("Delete");
    new_tag_btn_.set_label("New Tag");
    btn_bar->add(checkout_btn_);
    btn_bar->add(delete_btn_);
    btn_bar->add(new_tag_btn_);

    pack_start(*local_lbl,  false, false, 0);
    pack_start(*local_sw,   true,  true,  0);
    pack_start(*remote_lbl, false, false, 0);
    pack_start(*remote_sw,  true,  true,  0);
    pack_start(*tag_lbl,    false, false, 0);
    pack_start(*tag_sw,     true,  true,  0);
    pack_start(*btn_bar,    false, false, 0);
}

void BranchView::set_branches(const std::vector<BranchInfo>& branches,
                               const std::vector<RemoteInfo>&,
                               const std::vector<TagInfo>&) {
    impl_->local_model_->clear();
    impl_->remote_model_->clear();

    for (const auto& b : branches) {
        if (b.is_remote) {
            auto row = *impl_->remote_model_->append();
            row[B_COLS.name] = b.name;
            row[B_COLS.full_name] = b.name;
            continue;
        }
        auto row = *impl_->local_model_->append();
        row[B_COLS.icon]       = icons::status_icon(b.is_current ? "added" : "modified");
        row[B_COLS.name]       = b.name;
        row[B_COLS.is_current] = b.is_current;
        row[B_COLS.full_name]  = b.name;
        if (b.is_current)
            local_view_.get_selection()->select(row);
    }
}

void BranchView::set_tags(const std::vector<TagInfo>& tags) {
    impl_->tag_model_->clear();
    for (const auto& t : tags) {
        auto row = *impl_->tag_model_->append();
        row[T_COLS.icon]   = icons::status_icon("renamed");
        row[T_COLS.name]   = t.name;
        row[T_COLS.target] = t.target_id;
    }
}

void BranchView::set_remotes(const std::vector<RemoteInfo>& remotes) {
    impl_->remote_model_->clear();
    for (const auto& r : remotes) {
        auto row = *impl_->remote_model_->append();
        row[B_COLS.name]      = r.name;
        row[B_COLS.full_name] = r.name;
    }
}
