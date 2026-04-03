#include "log_view.hpp"
#include "../git_engine.hpp"
#include "common.hpp"
#include <algorithm>
#include <cctype>

namespace {
    struct LogCols : public Gtk::TreeModel::ColumnRecord {
        Gtk::TreeModelColumn<Glib::ustring> graph;
        Gtk::TreeModelColumn<Glib::ustring> short_hash;
        Gtk::TreeModelColumn<Glib::ustring> author;
        Gtk::TreeModelColumn<Glib::ustring> date_rel;
        Gtk::TreeModelColumn<Glib::ustring> message;
        Gtk::TreeModelColumn<std::string>   full_id;

        LogCols() {
            add(graph);
            add(short_hash);
            add(author);
            add(date_rel);
            add(message);
            add(full_id);
        }
    };
    LogCols LCOLS;
}

// ─── LogView::Impl ────────────────────────────────────────────────────────────

struct LogView::Impl {
    Gtk::TreeView& tree_view_;
    Glib::RefPtr<Gtk::ListStore> model_;
    sigc::signal<void(const std::string&)> signal_commit_selected_;

    // Filter entries
    Gtk::Entry message_filter_;
    Gtk::Entry author_filter_;

    std::vector<CommitInfo> all_commits_;

    Impl(Gtk::TreeView& tv) : tree_view_(tv) {
        model_ = Gtk::ListStore::create(LCOLS);
        tree_view_.set_model(model_);
        tree_view_.set_headers_visible(true);
        tree_view_.set_search_column(LCOLS.message);
        tree_view_.set_rules_hint(true);

        auto* graph_col = Gtk::manage(new Gtk::TreeView::Column(""));
        auto* graph_r = Gtk::manage(new Gtk::CellRendererText());
        graph_col->pack_start(*graph_r, false);
        graph_col->add_attribute(graph_r->property_text(), LCOLS.graph);
        graph_col->set_fixed_width(30);

        auto* hash_col = Gtk::manage(new Gtk::TreeView::Column("Hash"));
        hash_col->pack_start(LCOLS.short_hash, false);
        hash_col->set_fixed_width(80);

        auto* author_col = Gtk::manage(new Gtk::TreeView::Column("Author"));
        author_col->pack_start(LCOLS.author, true);
        author_col->set_min_width(120);

        auto* date_col = Gtk::manage(new Gtk::TreeView::Column("Date"));
        date_col->pack_start(LCOLS.date_rel, false);
        date_col->set_fixed_width(110);

        auto* msg_col = Gtk::manage(new Gtk::TreeView::Column("Message"));
        msg_col->pack_start(LCOLS.message, true);

        tree_view_.append_column(*graph_col);
        tree_view_.append_column(*hash_col);
        tree_view_.append_column(*author_col);
        tree_view_.append_column(*date_col);
        tree_view_.append_column(*msg_col);

        message_filter_.set_placeholder_text("Filter by message…");
        author_filter_.set_placeholder_text("Filter by author…");

        message_filter_.signal_changed().connect(sigc::mem_fun(*this, &Impl::apply_filter));
        author_filter_.signal_changed().connect(sigc::mem_fun(*this, &Impl::apply_filter));

        tree_view_.signal_cursor_changed().connect([&] {
            auto sel = tree_view_.get_selection();
            auto it = sel->get_selected();
            if (it) {
                auto row = *it;
                std::string id = std::string(row[LCOLS.full_id]);
                signal_commit_selected_.emit(id);
            }
        });
    }

    void apply_filter() {
        model_->clear();
        auto msg_q = message_filter_.get_text();
        auto auth_q = author_filter_.get_text();
        Glib::ustring msg_lower, auth_lower;
        if (!msg_q.empty()) msg_lower = msg_q.lowercase();
        if (!auth_q.empty()) auth_lower = auth_q.lowercase();

        for (const auto& c : all_commits_) {
            bool match_msg = msg_lower.empty() ||
                (Glib::ustring(c.summary).lowercase().find(msg_lower) != Glib::ustring::npos ||
                 Glib::ustring(c.body).lowercase().find(msg_lower) != Glib::ustring::npos);
            bool match_auth = auth_lower.empty() ||
                Glib::ustring(c.author_name).lowercase().find(auth_lower) != Glib::ustring::npos;
            if (!match_msg || !match_auth) continue;

            auto row = *model_->append();
            row[LCOLS.graph]      = "●"; // simplified — full graph needs custom renderer
            row[LCOLS.short_hash] = c.short_id;
            row[LCOLS.author]     = c.author_name;
            row[LCOLS.date_rel]   = ui::format_relative_time(c.time);
            row[LCOLS.message]    = c.summary;
            row[LCOLS.full_id]   = c.id;
        }
    }
};

// ─── LogView ──────────────────────────────────────────────────────────────────

LogView::LogView()
    : impl_(std::make_unique<Impl>(tree_view_))
{
    set_hexpand(true);
    set_vexpand(true);

    auto* vbox = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL, 4);
    vbox->set_border_width(4);

    auto* filter_bar = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL, 6);
    filter_bar->pack_start(*Gtk::make_managed<Gtk::Label>("Author:"), false, false, 0);
    filter_bar->pack_start(impl_->author_filter_, true, true, 0);
    filter_bar->pack_start(*Gtk::make_managed<Gtk::Label>("Message:"), false, false, 0);
    filter_bar->pack_start(impl_->message_filter_, true, true, 0);
    vbox->pack_start(*filter_bar, false, false, 0);
    vbox->pack_start(tree_view_, true, true, 0);

    add(*vbox);
}

void LogView::set_commits(const std::vector<CommitInfo>& commits) {
    impl_->all_commits_ = commits;
    impl_->apply_filter();
}

void LogView::clear() {
    impl_->all_commits_.clear();
    impl_->model_->clear();
    impl_->message_filter_.set_text("");
    impl_->author_filter_.set_text("");
}

sigc::signal<void(const std::string&)>& LogView::signal_commit_selected() {
    return impl_->signal_commit_selected_;
}
