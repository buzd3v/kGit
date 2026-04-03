#include "main_window.hpp"

#include "../git_engine.hpp"
#include "../utils/diff_tool.hpp"
#include "clone_dialog.hpp"
#include "commit_dialog.hpp"
#include "settings_dialog.hpp"
#include "repo_browser.hpp"
#include "common.hpp"

#include <giomm/menu.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace {

// ─── Icon cache ──────────────────────────────────────────────────────────────

Glib::RefPtr<Gdk::Pixbuf> cached_icon(const std::string& key)
{
    static std::map<std::string, Glib::RefPtr<Gdk::Pixbuf>> cache;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    static const struct { const char* k; int r, g, b; } icons[] = {
        {"added",      34, 197,  94},
        {"modified",   59, 130, 246},
        {"deleted",   239,  68,  68},
        {"renamed",   168,  85, 247},
        {"untracked", 156, 163, 175},
        {"conflicted",249, 115,  22},
    };

    for (auto& ic : icons) {
        if (key == ic.k) {
            std::vector<uint8_t> pixels(16 * 16 * 3);
            for (int i = 0; i < 16 * 16; ++i) {
                pixels[i*3+0] = static_cast<uint8_t>(ic.r);
                pixels[i*3+1] = static_cast<uint8_t>(ic.g);
                pixels[i*3+2] = static_cast<uint8_t>(ic.b);
            }
            auto pix = Gdk::Pixbuf::create_from_data(
                pixels.data(), Gdk::Colorspace::COLORSPACE_RGB, false, 8, 16, 16, 16*3);
            cache[key] = pix;
            return pix;
        }
    }
    // fallback: gray dot
    std::vector<uint8_t> gray(16 * 16 * 3, 156);
    auto pix = Gdk::Pixbuf::create_from_data(
        gray.data(), Gdk::Colorspace::COLORSPACE_RGB, false, 8, 16, 16, 16*3);
    cache[key] = pix;
    return pix;
}

} // anonymous namespace

// ─── MainWindow ─────────────────────────────────────────────────────────────

MainWindow::MainWindow(const std::string& repo_path)
{
    set_default_size(1100, 700);
    build_ui();
    if (!repo_path.empty()) load_repo(repo_path);
}

MainWindow::~MainWindow() = default;

// ─── UI Construction ──────────────────────────────────────────────────────────

void MainWindow::build_ui()
{
    set_title("kGit");

    auto header = Gtk::manage(new Gtk::HeaderBar());
    header->set_show_close_button(true);
    header->set_title("kGit");
    repo_label_.set_xalign(0);
    repo_label_.set_ellipsize(Pango::ELLIPSIZE_MIDDLE);
    branch_label_.set_xalign(1);
    header->pack_start(repo_label_);
    header->pack_end(branch_label_);
    set_titlebar(*header);

    toolbar_.set_toolbar_style(Gtk::TOOLBAR_BOTH_HORIZ);
    btn_clone_tool_.set_tooltip_text("Clone a repository");
    btn_commit_tool_.set_tooltip_text("Commit staged changes");
    btn_push_.set_tooltip_text("Push to remote");
    btn_pull_.set_tooltip_text("Pull from remote");
    btn_fetch_.set_tooltip_text("Fetch from remote");
    btn_merge_.set_tooltip_text("Merge branches");
    btn_branch_.set_tooltip_text("Create new branch");
    btn_refresh_.set_tooltip_text("Refresh");

    btn_clone_tool_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_clone));
    btn_commit_tool_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_commit));
    btn_push_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_push));
    btn_pull_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_pull));
    btn_fetch_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_fetch));
    btn_merge_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_merge));
    btn_branch_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_new_branch));
    btn_refresh_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_refresh));

    toolbar_.add(btn_clone_tool_);
    toolbar_.add(btn_commit_tool_);
    toolbar_.add(btn_push_);
    toolbar_.add(btn_pull_);
    toolbar_.add(btn_fetch_);
    toolbar_.add(btn_merge_);
    toolbar_.add(btn_branch_);
    toolbar_.add(btn_refresh_);
    toolbar_.add(btn_settings_);
    btn_settings_.set_tooltip_text("Settings");
    btn_settings_.signal_clicked().connect([this] {
        SettingsDialog dlg(*this);
        dlg.signal_settings_changed().connect([&](const Settings& s) {
            // Apply theme preference
            switch (s.theme) {
                case Theme::Light:  ui::apply_dark_theme(false); break;
                case Theme::Dark:   ui::apply_dark_theme(true);  break;
                case Theme::System: /* follow system — already default */ break;
            }
        });
        dlg.run();
    });

    main_box_.set_hexpand(true);
    main_box_.set_vexpand(true);
    main_box_.pack_start(toolbar_, false, false, 0);
    main_box_.pack_start(paned_,   true,  true,  0);

    setup_file_tree();
    setup_changes_panel();
    setup_log_panel();
    setup_diff_panel();
    setup_branch_panel();
    // Repo Browser tab — added once engine_ is available in load_repo()
    // (setup_repo_browser_panel() called from load_repo after engine_ exists)

    paned_.pack1(left_box_, true, false);
    paned_.pack2(notebook_, true, false);
    paned_.set_position(280);

    add(main_box_);
    show_all();
}

