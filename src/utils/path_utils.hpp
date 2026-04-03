#pragma once

#include <string>

namespace path {

/// Returns true if `child` is inside (or equal to) `parent`.
bool is_within(const std::string& child, const std::string& parent);

/// Join two path components.
std::string join(const std::string& base, const std::string& name);

/// Normalize a path (resolve . and .., collapse separators).
std::string normalize(const std::string& p);

/// Return the last path component.
std::string filename(const std::string& p);

/// Return the parent directory.
std::string parent(const std::string& p);

/// Return true if the path exists.
bool exists(const std::string& p);

/// Return true if the path is a directory.
bool is_dir(const std::string& p);

/// Walk upward from `path` until a .git directory is found.
/// Returns empty string if not inside a git repository.
std::string find_git_root(const std::string& path);

} // namespace path
