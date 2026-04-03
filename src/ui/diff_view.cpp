#include "diff_view.hpp"

DiffView::DiffView()
{
    set_hexpand(true);
    set_vexpand(true);
    text_view_.set_editable(false);
    text_view_.set_wrap_mode(Gtk::WRAP_NONE);
    add(text_view_);
}

void DiffView::set_diff(const std::vector<struct DiffFile>&)
{
    auto buf = Gtk::TextBuffer::create();
    buf->set_text("Diff view placeholder — implement side-by-side rendering here.");
    text_view_.set_buffer(buf);
}

void DiffView::clear()
{
    text_view_.get_buffer()->set_text("");
}
