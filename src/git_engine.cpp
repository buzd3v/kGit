#include "git_engine.hpp"

#include <git2.h>
#include <git2/commit.h>
#include <git2/diff.h>
#include <git2/index.h>
#include <git2/repository.h>
#include <git2/revparse.h>
#include <git2/tree.h>
#include <git2/branch.h>
#include <git2/remote.h>
#include <git2/refs.h>
#include <git2/checkout.h>
#include <git2/merge.h>
#include <git2/tag.h>
#include <git2/signature.h>
#include <git2/clone.h>
#include <git2/status.h>
#include <git2/pathspec.h>

#include <algorithm>
#include <cstring>
#include <ctime>
#include <sstream>

// ─── Helpers ─────────────────────────────────────────────────────────────────

namespace {
    RepoStatus::Change delta_to_change(git_delta_t d)
    {
        switch (d) {
            case GIT_DELTA_ADDED:     return RepoStatus::Change::Added;
            case GIT_DELTA_DELETED:    return RepoStatus::Change::Deleted;
            case GIT_DELTA_MODIFIED:   return RepoStatus::Change::Modified;
            case GIT_DELTA_RENAMED:    return RepoStatus::Change::Renamed;
            case GIT_DELTA_COPIED:     return RepoStatus::Change::Copied;
            case GIT_DELTA_TYPECHANGE: return RepoStatus::Change::TypeChange;
            default:                   return RepoStatus::Change::Unmodified;
        }
    }
} // anonymous namespace

// ─── GitEngine ───────────────────────────────────────────────────────────────

GitEngine::GitEngine()  { git_libgit2_init(); }
GitEngine::~GitEngine() { close(); git_libgit2_shutdown(); }

// ─── Init / Open / Close ─────────────────────────────────────────────────────

GitResult<git_repository*> GitEngine::init(const std::string& path, bool bare)
{
    git_repository* out = nullptr;
    int err = git_repository_init(&out, path.c_str(), bare ? 1 : 0);
    if (err < 0) { set_error(err); return {nullptr, last_error_}; }
    repo_ = out;
    return {out, {}};
}

GitResult<git_repository*> GitEngine::open(const std::string& path)
{
    git_repository* out = nullptr;
    std::string target = path.empty() ? "." : path;
    int err = git_repository_open(&out, target.c_str());
    if (err < 0) { set_error(err, "git_repository_open failed: " + target); return {nullptr, last_error_}; }
    repo_ = out;
    return {out, {}};
}

void GitEngine::close()
{
    if (repo_) { git_repository_free(repo_); repo_ = nullptr; }
}

// ─── Error helpers ────────────────────────────────────────────────────────────

GitError GitEngine::git_error()
{
    const ::git_error* e = ::git_error_last();
    return { e ? e->klass : -1, e ? e->message : "unknown error" };
}

void GitEngine::set_error(int code, const std::string& msg)
{
    last_error_ = {code, msg.empty() ? git_error().message : msg};
}

void GitEngine::set_error(int code) { set_error(code, {}); }

// ─── Signature ───────────────────────────────────────────────────────────────

GitResult<git_signature*> GitEngine::make_signature(const std::string& name,
                                                     const std::string& email) const
{
    git_signature* sig = nullptr;
    int err = git_signature_now(&sig,
        name.empty() ? "kGit User" : name.c_str(),
        email.empty() ? "kgit@localhost" : email.c_str());
    if (err < 0) return {nullptr, git_error()};
    return {sig, {}};
}

// ─── Status ─────────────────────────────────────────────────────────────────

GitResult<std::vector<RepoStatus>> GitEngine::status_all()
{
    if (!repo_) return {{}, {ENOTRECOVERABLE, "No repository open"}};

    git_status_list* list = nullptr;
    int err = git_status_list_new(&list, repo_, nullptr);
    if (err < 0) { set_error(err); return {{}, last_error_}; }

    size_t count = git_status_list_entrycount(list);
    std::vector<RepoStatus> result;
    result.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        const git_status_entry* entry = git_status_byindex(list, i);
        if (entry->head_to_index) {
            RepoStatus s;
            s.staged = true;
            s.path = entry->head_to_index->new_file.path
                     ? entry->head_to_index->new_file.path : "";
            s.change = delta_to_change(entry->head_to_index->status);
            result.push_back(s);
        }
        if (entry->index_to_workdir) {
            RepoStatus ws;
            ws.staged = false;
            ws.path = entry->index_to_workdir->new_file.path
                      ? entry->index_to_workdir->new_file.path : "";
            ws.change = delta_to_change(entry->index_to_workdir->status);
            result.push_back(std::move(ws));
        }
    }

    git_status_list_free(list);
    return {result, {}};
}

