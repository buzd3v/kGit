#pragma once
// Common includes and helpers shared across all UI components.

#include <gtkmm.h>
#include <string>
#include <vector>
#include <map>

// ─── Icon helpers ───────────────────────────────────────────────────────────

namespace icons {
    /// Returns a 16×16 solid-color pixbuf for a git status string.
    Glib::RefPtr<Gdk::Pixbuf> status_icon(const std::string& status);

    /// Returns a generic file icon.
    Glib::RefPtr<Gdk::Pixbuf> file_icon(const std::string& name);

    /// Returns a folder icon.
    Glib::RefPtr<Gdk::Pixbuf> folder_icon();

    /// Map RepoStatus::Change to a status string for status_icon().
    inline std::string change_to_string(int change) {
        switch (change) {
            case 1:  return "added";      // GIT_STATUS_INDEX_NEW
            case 2:  return "deleted";    // GIT_STATUS_INDEX_DELETED
            case 4:  return "modified";  // GIT_STATUS_INDEX_MODIFIED
            case 8:  return "renamed";   // GIT_STATUS_INDEX_RENAMED
            case 16: return "copied";    // GIT_STATUS_INDEX_TYPECHANGE
            case 32: return "untracked";  // GIT_STATUS_WT_NEW
            case 64: return "deleted";    // GIT_STATUS_WT_DELETED
            case 128: return "modified";  // GIT_STATUS_WT_MODIFIED
            case 256: return "renamed";   // GIT_STATUS_WT_RENAMED
            case 512: return "copied";    // GIT_STATUS_WT_TYPECHANGE
            case 768: return "conflicted"; // GIT_STATUS_CONFLICTED
            default: return "untracked";
        }
    }
}

// ─── Shared styling helpers ─────────────────────────────────────────────────

namespace ui {
    /// Apply or remove GTK dark theme.
    void apply_dark_theme(bool dark);

    /// Set a widget's font to monospace.
    void set_monospace(Gtk::Widget& w);
    void set_monospace(Gtk::TextView& tv);

    /// Convert a relative time_t to a human-readable string.
    std::string format_relative_time(std::time_t t);
}
