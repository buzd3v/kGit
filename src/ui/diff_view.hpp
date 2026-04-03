#pragma once

#include <gtkmm.h>
#include <memory>
#include <vector>

struct DiffFile;

class DiffView : public Gtk::Box {
public:
    DiffView();
    void set_diff(const std::vector<DiffFile>& files);
    void clear();

    /// Emitted when user clicks "Revert" for a file.
    sigc::signal<void(const std::string& path)>& signal_file_revert();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
