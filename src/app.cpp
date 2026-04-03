#include "app.hpp"
#include "ui/main_window.hpp"

#include <gtkmm/application.h>
#include <iostream>

App::App(int argc, char* argv[])
    : mode_(Mode::None), path_(), argc_(argc), argv_(argv)
{
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--clone" || arg == "--open" || arg == "--init") && i + 1 < argc) {
            if (arg == "--clone")      mode_ = Mode::Clone;
            else if (arg == "--open") mode_ = Mode::Open;
            else if (arg == "--init") mode_ = Mode::Init;
            path_ = argv[++i];
        }
    }
}

int App::run()
{
    app_ = Gtk::Application::create("re.kien.kGit", Gio::APPLICATION_NON_UNIQUE);

    app_->signal_activate().connect([this]() {
        auto win = std::make_unique<MainWindow>(path_);
        app_->add_window(*win);
        win->show_all();
    });

    return app_->run(argc_, argv_);
}
