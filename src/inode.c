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

#define ILIST_EXTSIZE	16
#define IMAP_EXTSIZE	8

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
	/*
	 * TODO: implement the functionality of 'lastino'
	 */
	/*
	if (inum > sb->lastino) {
		fprintf(stderr, "inode number %llu is invalid for %s\n",
			inum, fsm->fsm_mntpt);
		return NULL;
	}*/
	offset = inum << LOG_INOSIZE;
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
	offset = (blkno << LOG_ONE_K) + off;
	lseek(fsm->fsm_devfd, offset, SEEK_SET);
	if (read(fsm->fsm_devfd, &mino->mino_dip, sizeof(struct dinode)) !=
		 sizeof(struct dinode)) {
		fprintf(stderr, "Failed to read inode %llu\n", inum);
		free(mino);
		return NULL;
	}
	mino->mino_number = inum;
	mino->mino_fsm = fsm;
	mino->mino_bno = blkno;
	return mino;
}

/*
 * Write the inode on disk.
 */

int
iwrite(
	struct minode	*ino)
{
	fs_u64_t	offset;

	assert(ino != NULL && ino->mino_fsm != NULL && ino->mino_bno != 0);
	offset = (ino->mino_bno << LOG_ONE_K) +
		  ((ino->mino_number) << LOG_INOSIZE);
	fprintf(stdout, "INFO: writing inode number %llu at ilist block number"
		" %llu\n", ino->mino_number, ino->mino_bno);
	lseek(ino->mino_fsm->fsm_devfd, offset, SEEK_SET);
	if (write(ino->mino_fsm->fsm_devfd, &ino->mino_dip,
		  INOSIZE) != INOSIZE) {
		fprintf(stderr, "ERROR: failed to write inode number %llu:"
			" %s\n",ino->mino_number, strerror(errno));
		return 1;
	}

	return 0;
}

/*
 * Get free inode.
 * This comes into picture whenever an inode needs
 * to be allocated.
 */

static int
get_free_inum(
	struct fsmem	*fsm,
	fs_u64_t	*inump)
{
	fs_u64_t	off = 0, blkno;
	fs_u64_t	inum = 0;
	fs_u32_t	rd, len;
	char		*buf = NULL, bit;
	int		i, j, error = 0, nbytes = 0;

	*inump = 0;

	/*
	 * If there is a free inode in the already allocated
	 * extents corresponding to IMAP file (IFIMP) then
	 * we don't need to allocate a new extent.
	 */

	if ((fsm->fsm_imapip->size << 3) > fsm->fsm_sb->iused) {
		buf = (char *)malloc(ONE_K);
		if (!buf) {
			fprintf(stderr, "ERROR: Failed to allocate memory "
				"for imap buffer\n");
			return ENOMEM;
		}
		memset(buf, 0, ONE_K);
		while (1) {
			if ((rd = internal_read(fsm->fsm_devfd, fsm->fsm_imapip,
						buf, off, ONE_K)) != ONE_K) {
				fprintf(stderr, "ERROR: Failed to read imap "
					"file for %s\n", fsm->fsm_mntpt);
				return errno;
			}
			/*
			 * scan the buffer and look for first set bit
			 */
			for (i = 0; i < ONE_K; i++) {
				if (buf[i] & 0xff != 0) {
					break;
				}
			}
			inum += (fs_u64_t)i << 3;
			if (i != ONE_K) {
				/*
				 * found the free inode.
				 * reset the bit.
				 */
				for (bit = 1, j = 0; j < 8; j++) {
					if (bit & buf[i]) {
						buf[i] &= ~bit;
						break;
					}
					bit <<= 1;
					inum++;
				}
				break;
			}
			off += ONE_K;
		}
		assert(off < fsm->fsm_imapip->size);
		fprintf(stdout, "Found the inode %llu free\n", inum);
		if (metadata_write(fsm, off, buf, fsm->fsm_imapip) != ONE_K) {
			error = errno;
			free(buf);
			return error;
		}

		/*
		 * Write the superblock after incrementing the
		 * used inode count.
		 */

		fsm->fsm_sb->iused++;
		lseek(fsm->fsm_devfd, SB_OFFSET, SEEK_SET);
		if (write(fsm->fsm_devfd, fsm->fsm_sb,
		    sizeof(struct super_block)) != sizeof(struct super_block)) {
			fprintf(stderr, "get_free_inum: Failed to write "
				"super block\n");
			error = errno;
		}
		free(buf);
		return error;
	}

	/*
	 * We need to allocate new extent to imap file.
	 * For now, we're allocating extent of 8 blocks.
	 */

	if ((error = bmap_alloc(fsm, fsm->fsm_imapip, IMAP_EXTSIZE,
				&blkno, &len)) != 0) {
		fprintf("get_free_inum: imap allocation failed for %s\n",
			fsm->fsm_mntpt);
		return error;
	}
	nbytes = IMAP_EXTSIZE << LOG_ONE_K;
	buf = (char *) malloc(nbytes);
	if (!buf) {
		fprintf(stderr, "get_free_inum: memory allocation failed for "
			"imap extent for %s\n", fsm->fsm_mntpt);
		return ENOMEM;
	}
	memset(buf, 0xff, nbytes);
	buf[0] |= 0x1;
	lseek(fsm->fsm_devfd, blkno << LOG_ONE_K, SEEK_SET);
	if (write(fsm->fsm_devfd, buf, nbytes) != nbytes) {
		error = errno;
		fprintf(stderr, "get_free_inum: Failed to write new imap extent"
			" at %llu for %s\n", blkno, fsm->fsm_mntpt);
		free(buf);
		return error;
	}

	/*
	 * New extent of 8K means 8K * 8 = 64K new free inodes, one
	 * of which will be utilized and rest are marked free in imap.
	 */

	fsm->fsm_sb->iused += nbytes << 3 - 1;
	lseek(fsm->fsm_devfd, SB_OFFSET, SEEK_SET);
	if (write(fsm->fsm_devfd, fsm->fsm_sb, sizeof(struct super_block)) !=
	    sizeof(struct super_block)) {
		fprintf(stderr, "get_free_inum: Failed to write super block"
			" for %s\n", fsm->fsm_mntpt);
		error = errno;
	} else {
		*inump = inum;
	}

	free(buf);
	return error;
}

