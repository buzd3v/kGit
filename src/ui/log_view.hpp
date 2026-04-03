#pragma once

#include <gtkmm.h>
#include <memory>
#include <vector>

struct CommitInfo;

class LogView : public Gtk::ScrolledWindow {
public:
    LogView();

    void set_commits(const std::vector<CommitInfo>& commits);
    void clear();

    sigc::signal<void(const std::string& commit_id)>& signal_commit_selected();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Gtk::TreeView tree_view_;
};
