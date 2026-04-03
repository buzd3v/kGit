#pragma once

#include <gtkmm.h>
#include <memory>
#include <vector>

class GitEngine;
struct RepoStatus;

class CommitDialog : public Gtk::Dialog {
public:
    CommitDialog(Gtk::Window& parent, GitEngine& engine);
    ~CommitDialog();

    void set_status(const std::vector<RepoStatus>& statuses);

    std::string message() const;
    bool        amend()   const;
    bool        signoff()  const;
    bool        gpg_sign() const;
    std::string gpg_key()  const;

    sigc::signal<void(const std::string& msg, bool amend, bool signoff,
                      bool gpg_sign, const std::string& gpg_key)>&
    signal_commit_ready();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    GitEngine& engine_;
    Gtk::TextView message_view_;

    // Owned by CommitDialog (not Impl) so Impl's constructor can reference them.
    Gtk::TreeView   staged_view_;
    Gtk::TreeView   unstaged_view_;
    Gtk::CheckButton amend_check_;
    Gtk::CheckButton signoff_check_;
    Gtk::CheckButton gpg_sign_check_;
    Gtk::Entry       gpg_key_entry_;

    sigc::signal<void(const std::string& msg, bool amend, bool signoff,
                      bool gpg_sign, const std::string& gpg_key)> signal_commit_ready_;
};