void GitEngine::status_refresh()
{
    if (!repo_) return;
    git_index* idx = nullptr;
    if (git_repository_index(&idx, repo_) == 0) {
        git_index_read(idx, 1);
        git_index_free(idx);
    }
}

// ─── Index operations ────────────────────────────────────────────────────────

GitResult<void> GitEngine::stage(const std::string& path)
{
    if (!repo_) { set_error(ENOTRECOVERABLE, "No repository open"); return {last_error_}; }
    git_index* idx = nullptr;
    int err = git_repository_index(&idx, repo_);
    if (err < 0) { set_error(err); return {last_error_}; }
    err = git_index_add_bypath(idx, path.c_str());
    git_index_write(idx);
    git_index_free(idx);
    if (err < 0) { set_error(err); return {last_error_}; }
    return {};
}

GitResult<void> GitEngine::unstage(const std::string& path)
{
    if (!repo_) { set_error(ENOTRECOVERABLE, "No repository open"); return last_error_; }
    git_index* idx = nullptr;
    int err = git_repository_index(&idx, repo_);
    if (err < 0) { set_error(err); return last_error_; }
    err = git_index_remove(idx, path.c_str(), 0);
    git_index_write(idx);
    git_index_free(idx);
    if (err < 0) { set_error(err); return last_error_; }
    return {};
}

GitResult<void> GitEngine::stage_all()
{
    if (!repo_) { set_error(ENOTRECOVERABLE, "No repository open"); return last_error_; }
    git_index* idx = nullptr;
    int err = git_repository_index(&idx, repo_);
    if (err < 0) { set_error(err); return last_error_; }
    err = git_index_add_all(idx, nullptr, 0, nullptr, nullptr);
    git_index_write(idx);
    git_index_free(idx);
    if (err < 0) { set_error(err); return last_error_; }
    return {};
}

GitResult<void> GitEngine::remove(const std::string& path)
{
    if (!repo_) { set_error(ENOTRECOVERABLE, "No repository open"); return last_error_; }
    git_index* idx = nullptr;
    int err = git_repository_index(&idx, repo_);
    if (err < 0) { set_error(err); return last_error_; }
    err = git_index_remove(idx, path.c_str(), 0);
    git_index_write(idx);
    git_index_free(idx);
    if (err < 0) { set_error(err); return last_error_; }
    return {};
}

// ─── Commit ─────────────────────────────────────────────────────────────────

GitResult<std::string> GitEngine::commit(const CommitOpts& opts)
{
    if (!repo_) return {{}, {ENOTRECOVERABLE, "No repository open"}};

    git_index* idx = nullptr;
    int err = git_repository_index(&idx, repo_);
    if (err < 0) { set_error(err); return {{}, last_error_}; }
    git_index_write(idx);
    git_oid tree_id;
    err = git_index_write_tree(&tree_id, idx);
    git_index_free(idx);
    if (err < 0) { set_error(err); return {{}, last_error_}; }

    git_tree* tree = nullptr;
    err = git_tree_lookup(&tree, repo_, &tree_id);
    if (err < 0) { set_error(err); return {{}, last_error_}; }

    std::vector<const git_commit*> parents;
    if (!opts.amend) {
        git_reference* head = nullptr;
        if (git_repository_head(&head, repo_) == 0) {
            git_commit* parent = nullptr;
            if (git_commit_lookup(&parent, repo_, git_reference_target(head)) == 0)
                parents.push_back(parent);
            git_reference_free(head);
        }
    } else {
        // For amend: look up the current HEAD commit directly
        git_reference* head = nullptr;
        if (git_repository_head(&head, repo_) == 0) {
            git_commit* head_cmt = nullptr;
            if (git_commit_lookup(&head_cmt, repo_, git_reference_target(head)) == 0)
                parents.push_back(head_cmt);
            git_reference_free(head);
        }
    }

    auto sig_result = make_signature("", "");
    if (!sig_result.ok()) { git_tree_free(tree); return {{}, sig_result.error}; }
    git_signature* sig = sig_result.value;

    git_oid commit_id;
    err = git_commit_create(&commit_id, repo_, "HEAD", sig, sig, nullptr,
                            opts.message.c_str(), tree,
                            static_cast<int>(parents.size()),
                            parents.data());

    for (auto p : parents) git_commit_free(const_cast<git_commit*>(p));
    git_tree_free(tree);
    git_signature_free(sig);
    if (err < 0) { set_error(err); return {{}, last_error_}; }
    return {oid_to_str(commit_id), {}};
}