// ─── File Tree ───────────────────────────────────────────────────────────────

void MainWindow::setup_file_tree()
{
    file_model_ = Gtk::ListStore::create(cols_);
    file_tree_.set_model(file_model_);
    file_tree_.set_headers_visible(false);

    auto* icon_col = Gtk::manage(new Gtk::TreeView::Column());
    icon_col->pack_start(cols_.col_icon, false);
    icon_col->set_fixed_width(24);

    auto* name_col = Gtk::manage(new Gtk::TreeView::Column("File"));
    name_col->pack_start(cols_.col_name, true);

    file_tree_.append_column(*icon_col);
    file_tree_.append_column(*name_col);
    file_tree_.set_search_column(cols_.col_name);
    file_tree_.set_activate_on_single_click(true);

    file_scroll_.add(file_tree_);
    left_box_.pack_start(file_scroll_, true, true, 0);

    file_tree_.signal_row_activated().connect(
        [this](const Gtk::TreeModel::Path&, Gtk::TreeViewColumn*) {
            select_file_for_diff();
        });

    auto menu = Gtk::make_managed<Gtk::Menu>();
    Gtk::MenuItem diff_item("Show Diff");
    Gtk::MenuItem revert_item("Revert Changes");
    Gtk::MenuItem rm_item("Delete File");
    diff_item.signal_activate().connect([this]{ select_file_for_diff(); });
    revert_item.signal_activate().connect([this]{ on_revert_selected(); });
    rm_item.signal_activate().connect([this]{ on_delete_selected(); });
    menu->append(diff_item);
    menu->append(revert_item);
    menu->append(rm_item);
    menu->show_all();

    file_tree_.signal_button_press_event().connect(
        [menu](GdkEventButton* ev) -> bool {
            if (ev->button == 3) { menu->popup_at_pointer(reinterpret_cast<GdkEvent*>(ev)); return true; }
            return false;
        });
}

// ─── Changes Panel ───────────────────────────────────────────────────────────

