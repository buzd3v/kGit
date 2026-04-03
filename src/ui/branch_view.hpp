#pragma once

#include <gtkmm.h>
#include <memory>
#include <vector>

struct BranchInfo;
struct RemoteInfo;
struct TagInfo;

class BranchView : public Gtk::Box {
public:
    BranchView();

    void set_branches(const std::vector<BranchInfo>& branches,
                      const std::vector<RemoteInfo>& remotes,
                      const std::vector<TagInfo>& tags);
    void set_tags(const std::vector<TagInfo>& tags);
    void set_remotes(const std::vector<RemoteInfo>& remotes);

    sigc::signal<void(const std::string& name)>& signal_checkout()       { return signal_checkout_; }
    sigc::signal<void(const std::string& name)>& signal_delete_branch()  { return signal_delete_branch_; }
    sigc::signal<void(const std::string& name)>& signal_delete_tag()     { return signal_delete_tag_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    Gtk::TreeView local_view_;
    Gtk::TreeView remote_view_;
    Gtk::TreeView tag_view_;
    Gtk::Button   checkout_btn_;
    Gtk::Button   delete_btn_;
    Gtk::Button   new_tag_btn_;

    // Signals owned directly by BranchView so Impl can reference them by pointer.
    sigc::signal<void(const std::string&)> signal_checkout_;
    sigc::signal<void(const std::string&)> signal_delete_branch_;
    sigc::signal<void(const std::string&)> signal_delete_tag_;
};
