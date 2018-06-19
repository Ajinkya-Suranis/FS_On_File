#include "layout.h"
#include "types.h"
#include "fs.h"
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
	if ((error = bmap(