GitResult<CommitInfo> GitEngine::head_commit()
{
    if (!repo_) return {{}, {ENOTRECOVERABLE, "No repository open"}};
    git_commit* cmt = nullptr;
    git_reference* head = nullptr;
    if (git_repository_head(&head, repo_) < 0) return {{}, {ENOENT, "HEAD not found"}};
    int err = git_commit_lookup(&cmt, repo_, git_reference_target(head));
    git_reference_free(head);
    if (err < 0) { set_error(err); return {{}, last_error_}; }
    CommitInfo info = commit_info_from_cmt(cmt);
    git_commit_free(cmt);
    return {info, {}};
}

// ─── Diff ───────────────────────────────────────────────────────────────────

namespace {
    int diff_file_cb(const git_diff_delta*, float, void* payload)
    {
        static_cast<std::vector<DiffFile>*>(payload)->emplace_back();
        return 0;
    }

    int diff_hunk_cb(const git_diff_delta* delta, const git_diff_hunk* hunk, void* payload)
    {
        auto* files = static_cast<std::vector<DiffFile>*>(payload);
        if (!files->empty() && hunk) {
            files->back().hunks.push_back(DiffHunk{});
            auto& hh = files->back().hunks.back();
            hh.header.assign(hunk->header, hunk->header_len);
            if (delta) {
                files->back().old_path = delta->old_file.path ? delta->old_file.path : "";
                files->back().new_path = delta->new_file.path ? delta->new_file.path : "";
            }
        }
        return 0;
    }

    int diff_line_cb(const git_diff_delta*, const git_diff_hunk*, const git_diff_line* line, void* payload)
    {
        auto* files = static_cast<std::vector<DiffFile>*>(payload);
        if (!files->empty() && !files->back().hunks.empty() && line) {
            files->back().hunks.back().lines.emplace_back(line->content, line->content_len);
            if (line->origin == GIT_DIFF_LINE_ADDITION) files->back().additions++;
            if (line->origin == GIT_DIFF_LINE_DELETION) files->back().deletions++;
        }
        return 0;
    }
} // anonymous namespace

DiffResult GitEngine::diff_workdir_to_index()
{
    DiffResult result;
    if (!repo_) { result.error = {ENOTRECOVERABLE, "No repository open"}; return result; }
    git_diff* diff = nullptr;
    int err = git_diff_index_to_workdir(&diff, repo_, nullptr, nullptr);
    if (err < 0) { result.error = git_error(); return result; }
    std::vector<DiffFile> files;
    git_diff_foreach(diff, diff_file_cb, nullptr, diff_hunk_cb, diff_line_cb, &files);
    git_diff_free(diff);
    result.files = std::move(files);
    return result;
}

DiffResult GitEngine::diff_index_to_head()
{
    DiffResult result;
    if (!repo_) { result.error = {ENOTRECOVERABLE, "No repository open"}; return result; }
    git_index* idx = nullptr;
    if (git_repository_index(&idx, repo_) < 0) { result.error = git_error(); return result; }

    // Get HEAD tree
    git_tree* tree = nullptr;
    git_reference* head = nullptr;
    if (git_repository_head(&head, repo_) == 0) {
        git_commit* head_cmt = nullptr;
        if (git_commit_lookup(&head_cmt, repo_, git_reference_target(head)) == 0)
            git_commit_tree(&tree, head_cmt);
        git_commit_free(head_cmt);
        git_reference_free(head);
    }
    if (!tree) { git_index_free(idx); result.error = {ENOENT, "HEAD commit not found"}; return result; }

    git_diff* diff = nullptr;
    int err = git_diff_tree_to_index(&diff, repo_, tree, idx, nullptr);
    git_index_free(idx);
    git_tree_free(tree);
    if (err < 0) { result.error = git_error(); return result; }
    std::vector<DiffFile> files;
    git_diff_foreach(diff, diff_file_cb, nullptr, diff_hunk_cb, diff_line_cb, &files);
    git_diff_free(diff);
    result.files = std::move(files);
    return result;
}

