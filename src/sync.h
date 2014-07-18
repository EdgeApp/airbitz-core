/**
 * @file
 * Core libgit2-based file-syncing algorithm.
 */

#ifndef SYNC_H
#define SYNC_H

#include <git2.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Syncs with a remote repository.
 * Performs a fetch, a file-system sync, and a push (if necessary).
 * @return 1 if files have changed on disk, 0 if the sync was successful
 * but produced no changes, or < 0 to indicate an error.
 */
int sync_repo(git_repository *repo, const char *server);

#ifdef __cplusplus
}
#endif

#endif
