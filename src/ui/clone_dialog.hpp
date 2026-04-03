#pragma once

#include <gtkmm.h>
#include <memory>

class GitEngine;

class CloneDialog : public Gtk::Dialog {
public:
    CloneDialog(Gtk::Window& parent, GitEngine& engine);
    ~CloneDialog();

    std::string url()        const;
    std::string target_dir() const;
    std::string branch()     const;
    bool        bare()       const;
    std::string ssh_key()   const;
    std::string username()  const;
    std::string password()  const;

    /// Show or hide the progress bar.
    void show_progress(bool show);
    void set_progress(double fraction);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    GitEngine& engine_;
    Gtk::Entry url_entry_;
    Gtk::Entry target_entry_;
    Gtk::Entry branch_entry_;
    Gtk::CheckButton bare_check_;
};
