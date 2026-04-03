#pragma once

#include <gtkmm.h>
#include <vector>

struct RepoStatus;

class StatusView : public Gtk::ScrolledWindow {
public:
    StatusView();

    void set_status(const std::vector<RepoStatus>& statuses);
    void clear();

    sigc::signal<void(const std::string& path, bool staged)>& signal_file_toggled() {
        return signal_file_toggled_;
    }

private:
    Gtk::TreeView tree_view_;
    sigc::signal<void(const std::string&, bool)> signal_file_toggled_;
};
