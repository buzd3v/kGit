#pragma once
// Common includes and helpers shared across all UI components.

#include <gtkmm.h>
#include <string>
#include <vector>
#include <map>

// Icon helpers for status types
namespace icons {
    Gtk::Image* status_icon(const std::string& status);
}

// Shared styling helpers
namespace ui {
    void apply_dark_theme(Gtk::Widget& w);
    void set_monospace(Gtk::Widget& w);
}