DiffResult GitEngine::diff_commits(const std::string& old_id, const std::string& new_id)
{
    DiffResult result;
    if (!repo_) { result.error = {ENOTRECOVERABLE, "No repository open"}; return result; }
    git_oid a, b;
    if (git_oid_fromstr(&a, old_id.c_str()) < 0 ||
        git_oid_fromstr(&b, new_id.c_str()) < 0) {
        result.error = {EINVAL, "Invalid commit id"}; return result;
    }
    git_commit *oc = nullptr, *nc = nullptr;
    if (git_commit_lookup(&oc, repo_, &a) < 0 || git_commit_lookup(&nc, repo_, &b) < 0) {
        result.error = {ENOENT, "Commit not found"}; return result;
    }
    git_tree *ot = nullptr, *nt = nullptr;
    git_commit_tree(&ot, oc);
    git_commit_tree(&nt, nc);
    git_diff* diff = nullptr;
    int err = git_diff_tree_to_tree(&diff, repo_, ot, nt, nullptr);
    git_commit_free(oc); git_commit_free(nc);
    git_tree_free(ot);  git_tree_free(nt);
    if (err < 0) { result.error = git_error(); return result; }
    std::vector<DiffFile> files;
    git_diff_foreach(diff, diff_file_cb, nullptr, diff_hunk_cb, diff_line_cb, &files);
    git_diff_free(diff);
    result.files = std::move(files);
    return result;
}

DiffResult GitEngine::diff_file_workdir(const std::string& path)
{
    DiffResult result;
    if (!repo_) { result.error = {ENOTRECOVERABLE, "No repository open"}; return result; }
    git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
    char* ps_str = const_cast<char*>(path.c_str());
    git_strarray ps = {&ps_str, 1};
    opts.pathspec = ps;
    git_diff* diff = nullptr;
    int err = git_diff_index_to_workdir(&diff, repo_, nullptr, &opts);
    if (err < 0) { result.error = git_error(); return result; }
    std::vector<DiffFile> files;
    git_diff_foreach(diff, diff_file_cb, nullptr, diff_hunk_cb, diff_line_cb, &files);
    git_diff_free(diff);
    result.files = std::move(files);
    return result;
}

// ─── Log ─────────────────────────────────────────────────────────────────────

GitResult<std::vector<CommitInfo>> GitEngine::log(size_t max_count)
{
    std::vector<CommitInfo> result;
    if (!repo_) return {{}, {ENOTRECOVERABLE, "No repository open"}};
    git_revwalk* walker = nullptr;
    if (git_revwalk_new(&walker, repo_) < 0) return {{}, git_error()};
    git_revwalk_push_head(walker);
    git_revwalk_sorting(walker, GIT_SORT_TIME);
    git_oid oid;
    size_t count = 0;
    while (git_revwalk_next(&oid, walker) == 0 && count < max_count) {
        git_commit* cmt = nullptr;
        if (git_commit_lookup(&cmt, repo_, &oid) < 0) break;
        result.push_back(commit_info_from_cmt(cmt));
        git_commit_free(cmt);
        ++count;
    }
    git_revwalk_free(walker);
    return {result, {}};
}

GitResult<std::vector<CommitInfo>> GitEngine::log_file(const std::string& path, size_t max_count)
{
    std::vector<CommitInfo> result;
    if (!repo_) return {{}, {ENOTRECOVERABLE, "No repository open"}};
    char* ps_str = const_cast<char*>(path.c_str());
    git_strarray ps_strarr = {&ps_str, 1};
    git_pathspec* ps = nullptr;
    if (git_pathspec_new(&ps, &ps_strarr) < 0) return {{}, git_error()};
    git_revwalk* walker = nullptr;
    if (git_revwalk_new(&walker, repo_) < 0) return {{}, git_error()};
    git_revwalk_push_head(walker);
    git_revwalk_sorting(walker, GIT_SORT_TIME);
    git_oid oid;
    size_t count = 0;
    while (git_revwalk_next(&oid, walker) == 0 && count < max_count) {
        git_commit* cmt = nullptr;
        if (git_commit_lookup(&cmt, repo_, &oid) < 0) break;
        git_tree* tree = nullptr;
        if (git_commit_tree(&tree, cmt) == 0) {
            git_pathspec_match_list* matches = nullptr;
            if (git_pathspec_match_tree(&matches, tree, GIT_PATHSPEC_NO_MATCH_ERROR, ps) == 0 && matches &&
                git_pathspec_match_list_entrycount(matches) > 0) {
                result.push_back(commit_info_from_cmt(cmt));
                ++count;
            }
            if (matches) git_pathspec_match_list_free(matches);
            git_tree_free(tree);
        }
        git_commit_free(cmt);
    }
    git_revwalk_free(walker);
    git_pathspec_free(ps);
    return {result, {}};
}