void MainWindow::setup_changes_panel()
{
    staged_model_   = Gtk::ListStore::create(cols_);
    unstaged_model_ = Gtk::ListStore::create(cols_);

    staged_view_.set_model(staged_model_);
    staged_view_.set_headers_visible(false);
    staged_view_.append_column("", cols_.col_icon);
    if (auto* c = staged_view_.get_column(0)) c->set_fixed_width(20);
    staged_view_.append_column("Staged", cols_.col_name);
    if (auto* c = staged_view_.get_column(1)) c->set_expand(true);

    unstaged_view_.set_model(unstaged_model_);
    unstaged_view_.set_headers_visible(false);
    unstaged_view_.append_column("", cols_.col_icon);
    if (auto* c = unstaged_view_.get_column(0)) c->set_fixed_width(20);
    unstaged_view_.append_column("Working Tree", cols_.col_name);
    if (auto* c = unstaged_view_.get_column(1)) c->set_expand(true);

    unstaged_view_.signal_row_activated().connect(
        [this](const Gtk::TreeModel::Path&, Gtk::TreeViewColumn*) { on_stage_selected(); });
    staged_view_.signal_row_activated().connect(
        [this](const Gtk::TreeModel::Path&, Gtk::TreeViewColumn*) { on_unstage_selected(); });

    btn_stage_all_.set_label("Stage All");
    btn_stage_selected_.set_label("Stage");
    btn_unstage_selected_.set_label("Unstage");
    btn_commit_.set_label("Commit");
    btn_stage_all_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_stage_all));
    btn_stage_selected_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_stage_selected));
    btn_unstage_selected_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_unstage_selected));
    btn_commit_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_commit));

    auto* split = Gtk::manage(new Gtk::Paned(Gtk::ORIENTATION_VERTICAL));
    auto* staged_sw   = Gtk::manage(new Gtk::ScrolledWindow());
    auto* unstaged_sw = Gtk::manage(new Gtk::ScrolledWindow());
    staged_sw->add(staged_view_);
    unstaged_sw->add(unstaged_view_);
    split->pack1(*staged_sw,    true, false);
    split->pack2(*unstaged_sw,  true, false);
    split->set_position(200);

    auto* staged_lbl    = Gtk::manage(new Gtk::Label("<b>Staged</b>"));
    auto* unstaged_lbl  = Gtk::manage(new Gtk::Label("<b>Unstaged</b>"));
    staged_lbl->set_use_markup(true);
    unstaged_lbl->set_use_markup(true);

    auto* vbox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 4));
    vbox->set_border_width(8);
    auto* hbox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 4));
    hbox->pack_start(btn_stage_all_,        false, false, 0);
    hbox->pack_start(btn_stage_selected_,  false, false, 0);
    hbox->pack_start(btn_unstage_selected_,false, false, 0);
    hbox->pack_end(btn_commit_, false, false, 0);

    vbox->pack_start(*staged_lbl,   false, false, 0);
    vbox->pack_start(*staged_sw,    true,  true,  0);
    vbox->pack_start(*unstaged_lbl, false, false, 0);
    vbox->pack_start(*unstaged_sw,  true,  true,  0);
    vbox->pack_start(*hbox,         false, false, 0);

    changes_scroll_.add(*vbox);
    notebook_.append_page(changes_scroll_, Glib::ustring("Changes"));
}

// ─── Log Panel ──────────────────────────────────────────────────────────────

void MainWindow::setup_log_panel()
{
    log_model_ = Gtk::ListStore::create(log_cols_);
    log_tree_.set_model(log_model_);
    log_tree_.set_headers_visible(true);

    auto* hash_col   = Gtk::manage(new Gtk::TreeView::Column("Hash"));
    hash_col->pack_start(log_cols_.col_short_hash, false);
    hash_col->set_fixed_width(70);

    auto* author_col = Gtk::manage(new Gtk::TreeView::Column("Author"));
    author_col->pack_start(log_cols_.col_author, true);

    auto* date_col = Gtk::manage(new Gtk::TreeView::Column("Date"));
    date_col->pack_start(log_cols_.col_date, false);
    date_col->set_fixed_width(140);

    auto* msg_col = Gtk::manage(new Gtk::TreeView::Column("Message"));
    msg_col->pack_start(log_cols_.col_message, true);

    log_tree_.append_column(*hash_col);
    log_tree_.append_column(*author_col);
    log_tree_.append_column(*date_col);
    log_tree_.append_column(*msg_col);
    log_tree_.set_search_column(log_cols_.col_message);

    log_tree_.signal_row_activated().connect(
        [this](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*) {
            auto it = log_model_->get_iter(path);
            if (it) {
                Gtk::TreeModel::Row row = *it;
                Glib::ustring hash = row.get_value(log_cols_.col_short_hash);
                show_diff_for_commit(hash);
            }
        });

    log_scroll_.add(log_tree_);
    notebook_.append_page(log_scroll_, Glib::ustring("Log"));
}

