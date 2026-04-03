#pragma once

#include <git2.h>

#include <ctime>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// ─── Forward Declarations ───────────────────────────────────────────────────

struct git_repository;
struct git_commit;
struct git_tree;
struct git_diff;
struct git_index;
struct git_reference;
struct git_remote;
struct git_tag;
struct git_signature;
struct git_odb;
struct git_reflog;
struct git_config;
struct git_diff_stats;

// ─── Result / Error ─────────────────────────────────────────────────────────

struct GitError {
    int code = 0;
    std::string message;
};

template <typename T>
struct GitResult {
    T value{};
    GitError error;

    bool ok() const { return error.code == 0; }
    explicit operator bool() const { return ok(); }
};

template <>
struct GitResult<void> {
    GitError error = {};
    GitResult() = default;
    /* implicit */ GitResult(const GitError& e) : error(e) {}
    bool ok() const { return error.code == 0; }
    explicit operator bool() const { return ok(); }
};

// ─── Data Types ─────────────────────────────────────────────────────────────

struct RepoStatus {
    enum class Change {
        Unmodified, Added, Deleted, Modified, Renamed,
        Copied, Ignored, Untracked, TypeChange, Unreadable, Conflicted
    };
    std::string path;
    Change change = Change::Unmodified;
    bool staged = false;
};

struct DiffHunk {
    std::string header;
    std::vector<std::string> lines; // each line prefixed with ' ', '+', '-'
};

struct DiffFile {
    std::string old_path;
    std::string new_path;
    std::vector<DiffHunk> hunks;
    int additions = 0;
    int deletions = 0;
};

struct DiffResult {
    std::vector<DiffFile> files;
    GitError error;
    bool ok() const { return error.code == 0; }
};

struct CommitInfo {
    std::string id;          // full 40-char SHA
    std::string short_id;    // 7-char
    std::string summary;
    std::string body;
    std::string author_name;
    std::string author_email;
    std::string committer_name;
    std::string committer_email;
    std::time_t time = 0;
    std::vector<std::string> parent_ids;
};

struct BranchInfo {
    std::string name;
    bool is_head = false;
    bool is_remote = false;
    bool is_current = false;
};

struct RemoteInfo {
    std::string name;
    std::string fetch_url;
    std::string push_url;
};

struct TagInfo {
    std::string name;
    std::string target_id;
    std::string message;
    std::string tagger_name;
    std::string tagger_email;
    std::time_t time = 0;
};

struct CheckoutOpts {
    bool force = false;
    bool create_branch = false;
    std::string branch_name;
    std::string start_point; // commit-ish
};

struct CloneOpts {
    bool bare = false;
    std::string branch;
    std::optional<std::string> working_dir; // local target
    std::optional<std::string> ssh_key;
    std::function<bool(const std::string&, const std::string&)> progress_cb;
    std::function<bool(const git_oid*, size_t, int)> transfer_progress_cb;
    std::function<bool(const std::string&, const std::string&)> cred_cb;
};

struct CommitOpts {
    std::string message;
    bool amend = false;
    std::optional<std::string> author_name;
    std::optional<std::string> author_email;
    /// Prepend "Signed-off-by:" line to the message.
    bool signoff = false;
    /// Request GPG signing of the commit.
    bool gpg_sign = false;
    /// GPG key ID (empty = use default key).
    std::string gpg_key_id;
};

// ─── GitEngine ───────────────────────────────────────────────────────────────

class GitEngine {
public:
    GitEngine();
    ~GitEngine();

    GitEngine(const GitEngine&) = delete;
    GitEngine& operator=(const GitEngine&) = delete;
    GitEngine(GitEngine&&) = delete;
    GitEngine& operator=(GitEngine&&) = delete;

    /// Initialise a new repository at `path`.
    GitResult<git_repository*> init(const std::string& path, bool bare = false);

    /// Open an existing repository. Empty path = auto-detect from cwd.
    GitResult<git_repository*> open(const std::string& path = {});

    /// Clone a remote repository.
    GitResult<git_repository*> clone(const std::string& url, const CloneOpts& opts = {});

