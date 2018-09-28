#include "layout.h"
#include "types.h"
#include "fs.h"
#include "bmap.h"
#include "inode.h"
#include "fileops.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#define DIR_ALLOCSZ	8

/*
 * Add a file entry to the directory.
 */

int
add_direntry(
	struct fsmem	*fsm,
	struct minode	*parent,
	char		*name,
	fs_u64_t	inum)
{
	struct direntry	ent;
	fs_u64_t	blkno, len;
	char		*buf = NULL;
	int		error = 0;

	assert(inum != 0);
	assert((parent->mino_nblocks << LOG_ONE_K) >=
		parent->mino_dirspec.ds_ndirents * DIRENTRY_LEN);
	if ((parent->mino_nblocks << LOG_ONE_K) ==
	     parent->mino_dirspec.ds_ndirents * DIRENTRY_LEN) {

		/*
		 * The directory extents are full of entries; no space
		 * for a new one.
		 * Allocate a new extent to the directory and write the
		 * directory entry.
		 */

		if ((error = bmap_alloc(fsm, parent, DIR_ALLOCSZ, &blkno,
					&len)) != 0) {
			fprintf(stderr, "add_direntry: bmap allocation failed "
				"for directory inode %llu for %s\n",
				parent->mino_inumber, fsm->fsm_mntpt);
			return error;
		}
		assert(len <= DIR_ALLOCSZ);
		buf = (char *)malloc(len << LOG_ONE_K);
		memset(buf, 0, len << LOG_ONE_K);
		memset(&ent, 0, DIRENTRY_LEN);
		strncpy(ent->name, name, strlen(name));
		ent->inumber = inum;
		memcpy(buf, &ent, DIRENTRY_LEN);
		lseek(fsm->fsm_devfd, blkno << LOG_ONE_K, SEEK_SET);
		if (write(fsm->fsm_devfd, buf, len << LOG_ONE_K) !=
			  len << LOG_ONE_K) {
			fprintf(stderr, "add_direntry: Failed to write "
				"new directory block %llu for %s\n", blkno,
				fsm->fsm_mntpt);
			free(buf);
			return errno;
		}
	} else {

		/*
		 * we've got some free space inside the existing directory
		 * blocks. If we've free space at the end of directory
		 * blocks, then we don't need to search the blocks for
		 * free space. If even the last directory entry is occupied
		 * then we need to do the search.
		 */

		memset(&ent, 0, DIRENTRY_LEN);
		strncpy(ent->name, name, strlen(name));
		ent->inumber = inum;
		if (parent->mino_size < (parent->mino_nblocks << LOG_ONE_K)) {
			if (metadata_write(fsm, parent->mino_size, (char *)&ent,
					   DIRENTRY_LEN) != DIRENTRY_LEN) {
				return errno;
			}
			parent->mino_size += DIRENTRY_LEN;
		} else {
			buf = (char *)malloc(DIR_ALLOCSZ << LOG_ONE_K);
			if (buf == NULL) {
				fprintf(stderr, "add_direntry: Failed to "
					"allocate memory for internal buffer"
					" for %s\n", fsm->fsm_mntpt);
				return ENOMEM;
			}
			memset(buf, 0, DIR_ALLOCSZ << LOG_ONE_K);
			
		}
