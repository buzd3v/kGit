#pragma once

#include <gtkmm.h>
#include <string>

enum class Theme { System = 0, Light = 1, Dark = 2 };

struct Settings {
    std::string diff_tool_path;
    std::string ssh_key_path;
    std::string gpg_key_path;
    bool use_cred_helper = false;
    bool follow_system_theme = true;
    Theme theme = Theme::System;
};

class SettingsDialog : public Gtk::Dialog {
public:
    explicit SettingsDialog(Gtk::Window& parent);
    ~SettingsDialog();

    std::string diff_tool_path() const;
    std::string default_ssh_key() const;
    bool use_credential_helper() const;
    bool follow_system_theme() const;

    /// Load settings from ~/.config/kgit/settings.conf
    static Settings load();
    /// Save settings to ~/.config/kgit/settings.conf (INI format)
    static void save(const Settings& s);

    sigc::signal<void(const Settings&)>& signal_settings_changed() { return signal_settings_changed_; }

private:
    Gtk::Entry         diff_tool_entry_;
    Gtk::Entry         ssh_key_entry_;
    Gtk::Entry         gpg_key_entry_;
    Gtk::ComboBoxText  theme_combo_;
    Gtk::CheckButton   use_cred_helper_;
    Gtk::CheckButton   follow_system_theme_;

    sigc::signal<void(const Settings&)> signal_settings_changed_;
};
