#include "commit_dialog.hpp"
#include "../git_engine.hpp"

CommitDialog::CommitDialog(Gtk::Window& parent, GitEngine& engine)
    : Gtk::Dialog("Commit", parent, true)
    , engine_(engine)
{
    set_default_size(700, 500);
    auto box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL, 6);
    box->set_border_width(12);

    // Message area
    auto lbl = Gtk::make_managed<Gtk::Label>("Commit message:");
    message_view_.set_hexpand(true);
    message_view_.set_vexpand(true);
    message_view_.set_wrap_mode(Gtk::WRAP_WORD);
    auto msg_scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    msg_scroll->add(message_view_);
    msg_scroll->set_hexpand(true);
    msg_scroll->set_vexpand(true);

    // Staged / unstaged lists
    auto split = Gtk::make_managed<Gtk::Paned>(Gtk::ORIENTATION_HORIZONTAL);
    split->add1(*Gtk::make_managed<Gtk::Label>("Staged"));
    split->add2(*Gtk::make_managed<Gtk::Label>("Unstaged"));
    split->set_position(350);

    box->pack_start(*lbl,        false, false, 0);
    box->pack_start(*msg_scroll, true,  true,  0);
    box->pack_start(*split,      false, false, 6);
    box->pack_start(amend_check_,false, false, 0);

    add_button("_Commit", Gtk::RESPONSE_OK);
    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    get_content_area()->pack_start(*box);
    show_all();
}

std::string CommitDialog::message() const {
    auto buf = message_view_.get_buffer();
    return buf ? buf->get_text() : "";
}
bool CommitDialog::amend() const { return amend_check_.get_active(); }