// ─── Branches ────────────────────────────────────────────────────────────────

GitResult<std::vector<BranchInfo>> GitEngine::branches()
{
    std::vector<BranchInfo> result;
    if (!repo_) return {{}, {ENOTRECOVERABLE, "No repository open"}};
    git_branch_iterator* it = nullptr;
    if (git_branch_iterator_new(&it, repo_, GIT_BRANCH_ALL) < 0) return {{}, git_error()};
    git_reference* ref = nullptr;
    git_branch_t type;
    while (git_branch_next(&ref, &type, it) == 0) {
        BranchInfo bi;
        bi.name = git_reference_shorthand(ref);
        bi.is_remote = (type == GIT_BRANCH_REMOTE);
        bi.is_head   = (type == GIT_BRANCH_LOCAL);
        git_reference* head = nullptr;
        if (git_repository_head(&head, repo_) == 0) {
            bi.is_current = (git_oid_cmp(git_reference_target(ref), git_reference_target(head)) == 0);
            git_reference_free(head);
        }
        result.push_back(std::move(bi));
        git_reference_free(ref);
    }
    git_branch_iterator_free(it);
    return {result, {}};
}

GitResult<git_reference*> GitEngine::branch_create(const std::string& name, const std::string& target)
{
    auto cr = resolve_commit(target.empty() ? "HEAD" : target);
    if (!cr.ok()) return {nullptr, cr.error};
    git_reference* out = nullptr;
    int err = git_branch_create(&out, repo_, name.c_str(), cr.value, 0);
    git_commit_free(cr.value);
    if (err < 0) { set_error(err); return {nullptr, last_error_}; }
    return {out, {}};
}

GitResult<void> GitEngine::branch_checkout(const std::string& name, const CheckoutOpts& opts)
{
    return checkout(name, opts);
}

GitResult<void> GitEngine::branch_delete(const std::string& name)
{
    auto r = resolve_ref(name);
    if (!r.ok()) return last_error_;
    int err = git_branch_delete(r.value);
    git_reference_free(r.value);
    if (err < 0) { set_error(err); return last_error_; }
    return {};
}

GitResult<void> GitEngine::branch_rename(const std::string& old_name, const std::string& new_name)
{
    auto r = resolve_ref(old_name);
    if (!r.ok()) return last_error_;
    git_reference* out = nullptr;
    int err = git_branch_move(&out, r.value, new_name.c_str(), 0);
    git_reference_free(r.value);
    if (err < 0) { set_error(err); return last_error_; }
    return {};
}

// ─── Remotes ─────────────────────────────────────────────────────────────────

GitResult<std::vector<RemoteInfo>> GitEngine::remotes()
{
    std::vector<RemoteInfo> result;
    if (!repo_) return {{}, {ENOTRECOVERABLE, "No repository open"}};
    git_strarray names = {};
    if (git_remote_list(&names, repo_) < 0) return {{}, git_error()};
    for (size_t i = 0; i < names.count; ++i) {
        git_remote* remote = nullptr;
        if (git_remote_lookup(&remote, repo_, names.strings[i]) < 0) continue;
        RemoteInfo ri;
        ri.name = names.strings[i];
        ri.fetch_url = git_remote_url(remote) ? git_remote_url(remote) : "";
        ri.push_url  = git_remote_pushurl(remote) ? git_remote_pushurl(remote) : "";
        result.push_back(std::move(ri));
        git_remote_free(remote);
    }
    git_strarray_free(&names);
    return {result, {}};
}

GitResult<void> GitEngine::remote_add(const std::string& name, const std::string& url)
{
    if (!repo_) { set_error(ENOTRECOVERABLE, "No repository open"); return last_error_; }
    git_remote* remote = nullptr;
    int err = git_remote_create(&remote, repo_, name.c_str(), url.c_str());
    if (err < 0) { set_error(err); return last_error_; }
    git_remote_free(remote);
    return {};
}

GitResult<void> GitEngine::remote_remove(const std::string& name)
{
    if (!repo_) { set_error(ENOTRECOVERABLE, "No repository open"); return last_error_; }
    int err = git_remote_delete(repo_, name.c_str());
    if (err < 0) { set_error(err); return last_error_; }
    return {};
}

