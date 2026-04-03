#include "clone_dialog.hpp"
#include "../git_engine.hpp"
#include <fstream>
#include <sstream>
#include <dirent.h>

namespace {
    std::vector<std::string> scan_ssh_keys() {
        std::vector<std::string> keys;
        std::string home = g_get_home_dir() ? g_get_home_dir() : "";
        if (home.empty()) return keys;
        std::string ssh_dir = home + "/.ssh";
        DIR* d = opendir(ssh_dir.c_str());
        if (!d) return keys;
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr) {
            std::string name = ent->d_name;
            if (name == "." || name == "..") continue;
            if (name == "id_rsa" || name == "id_ed25519" || name == "id_ecdsa" ||
                name == "id_ed25519_sk" || name == "id_rsa" ||
                (name.rfind("id_", 0) == 0 && name.find(".pub") != std::string::npos)) {
                keys.push_back(name);
            }
        }
        closedir(d);
        return keys;
    }

    bool is_ssh_url(const std::string& url) {
        return url.rfind("ssh://", 0) == 0 ||
               url.rfind("git@", 0) == 0 ||
               url.rfind("git://", 0) == 0;
    }

    bool is_https_url(const std::string& url) {
        return url.rfind("https://", 0) == 0 ||
               url.rfind("http://", 0) == 0;
    }
}

// ─── CloneDialog::Impl ────────────────────────────────────────────────────────

struct CloneDialog::Impl {
    GitEngine& engine_;

    Gtk::Entry& url_entry_;
    Gtk::Entry& target_entry_;
    Gtk::Entry& branch_entry_;
    Gtk::CheckButton& bare_check_;

    Gtk::ComboBoxText ssh_key_combo_;
    Gtk::Entry        username_entry_;
    Gtk::Entry        password_entry_;
    Gtk::CheckButton  bare_repo_check_;
    Gtk::ProgressBar  progress_bar_;
    Gtk::Label        cred_lbl_;

    Impl(GitEngine& engine,
         Gtk::Entry& url, Gtk::Entry& target, Gtk::Entry& branch,
         Gtk::CheckButton& bare)
        : engine_(engine), url_entry_(url), target_entry_(target),
          branch_entry_(branch), bare_check_(bare)
    {
        // Populate SSH key combo
        ssh_key_combo_.append("(default)", "(default)");
        for (const auto& k : scan_ssh_keys())
            ssh_key_combo_.set_active_id("~/.ssh/" + k);

        username_entry_.set_placeholder_text("username");
        password_entry_.set_placeholder_text("password");
        password_entry_.set_visibility(false);

        // Wire URL change → toggle credential visibility
        url_entry_.signal_changed().connect([&] {
            std::string url = url_entry_.get_text();
            bool ssh = is_ssh_url(url);
            bool https = is_https_url(url);
            ssh_key_combo_.set_visible(ssh);
            username_entry_.set_visible(https || ssh);
            password_entry_.set_visible(https);
            cred_lbl_.set_visible(https || ssh);
        });
    }
};

// ─── CloneDialog ───────────────────────────────────────────────────────────────

CloneDialog::CloneDialog(Gtk::Window& parent, GitEngine& engine)
    : Gtk::Dialog("Clone Repository", parent, true)
    , engine_(engine)
    , impl_(std::make_unique<Impl>(engine, url_entry_, target_entry_,
                                   branch_entry_, bare_check_))
{
    set_default_size(550, 400);
    auto box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL, 8);
    box->set_border_width(12);

    auto grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(8);
    grid->set_column_spacing(8);

    // URL row
    grid->attach(*Gtk::make_managed<Gtk::Label>("Repository URL:"), 0, 0, 1, 1);
    url_entry_.set_placeholder_text("https://github.com/user/repo.git");
    grid->attach(url_entry_, 1, 0, 1, 1);

    // Target dir row
    grid->attach(*Gtk::make_managed<Gtk::Label>("Target directory:"), 0, 1, 1, 1);
    target_entry_.set_placeholder_text(".");
    grid->attach(target_entry_, 1, 1, 1, 1);

    // Branch row
    grid->attach(*Gtk::make_managed<Gtk::Label>("Branch (optional):"), 0, 2, 1, 1);
    branch_entry_.set_placeholder_text("(default branch)");
    grid->attach(branch_entry_, 1, 2, 1, 1);

    // SSH key row (visible for ssh:// URLs)
    impl_->cred_lbl_.set_text("SSH Key:");
    impl_->cred_lbl_.set_visible(false);
    impl_->ssh_key_combo_.set_visible(false);
    grid->attach(impl_->cred_lbl_, 0, 3, 1, 1);
    grid->attach(impl_->ssh_key_combo_, 1, 3, 1, 1);

    // Credentials row (for HTTPS)
    impl_->username_entry_.set_visible(false);
    impl_->password_entry_.set_visible(false);
    grid->attach(*Gtk::make_managed<Gtk::Label>("Username:"), 0, 4, 1, 1);
    grid->attach(impl_->username_entry_, 1, 4, 1, 1);
    grid->attach(*Gtk::make_managed<Gtk::Label>("Password:"), 0, 5, 1, 1);
    grid->attach(impl_->password_entry_, 1, 5, 1, 1);

    // Bare repo
    bare_check_.set_label("Bare repository");
    bare_check_.set_visible(true);
    grid->attach(bare_check_, 1, 6, 1, 1);

    // Progress bar
    impl_->progress_bar_.set_visible(false);
    impl_->progress_bar_.set_text("Cloning…");
    box->pack_start(*grid);
    box->pack_start(impl_->progress_bar_, false, false, 0);

    add_button("_Clone", Gtk::RESPONSE_OK);
    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    get_content_area()->pack_start(*box);
    show_all();
}

CloneDialog::~CloneDialog() = default;

std::string CloneDialog::url()        const { return url_entry_.get_text(); }
std::string CloneDialog::target_dir() const { return target_entry_.get_text(); }
std::string CloneDialog::branch()     const { return branch_entry_.get_text(); }
bool        CloneDialog::bare()       const { return bare_check_.get_active(); }

std::string CloneDialog::ssh_key() const {
    std::string id = impl_->ssh_key_combo_.get_active_id();
    if (id.empty() || id == "(default)") return {};
    return id;
}

std::string CloneDialog::username() const { return impl_->username_entry_.get_text(); }
std::string CloneDialog::password() const { return impl_->password_entry_.get_text(); }

void CloneDialog::show_progress(bool show) {
    impl_->progress_bar_.set_visible(show);
}

void CloneDialog::set_progress(double fraction) {
    impl_->progress_bar_.set_fraction(fraction);
}