// ─── Diff Panel ─────────────────────────────────────────────────────────────

void MainWindow::setup_diff_panel()
{
    diff_view_.set_editable(false);
    diff_view_.set_wrap_mode(Gtk::WRAP_NONE);
    diff_scroll_.add(diff_view_);
    diff_scroll_.set_border_width(4);
    notebook_.append_page(diff_scroll_, Glib::ustring("Diff"));
}

// ─── Branch Panel ─────────────────────────────────────────────────────────────

void MainWindow::setup_branch_panel()
{
    branch_model_ = Gtk::ListStore::create(branch_cols_);
    branch_tree_.set_model(branch_model_);
    branch_tree_.set_headers_visible(false);
    branch_tree_.append_column("Branch / Tag", branch_cols_.col_name);

    Gtk::Button checkout_btn("Checkout"), new_btn("New Branch"), del_btn("Delete");
    checkout_btn.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_branch_checkout));
    new_btn.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_new_branch));
    del_btn.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_branch_delete));

    auto* vbox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 4));
    vbox->set_border_width(8);
    vbox->pack_start(branch_tree_, true, true, 0);
    auto* bb = Gtk::manage(new Gtk::ButtonBox(Gtk::ORIENTATION_HORIZONTAL));
    bb->add(checkout_btn); bb->add(new_btn); bb->add(del_btn);
    vbox->pack_start(*bb, false, false, 0);

    branch_scroll_.add(*vbox);
    notebook_.append_page(branch_scroll_, Glib::ustring("Branches"));
}

void MainWindow::setup_repo_browser_panel()
{
    if (!engine_ || !engine_->repo()) return;
    repo_browser_ = std::make_unique<RepoBrowser>(*engine_);
    repo_browser_->signal_open_file().connect([&](const std::string& path,
                                                   const std::string& content) {
        // Show file content in a dialog.
        auto* dlg = new Gtk::Dialog("File: " + path, *this, true);
        auto* tv = Gtk::make_managed<Gtk::TextView>();
        tv->set_editable(false);
        ui::set_monospace(*tv);
        tv->get_buffer()->set_text(content);
        auto* scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
        scroll->add(*tv);
        dlg->get_content_area()->pack_start(*scroll, true, true, 0);
        dlg->add_button("_Close", Gtk::RESPONSE_CLOSE);
        dlg->set_default_size(700, 500);
        dlg->signal_response().connect([dlg](int) { delete dlg; });
        dlg->show_all();
    });
    notebook_.append_page(*repo_browser_, Glib::ustring("Repo Browser"));
}

// ─── Load / Refresh ─────────────────────────────────────────────────────────