GitResult<void> GitEngine::fetch(const std::string& remote, const std::string&)
{
    if (!repo_) { set_error(ENOTRECOVERABLE, "No repository open"); return last_error_; }
    git_remote* r = nullptr;
    std::string name = remote.empty() ? "origin" : remote;
    if (git_remote_lookup(&r, repo_, name.c_str()) < 0) return last_error_;
    git_fetch_options opts = GIT_FETCH_OPTIONS_INIT;
    int err = git_remote_fetch(r, nullptr, &opts, nullptr);
    git_remote_free(r);
    if (err < 0) { set_error(err); return last_error_; }
    return {};
}

GitResult<void> GitEngine::push(const std::string& remote, const std::string& refspec, bool)
{
    if (!repo_) { set_error(ENOTRECOVERABLE, "No repository open"); return last_error_; }
    git_remote* r = nullptr;
    if (git_remote_lookup(&r, repo_, remote.c_str()) < 0) return last_error_;
    char* rs = const_cast<char*>(refspec.c_str());
    git_strarray refs = {&rs, 1};
    git_push_options opts = GIT_PUSH_OPTIONS_INIT;
    int err = git_remote_push(r, &refs, &opts);
    git_remote_free(r);
    if (err < 0) { set_error(err); return last_error_; }
    return {};
}

// ─── Checkout ───────────────────────────────────────────────────────────────

GitResult<void> GitEngine::checkout(const std::string& target, const CheckoutOpts& opts)
{
    if (!repo_) { set_error(ENOTRECOVERABLE, "No repository open"); return last_error_; }
    git_object* obj = nullptr;
    int err = git_revparse_single(&obj, repo_, target.c_str());
    if (err < 0) { set_error(err); return last_error_; }
    git_checkout_options copts = GIT_CHECKOUT_OPTIONS_INIT;
    copts.checkout_strategy = opts.force ? GIT_CHECKOUT_FORCE : GIT_CHECKOUT_SAFE;
    if (opts.create_branch && !opts.branch_name.empty()) {
        git_commit* cmt = nullptr;
        if (git_commit_lookup(&cmt, repo_, git_object_id(obj)) < 0) {
            git_object_free(obj); set_error(-1, "Cannot create branch from non-commit"); return last_error_;
        }
        git_reference* new_ref = nullptr;
        err = git_branch_create(&new_ref, repo_, opts.branch_name.c_str(), cmt, 0);
        git_commit_free(cmt);
        if (err < 0) { git_object_free(obj); set_error(err); return last_error_; }
        if (new_ref) git_reference_free(new_ref);
    }
    err = git_checkout_tree(repo_, obj, &copts);
    if (err < 0) { set_error(err); git_object_free(obj); return last_error_; }
    git_repository_set_head(repo_, target.c_str());
    git_object_free(obj);
    return {};
}

// ─── Merge ───────────────────────────────────────────────────────────────────

GitResult<void> GitEngine::merge(const std::string& branch)
{
    if (!repo_) { set_error(ENOTRECOVERABLE, "No repository open"); return last_error_; }
    auto ref_r = resolve_ref(branch);
    if (!ref_r.ok()) return last_error_;
    git_commit* their_commit = nullptr;
    int err = git_commit_lookup(&their_commit, repo_, git_reference_target(ref_r.value));
    if (err < 0) { git_reference_free(ref_r.value); set_error(err); return last_error_; }
    git_reference* head = nullptr;
    git_commit* our_commit = nullptr;
    if (git_repository_head(&head, repo_) == 0) {
        git_commit_lookup(&our_commit, repo_, git_reference_target(head));
        git_reference_free(head);
    }
    git_merge_options mopts = GIT_MERGE_OPTIONS_INIT;
    git_index* index = nullptr;
    err = git_merge_commits(&index, repo_, our_commit, their_commit, &mopts);
    if (our_commit) git_commit_free(our_commit);
    git_commit_free(their_commit);
    git_reference_free(ref_r.value);
    if (err < 0) { set_error(err); return last_error_; }
    if (index) { git_index_write(index); git_index_free(index); }
    return {};
}

// ─── Tags ───────────────────────────────────────────────────────────────────