/*
 * Add an inode entry into the ilist file.
 * If required, do the allocation in the fixed
 * size of 64 blocks. If allocation doesn't succeed
 * for those many blocks, then any non-zero number
 * of blocks would work.
 */

static int
add_ilist_entry(
	struct fsmem	*fsm,
	fs_u64_t	inum,
	fs_u32_t	type)
{
	struct dinode	dp;
	fs_u64_t	blkno, offset, off;
	fs_u32_t	len;
	char		*buf = NULL;
	int		error = 0;

	assert((fsm->fsm_sb->iused - 1) << LOG_INOSIZE <= fsm->fsm_ilip->size);
	assert((inum + 1) << LOG_INOSIZE  <= fsm->fsm_ilip->size);

	if ((inum + 1) << LOG_INOSIZE == fsm->fsm_ilip->size) {

		/*
		 * The ilist file is full of used inodes; no entry for
		 * a new one. Allocate an extent of 16 blocks for the
		 * ilist file.
		 */

		if ((error = bmap_alloc(fsm, fsm->fsm_ilip, ILIST_EXTSIZE,
					&blkno, &len)) != 0) {
			fprintf(stderr, "add_ilist_entry: ilist allocation "
				"failed for %s\n", fsm->fsm_mntpt);
			return error;
		}
		buf = (char *)malloc(ILIST_EXTSIZE);
		if (!buf) {
			fprintf(stderr, "add_ilist_entry: failed to allocate "
				"memory for ilist extent for %s\n",
				fsm->fsm_mntpt);
			return ENOMEM;
		}
		memset(buf, 0, ILIST_EXTSIZE);
		memset(&dp, 0, sizeof(struct dinode));

		/*
 		 * Initialize the inode data.
		 */

		dp.type = type;
		dp.size = ILIST_EXTSIZE << LOG_ONE_K;
		dp.orgtype = ORG_DIRECT;
		memcpy(buf, &dp, sizeof(struct dinode));

		offset = blkno << LOG_ONE_K;
		lseek(fsm->fsm_devfd, offset, SEEK_SET);
		if (write(fsm->fsm_devfd, buf, ILIST_EXTSIZE << LOG_ONE_K) !=
			  ILIST_EXTSIZE << LOG_ONE_K) {
			error = errno;
			fprintf(stderr, "add_ilist_entry: failed to write "
				"new ilist extent for %s\n", fsm->fsm_mntpt);
			free(buf);
			return error;
		}

		/*
		 * Increase the size of ilist inode by 16 blocks
		 */

		fsm->fsm_ilip->mino_dip.size += ILIST_EXTSIZE << LOG_ONE_K;
		if (error = iwrite(fsm->fsm_ilip)) {
			fprintf(stderr, "add_ilist_entry: failed to add inode "
				"number %llu to ilist for %s\n", inum,
				fsm->fsm_mntpt);
		}
		free(buf);
		return error;
	}

	/*
	 * We got a free slot inside an already allocated ilist
	 * extent. Write the inode metadata in that slot.
	 */

	offset = inum << LOG_INOSIZE;
	dp.type = type;
	dp.size = ILIST_EXTSIZE << LOG_ONE_K;
	dp.orgtype = ORG_DIRECT;
	if ((error = bmap(fsm->fsm_devfd, fsm->fsm_ilip, &blkno, &len, &off,
			  offset)) != 0) {
		fprintf(stderr, "add_ilist_entry: bmap failed at offset %llu"
			" for ilist inode of %s\n", offset, fsm->fsm_mntpt);
		return error;
	}
	offset = blkno << LOG_ONE_K + off;
	fprintf(stdout, "add_ilist_entry: INFO: Writing inode %llu at offset"
		" %llu for %s\n", inum, offset, fsm->fsm_mntpt);
	lseek(fsm->fsm_devfd, offset, SEEK_SET);
	if (write(fsm->fsm_devfd, &dp, sizeof(struct dinode)) !=
	    sizeof(struct dinode)) {
		fprintf(stderr, "add_ilist_entry: failed to write inode %llu"
			" to ilist for %s\n", inum, fsm->fsm_mntpt);
		error = errno;
	}

	return error;
}

/*
 * Allocate a new inode.
 */

int
inode_alloc(
	struct fsmem	*fsm,
	fs_u32_t	type,
	fs_u64_t	*inump)
{
	int		error = 0;

	/*
	 * Get the free inode number from imap first.
	 */

	if ((error = get_free_inum(fsm, inump)) != 0) {
		fprintf(stderr, "inode_alloc: Failed to get free inode "
			"for %s\n", fsm->fsm_mntpt);
		return error;
	}
	if ((error = add_ilist_entry(fsm, *inump, type)) != 0) {
		fprintf(stderr, "inode_alloc: Failed to add ilist entry "
			"of %llu for %s\n", *inump, fsm->fsm_mntpt);
	}

	return error;
}