void MainWindow::load_repo(const std::string& path)
{
    engine_ = std::make_unique<GitEngine>();
    auto result = engine_->open(path);
    if (!result.ok()) {
        Gtk::MessageDialog dlg(*this,
            "Failed to open repository:\n" + result.error.message,
            true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        dlg.run();
        return;
    }

    const char* wd = git_repository_workdir(engine_->repo());
    std::string display = wd ? std::string(wd) : path;
    repo_label_.set_text(display);
    set_title("kGit — " + display);

    refresh_status();
    refresh_log();
    refresh_branches();
    update_branch_label();
    setup_repo_browser_panel();
}

void MainWindow::refresh_status()
{
    if (!engine_ || !engine_->repo()) return;

    staged_model_->clear();
    unstaged_model_->clear();
    file_model_->clear();

    auto staged_result   = engine_->diff_index_to_head();
    auto unstaged_result = engine_->diff_workdir_to_index();

    if (staged_result.ok()) {
        for (const auto& file : staged_result.files) {
            Gtk::TreeModel::Row row = *(staged_model_->append());
            std::string name = file.new_path.empty() ? file.old_path : file.new_path;
            row[cols_.col_name]    = name;
            row[cols_.col_path]    = name;
            row[cols_.col_staged]  = true;
            int ch = 1;
            if (file.old_path.empty()) ch = 1;           // added
            else if (file.new_path.empty()) ch = 2;      // deleted
            else ch = 3;                                 // modified
            row[cols_.col_change] = ch;
            row[cols_.col_icon]  = icon_for_change(ch);
        }
    }

    if (unstaged_result.ok()) {
        for (const auto& file : unstaged_result.files) {
            Gtk::TreeModel::Row row = *(unstaged_model_->append());
            std::string name = file.new_path.empty() ? file.old_path : file.new_path;
            row[cols_.col_name]    = name;
            row[cols_.col_path]    = name;
            row[cols_.col_staged]  = false;
            int ch = 3;
            if (file.additions > 0 && file.deletions == 0) ch = 1;
            else if (file.deletions > 0 && file.additions == 0) ch = 2;
            else if (file.old_path.empty()) ch = 4;
            row[cols_.col_change] = ch;
            row[cols_.col_icon]  = icon_for_change(ch);
        }
    }

    auto status = engine_->status_all();
    if (status.ok()) {
        for (const auto& s : status.value) {
            if (!s.staged && s.change == RepoStatus::Change::Untracked) {
                Gtk::TreeModel::Row row = *(unstaged_model_->append());
                row[cols_.col_name]    = s.path;
                row[cols_.col_path]    = s.path;
                row[cols_.col_staged]  = false;
                row[cols_.col_change]  = 4;
                row[cols_.col_icon]    = icon_for_change(4);
            }
        }
    }
}

void MainWindow::refresh_log()
{
    if (!engine_ || !engine_->repo()) return;
    auto result = engine_->log(500);
    if (!result.ok()) return;

    log_model_->clear();
    for (const auto& c : result.value) {
        Gtk::TreeModel::Row row = *(log_model_->append());
        row[log_cols_.col_short_hash] = c.short_id;
        row[log_cols_.col_author]    = c.author_name;
        row[log_cols_.col_date]      = format_time(c.time);
        row[log_cols_.col_message]  = c.summary;
    }
}

void MainWindow::refresh_branches()
{
    if (!engine_ || !engine_->repo()) return;
    auto branches = engine_->branches();
    if (!branches.ok()) return;

    branch_model_->clear();
    for (const auto& b : branches.value) {
        Gtk::TreeModel::Row row = *(branch_model_->append());
        row[branch_cols_.col_name]    = (b.is_remote ? "remotes/" : "") + b.name;
        row[branch_cols_.col_current] = b.is_current;
        row[branch_cols_.col_remote]  = b.is_remote;
    }

    auto tags = engine_->tags();
    if (tags.ok()) {
        for (const auto& t : tags.value) {
            Gtk::TreeModel::Row row = *(branch_model_->append());
            row[branch_cols_.col_name]    = "tag: " + t.name;
            row[branch_cols_.col_current] = false;
            row[branch_cols_.col_remote]  = false;
        }
    }
}

void MainWindow::update_branch_label()
{
    if (!engine_ || !engine_->repo()) return;
    git_reference* head = nullptr;
    if (git_repository_head(&head, engine_->repo()) == 0) {
        branch_label_.set_markup("<b>" + std::string(git_reference_shorthand(head)) + "</b>");
        git_reference_free(head);
    }
}

// ─── Icon helper ─────────────────────────────────────────────────────────────

Glib::RefPtr<Gdk::Pixbuf> MainWindow::icon_for_change(int change) const
{
    switch (change) {
        case 1: return cached_icon("added");
        case 2: return cached_icon("deleted");
        case 3: return cached_icon("modified");
        case 4: return cached_icon("untracked");
        case 10: return cached_icon("conflicted");
        default: return cached_icon("modified");
    }
}

// ─── Diff rendering ─────────────────────────────────────────────────────────

void MainWindow::render_diff(const DiffResult& result)
{
    auto tag_table = Gtk::TextTagTable::create();
    auto green_tag  = Gtk::TextTag::create(); green_tag->property_foreground()  = "#22c55e";
    auto red_tag    = Gtk::TextTag::create(); red_tag->property_foreground()    = "#ef4444";
    auto hunk_tag   = Gtk::TextTag::create(); hunk_tag->property_foreground()   = "#60a5fa";
    auto bold_tag   = Gtk::TextTag::create(); bold_tag->property_weight()        = Pango::WEIGHT_BOLD;
    tag_table->add(green_tag); tag_table->add(red_tag);
    tag_table->add(hunk_tag);  tag_table->add(bold_tag);

    auto buf = Gtk::TextBuffer::create(tag_table);
    auto end = buf->end();

    if (!result.ok()) {
        buf->insert(end, "Error: " + result.error.message);
        diff_view_.set_buffer(buf);
        return;
    }
    if (result.files.empty()) {
        buf->insert(end, "(no changes)");
        diff_view_.set_buffer(buf);
        return;
    }

    for (const auto& file : result.files) {
        buf->insert_with_tag(end, "--- " + file.old_path + "\n", bold_tag);
        buf->insert_with_tag(end, "+++ " + file.new_path + "\n", bold_tag);
        for (const auto& hunk : file.hunks) {
            if (!hunk.header.empty())
                buf->insert_with_tag(end, hunk.header + "\n", hunk_tag);
            for (const auto& line : hunk.lines) {
                if (line.rfind("+", 0) == 0)
                    buf->insert_with_tag(end, line + "\n", green_tag);
                else if (line.rfind("-", 0) == 0)
                    buf->insert_with_tag(end, line + "\n", red_tag);
                else if (line.rfind("@@", 0) == 0)
                    buf->insert_with_tag(end, line + "\n", hunk_tag);
                else
                    buf->insert(end, line + "\n");
            }
        }
        buf->insert(end, "\n");
    }

    diff_view_.set_buffer(buf);
}

// ─── Signal Handlers ────────────────────────────────────────────────────────

void MainWindow::on_clone()
{
    if (!engine_) return;
    CloneDialog dlg(*this, *engine_);
    if (dlg.run() == Gtk::RESPONSE_OK) {
        CloneOpts opts;
        opts.branch = dlg.branch();
        opts.bare = dlg.bare();
        if (!dlg.target_dir().empty()) opts.working_dir = dlg.target_dir();
        std::string ssh_key = dlg.ssh_key();
        if (!ssh_key.empty()) opts.ssh_key = ssh_key;
        auto r = engine_->clone(dlg.url(), opts);
        if (!r.ok()) {
            Gtk::MessageDialog(*this, "Clone failed: " + r.error.message,
                              true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true).run();
        } else {
            refresh_status();
            refresh_log();
        }
    }
    dlg.hide();
}

void MainWindow::on_commit()
{
    if (!engine_ || !engine_->repo()) return;
    if (staged_model_->children().empty()) {
        Gtk::MessageDialog(*this, "Nothing to commit — stage some files first.",
                          true, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true).run();
        return;
    }
    CommitDialog dlg(*this, *engine_);
    dlg.signal_commit_ready().connect([&](const std::string& msg, bool amend,
                                          bool signoff, bool gpg_sign,
                                          const std::string& gpg_key) {
        CommitOpts opts;
        opts.message  = msg;
        opts.amend    = amend;
        opts.signoff  = signoff;
        opts.gpg_sign = gpg_sign;
        opts.gpg_key_id = gpg_key;
        auto r = engine_->commit(opts);
        if (!r.ok()) {
            Gtk::MessageDialog(*this, "Commit failed: " + r.error.message,
                              true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true).run();
        } else {
            refresh_status();
            refresh_log();
            update_branch_label();
        }
    });
    dlg.run();
    dlg.hide();
}

void MainWindow::on_push()
{
    if (!engine_ || !engine_->repo()) return;
    auto remotes = engine_->remotes();
    std::string remote_name = (remotes.ok() && !remotes.value.empty())
                               ? remotes.value[0].name : "origin";
    auto r = engine_->push(remote_name, "refs/heads/" + current_branch(), false);
    if (!r.ok()) {
        Gtk::MessageDialog(*this, "Push failed: " + r.error.message,
                          true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true).run();
    }
}

void MainWindow::on_pull()  { /* TODO */ }
void MainWindow::on_fetch() { /* TODO */ }
void MainWindow::on_merge() { /* TODO */ }

void MainWindow::on_new_branch()
{
    Gtk::Dialog dlg("New Branch", *this, true);
    dlg.set_default_response(Gtk::RESPONSE_OK);
    auto* entry = Gtk::manage(new Gtk::Entry());
    entry->set_placeholder_text("branch-name");
    auto* vb = dlg.get_content_area();
    vb->pack_start(*Gtk::manage(new Gtk::Label("Branch name:")), false, false, 0);
    vb->pack_start(*entry, false, false, 4);
    dlg.add_button("_Create", Gtk::RESPONSE_OK);
    dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dlg.show_all();
    if (dlg.run() == Gtk::RESPONSE_OK) {
        std::string name = entry->get_text();
        if (!name.empty()) {
            auto r = engine_->branch_create(name, "HEAD");
            if (!r.ok()) {
                Gtk::MessageDialog(*this, "Failed to create branch: " + r.error.message,
                                  true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true).run();
            } else {
                refresh_branches();
                update_branch_label();
            }
        }
    }
}

void MainWindow::on_refresh()
{
    refresh_status();
    refresh_log();
    refresh_branches();
    update_branch_label();
}

// ─── Stage / Unstage ─────────────────────────────────────────────────────────

void MainWindow::on_stage_selected()
{
    if (!engine_) return;
    Glib::RefPtr<Gtk::TreeModel> model;
    auto it = unstaged_view_.get_selection()->get_selected(model);
    if (!it) return;
    std::string fpath = (*it).get_value(cols_.col_path);
    if (engine_->stage(fpath).ok()) refresh_status();
}

void MainWindow::on_unstage_selected()
{
    if (!engine_) return;
    Glib::RefPtr<Gtk::TreeModel> model;
    auto it = staged_view_.get_selection()->get_selected(model);
    if (!it) return;
    std::string fpath = (*it).get_value(cols_.col_path);
    if (engine_->unstage(fpath).ok()) refresh_status();
}

void MainWindow::on_stage_all()
{
    if (!engine_) return;
    engine_->stage_all();
    refresh_status();
}

// ─── File / Diff ─────────────────────────────────────────────────────────────

void MainWindow::select_file_for_diff()
{
    Glib::RefPtr<Gtk::TreeModel> model;
    auto it = file_tree_.get_selection()->get_selected(model);
    if (it) {
        show_diff_for_file((*it).get_value(cols_.col_path));
        return;
    }
    auto uit = unstaged_view_.get_selection()->get_selected(model);
    if (uit) {
        show_diff_for_file((*uit).get_value(cols_.col_path));
        return;
    }
}

void MainWindow::show_diff_for_file(const std::string& path)
{
    if (!engine_ || path.empty()) return;
    auto result = engine_->diff_file_workdir(path);
    render_diff(result);
    notebook_.set_current_page(2);
}

void MainWindow::show_diff_for_commit(const Glib::ustring& short_hash)
{
    if (!engine_) return;
    auto log_result = engine_->log(1);
    if (!log_result.ok() || log_result.value.empty()) return;
    std::string parent = log_result.value[0].parent_ids.empty()
                          ? std::string(short_hash)
                          : log_result.value[0].parent_ids[0];
    auto result = engine_->diff_commits(parent, std::string(short_hash));
    render_diff(result);
    notebook_.set_current_page(2);
}

// ─── Helpers ────────────────────────────────────────────────────────────────

std::string MainWindow::format_time(std::time_t t) const
{
    char buf[64] = {};
    struct tm tm_buf;
    if (localtime_r(&t, &tm_buf))
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm_buf);
    return std::string(buf);
}

