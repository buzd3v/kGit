#include "settings_dialog.hpp"
#include "common.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace {
    std::string config_path() {
        std::string home = g_get_home_dir() ? g_get_home_dir() : "";
        if (home.empty()) home = ".";
        return home + "/.config/kgit/settings.conf";
    }

    void ensure_config_dir() {
        std::string home = g_get_home_dir() ? g_get_home_dir() : "";
        if (home.empty()) home = ".";
        std::string dir = home + "/.config/kgit";
        mkdir(dir.c_str(), 0755);
    }
}

// ─── Settings struct ──────────────────────────────────────────────────────────

Settings SettingsDialog::load() {
    Settings s;
    std::ifstream f(config_path());
    if (!f) return s;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "diff_tool")    s.diff_tool_path = val;
        else if (key == "ssh_key") s.ssh_key_path = val;
        else if (key == "use_cred_helper") s.use_cred_helper = (val == "true");
        else if (key == "gpg_key") s.gpg_key_path = val;
        else if (key == "theme")   s.theme = static_cast<Theme>(std::stoi(val));
        else if (key == "follow_system_theme") s.follow_system_theme = (val == "true");
    }
    return s;
}

void SettingsDialog::save(const Settings& s) {
    ensure_config_dir();
    std::ofstream f(config_path());
    if (!f) return;
    f << "diff_tool=" << s.diff_tool_path << "\n";
    f << "ssh_key=" << s.ssh_key_path << "\n";
    f << "use_cred_helper=" << (s.use_cred_helper ? "true" : "false") << "\n";
    f << "gpg_key=" << s.gpg_key_path << "\n";
    f << "theme=" << static_cast<int>(s.theme) << "\n";
    f << "follow_system_theme=" << (s.follow_system_theme ? "true" : "false") << "\n";
}

// ─── SettingsDialog ────────────────────────────────────────────────────────────

SettingsDialog::SettingsDialog(Gtk::Window& parent)
    : Gtk::Dialog("Settings", parent, true)
{
    set_default_size(480, 320);
    auto box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL, 8);
    box->set_border_width(12);

    diff_tool_entry_.set_placeholder_text("git difftool");
    ssh_key_entry_.set_placeholder_text("~/.ssh/id_ed25519");
    gpg_key_entry_.set_placeholder_text("GPG key ID (default key if blank)");

    // Theme selector
    theme_combo_.append("0", "Follow System");
    theme_combo_.append("1", "Light");
    theme_combo_.append("2", "Dark");
    theme_combo_.set_active_id("0");

    auto grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(8);
    grid->set_column_spacing(8);

    grid->attach(*Gtk::make_managed<Gtk::Label>("Diff tool:"),         0, 0, 1, 1);
    grid->attach(diff_tool_entry_,                                        1, 0, 1, 1);
    grid->attach(*Gtk::make_managed<Gtk::Label>("SSH key:"),            0, 1, 1, 1);
    grid->attach(ssh_key_entry_,                                         1, 1, 1, 1);
    grid->attach(*Gtk::make_managed<Gtk::Label>("GPG signing key:"),    0, 2, 1, 1);
    grid->attach(gpg_key_entry_,                                         1, 2, 1, 1);
    grid->attach(*Gtk::make_managed<Gtk::Label>("Theme:"),              0, 3, 1, 1);
    grid->attach(theme_combo_,                                           1, 3, 1, 1);
    grid->attach(use_cred_helper_,                                       0, 4, 2, 1);
    grid->attach(follow_system_theme_,                                   0, 5, 2, 1);

    use_cred_helper_.set_label("Use git credential helper");
    follow_system_theme_.set_label("Follow system theme");

    auto* btn_bar = Gtk::make_managed<Gtk::ButtonBox>(Gtk::ORIENTATION_HORIZONTAL);
    auto* reset_btn = Gtk::make_managed<Gtk::Button>("Reset to Defaults");
    btn_bar->add(*reset_btn);
    btn_bar->add(*Gtk::make_managed<Gtk::Button>("_Cancel"));
    btn_bar->add(*Gtk::make_managed<Gtk::Button>("_Save"));
    btn_bar->set_layout(Gtk::BUTTONBOX_END);

    reset_btn->signal_clicked().connect([&] {
        diff_tool_entry_.set_text("");
        ssh_key_entry_.set_text("");
        gpg_key_entry_.set_text("");
        theme_combo_.set_active_id("0");
        use_cred_helper_.set_active(false);
        follow_system_theme_.set_active(true);
    });

    box->pack_start(*grid);
    box->pack_start(*btn_bar, false, false, 0);

    // Load saved settings
    auto s = load();
    if (!s.diff_tool_path.empty()) diff_tool_entry_.set_text(s.diff_tool_path);
    if (!s.ssh_key_path.empty())    ssh_key_entry_.set_text(s.ssh_key_path);
    if (!s.gpg_key_path.empty())   gpg_key_entry_.set_text(s.gpg_key_path);
    theme_combo_.set_active_id(std::to_string(static_cast<int>(s.theme)));
    use_cred_helper_.set_active(s.use_cred_helper);
    follow_system_theme_.set_active(s.follow_system_theme);

    add_button("_Save", Gtk::RESPONSE_OK);
    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    get_content_area()->pack_start(*box);

    signal_response().connect([&](int resp) {
        if (resp == Gtk::RESPONSE_OK) {
            Settings s;
            s.diff_tool_path   = diff_tool_entry_.get_text();
            s.ssh_key_path     = ssh_key_entry_.get_text();
            s.gpg_key_path     = gpg_key_entry_.get_text();
            s.use_cred_helper  = use_cred_helper_.get_active();
            s.follow_system_theme = follow_system_theme_.get_active();
            s.theme = static_cast<Theme>(theme_combo_.get_active_id().empty() ? 0
                                                    : std::stoi(theme_combo_.get_active_id()));
            save(s);
            signal_settings_changed_.emit(s);
        }
    });

    show_all();
}

SettingsDialog::~SettingsDialog() = default;

std::string SettingsDialog::diff_tool_path() const   { return diff_tool_entry_.get_text(); }
std::string SettingsDialog::default_ssh_key() const { return ssh_key_entry_.get_text(); }
bool        SettingsDialog::use_credential_helper() const { return use_cred_helper_.get_active(); }
bool        SettingsDialog::follow_system_theme() const { return follow_system_theme_.get_active(); }
