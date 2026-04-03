#include "main_window.hpp"
#include "../git_engine.hpp"

#include <giomm/menu.h>
#include <giomm/simpleactiongroup.h>

#include <iostream>

namespace {
    const char* KGIT_ICON_NAME = "kgit";
}

MainWindow::MainWindow(const std::string& repo_path)
{
    set_default_size(1100, 700);
    build_ui();
    if (!repo_path.empty()) {
        load_repo(repo_path);
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::build_ui()
{
    set_title("kGit");

    // ── Header bar ──────────────────────────────────────────────────────────
    auto header = Gtk::manage(new Gtk::HeaderBar());
    header->set_show_close_button(true);
    header->set_title("kGit");
    repo_label_.set_xalign(0);
    branch_label_.set_xalign(1);
    header->pack_start(repo_label_);
    header->pack_end(branch_label_);
    set_titlebar(*header);

    // ── Toolbar ─────────────────────────────────────────────────────────────
    toolbar_.set_toolbar_style(Gtk::TOOLBAR_BOTH_HORIZ);
    btn_clone_.set_label("Clone");
    btn_commit_.set_label("Commit");
    btn_push_.set_label("Push");
    btn_pull_.set_label("Pull");
    btn_fetch_.set_label("Fetch");
    btn_merge_.set_label("Merge");
    btn_branch_.set_label("Branch");
    btn_refresh_.set_label("Refresh");

    btn_clone_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_clone));
    btn_commit_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_commit));
    btn_push_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_push));
    btn_pull_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_pull));
    btn_fetch_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_fetch));
    btn_merge_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_merge));
    btn_branch_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_new_branch));
    btn_refresh_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_refresh));

    toolbar_.add(btn_clone_);
    toolbar_.add(btn_commit_);
    toolbar_.add(btn_push_);
    toolbar_.add(btn_pull_);
    toolbar_.add(btn_fetch_);
    toolbar_.add(btn_merge_);
    toolbar_.add(btn_branch_);
    toolbar_.add(btn_refresh_);

    // ── Paned layout ───────────────────────────────────────────────────────
    main_box_.set_hexpand(true);
    main_box_.set_vexpand(true);
    main_box_.pack_start(toolbar_, false, false, 0);
    main_box_.pack_start(paned_,   true,  true,  0);

    setup_file_tree();
    setup_panels();

    paned_.pack1(file_scroll_, true, false);
    paned_.pack2(notebook_,     true, false);
    paned_.set_position(280);

    add(main_box_);
    show_all();
}

void MainWindow::setup_file_tree()
{
    // NOTE: Add column record to define model columns:
    //   - filename (string), status_icon (Pixbuf), staged (bool)
    file_scroll_.add(file_tree_);
    file_tree_.signal_row_activated().connect(
        [this](const Gtk::TreeModel::Path& p, Gtk::TreeViewColumn*) { on_file_row_activated(p); });

    // Right-click context menu
    auto menu = Gtk::make_managed<Gtk::Menu>();
    Gtk::MenuItem diff_item("Show Diff");
    Gtk::MenuItem revert_item("Revert");
    Gtk::MenuItem rm_item("Delete");
    diff_item.signal_activate().connect([this] {
        auto sel = file_tree_.get_selection();
        (void)sel;
        show_diff_for_file("");
    });
    menu->append(diff_item);
    menu->append(revert_item);
    menu->append(rm_item);
    menu->show_all();
    file_tree_.signal_button_press_event().connect([menu](GdkEventButton* ev) -> bool {
        if (ev->button == 3) { menu->popup_at_pointer((GdkEvent*)ev); return true; }
        return false;
    });
}

void MainWindow::setup_panels()
{
    auto changes_lbl = Gtk::make_managed<Gtk::Label>("Changes view (TODO)");
    auto log_lbl     = Gtk::make_managed<Gtk::Label>("Commit log (TODO)");
    auto diff_lbl    = Gtk::make_managed<Gtk::Label>("Diff view (TODO)");
    auto branch_lbl  = Gtk::make_managed<Gtk::Label>("Branches (TODO)");

    notebook_.append_page(*Gtk::make_managed<Gtk::Label>("Changes (TODO)"), Glib::ustring("Changes"));
    notebook_.append_page(*Gtk::make_managed<Gtk::Label>("Log (TODO)"),    Glib::ustring("Log"));
    notebook_.append_page(*Gtk::make_managed<Gtk::Label>("Diff (TODO)"),   Glib::ustring("Diff"));
    notebook_.append_page(*Gtk::make_managed<Gtk::Label>("Branches (TODO)"), Glib::ustring("Branches"));
}

void MainWindow::load_repo(const std::string& path)
{
    engine_ = std::make_unique<GitEngine>();
    auto result = engine_->open(path);
    if (!result.ok()) {
        Gtk::MessageDialog dlg(*this, "Failed to open repository: " + result.error.message,
                               true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        dlg.run();
        return;
    }
    repo_label_.set_text(path);
    refresh_status();
}

void MainWindow::refresh_status()
{
    if (!engine_ || !engine_->repo()) return;
    auto result = engine_->status_all();
    (void)result;
    // TODO: populate file_model_ with status entries
}

void MainWindow::on_clone()       { /* TODO: open CloneDialog */ }
void MainWindow::on_commit()      { /* TODO: open CommitDialog */ }
void MainWindow::on_push()        { /* TODO */ }
void MainWindow::on_pull()        { /* TODO */ }
void MainWindow::on_fetch()       { /* TODO */ }
void MainWindow::on_merge()        { /* TODO */ }
void MainWindow::on_new_branch()  { /* TODO: open BranchDialog */ }
void MainWindow::on_refresh()     { refresh_status(); }

void MainWindow::on_file_row_activated(const Gtk::TreeModel::Path&) {}
void MainWindow::show_diff_for_file(const std::string&) {}
