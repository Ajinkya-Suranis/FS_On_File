#include "layout.h"
#include "types.h"
#include "fs.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

static int	bmap_direct(struct dinode *, fs_u64_t *, fs_u64_t *,
			    fs_u64_t *, fs_u64_t);
static int	bmap_indirect(int, struct dinode *, fs_u64_t *, fs_u64_t *,
			      fs_u64_t *, fs_u64_t);
static int	bmap_2indirect(int, struct dinode *, fs_u64_t *, fs_u64_t *,
			       fs_u64_t *, fs_u64_t);

static int
bmap_direct(
        struct dinode   *dp,
        fs_u64_t        *blknop,
        fs_u64_t        *lenp,
        fs_u64_t        *offp,
        fs_u64_t        offset)
{
        int             i;
        fs_u64_t        total = 0, blkno, len;

	assert(dp != NULL);
        for (i = 0; i < MAX_DIRECT; i++) {
                blkno = dp->orgarea.dir[i].blkno;
                len = dp->orgarea.dir[i].len;
                if ((len == 0) || (total + (ONE_K * len)) > offset) {
                        break;
                }
                total += (ONE_K * len);
        }
        if (i == MAX_DIRECT || len == 0) {
                return EINVAL;
        }
        *len = total + (ONE_K * len) - offset;
        *offp = blkno * ONE_K + (offset - total);
        *blknop = blkno;

        return 0;
}

static int
bmap_indirect(
        int             fd,
        struct dinode   *dp,
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
        ndirects = (int)(INDIR_BLKSZ/(sizeof(struct direct)));

        for (i = 0; i < MAX_INDIRECT; i++) {
                blkno = dp->orgarea.indir[i].blkno;
                lseek(fd, blkno * ONE_K, SEEK_SET);
                if (read(fd, buf, MAX_INDIRECT) != MAX_INDIRECT) {
                        error = errno;
                        goto out;
                }
                dir = (struct direct *)buf;
                for (j = 0; j < ndirects; j++) {
                        blkno = dir[j].blkno;
                        len = dir[j].len;
                        if (dir[j].len == 0 ||
                            (total + (ONE_K * len)) > offset) {
                                break;
                        }
                        total += (ONE_K * len);
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
        *lenp = total + (ONE_K * len) - offset;
        *offp = (blkno * ONE_K) + offset - total;
        *blkno = blkno;

out:
        free(buf);
        return error;
}

static int
bmap_2indirect(
        int             fd,
        struct dinode   *dp,
        fs_u64_t        *blknop,
        fs_u64_t        *lenp,
        fs_u64_t        *offp,
        fs_u64_t        offset)
{
        struct direct   *dir;
        fs_u64_t        blkno, *indir, total = 0;
        char            *indirbuf = NULL, *dirbuf = NULL;
        int             i, j, k, nindirs, ndirs, erro = 0;

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
                blkno = dp->orgarea.indir[i].blkno;
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
        *offp = (blkno * ONE_K) + offset - total;
        *blknop = blkno;

out:
        free(indirbuf);
        free(dirbuf);
        return error;
}

int
bmap(
	int		fd,
	struct dinode	*dp,
        fs_u64_t        *blknop,
        fs_u64_t        *lenp,
        fs_u64_t        *offp,
        fs_u64_t        offset)
{
        int             error = 0;

        assert(dp->orgtype == ORG_DIRECT || dp->orgtype == ORG_INDIRECT ||
               dp->orgtype == ORG_2INDIRECT);
        if (dp->orgtype == ORG_DIRECT) {
                error = bmap_direct(dp, blknop, lenp,
                                    offp, offset);
        } else if (dp->orgtype == ORG_INDIRECT) {
                error = bmap_indirect(fd, dp, blknop, lenp,
                                      offp, offset);
        } else {
                error = bmap_2indirect(fd, dp, blknop, lenp,
                                       offp, offset);
        }

        return error;
}