GitResult<std::vector<TagInfo>> GitEngine::tags()
{
    std::vector<TagInfo> result;
    if (!repo_) return {{}, {ENOTRECOVERABLE, "No repository open"}};
    git_strarray tag_names = {};
    if (git_tag_list(&tag_names, repo_) < 0) return {{}, git_error()};
    for (size_t i = 0; i < tag_names.count; ++i) {
        TagInfo ti;
        ti.name = tag_names.strings[i];
        git_reference* ref = nullptr;
        std::string ref_path = "refs/tags/" + std::string(tag_names.strings[i]);
        if (git_reference_lookup(&ref, repo_, ref_path.c_str()) == 0) {
            if (git_reference_is_tag(ref)) {
                git_object* obj = nullptr;
                if (git_reference_peel(&obj, ref, GIT_OBJ_ANY) == 0 && obj) {
                    if (git_object_type(obj) == GIT_OBJ_TAG) {
                        git_tag* t = reinterpret_cast<git_tag*>(obj);
                        ti.target_id = oid_to_str(*git_tag_target_id(t));
                        ti.message   = git_tag_message(t) ? git_tag_message(t) : "";
                        const git_signature* sig = git_tag_tagger(t);
                        if (sig) { ti.tagger_name = sig->name ? sig->name : ""; ti.tagger_email = sig->email ? sig->email : ""; ti.time = sig->when.time; }
                    } else {
                        ti.target_id = oid_to_str(*git_object_id(obj));
                    }
                    git_object_free(obj);
                }
            }
            git_reference_free(ref);
        }
        result.push_back(std::move(ti));
    }
    git_strarray_free(&tag_names);
    return {result, {}};
}

GitResult<void> GitEngine::tag_create(const std::string& name, const std::string& target,
                                       const std::string& message)
{
    if (!repo_) { set_error(ENOTRECOVERABLE, "No repository open"); return last_error_; }
    git_object* obj = nullptr;
    if (git_revparse_single(&obj, repo_, target.c_str()) < 0) return last_error_;
    if (message.empty()) {
        git_reference_symbolic_create(nullptr, repo_, ("refs/tags/" + name).c_str(),
                                      target.c_str(), 0, nullptr);
        git_object_free(obj);
    } else {
        auto sig = make_signature("", "");
        if (!sig.ok()) { git_object_free(obj); return last_error_; }
        git_oid oid;
        int err = git_tag_create(&oid, repo_, name.c_str(), obj, sig.value, message.c_str(), 0);
        git_signature_free(sig.value);
        git_object_free(obj);
        if (err < 0) { set_error(err); return last_error_; }
    }
    return {};
}

GitResult<void> GitEngine::tag_delete(const std::string& name)
{
    if (!repo_) { set_error(ENOTRECOVERABLE, "No repository open"); return last_error_; }
    int err = git_tag_delete(repo_, name.c_str());
    if (err < 0) { set_error(err); return last_error_; }
    return {};
}

// ─── Tree / Blob ─────────────────────────────────────────────────────────────

GitResult<std::string> GitEngine::blob_content(const std::string& commitish, const std::string&)
{
    if (!repo_) return {{}, {ENOTRECOVERABLE, "No repository open"}};
    git_object* obj = nullptr;
    if (git_revparse_single(&obj, repo_, commitish.c_str()) < 0) return {{}, git_error()};
    git_blob* blob = nullptr;
    int err = git_blob_lookup_prefix(&blob, repo_, git_object_id(obj), 0);
    git_object_free(obj);
    if (err < 0) { set_error(err); return {{}, last_error_}; }
    std::string content(static_cast<const char*>(git_blob_rawcontent(blob)),
                        static_cast<size_t>(git_blob_rawsize(blob)));
    git_blob_free(blob);
    return {content, {}};
}

GitResult<std::vector<std::string>> GitEngine::tree_ls(const std::string& commitish)
{
    std::vector<std::string> result;
    if (!repo_) return {{}, {ENOTRECOVERABLE, "No repository open"}};
    git_object* obj = nullptr;
    if (git_revparse_single(&obj, repo_, commitish.c_str()) < 0) return {{}, git_error()};
    git_tree* tree = nullptr;
    if (git_commit_tree(&tree, reinterpret_cast<git_commit*>(obj)) < 0) {
        git_object_free(obj); return {{}, git_error()};
    }
    for (size_t i = 0; i < git_tree_entrycount(tree); ++i)
        result.push_back(git_tree_entry_name(git_tree_entry_byindex(tree, i)));
    git_tree_free(tree);
    git_object_free(obj);
    return {result, {}};
}

// ─── Clone ───────────────────────────────────────────────────────────────────

