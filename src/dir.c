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
	struct direntry	ent, *buf = NULL;
	fs_u64_t	blkno, len, offset = 0;
	int		i, error = 0, nent, remain;

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
				parent->mino_number, fsm->fsm_mntpt);
			return error;
		}
		assert(len <= DIR_ALLOCSZ);
		buf = (struct direntry *)malloc(len << LOG_ONE_K);
		memset(buf, 0, len << LOG_ONE_K);
		strncpy(buf->name, name, strlen(name));
		buf->inumber = inum;
		lseek(fsm->fsm_devfd, blkno << LOG_ONE_K, SEEK_SET);
		printf("add_direntry: Writing at %llu blkno\n", blkno);
		if (write(fsm->fsm_devfd, buf, len << LOG_ONE_K) !=
			  len << LOG_ONE_K) {
			fprintf(stderr, "add_direntry: Failed to write "
				"new directory block %llu for %s\n", blkno,
				fsm->fsm_mntpt);
			free(buf);
			return errno;
		}
		printf("add_direntry: Current dir size is: %llu\n",
			parent->mino_size);
		parent->mino_size += DIRENTRY_LEN;
	} else {

		/*
		 * we've got some free space inside the existing directory
		 * blocks. If we've free space at the end of directory
		 * blocks, then we don't need to search the blocks for
		 * free space. If even the last directory entry is occupied
		 * then we need to do the search.
		 */

		memset(&ent, 0, DIRENTRY_LEN);
		strncpy(ent.name, name, strlen(name));
		ent.inumber = inum;
		if (parent->mino_size < (parent->mino_nblocks << LOG_ONE_K)) {
			if (metadata_write(fsm, parent->mino_size, (char *)&ent,
					   DIRENTRY_LEN, parent) !=
					   DIRENTRY_LEN) {
				return errno;
			}
			parent->mino_size += DIRENTRY_LEN;
		} else {
			buf = (struct direntry *)malloc(DIR_ALLOCSZ << LOG_ONE_K);
			if (buf == NULL) {
				fprintf(stderr, "add_direntry: Failed to "
					"allocate memory for internal buffer"
					" for %s\n", fsm->fsm_mntpt);
				return ENOMEM;
			}
			memset(buf, 0, DIR_ALLOCSZ << LOG_ONE_K);

			/*
			 * Search for a vacant directory entry in the
			 * already allocated extents. We must be able to
			 * find such entry; if not, then there is some
			 * inconsistency in the directory metadata!
			 */

			for (;;) {
				if (offset == parent->mino_size) {
					break;
				}
				remain = MIN(parent->mino_size - offset,
					     DIR_ALLOCSZ << LOG_ONE_K);
				nent = internal_readdir(parent, buf, offset,
							remain/DIRENTRY_LEN);
				if (nent < remain/DIRENTRY_LEN) {
					free(buf);
					return errno;
				}
				for (i = 0; i < nent; i++) {

					/*
					 * look for the direntry for which
					 * inode number is zero.
					 * Since inode number of ilist inode
					 * is 0, it will be an exception
					 */

					if (offset == 0 && i == 0) {
						continue;
					}
					if (buf[i].inumber == 0) {
						fprintf(stdout, "Found vacant "
							"entry at %llu offset",
							offset);
						memcpy(buf[i].name, name,
							strlen(name));
						buf[i].inumber = inum;
						break;
					}
					offset += DIRENTRY_LEN;
				}
				if (i != nent) {
					break;
				}
				offset += (fs_u64_t)remain;
			}
			if (offset == parent->mino_size) {
				fprintf(stderr, "FATAL BUG: no free space "
					"found in directory blocks\n");
				assert(0);
				free(buf);
				return EIO;
			}
			if (metadata_write(fsm, offset, (char *)buf, remain,
					   parent) != remain) {
				free(buf);
				return errno;
			}
		}
	}

	parent->mino_dirspec.ds_ndirents++;
	error = iwrite(parent);
	if (buf) {
		free(buf);
	}

	return error;
}