std::string MainWindow::current_branch() const
{
    if (!engine_ || !engine_->repo()) return {};
    git_reference* head = nullptr;
    if (git_repository_head(&head, engine_->repo()) == 0) {
        std::string name = git_reference_shorthand(head);
        git_reference_free(head);
        return name;
    }
    return {};
}

// ─── Branch operations ──────────────────────────────────────────────────────

void MainWindow::on_branch_checkout()
{
    Glib::RefPtr<Gtk::TreeModel> model;
    auto it = branch_tree_.get_selection()->get_selected(model);
    if (!it) return;
    std::string name = (*it).get_value(branch_cols_.col_name);
    if (name.rfind("remotes/", 0) == 0) name = name.substr(8);
    if (name.rfind("tag: ", 0) == 0)   name = name.substr(5);
    auto r = engine_->checkout(name);
    if (!r.ok()) {
        Gtk::MessageDialog(*this, "Checkout failed: " + r.error.message,
                          true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true).run();
    } else {
        refresh_status();
        refresh_branches();
        update_branch_label();
    }
}

void MainWindow::on_branch_delete()
{
    Glib::RefPtr<Gtk::TreeModel> model;
    auto it = branch_tree_.get_selection()->get_selected(model);
    if (!it) return;
    std::string name = (*it).get_value(branch_cols_.col_name);
    if (name.rfind("tag: ", 0) == 0) {
        name = name.substr(5);
        auto r = engine_->tag_delete(name);
        if (!r.ok()) {
            Gtk::MessageDialog(*this, "Delete failed: " + r.error.message,
                              true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true).run();
        }
    } else {
        if (name.rfind("remotes/", 0) == 0) name = name.substr(8);
        if ((*it).get_value(branch_cols_.col_current)) {
            Gtk::MessageDialog(*this, "Cannot delete the current branch.",
                              true, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true).run();
            return;
        }
        auto r = engine_->branch_delete(name);
        if (!r.ok()) {
            Gtk::MessageDialog(*this, "Delete failed: " + r.error.message,
                              true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true).run();
        }
    }
    refresh_branches();
}

void MainWindow::on_revert_selected()
{
    Glib::RefPtr<Gtk::TreeModel> model;
    auto it = file_tree_.get_selection()->get_selected(model);
    if (!it) return;
    std::string fpath = (*it).get_value(cols_.col_path);
    if (fpath.empty()) return;

    // Revert the specific file to its HEAD state.
    CheckoutOpts opts;
    opts.start_point = "HEAD";
    auto r = engine_->checkout(fpath, opts);
    if (!r.ok()) {
        Gtk::MessageDialog(*this, "Revert failed: " + r.error.message,
                          true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true).run();
    } else {
        refresh_status();
    }
}

void MainWindow::on_delete_selected()
{
    Glib::RefPtr<Gtk::TreeModel> model;
    auto it = file_tree_.get_selection()->get_selected(model);
    if (!it) return;
    std::string fpath = (*it).get_value(cols_.col_path);
    Gtk::MessageDialog confirm(*this, "Delete " + fpath + "?", true,
                              Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
    if (confirm.run() == Gtk::RESPONSE_YES) {
        std::filesystem::remove(fpath);
        refresh_status();
    }
}
