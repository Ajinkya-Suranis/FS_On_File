#include "layout.h"
#include "types.h"
#include "fs.h"
#include "inode.h"
#include <errno.h>
#include <assert.h>
#include "fileops.h"

#define EMAP_BLKSZ	8192

static fs_u64_t	traverse_emapbuf(char *, fs_u64_t, fs_u64_t *, int);

static fs_u64_t
traverse_emapbuf(
	char		*buf,
	fs_u64_t	req,
	fs_u64_t	*lenp,
	int		bufsz)
{
	int		i, j, next = 0;
	fs_u64_t	nbits = 0, start, end;

	start = end = -1;
	for (i = 0; i < bufsz; i++) {
		next = 0;
		for (j = 0; j < 8; j++) {
			if (buf[i] & (1 << j)) {
				if (start == -1) {
					assert(next == 0);
					start = end = nbits + (fs_u64_t)j;
				} else {
					end++;
				}
				buf[i] &= ~(1 << j);
				if (j == 7) {
					next = 1;
				}
			}
		}
		if (start == -1) {
			continue;
		}
		if ((end - start + 1) >= req || !next) {
			break;
		}
		nbits += 8;
	}
	if (start == -1) {
		return 0;
	}
	*lenp = end - start + 1;
	return (nbits + start);
}

/*
 * Allocate the 'req' number of blocks.
 * Returns zero on successful allocation.
 * 'Successful' allocation does not mean
 * all 'req' number of blocks are allocated;
 * it means that we found at least one block
 * free in the file system.
 * The starting block number and length of
 * allocated chunk is filled in *blkno and *lenp
 * respectively.
 * The caller may need to call this function again
 * in case the allocated chunk size is less than
 * the requested one.
 */

int
allocate(
	struct fsmem	*fsm,
	fs_u64_t	req,
	fs_u64_t	*blknop,
	fs_u64_t	*lenp)
{
	fs_u64_t	off = 0, sz, blkno = 0;
	fs_u64_t	ret;
	char		*buf = NULL;
	int		rd, readsz, found = 0;

	*blknop = *lenp = 0;
	if (req == 0) {
		return EINVAL;
	}
	if (fsm->fsm_sb->freeblks == 0) {
		return ENOSPC;
	}
	buf = (char *)malloc(EMAP_BLKSZ);
	if (!buf) {
		fprintf(stderr, "allocate: Failed to allocate memory for "
			"emap buffer\n");
		return ENOMEM;
	}

	/*
	 * read the emap file in 8K chunks and return the
	 * continuous available blocks which satisfies the
	 * request. i.e. first fit match.
	 * TODO: Optimize this by categorizing the 32K block-
	 * sized allocation units.
	 */

	sz = fsm->fsm_emapip->mino_size;
	for (;;) {
		if (off >= sz) {
			break;
		}
		readsz = MIN(EMAP_BLKSZ, (int)(sz - off));
		memset(buf, 0, EMAP_BLKSZ);
		if ((rd = internal_read(fsm->fsm_devfd, fsm->fsm_emapip, buf,
				   off, readsz)) != readsz) {
			fprintf("allocate: Failed to read emap file at offset"
				" %llu for %s\n", off, fsm->fsm_mntpt);
			free(buf);
			return EIO;
		}

		/*
		 * Traverse the bitmap and look for continuous
		 * chunk of set bit(s).
		 */

		ret = traverse_emapbuf(buf, req, lenp, readsz);
		if (*lenp) {
			found = 1;
			blkno += ret;
			break;
		}
		blkno += (readsz << 3);
		off += readsz;
	}
	if (!found) {
		/*
		 * Super block says we've enough space,
		 * but found no free block in emap.
		 * This must be because of some inconsistency!
		 */
		assert(0);
		free(buf);
		return ENOSPC;
	}

	/*
	 * write the buffer to emap file, sinceit's changed.
	 * TODO: Fix issue #2 in github for optimization.
	 */

	*blknop = blkno;
	free(buf);
	return 0;
}
