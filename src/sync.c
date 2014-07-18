/**
 * @file
 * Core libgit2-based file-syncing algorithm.
 */

#include "sync.h"
#include <git2/sys/commit.h>
#include <string.h>

#define git_check(f) if ((e = f) < 0) goto exit;

#define SYNC_REFSPEC                    "refs/heads/master:refs/heads/incoming"
#define SYNC_REF_REMOTE                 "refs/heads/incoming"
#define SYNC_REF_MASTER                 "refs/heads/master"
#define SYNC_GIT_NAME                   "wallet"
#define SYNC_GIT_EMAIL                  "wallet@airbitz.co"

/**
 * Checks out the given branch.
 */
static int sync_checkout(git_repository *repo,
                         const char *name)
{
    int e = 0;
    git_signature *sig = NULL;

    git_check(git_signature_now(&sig, SYNC_GIT_NAME, SYNC_GIT_EMAIL));
    git_check(git_repository_set_head(repo, name, sig, "checkout"));

    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = GIT_CHECKOUT_FORCE | GIT_CHECKOUT_REMOVE_UNTRACKED;
    git_check(git_checkout_head(repo, &opts));

exit:
    if (sig)            git_signature_free(sig);
    return e;
}

/**
 * Commits a tree to the master branch of the repository.
 */
static int sync_commit_master(git_repository *repo,
                              const char *message,
                              git_oid *tree_id,
                              size_t parent_count,
                              const git_oid *parents[])
{
    int e;
    git_signature *sig = NULL;
    git_oid commit_id;

    git_check(git_signature_now(&sig, SYNC_GIT_NAME, SYNC_GIT_EMAIL));

    git_check(git_commit_create_from_ids(&commit_id, repo, SYNC_REF_MASTER,
        sig, sig, NULL, message,
        tree_id, parent_count, parents));

exit:
    if (sig)            git_signature_free(sig);
    return e;
}

/**
 * Advances the ref with the given name, causing it to point to the object
 * with the given id.
 */
static int sync_fast_forward(git_repository *repo,
                             const char *name,
                             git_oid *id)
{
    int e = 0;
    git_signature *sig = NULL;
    git_reference *old_ref = NULL;
    git_reference *new_ref = NULL;

    git_check(git_signature_now(&sig, SYNC_GIT_NAME, SYNC_GIT_EMAIL));
    e = git_reference_lookup(&old_ref, repo, name);
    if (!e)
    {
        git_reference_set_target(&new_ref, old_ref, id, sig, "fast-forward");
    }
    else if (e == GIT_ENOTFOUND)
    {
        git_check(git_reference_create(&new_ref, repo, name, id, 0, sig, "create branch"));
    }

exit:
    if (sig)            git_signature_free(sig);
    if (old_ref)        git_reference_free(old_ref);
    if (new_ref)        git_reference_free(new_ref);
    return e;
}

/**
 * Fetches the contents of the server into the "incoming" branch.
 */
static int sync_fetch(git_repository *repo, const char *server)
{
    int e;
    git_signature *sig = NULL;
    git_remote *remote = NULL;

    git_check(git_signature_now(&sig, SYNC_GIT_NAME, SYNC_GIT_EMAIL));
    git_check(git_remote_create_anonymous(&remote, repo, server, SYNC_REFSPEC));
    git_check(git_remote_fetch(remote, sig, "fetch"));

exit:
    if (sig)        git_signature_free(sig);
    if (remote)     git_remote_free(remote);
    return e;
}

/**
 * Reads the tree object out of a commit object, or returns an empty tree
 * if the commit id is zero.
 */
static int sync_get_tree(git_oid *out,
                         git_repository *repo,
                         const git_oid *commit_id)
{
    int e;
    git_treebuilder *tb = NULL;
    git_commit *commit = NULL;

    if (git_oid_iszero(commit_id))
    {
        git_check(git_treebuilder_create(&tb, NULL));
        git_check(git_treebuilder_write(out, repo, tb));
    }
    else
    {
        git_check(git_commit_lookup(&commit, repo, commit_id));
        git_oid_cpy(out, git_commit_tree_id(commit));
    }

exit:
    if (tb)             git_treebuilder_free(tb);
    if (commit)         git_commit_free(commit);
    return e;
}

/**
 * Determines whether or not the working directory has non-committed changes.
 */
