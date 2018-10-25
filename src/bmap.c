#include "layout.h"
#include "types.h"
#include "fs.h"
#include "inode.h"
#include "allocate.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

static int	bmap_direct(struct minode *, fs_u64_t *, fs_u64_t *,
			    fs_u64_t *, fs_u64_t);
static int	bmap_indirect(int, struct minode *, fs_u64_t *, fs_u64_t *,
			      fs_u64_t *, fs_u64_t);
static int	bmap_2indirect(int, struct minode *, fs_u64_t *, fs_u64_t *,
			       fs_u64_t *, fs_u64_t);

static int
bmap_direct(
        struct minode   *mp,
        fs_u64_t        *blknop,
        fs_u64_t        *lenp,
        fs_u64_t        *offp,
        fs_u64_t        offset)
{
        int             i;
        fs_u64_t        total = 0, blkno, len;

	printf("Entered bmap_direct\n");
	assert(mp != NULL);
        for (i = 0; i < MAX_DIRECT; i++) {
                blkno = mp->mino_orgarea.dir[i].blkno;
                len = mp->mino_orgarea.dir[i].len;
		printf("bmap_direct: blkno %llu, len: %llu\n", blkno, len);
                if ((len == 0) || (total + (len << LOG_ONE_K)) > offset) {
                        break;
                }
                total += len << LOG_ONE_K;
        }
        if (i == MAX_DIRECT || len == 0) {
                return EINVAL;
        }
        *lenp = total + (len << LOG_ONE_K) - offset;
        *offp = offset - total;
        *blknop = blkno;

        return 0;
}

static int
bmap_indirect(
        int             fd,
        struct minode   *mp,
        fs_u64_t        *blknop,
        fs_u64_t        *lenp,
        fs_u64_t        *offp,
        fs_u64_t        offset)
{
        struct direct   *dir;
        int             i, j, ndirects, error = 0;
        char            *buf = NULL;
        fs_u64_t        total = 0, blkno, len;

        buf = (char *)malloc(INDIR_BLKSZ);
        if (buf == NULL) {
                return ENOMEM;
        }
        ndirects = INDIR_BLKSZ/(sizeof(struct direct));

        for (i = 0; i < MAX_INDIRECT; i++) {
                blkno = mp->mino_orgarea.indir[i].ind_blkno;
                lseek(fd, blkno << LOG_ONE_K, SEEK_SET);
                if (read(fd, buf, INDIR_BLKSZ) != INDIR_BLKSZ) {
                        error = errno;
                        goto out;
                }
                dir = (struct direct *)buf;
                for (j = 0; j < ndirects; j++) {
                        blkno = dir[j].blkno;
                        len = dir[j].len;
                        if (dir[j].len == 0 ||
                            (total + (len << LOG_ONE_K)) > offset) {
                                break;
                        }
                        total += (len << LOG_ONE_K);
                }
                if (j < ndirects) {
                        break;
                }
        }
        if (i == MAX_INDIRECT || len == 0) {
                /* couldn't find the block number.
		 * this means either the passed offset
		 * is dead wrong or there is fatal bug
		 * in the bmap code itself.
		 */
                error = EINVAL;
                goto out;
        }
        *lenp = total + (len << LOG_ONE_K) - offset;
        *offp = offset - total;
        *blknop = blkno;

out:
        free(buf);
        return error;
}

static int
bmap_2indirect(
        int             fd,
        struct minode   *mp,
        fs_u64_t        *blknop,
        fs_u64_t        *lenp,
        fs_u64_t        *offp,
        fs_u64_t        offset)
{
        struct direct   *dir;
        fs_u64_t        blkno, *indir, total = 0, len;
        char            *indirbuf = NULL, *dirbuf = NULL;
        int             i, j, k, nindirs, ndirs, error = 0;

        /*
	 * Firstly allocate a couple of INDIR_BLKSZ(8K) sized
	 * buffers for second and first indirect blocks
	 */

        indirbuf = (char *)malloc(INDIR_BLKSZ);
        if (!indirbuf) {
                return ENOMEM;
        }
        dirbuf = (char *)malloc(INDIR_BLKSZ);
        if (!dirbuf) {
                free(indirbuf);
                return ENOMEM;
        }
        nindirs = INDIR_BLKSZ/(sizeof(fs_u64_t));
        ndirs = INDIR_BLKSZ/(sizeof(struct direct));
        for (i = 0; i < MAX_INDIRECT; i++) {
                blkno = mp->mino_orgarea.indir[i].ind_blkno;
                lseek(fd, (blkno * ONE_K), SEEK_SET);
                if (read(fd, indirbuf, INDIR_BLKSZ) != INDIR_BLKSZ) {
                        error = errno;
                        goto out;
                }
                indir = (fs_u64_t *)indirbuf;
                for (j = 0; j < nindirs; j++) {
                        blkno = indir[j];
                        lseek(fd, (blkno * ONE_K), SEEK_SET);
                        if (read(fd, dirbuf, INDIR_BLKSZ) != INDIR_BLKSZ) {
                                error = errno;
                                goto out;
                        }
                        dir = (struct direct *)dirbuf;
                        for (k = 0; k < ndirs; k++) {
                                blkno = dir[k].blkno;
                                len = dir[k].len;
                                if (len == 0 ||
                                    (total + (ONE_K * len)) > offset) {
                                        break;
                                }
                                total += (ONE_K * len);
                        }
                        if (k < ndirs) {
                                break;
                        }
                }
                if (j < nindirs) {
                        break;
                }
        }
        if (len == 0 || i == MAX_INDIRECT) {
                error = EINVAL;
                goto out;
        }
        *lenp = total + (ONE_K * len) - offset;
        *offp = offset - total;
        *blknop = blkno;

out:
        free(indirbuf);
        free(dirbuf);
        return error;
}

