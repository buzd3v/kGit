#pragma once

#include <gtkmm.h>
#include <vector>

struct BranchInfo;

class BranchView : public Gtk::Box {
public:
    BranchView();

    void set_branches(const std::vector<BranchInfo>& branches,
                      const std::vector<struct RemoteInfo>& remotes,
                      const std::vector<struct TagInfo>& tags);

    sigc::signal<void(const std::string& name)>& signal_checkout() { return signal_checkout_; }
    sigc::signal<void(const std::string& name)>& signal_delete_branch() { return signal_delete_branch_; }
    sigc::signal<void(const std::string& name)>& signal_delete_tag() { return signal_delete_tag_; }

private:
    Gtk::TreeView local_view_, remote_view_, tag_view_;
    Gtk::Button checkout_btn_, delete_btn_, new_tag_btn_;
    sigc::signal<void(const std::string&)> signal_checkout_;
    sigc::signal<void(const std::string&)> signal_delete_branch_;
    sigc::signal<void(const std::string&)> signal_delete_tag_;
};