static int sync_local_dirty(int *out,
                            git_repository *repo,
                            git_oid *commit_id)
{
    int e = 0;
    git_tree *tree = NULL;
    git_diff *diff = NULL;

    git_oid tree_id;
    git_check(sync_get_tree(&tree_id, repo, commit_id));
    git_check(git_tree_lookup(&tree, repo, &tree_id));

    git_diff_options diff_options = GIT_DIFF_OPTIONS_INIT;
    diff_options.flags |= GIT_DIFF_INCLUDE_UNTRACKED;
    git_check(git_diff_tree_to_workdir(&diff, repo, tree, &diff_options));

    *out = git_diff_num_deltas(diff);

exit:
    if (tree)           git_tree_free(tree);
    if (diff)           git_diff_free(diff);
    return e;
}

/**
 * Looks up a reference, ignoring not-found errors.
 */
static int sync_lookup_soft(git_oid *out,
                            git_repository *repo,
                            const char *name)
{
    int e = git_reference_name_to_id(out, repo, name);
    if (e == GIT_ENOTFOUND)
    {
        giterr_clear();
        return 0;
    }
    return e;
}

/**
 * Creates a git tree object representing the state of the working directory.
 */
static int sync_workdir_tree(git_oid *out,
                             git_repository *repo)
{
    int e;
    git_index *index = NULL;

    git_check(git_repository_index(&index, repo));
    git_check(git_index_clear(index));
    git_strarray paths = {NULL, 0};
    git_check(git_index_add_all(index, &paths, 0, NULL, NULL));
    git_check(git_index_write_tree(out, index));

exit:
    if (index)          git_index_free(index);
    return e;
}

/**
 * Merges two tree objects, producing a third tree.
 * The base tree allows the algorithm to distinguish between adds and deletes.
 * The algorithm always prefers the item from tree1 when there is a conflict.
 */
static int sync_merge_trees(git_oid *out,
                            git_repository *repo,
                            const git_oid *base_id,
                            const git_oid *id1,
                            const git_oid *id2)
{
    int e;
    git_tree *base_tree = NULL;
    git_tree *tree1 = NULL;
    git_tree *tree2 = NULL;
    git_treebuilder *tb = NULL;
    size_t size1, size2;
    size_t i1 = 0;
    size_t i2 = 0;
    const git_tree_entry *e1;
    const git_tree_entry *e2;
    enum { ONLY1 = 1, ONLY2 = 2, BOTH = 3 } state = BOTH;

    git_check(git_tree_lookup(&base_tree, repo, base_id));
    git_check(git_tree_lookup(&tree1, repo, id1));
    git_check(git_tree_lookup(&tree2, repo, id2));
    git_check(git_treebuilder_create(&tb, NULL));
    size1 = git_tree_entrycount(tree1);
    size2 = git_tree_entrycount(tree2);

    while (1)
    {
        // Advance to next file:
        if (state == ONLY1 || state == BOTH)
        {
            e1 = i1 < size1 ? git_tree_entry_byindex(tree1, i1++) : NULL;
        }
        if (state & ONLY2 || state == BOTH)
        {
            e2 = i2 < size2 ? git_tree_entry_byindex(tree2, i2++) : NULL;
        }

        // Determine state:
        if (e1 && e2)
        {
            int s = strcmp(git_tree_entry_name(e1), git_tree_entry_name(e2));
            state = s < 0 ? ONLY1 :
                    s > 0 ? ONLY2 :
                    BOTH;
        }
        else if (e1 && !e2)
        {
            state = ONLY1;
        }
        else if (!e1 && e2)
        {
            state = ONLY2;
        }
        else
        {
            break;
        }

        // Grab the entry in question:
        const git_tree_entry *entry =
            (state == ONLY1 || state == BOTH) ? e1 : e2;
        const git_tree_entry *base_entry =
            git_tree_entry_byname(base_tree, git_tree_entry_name(entry));

        // Decide what to do with the entry:
        if (state == BOTH && base_entry &&
            GIT_OBJ_TREE == git_tree_entry_type(e1) &&
            GIT_OBJ_TREE == git_tree_entry_type(e2) &&
            GIT_OBJ_TREE == git_tree_entry_type(base_entry))
        {
            // Merge sub-trees:
            git_oid new_tree;
            git_check(sync_merge_trees(&new_tree, repo,
                git_tree_entry_id(base_entry),
                git_tree_entry_id(e1),
                git_tree_entry_id(e2)));
            git_check(git_treebuilder_insert(NULL, tb,
                git_tree_entry_name(e1),
                &new_tree,
                git_tree_entry_filemode(e1)));
        }
        else if (state == BOTH || !base_entry)
        {
            // Entry was added, or already present:
            git_check(git_treebuilder_insert(NULL, tb,
                git_tree_entry_name(entry),
                git_tree_entry_id(entry),
                git_tree_entry_filemode(entry)));
        }
    }

    // Write tree:
    git_check(git_treebuilder_write(out, repo, tb));

exit:
    if (base_tree)      git_tree_free(base_tree);
    if (tree1)          git_tree_free(tree1);
    if (tree2)          git_tree_free(tree2);
    if (tb)             git_treebuilder_free(tb);
    return e;
}

