#include "commit_dialog.hpp"
#include "../git_engine.hpp"
#include "common.hpp"
#include <iostream>

namespace {
    struct StagedCols : public Gtk::TreeModel::ColumnRecord {
        Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> icon;
        Gtk::TreeModelColumn<Glib::ustring>             path;
        Gtk::TreeModelColumn<int>                      change_type;

        StagedCols() {
            add(icon);
            add(path);
            add(change_type);
        }
    };
    StagedCols STAGED_COLS;
}

// ─── CommitDialog::Impl ────────────────────────────────────────────────────────

struct CommitDialog::Impl {
    GitEngine& engine_;
    Glib::RefPtr<Gtk::ListStore> staged_model_;
    Glib::RefPtr<Gtk::ListStore> unstaged_model_;

    Impl(GitEngine& engine) : engine_(engine) {
        staged_model_   = Gtk::ListStore::create(STAGED_COLS);
        unstaged_model_ = Gtk::ListStore::create(STAGED_COLS);
    }

    void populate_staged(const std::vector<RepoStatus>& statuses) {
        staged_model_->clear();
        for (const auto& s : statuses) {
            if (!s.staged) continue;
            auto row = *staged_model_->append();
            int gt = static_cast<int>(s.change);
            row[STAGED_COLS.icon]        = icons::status_icon(icons::change_to_string(gt));
            row[STAGED_COLS.path]        = s.path;
            row[STAGED_COLS.change_type] = gt;
        }
    }

    void populate_unstaged(const std::vector<RepoStatus>& statuses) {
        unstaged_model_->clear();
        for (const auto& s : statuses) {
            if (s.staged) continue;
            auto row = *unstaged_model_->append();
            int gt = static_cast<int>(s.change);
            row[STAGED_COLS.icon]        = icons::status_icon(icons::change_to_string(gt));
            row[STAGED_COLS.path]        = s.path;
            row[STAGED_COLS.change_type] = gt;
        }
    }
};

// ─── CommitDialog ──────────────────────────────────────────────────────────────

CommitDialog::CommitDialog(Gtk::Window& parent, GitEngine& engine)
    : Gtk::Dialog("Commit", parent, true)
    , engine_(engine)
    , impl_(std::make_unique<Impl>(engine))
{
    set_default_size(700, 550);
    auto box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL, 6);
    box->set_border_width(12);

    // ── Commit message ────────────────────────────────────────────────────────
    box->pack_start(*Gtk::make_managed<Gtk::Label>("Commit message:"), false, false, 0);
    message_view_.set_hexpand(true);
    message_view_.set_vexpand(true);
    message_view_.set_wrap_mode(Gtk::WRAP_WORD);
    auto* msg_scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    msg_scroll->add(message_view_);
    msg_scroll->set_hexpand(true);
    msg_scroll->set_vexpand(true);
    box->pack_start(*msg_scroll, true, true, 0);

    // ── Staged / Unstaged lists ───────────────────────────────────────────────
    staged_view_.set_model(impl_->staged_model_);
    staged_view_.set_headers_visible(false);
    auto* sic = Gtk::manage(new Gtk::CellRendererPixbuf());
    staged_view_.append_column("", *sic);
    if (auto* c = staged_view_.get_column(0)) c->set_fixed_width(20);
    auto* stc = Gtk::manage(new Gtk::CellRendererText());
    staged_view_.append_column("Staged", *stc);
    staged_view_.get_column(1)->set_expand(true);
    staged_view_.get_column(1)->add_attribute(stc->property_text(), STAGED_COLS.path);

    unstaged_view_.set_model(impl_->unstaged_model_);
    unstaged_view_.set_headers_visible(false);
    auto* uic = Gtk::manage(new Gtk::CellRendererPixbuf());
    unstaged_view_.append_column("", *uic);
    if (auto* c = unstaged_view_.get_column(0)) c->set_fixed_width(20);
    auto* utc = Gtk::manage(new Gtk::CellRendererText());
    unstaged_view_.append_column("Working Tree", *utc);
    unstaged_view_.get_column(1)->set_expand(true);
    unstaged_view_.get_column(1)->add_attribute(utc->property_text(), STAGED_COLS.path);

    auto* split = Gtk::make_managed<Gtk::Paned>(Gtk::ORIENTATION_HORIZONTAL);
    split->set_position(340);
    auto* staged_sw   = Gtk::make_managed<Gtk::ScrolledWindow>();
    auto* unstaged_sw = Gtk::make_managed<Gtk::ScrolledWindow>();
    staged_sw->add(staged_view_);
    unstaged_sw->add(unstaged_view_);
    split->add1(*staged_sw);
    split->add2(*unstaged_sw);
    box->pack_start(*split, false, false, 6);

    // ── Checkboxes ───────────────────────────────────────────────────────────
    amend_check_.set_label("Amend last commit");
    signoff_check_.set_label("Sign off (--signoff)");
    gpg_sign_check_.set_label("GPG sign (--gpg-sign)");

    // GPG key field: only enabled when GPG sign is checked
    gpg_key_entry_.set_placeholder_text("Key ID or default");
    gpg_key_entry_.set_sensitive(false);
    gpg_sign_check_.signal_toggled().connect([&] {
        gpg_key_entry_.set_sensitive(gpg_sign_check_.get_active());
    });

    auto* checks = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL, 12);
    checks->pack_start(amend_check_,     false, false, 0);
    checks->pack_start(signoff_check_,   false, false, 0);
    checks->pack_start(gpg_sign_check_,  false, false, 0);
    checks->pack_start(gpg_key_entry_,   true,  true,  0);
    box->pack_start(*checks, false, false, 0);

    // ── Response signal ──────────────────────────────────────────────────────
    signal_response().connect([&](int resp) {
        if (resp == Gtk::RESPONSE_OK) {
            auto buf = message_view_.get_buffer();
            std::string msg = buf ? buf->get_text() : "";
            signal_commit_ready_.emit(
                msg,
                amend_check_.get_active(),
                signoff_check_.get_active(),
                gpg_sign_check_.get_active(),
                gpg_key_entry_.get_text()
            );
        }
    });

    add_button("_Commit", Gtk::RESPONSE_OK);
    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    get_content_area()->pack_start(*box);

    // Monospace for commit message
    ui::set_monospace(message_view_);

    show_all();
}

CommitDialog::~CommitDialog() = default;

void CommitDialog::set_status(const std::vector<RepoStatus>& statuses) {
    impl_->populate_staged(statuses);
    impl_->populate_unstaged(statuses);
}

std::string CommitDialog::message()  const {
    auto buf = message_view_.get_buffer();
    return buf ? buf->get_text() : "";
}
bool        CommitDialog::amend()    const { return amend_check_.get_active(); }
bool        CommitDialog::signoff()  const { return signoff_check_.get_active(); }
bool        CommitDialog::gpg_sign() const { return gpg_sign_check_.get_active(); }
std::string CommitDialog::gpg_key()  const { return gpg_key_entry_.get_text(); }

sigc::signal<void(const std::string&, bool, bool, bool, const std::string&)>&
CommitDialog::signal_commit_ready() {
    return signal_commit_ready_;
}
