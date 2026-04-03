#pragma once

#include "git_engine.hpp"

#include <gtkmm.h>
#include <memory>
#include <string>

class MainWindow : public Gtk::ApplicationWindow {
public:
    explicit MainWindow(const std::string& repo_path = {});
    ~MainWindow() override;

private:
    // ── Model columns (shared between file/status lists) ────────────────────
    class ModelColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        ModelColumns() {
            add(col_icon); add(col_name); add(col_path);
            add(col_staged); add(col_change);
        }
        Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> col_icon;
        Gtk::TreeModelColumn<Glib::ustring>            col_name;
        Gtk::TreeModelColumn<std::string>               col_path;
        Gtk::TreeModelColumn<bool>                     col_staged;
        Gtk::TreeModelColumn<int>                      col_change;
    };

    // ── Log columns ───────────────────────────────────────────────────────────
    class LogColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        LogColumns() {
            add(col_short_hash); add(col_author);
            add(col_date); add(col_message);
        }
        Gtk::TreeModelColumn<Glib::ustring> col_short_hash;
        Gtk::TreeModelColumn<Glib::ustring> col_author;
        Gtk::TreeModelColumn<Glib::ustring> col_date;
        Gtk::TreeModelColumn<Glib::ustring> col_message;
    };

    // ── Branch columns ─────────────────────────────────────────────────────────
    class BranchColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        BranchColumns() { add(col_name); add(col_current); add(col_remote); }
        Gtk::TreeModelColumn<Glib::ustring> col_name;
        Gtk::TreeModelColumn<bool>          col_current;
        Gtk::TreeModelColumn<bool>          col_remote;
    };

    ModelColumns     cols_;
    LogColumns      log_cols_;
    BranchColumns   branch_cols_;

    Glib::RefPtr<Gtk::ListStore> file_model_;
    Glib::RefPtr<Gtk::ListStore> staged_model_;
    Glib::RefPtr<Gtk::ListStore> unstaged_model_;
    Glib::RefPtr<Gtk::ListStore> log_model_;
    Glib::RefPtr<Gtk::ListStore> branch_model_;

    std::unique_ptr<GitEngine> engine_;

    // Widgets
    Gtk::Box          main_box_{Gtk::ORIENTATION_HORIZONTAL, 0};
    Gtk::Paned        paned_{Gtk::ORIENTATION_HORIZONTAL};

    // Left panel
    Gtk::Box          left_box_{Gtk::ORIENTATION_VERTICAL, 0};
    Gtk::ScrolledWindow file_scroll_;
    Gtk::TreeView     file_tree_;

    // Right panel
    Gtk::Notebook     notebook_;
    Gtk::ScrolledWindow changes_scroll_;
    Gtk::TreeView      staged_view_;
    Gtk::TreeView      unstaged_view_;
    Gtk::Button        btn_stage_selected_;
    Gtk::Button        btn_unstage_selected_;
    Gtk::Button        btn_stage_all_;
    Gtk::Button        btn_commit_;

    Gtk::ScrolledWindow log_scroll_;
    Gtk::TreeView      log_tree_;
    Gtk::ScrolledWindow diff_scroll_;
    Gtk::TextView      diff_view_;
    Gtk::ScrolledWindow branch_scroll_;
    Gtk::TreeView      branch_tree_;

    // Toolbar
    Gtk::Toolbar      toolbar_;
    Gtk::ToolButton  btn_clone_tool_{"Clone"}, btn_commit_tool_{"Commit"},
                    btn_push_{"Push"}, btn_pull_{"Pull"},
                    btn_fetch_{"Fetch"}, btn_merge_{"Merge"},
                    btn_branch_{"Branch"}, btn_refresh_{"Refresh"};

    Gtk::Label        repo_label_;
    Gtk::Label        branch_label_;

    // Helpers
    void build_ui();
    void setup_file_tree();
    void setup_changes_panel();
    void setup_log_panel();
    void setup_diff_panel();
    void setup_branch_panel();

    void load_repo(const std::string& path);
    void refresh_status();
    void refresh_log();
    void refresh_branches();
    void update_branch_label();

    void select_file_for_diff();
    void show_diff_for_file(const std::string& path);
    void show_diff_for_commit(const Glib::ustring& short_hash);
    void render_diff(const DiffResult& result);

    std::string format_time(std::time_t t) const;
    std::string current_branch() const;
    Glib::RefPtr<Gdk::Pixbuf> icon_for_change(int change) const;

    // Signal handlers
    void on_clone();
    void on_commit();
    void on_push();
    void on_pull();
    void on_fetch();
    void on_merge();
    void on_new_branch();
    void on_refresh();
    void on_stage_selected();
    void on_unstage_selected();
    void on_stage_all();
    void on_branch_checkout();
    void on_branch_delete();
    void on_revert_selected();
    void on_delete_selected();
};
