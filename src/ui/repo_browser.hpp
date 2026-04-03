#pragma once

#include <gtkmm.h>
#include <memory>
#include <string>

class GitEngine;

class RepoBrowser : public Gtk::Box {
public:
    explicit RepoBrowser(GitEngine& engine);
    ~RepoBrowser();

    /// Emitted when user activates (double-click or View File) a file blob.
    sigc::signal<void(const std::string& path, const std::string& content)>&
    signal_open_file();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
