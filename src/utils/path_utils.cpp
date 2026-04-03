#include "path_utils.hpp"

#include <giomm/file.h>
#include <glib.h>
#include <algorithm>
#include <filesystem>

namespace {

std::string join_path_impl(std::string_view base, std::string_view name)
{
    if (base.empty()) return std::string(name);
    if (base.back() == '/' || base.back() == '\\') base.remove_suffix(1);
    return std::string(base) + '/' + std::string(name);
}

} // anonymous namespace

namespace path {

bool is_within(const std::string& child, const std::string& parent)
{
    auto abs_child  = std::filesystem::absolute(child).lexically_normal().string();
    auto abs_parent = std::filesystem::absolute(parent).lexically_normal().string();
    return abs_child.rfind(abs_parent, 0) == 0;
}

std::string join(const std::string& base, const std::string& name)
{
    return join_path_impl(base, name);
}

std::string normalize(const std::string& p)
{
    return std::filesystem::path(p).lexically_normal().string();
}

std::string filename(const std::string& p)
{
    return std::filesystem::path(p).filename().string();
}

std::string parent(const std::string& p)
{
    return std::filesystem::path(p).parent_path().string();
}

bool exists(const std::string& p)
{
    return std::filesystem::exists(p);
}

bool is_dir(const std::string& p)
{
    return std::filesystem::is_directory(p);
}

std::string find_git_root(const std::string& path)
{
    namespace fs = std::filesystem;
    fs::path p = fs::absolute(path);
    while (!fs::exists(p / ".git")) {
        if (p == p.parent_path()) return {};
        p = p.parent_path();
    }
    return p.string();
}

} // namespace path
