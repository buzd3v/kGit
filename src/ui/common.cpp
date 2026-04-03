#include "common.hpp"
#include <fstream>
#include <sstream>

// ─── Icons ──────────────────────────────────────────────────────────────────

static Glib::RefPtr<Gdk::Pixbuf> make_color_icon(int r, int g, int b) {
    const int W = 16, H = 16;
    guchar rgba[W * H * 4];
    for (int i = 0; i < W * H; ++i) {
        rgba[i * 4 + 0] = static_cast<guchar>(r);
        rgba[i * 4 + 1] = static_cast<guchar>(g);
        rgba[i * 4 + 2] = static_cast<guchar>(b);
        rgba[i * 4 + 3] = 255;
    }
    return Gdk::Pixbuf::create_from_data(rgba, Gdk::COLORSPACE_RGB,
                                         true, 8, W, H, W * 4);
}

Glib::RefPtr<Gdk::Pixbuf> icons::status_icon(const std::string& status) {
    if (status == "added")     return make_color_icon(34, 197,  94); // green
    if (status == "modified")  return make_color_icon(234, 179,  88); // yellow/amber
    if (status == "deleted")   return make_color_icon(239,  68,  68); // red
    if (status == "renamed")   return make_color_icon(96, 165, 250); // blue
    if (status == "copied")    return make_color_icon(168,  85, 247); // purple
    if (status == "untracked") return make_color_icon(156, 163, 175); // gray
    if (status == "ignored")   return make_color_icon(107, 114, 128); // dark gray
    if (status == "conflicted") return make_color_icon(248, 113, 113); // bright red
    return make_color_icon(107, 114, 128); // default gray
}

Glib::RefPtr<Gdk::Pixbuf> icons::file_icon(const std::string& /*name*/) {
    return make_color_icon(229, 231, 235); // light gray file
}

Glib::RefPtr<Gdk::Pixbuf> icons::folder_icon() {
    return make_color_icon(251, 191,  36); // amber folder
}

// ─── Theme ──────────────────────────────────────────────────────────────────

void ui::apply_dark_theme(bool dark) {
    auto settings = Gtk::Settings::get_default();
    if (settings)
        settings->property_gtk_application_prefer_dark_theme().set_value(dark);
}

void ui::set_monospace(Gtk::Widget& w) {
    auto pango_ctx = w.get_pango_context();
    if (!pango_ctx) return;
    Pango::FontDescription desc("monospace");
    pango_ctx->set_font_description(desc);
    w.override_font(desc);
}

void ui::set_monospace(Gtk::TextView& tv) {
    Pango::FontDescription desc("monospace");
    tv.override_font(desc);
}

// ─── Relative time ───────────────────────────────────────────────────────────

std::string ui::format_relative_time(std::time_t t) {
    if (t == 0) return "—";

    std::time_t now = std::time(nullptr);
    std::time_t diff = now - t;

    if (diff < 60)     return "just now";
    if (diff < 3600)   return std::to_string(diff / 60) + " min ago";
    if (diff < 86400)  return std::to_string(diff / 3600) + " hr ago";
    if (diff < 604800) return std::to_string(diff / 86400) + " days ago";

    char buf[32];
    std::tm* tm = std::localtime(&t);
    if (!tm) return "—";
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
    return std::string(buf);
}
