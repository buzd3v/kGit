#pragma once

#include <string>
#include <gtkmm.h>

class App {
public:
    enum class Mode { Clone, Open, Init, None };

    explicit App(int argc, char* argv[]);

    Mode mode() const { return mode_; }
    const std::string& path() const { return path_; }

    int run();

private:
    Mode mode_ = Mode::None;
    std::string path_;
    int argc_ = 0;
    char** argv_ = nullptr;
    Glib::RefPtr<Gtk::Application> app_;
};