namespace {
    int cred_acquire_cb(git_cred** out, const char*, const char*, unsigned int allowed_types, void* payload)
    {
        auto* cb = static_cast<std::function<bool(std::string&, std::string&)>*>(payload);
        if (!cb) return GIT_EUSER;
        std::string user, pass;
        if ((*cb)(user, pass)) {
            if (allowed_types & GIT_CREDENTIAL_USERPASS_PLAINTEXT)
                return git_cred_userpass_plaintext_new(out, user.c_str(), pass.c_str());
            if (allowed_types & GIT_CREDENTIAL_SSH_KEY)
                return git_cred_ssh_key_new(out, user.c_str(), nullptr, pass.c_str(), nullptr);
        }
        return GIT_EUSER;
    }

    int transfer_progress_cb(const git_transfer_progress* stats, void* payload)
    {
        auto* opts = static_cast<CloneOpts*>(payload);
        if (opts && opts->transfer_progress_cb)
            opts->transfer_progress_cb(nullptr, stats->received_objects, static_cast<int>(stats->total_objects));
        return 0;
    }
} // anonymous namespace

GitResult<git_repository*> GitEngine::clone(const std::string& url, const CloneOpts& opts)
{
    git_repository* out = nullptr;
    git_clone_options copts = GIT_CLONE_OPTIONS_INIT;
    if (opts.progress_cb) {
        copts.fetch_opts.callbacks.transfer_progress = transfer_progress_cb;
        copts.fetch_opts.callbacks.payload = const_cast<CloneOpts*>(&opts);
    }
    if (opts.cred_cb) {
        copts.fetch_opts.callbacks.credentials = cred_acquire_cb;
        copts.fetch_opts.callbacks.payload = const_cast<CloneOpts*>(&opts);
    }
    if (!opts.branch.empty()) copts.checkout_branch = opts.branch.c_str();
    std::string target = opts.working_dir.value_or(".");
    int err = git_clone(&out, url.c_str(), target.c_str(), &copts);
    if (err < 0) { set_error(err, "clone failed: " + url); return {nullptr, last_error_}; }
    repo_ = out;
    return {out, {}};
}

// ─── Resolve helpers ─────────────────────────────────────────────────────────

GitResult<git_reference*> GitEngine::resolve_ref(const std::string& name)
{
    git_reference* ref = nullptr;
    int err = git_reference_lookup(&ref, repo_, name.c_str());
    if (err < 0) err = git_reference_lookup(&ref, repo_, ("refs/heads/" + name).c_str());
    if (err < 0) { set_error(err); return {nullptr, last_error_}; }
    return {ref, {}};
}

GitResult<git_commit*> GitEngine::resolve_commit(const std::string& commitish)
{
    git_object* obj = nullptr;
    int err = git_revparse_single(&obj, repo_, commitish.c_str());
    if (err < 0) { set_error(err); return {nullptr, last_error_}; }
    if (git_object_type(obj) != GIT_OBJ_COMMIT) {
        git_object_free(obj);
        set_error(EINVAL, "Not a commit: " + commitish);
        return {nullptr, last_error_};
    }
    return {reinterpret_cast<git_commit*>(obj), {}};
}

// ─── Static helpers ─────────────────────────────────────────────────────────

std::string GitEngine::oid_to_str(const git_oid& oid)
{
    char buf[GIT_OID_HEXSZ + 1];
    git_oid_fmt(buf, &oid);
    buf[GIT_OID_HEXSZ] = '\0';
    return buf;
}

// ─── CommitInfo builder ─────────────────────────────────────────────────────

CommitInfo GitEngine::commit_info_from_cmt(git_commit* cmt) const
{
    CommitInfo info;
    info.id       = oid_to_str(*git_commit_id(cmt));
    info.short_id = info.id.substr(0, 7);
    info.summary  = git_commit_summary(cmt);
    info.body     = git_commit_body(cmt) ? git_commit_body(cmt) : "";
    info.time     = git_commit_time(cmt);
    const git_signature* a = git_commit_author(cmt);
    if (a) { info.author_name = a->name ? a->name : ""; info.author_email = a->email ? a->email : ""; }
    const git_signature* c = git_commit_committer(cmt);
    if (c) { info.committer_name = c->name ? c->name : ""; info.committer_email = c->email ? c->email : ""; }
    info.parent_ids.reserve(git_commit_parentcount(cmt));
    for (unsigned i = 0; i < git_commit_parentcount(cmt); ++i)
        info.parent_ids.push_back(oid_to_str(*git_commit_parent_id(cmt, i)));
    return info;
}
