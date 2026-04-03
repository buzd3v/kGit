#pragma once

#include <string>
#include <vector>

class DiffToolLauncher {
public:
    DiffToolLauncher();
    void set_tool(const std::string& path, const std::vector<std::string>& args = {"difftool"});
    bool launch(const std::string& path, bool staged = false) const;

private:
    std::string tool_path_;
    std::vector<std::string> args_;
};