    /// Get the currently open repository.
    git_repository* repo() const { return repo_; }

    /// Close the currently open repository.
    void close();

    // ── Status ──────────────────────────────────────────────────────────────

    /// Enumerate all files with their status.
    GitResult<std::vector<RepoStatus>> status_all();

    /// Refresh the index (needed after filesystem changes).
    void status_refresh();

    // ── Index operations ────────────────────────────────────────────────────

    /// Stage a file (add to index).
    GitResult<void> stage(const std::string& path);

    /// Unstage a file (remove from index).
    GitResult<void> unstage(const std::string& path);

    /// Stage all changes (add all).
    GitResult<void> stage_all();

    /// Remove a file from the working tree and index.
    GitResult<void> remove(const std::string& path);

    // ── Commit ──────────────────────────────────────────────────────────────

    /// Create a new commit with the current index.
    GitResult<std::string> commit(const CommitOpts& opts);

    /// Get the commit pointed to by HEAD.
    GitResult<CommitInfo> head_commit();

    // ── Diff ────────────────────────────────────────────────────────────────

    /// Diff working tree vs index (what would be staged).
    DiffResult diff_workdir_to_index();

    /// Diff index vs HEAD (what would be committed).
    DiffResult diff_index_to_head();

    /// Diff two commits.
    DiffResult diff_commits(const std::string& old_id, const std::string& new_id);

    /// Diff a single file between workdir and index.
    DiffResult diff_file_workdir(const std::string& path);

    // ── Log ─────────────────────────────────────────────────────────────────

    /// Get commit history.
    GitResult<std::vector<CommitInfo>> log(size_t max_count = 100);

    /// Get history for a specific file.
    GitResult<std::vector<CommitInfo>> log_file(const std::string& path, size_t max_count = 100);

    // ── Branches ────────────────────────────────────────────────────────────

    GitResult<std::vector<BranchInfo>> branches();
    GitResult<git_reference*> branch_create(const std::string& name, const std::string& target = "HEAD");
    GitResult<void> branch_checkout(const std::string& name, const CheckoutOpts& opts = {});
    GitResult<void> branch_delete(const std::string& name);
    GitResult<void> branch_rename(const std::string& old_name, const std::string& new_name);

    // ── Remotes ─────────────────────────────────────────────────────────────

    GitResult<std::vector<RemoteInfo>> remotes();
    GitResult<void> remote_add(const std::string& name, const std::string& url);
    GitResult<void> remote_remove(const std::string& name);
    GitResult<void> fetch(const std::string& remote = {}, const std::string& refspec = {});
    GitResult<void> push(const std::string& remote, const std::string& refspec,
                         bool force = false);

    // ── Checkout ────────────────────────────────────────────────────────────

    GitResult<void> checkout(const std::string& target, const CheckoutOpts& opts = {});

    // ── Merge ──────────────────────────────────────────────────────────────

    GitResult<void> merge(const std::string& branch);

    // ── Tags ───────────────────────────────────────────────────────────────

    GitResult<std::vector<TagInfo>> tags();
    GitResult<void> tag_create(const std::string& name, const std::string& target,
                               const std::string& message = {});
    GitResult<void> tag_delete(const std::string& name);

    // ── Tree / Blob ────────────────────────────────────────────────────────

    /// Read file content at a given commit.
    GitResult<std::string> blob_content(const std::string& commitish, const std::string& path);

    /// List files in a tree.
    GitResult<std::vector<std::string>> tree_ls(const std::string& commitish);

    // ── Helpers ─────────────────────────────────────────────────────────────

    GitError last_error() const { return last_error_; }
    static std::string oid_to_str(const git_oid& oid);
    static GitError git_error();

private:
    git_repository* repo_ = nullptr;
    GitError last_error_;

    GitResult<git_signature*> make_signature(const std::string& name,
                                              const std::string& email) const;

    GitResult<git_reference*> resolve_ref(const std::string& name);

    GitResult<git_commit*> resolve_commit(const std::string& commitish);

    void set_error(int code, const std::string& msg);
    void set_error(int code);

    CommitInfo commit_info_from_cmt(git_commit* cmt) const;
};
