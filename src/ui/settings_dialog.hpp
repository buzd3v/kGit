#pragma once

#include <gtkmm.h>

class SettingsDialog : public Gtk::Dialog {
public:
    SettingsDialog(Gtk::Window& parent);

    std::string diff_tool_path() const;
    std::string default_ssh_key() const;
    bool use_credential_helper() const;

private:
    Gtk::Entry diff_tool_entry_;
    Gtk::Entry ssh_key_entry_;
    Gtk::CheckButton use_cred_helper_;
    Gtk::CheckButton follow_system_theme_;
};
