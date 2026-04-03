#include "status_view.hpp"

StatusView::StatusView()
{
    set_hexpand(true);
    set_vexpand(true);
    add(tree_view_);
}

void StatusView::set_status(const std::vector<RepoStatus>&)
{
    // TODO: populate tree view with staged/unstaged files
}

void StatusView::clear() {}
