#include "clone_dialog.hpp"
#include "../git_engine.hpp"

CloneDialog::CloneDialog(Gtk::Window& parent, GitEngine&)
    : Gtk::Dialog("Clone Repository", parent, true)
{
    auto box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL, 8);
    box->set_border_width(12);

    auto url_lbl  = Gtk::make_managed<Gtk::Label>("Repository URL:");
    auto tgt_lbl  = Gtk::make_managed<Gtk::Label>("Target directory:");
    auto br_lbl   = Gtk::make_managed<Gtk::Label>("Branch (optional):");

    url_entry_.set_placeholder_text("https://github.com/user/repo.git");
    target_entry_.set_placeholder_text(".");
    branch_entry_.set_placeholder_text("(default branch)");

    auto grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_column_spacing(8);
    grid->set_row_spacing(8);
    grid->attach(*url_lbl, 0, 0, 1, 1);
    grid->attach(url_entry_, 1, 0, 1, 1);
    grid->attach(*tgt_lbl, 0, 1, 1, 1);
    grid->attach(target_entry_, 1, 1, 1, 1);
    grid->attach(*br_lbl, 0, 2, 1, 1);
    grid->attach(branch_entry_, 1, 2, 1, 1);
    grid->attach(bare_check_, 1, 3, 1, 1);
    bare_check_.set_label("Bare repository");

    box->pack_start(*grid);
    add_action_widget(bare_check_, 0);
    add_button("_Clone", Gtk::RESPONSE_OK);
    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    get_content_area()->pack_start(*box);
    show_all();
}

std::string CloneDialog::url()        const { return url_entry_.get_text(); }
std::string CloneDialog::target_dir() const { return target_entry_.get_text(); }
std::string CloneDialog::branch()     const { return branch_entry_.get_text(); }
