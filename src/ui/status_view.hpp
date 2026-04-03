#pragma once

#include <gtkmm.h>
#include <vector>
#include <memory>

struct RepoStatus;

class StatusView : public Gtk::ScrolledWindow {
public:
    StatusView();

    void set_status(const std::vector<RepoStatus>& statuses);
    void clear();

    /// Emitted when user wants to stage/unstage or revert a file.
    sigc::signal<void(const std::string& path, bool staged)>& signal_file_toggled();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