/*
 * Convert direct orgtype to indirect and add
 * the extent entry to hte inode.
 */

static int
bmap_direct_to_indirect(
	struct fsmem	*fsm,
	struct minode	*mino,
	fs_u64_t	blkno,
	fs_u64_t	len)
{
	struct direct	*dir = NULL;
	fs_u64_t	off, blk, ln;
	fs_u32_t	extsz = INDIR_BLKSZ >> LOG_ONE_K;
	int		error = 0, i;

	if ((error = allocate(fsm, extsz , &blk, &ln)) != 0 || ln < extsz) {
		return error;
	}
	dir = (struct direct *)malloc(INDIR_BLKSZ);
	if (!dir) {
		fprintf(stderr, "bmap_direct_to_indirect: failed to allocate "
			"memory for indirect extent for %s\n", fsm->fsm_mntpt);
		return ENOMEM;
	}
	memset((void *)dir, 0, INDIR_BLKSZ);

	/*
	 * Copy the direct entries into the indirect extent buffer
	 * and write it to disk.
	 */

	for (i = 0; i < MAX_DIRECT; i++) {
		dir[i].blkno = mino->mino_orgarea.dir[i].blkno;
		dir[i].len = mino->mino_orgarea.dir[i].len;
		assert(dir[i].blkno != 0 && dir[i].len != 0);
	}

	/*
	 * Update the orgtype of inode on-disk and add indirect
	 * extent entry into the org area.
	 */

	dir[i].blkno = blkno;
	dir[i].len = len;
	bzero(&mino->mino_orgarea, sizeof(union org));
	mino->mino_orgarea.indir[0].ind_blkno = blk;
	mino->mino_orgtype = ORG_INDIRECT;

	off = blk << LOG_ONE_K;
	lseek(fsm->fsm_devfd, off, SEEK_SET);
	if (write(fsm->fsm_devfd, dir, INDIR_BLKSZ) != INDIR_BLKSZ) {
		fprintf(stderr, "bmap_direct_to_indirect: failed to write "
			"indirect block extent for %s\n", fsm->fsm_mntpt);
		error = errno;
	}
	free(dir);
	return error;
}

/*
 * Add an extent entry into the direct area of
 * inode. If there is no free space in direct area
 * of inode, then thr orgtype needs to be converted
 * to indirect.
 */

static int
bmap_direct_alloc(
	struct fsmem	*fsm,
	struct minode	*ino,
	fs_u64_t	blkno,
	fs_u64_t	len)
{
	int		i, error = 0;

	for (i = 0; i < MAX_DIRECT; i++) {
		if (ino->mino_orgarea.dir[i].blkno == 0) {
			break;
		}
	}
	if (i != MAX_DIRECT) {
		printf("Adding %llu blkno and %llu len to inode\n",
			blkno, len);
		/*
		 * we've found a vacant entry in inode.
		 * Fill it with new extent entry.
		 */
		ino->mino_orgarea.dir[i].blkno = blkno;
		ino->mino_orgarea.dir[i].len = len;
	} else {
		if ((error = bmap_direct_to_indirect(fsm, ino, blkno,
						     len)) != 0) {
			return error;
		}
	}
	error = iwrite(ino);
	return error;
}

int
bmap(
	int		fd,
	struct minode	*mp,
        fs_u64_t        *blknop,
        fs_u64_t        *lenp,
        fs_u64_t        *offp,
        fs_u64_t        offset)
{
        int             error = 0;

        assert(mp->mino_orgtype == ORG_DIRECT ||
	       mp->mino_orgtype == ORG_INDIRECT ||
               mp->mino_orgtype == ORG_2INDIRECT);
        if (mp->mino_orgtype == ORG_DIRECT) {
                error = bmap_direct(mp, blknop, lenp,
                                    offp, offset);
        } else if (mp->mino_orgtype == ORG_INDIRECT) {
                error = bmap_indirect(fd, mp, blknop, lenp,
                                      offp, offset);
        } else {
                error = bmap_2indirect(fd, mp, blknop, lenp,
                                       offp, offset);
        }

        return error;
}

/*
 * Allocate an extent and add its entry in
 * the bmap of an inode.
 */

int
bmap_alloc(
	struct fsmem	*fsm,
	struct minode	*ino,
	fs_u64_t	req,
	fs_u64_t	*blknop,
	fs_u64_t	*lenp)
{
	int		error;

	printf("Entered Writing inode \n");
	assert(ino->mino_orgtype == ORG_DIRECT ||
	       ino->mino_orgtype == ORG_INDIRECT ||
	       ino->mino_orgtype == ORG_2INDIRECT);

	if ((error = allocate(fsm, req, blknop, lenp)) != 0) {
		return error;
	}
	if (ino->mino_orgtype == ORG_DIRECT) {
		error = bmap_direct_alloc(fsm, ino, *blknop, *lenp);
	}/*
	} else if (ino->mino_orgtype == ORG_INDIRECT) {
		error = bmap_indirect_alloc(fsm, ino, *blknop, len);
	} else {
		error = bmap_2indirect_alloc(fsm, ino, *blknop, len);
	}*/

	/*
	 * In case of allocation success, increase the 'nblocks'
	 * of the inode and write to disk.
	 * Increasing the size of inode (if applicable) is the
	 * responsibility of caller.
	 */

	ino->mino_nblocks += *lenp;
	error = iwrite(ino);

	return error;
}
