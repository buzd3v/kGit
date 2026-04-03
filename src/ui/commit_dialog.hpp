#pragma once

#include <gtkmm.h>
#include <memory>

class GitEngine;

class CommitDialog : public Gtk::Dialog {
public:
    CommitDialog(Gtk::Window& parent, GitEngine& engine);

    std::string message() const;
    bool amend() const;

private:
    GitEngine& engine_;
    Gtk::TextView message_view_;
    Gtk::ScrolledWindow staged_scroll_, unstaged_scroll_;
    Gtk::TreeView staged_view_, unstaged_view_;
    Gtk::CheckButton amend_check_;
    Gtk::Button stage_btn_, unstage_btn_;
};
