#include "layout.h"
#include "types.h"
#include "fs.h"
#include "bmap.h"
#include "inode.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

struct minode *
iget(
	struct fsmem		*fsm,
	fs_u64_t		inum)
{
	struct super_block	*sb = fsm->fsm_sb;
	struct minode		*mino = NULL;
	fs_u64_t		offset;
	fs_u64_t		blkno, off, len;
	int			error = 0;

	assert(sb != NULL);
	if (inum > sb->lastino) {
		fprintf(stderr, "inode number %llu is invalid for %s\n",
			inum, fsm->fsm_mntpt);
		return NULL;
	}
	offset = inum * INOSIZE;
	mino = (struct minode *)malloc(sizeof(struct minode));
	if (!mino) {
		fprintf(stderr, "Failed to allocate memory for inode %llu\n",
			inum);
		return NULL;
	}
	if ((error = bmap(fsm->fsm_devfd, fsm->fsm_ilip, &blkno, &len,
			  &off, offset))) {
		fprintf(stderr, "Failed to bmap at %llu offset in ilist "
			"file\n", offset);
		free(mino);
		return NULL;
	}
	offset = (blkno * ONE_K) + off;
	lseek(fsm->fsm_devfd, offset, SEEK_SET);
	if (read(fsm->fsm_devfd, &mino->mino_dip, sizeof(struct dinode)) !=
		 sizeof(struct dinode)) {
		fprintf(stderr, "Failed to read inode %llu\n", inum);
		free(mino);
		return NULL;
	}
	mino->mino_number = inum;
	return mino;
}
