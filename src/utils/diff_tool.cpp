#include "diff_tool.hpp"

#include <glib.h>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
    const char* DEFAULT_DIFF_TOOL = "git";
    const char* DEFAULT_DIFF_ARGS = "difftool";
}

DiffToolLauncher::DiffToolLauncher()
    : tool_path_(DEFAULT_DIFF_TOOL)
    , args_({DEFAULT_DIFF_ARGS})
{}

void DiffToolLauncher::set_tool(const std::string& path, const std::vector<std::string>& args)
{
    tool_path_ = path;
    args_ = args;
}

bool DiffToolLauncher::launch(const std::string& path, bool staged) const
{
    std::vector<std::string> argv;
    argv.reserve(4 + args_.size());
    argv.push_back(tool_path_);
    for (const auto& a : args_) argv.push_back(a);
    if (staged) argv.push_back("--cached");
    argv.push_back("--");
    argv.push_back(path);

    std::vector<const char*> cargv;
    for (const auto& a : argv) cargv.push_back(a.c_str());
    cargv.push_back(nullptr);

    GPid child_pid = 0;
    GError* err = nullptr;
    int ret = g_spawn_async(nullptr, const_cast<char**>(cargv.data()),
                             nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, &child_pid, &err);
    if (err) g_error_free(err);
    return ret != 0;
}
