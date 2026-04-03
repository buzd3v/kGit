#include "log_view.hpp"

LogView::LogView()
{
    set_hexpand(true);
    set_vexpand(true);
    add(tree_view_);
}

void LogView::set_commits(const std::vector<CommitInfo>&)
{
    // TODO: populate tree view with columns: hash, author, date, message
}

void LogView::clear()
{
    if (model_) model_->clear();
}
