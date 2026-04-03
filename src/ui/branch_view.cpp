#include "branch_view.hpp"

BranchView::BranchView()
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6)
{
    set_hexpand(true);
    set_vexpand(true);

    auto local_lbl  = Gtk::make_managed<Gtk::Label>("Local Branches (TODO)");
    auto remote_lbl = Gtk::make_managed<Gtk::Label>("Remote Branches (TODO)");
    auto tag_lbl    = Gtk::make_managed<Gtk::Label>("Tags (TODO)");

    pack_start(*local_lbl,  false, false, 0);
    pack_start(local_view_,  true,  true,  0);
    pack_start(*remote_lbl, false, false, 0);
    pack_start(remote_view_, true,  true,  0);
    pack_start(*tag_lbl,    false, false, 0);
    pack_start(tag_view_,   true,  true,  0);
    pack_start(checkout_btn_, false, false, 0);
}
