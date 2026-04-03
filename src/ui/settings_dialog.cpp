#include "settings_dialog.hpp"

SettingsDialog::SettingsDialog(Gtk::Window& parent)
    : Gtk::Dialog("Settings", parent, true)
{
    auto box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL, 8);
    box->set_border_width(12);

    diff_tool_entry_.set_placeholder_text("git difftool");
    ssh_key_entry_.set_placeholder_text("~/.ssh/id_ed25519");

    auto grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(8);
    grid->set_column_spacing(8);
    grid->attach(*Gtk::make_managed<Gtk::Label>("Diff tool:"), 0, 0, 1, 1);
    grid->attach(diff_tool_entry_, 1, 0, 1, 1);
    grid->attach(*Gtk::make_managed<Gtk::Label>("SSH key:"), 0, 1, 1, 1);
    grid->attach(ssh_key_entry_, 1, 1, 1, 1);
    grid->attach(use_cred_helper_, 1, 2, 1, 1);
    grid->attach(follow_system_theme_, 1, 3, 1, 1);

    use_cred_helper_.set_label("Use git credential helper");
    follow_system_theme_.set_label("Follow system theme");

    box->pack_start(*grid);
    add_button("_Save", Gtk::RESPONSE_OK);
    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    get_content_area()->pack_start(*box);
    show_all();
}

std::string SettingsDialog::diff_tool_path() const   { return diff_tool_entry_.get_text(); }
std::string SettingsDialog::default_ssh_key() const   { return ssh_key_entry_.get_text(); }
bool        SettingsDialog::use_credential_helper() const { return use_cred_helper_.get_active(); }
