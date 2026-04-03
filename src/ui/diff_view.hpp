#pragma once

#include <gtkmm.h>
#include <string>
#include <vector>

class DiffView : public Gtk::ScrolledWindow {
public:
    DiffView();
    void set_diff(const std::vector<struct DiffFile>& files);
    void clear();

private:
    Gtk::TextView text_view_;
};