static int sync_push_cb(const char *ref, const char *msg, void *data)
{
    if (msg)
    {
        giterr_set_str(GITERR_REPOSITORY, msg);
        return GIT_ENONFASTFORWARD;
    }
    return 0;
}

/**
 * Pushes the master branch to the server.
 */
static int sync_push(git_repository *repo,
                     const char *server)
{
    int e;
    git_remote *remote = NULL;
    git_push *push = NULL;

    git_check(git_remote_create_anonymous(&remote, repo, server, SYNC_REFSPEC));
    git_check(git_remote_connect(remote, GIT_DIRECTION_PUSH));

    git_check(git_push_new(&push, remote));
    git_check(git_push_add_refspec(push, "refs/heads/master"));
    git_check(git_push_finish(push));

    if (!git_push_unpack_ok(push))
    {
        giterr_set_str(GITERR_REPOSITORY, "Remote could not unpack objects");
        e = GIT_ERROR;
        goto exit;
    }
    git_check(git_push_status_foreach(push, sync_push_cb, NULL));

exit:
    if (remote)     git_remote_free(remote);
    if (push)       git_push_free(push);
    return e;
}

/**
 * Updates the master branch with the latest changes, including local
 * changes and changes from the remote repository.
 */
static int sync_master(git_repository *repo,
                       int *need_checkout,
                       int *need_push)
{
    int e = 0;
    git_oid master_id = {{0}};
    git_oid remote_id = {{0}};
    git_oid base_id = {{0}};
    int master_dirty = 0;
    int remote_dirty = 0;
    int local_dirty = 0;

    // Find the relevant commit objects:
    git_check(sync_lookup_soft(&master_id, repo, SYNC_REF_MASTER));
    git_check(sync_lookup_soft(&remote_id, repo, SYNC_REF_REMOTE));
    if (!git_oid_iszero(&remote_id) && !git_oid_iszero(&master_id))
    {
        e = git_merge_base(&base_id, repo, &master_id, &remote_id);
        if (e < 0 && e != GIT_ENOTFOUND)
        {
            goto exit;
        }
    }

    // Figure out what needs syncing:
    master_dirty = git_oid_cmp(&master_id, &base_id);
    remote_dirty = git_oid_cmp(&remote_id, &base_id);
    git_check(sync_local_dirty(&local_dirty, repo, &master_id));

    if (remote_dirty)
    {
        if (master_dirty || local_dirty)
        {
            // 3-way merge:
            git_oid local_tree;
            git_oid base_tree;
            git_oid remote_tree;
            git_check(sync_workdir_tree(&local_tree, repo));
            git_check(sync_get_tree(&remote_tree, repo, &remote_id));
            git_check(sync_get_tree(&base_tree, repo, &base_id));

            // Do merge:
            git_oid merged_tree;
            git_check(sync_merge_trees(&merged_tree, repo, &base_tree, &remote_tree, &local_tree));

            // Commit to master:
            char const *message =
                local_dirty ? "merge local changes" : "merge";
            if (git_oid_iszero(&master_id))
            {
                const git_oid *parents[] = {&remote_id};
                git_check(sync_commit_master(repo, message, &merged_tree, 1, parents));
            }
            else
            {
                const git_oid *parents[] = {&master_id, &remote_id};
                git_check(sync_commit_master(repo, message, &merged_tree, 2, parents));
            }
        }
        else
        {
            // Fast-forward to remote:
            git_check(sync_fast_forward(repo, SYNC_REF_MASTER, &remote_id));
        }
    }
    else if (local_dirty)
    {
        // Commit local changes:
        git_oid local_tree;
        git_check(sync_workdir_tree(&local_tree, repo));
        if (git_oid_iszero(&master_id))
        {
            const git_oid *parents[] = {NULL};
            git_check(sync_commit_master(repo, "first commit", &local_tree, 0, parents));
        }
        else
        {
            const git_oid *parents[] = {&master_id};
            git_check(sync_commit_master(repo, "commit local changes", &local_tree, 1, parents));
        }
    }

    // Report the outcome:
    *need_checkout = remote_dirty;
    *need_push = local_dirty || master_dirty;

exit:
    return e;
}

int sync_repo(git_repository *repo,
              const char *server)
{
    int e = 0;
    int need_checkout, need_push;

    git_check(sync_fetch(repo, server));
    git_check(sync_master(repo, &need_checkout, &need_push));
    if (need_checkout)
    {
        git_check(sync_checkout(repo, SYNC_REF_MASTER));
    }
    if (need_push)
    {
        git_check(sync_push(repo, server));
    }

exit:
    return e;
}
