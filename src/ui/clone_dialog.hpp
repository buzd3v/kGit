#pragma once

#include <gtkmm.h>

class GitEngine;

class CloneDialog : public Gtk::Dialog {
public:
    CloneDialog(Gtk::Window& parent, GitEngine& engine);

    std::string url() const;
    std::string target_dir() const;
    std::string branch() const;

private:
    Gtk::Entry url_entry_;
    Gtk::Entry target_entry_;
    Gtk::Entry branch_entry_;
    Gtk::CheckButton bare_check_;
};
