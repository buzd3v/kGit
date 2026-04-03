#pragma once

#include <gtkmm.h>
#include <memory>

class GitEngine;

class MainWindow : public Gtk::ApplicationWindow {
public:
    explicit MainWindow(const std::string& repo_path = {});
    ~MainWindow() override;

protected:
    // Signal handlers
    void on_clone();
    void on_commit();
    void on_push();
    void on_pull();
    void on_fetch();
    void on_merge();
    void on_new_branch();
    void on_refresh();

    void on_file_row_activated(const Gtk::TreeModel::Path& path);
    void on_file_context_menu(const Gtk::TreeModel::Path& path,
                               const Gtk::TreeViewColumn* col);

    void show_diff_for_file(const std::string& path);

private:
    void build_ui();
    void setup_actions();
    void setup_file_tree();
    void setup_panels();
    void load_repo(const std::string& path);
    void refresh_status();

    std::unique_ptr<GitEngine> engine_;

    // Widgets
    Gtk::Box main_box_{Gtk::ORIENTATION_HORIZONTAL};
    Gtk::Paned paned_{Gtk::ORIENTATION_HORIZONTAL};

    // Left: file tree
    Gtk::ScrolledWindow file_scroll_;
    Gtk::TreeView file_tree_;
    Glib::RefPtr<Gtk::ListStore> file_model_;

    // Right: notebook panels
    Gtk::Notebook notebook_;
    Gtk::Label label_changes_{"Changes"};
    Gtk::Label label_log_{"Log"};
    Gtk::Label label_diff_{"Diff"};
    Gtk::Label label_branches_{"Branches"};

    // Toolbar
    Gtk::Toolbar toolbar_;
    Gtk::ToolButton btn_clone_, btn_commit_, btn_push_, btn_pull_,
                    btn_fetch_, btn_merge_, btn_branch_, btn_refresh_;
    Gtk::Label repo_label_;
    Gtk::Label branch_label_;
};
