#pragma once

#include <gtkmm.h>
#include <vector>

struct CommitInfo;

class LogView : public Gtk::ScrolledWindow {
public:
    LogView();

    void set_commits(const std::vector<CommitInfo>& commits);
    void clear();

    sigc::signal<void(const std::string& commit_id)>& signal_commit_selected() {
        return signal_commit_selected_;
    }

private:
    Gtk::TreeView tree_view_;
    Glib::RefPtr<Gtk::ListStore> model_;
    sigc::signal<void(const std::string&)> signal_commit_selected_;
};
