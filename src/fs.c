#include "fs.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

/*
 * if error is returned from this function, then the contents
 * of buffer 'buf' should not be trusted and be relied upon.
 */

int
read_dir(
	int		fd,
	struct dinode	*dp,
	fs_u64_t	offset,
	char		*buf,
	int		buflen)
{
	fs_u64_t	blkno, off, len;
	fs_u64_t	sz = dp->size, remain = buflen, count = 0;
	int		direntlen = sizeof(struct direntry);
	int		error = 0;

	/*
	 * validate the arguments first.
	 * Check specifically for 'buflen'; it should
	 * be multuple of 'sizeof(struct direntry)'.
	 */

	if (buflen <= 0 || !buf || !dp || (buflen % direntlen != 0)) {
		return EINVAL;
	}
	if (dp->mode != IFDIR) {
		return ENOTDIR;
	}

	/*
	 * if the directory is empty, we still return success
	 * along with zeroing out the passed buffer.
	 */

	if (sz == 0) {
		bzero(buf, buflen);
		return 0;
	}
	while (1) {
		assert(count <= buflen && count <= dp->size);
		if (count == buflen || count == sz) {
			break;
		}
		if ((error = bmap(fd, dp, &blkno, &off, &len, offset)) != 0) {
			return error;
		}
		readlen = MIN(len, MIN(sz, remain));
		lseek(fd, off, SEEK_SET);
		if (read(fd, (buf + count), readlen) != readlen) {
			error = errno;
			return error;
		}
		offset += readlen;
		count += readlen;
		remain -= readlen;
		sz -= readlen;
	}
	if (buflen > count) {
		bzero(buf + count, remain);
	}
	return 0;
}

/* TODO: free the *dp pointer at the end */

int
lookup_path(
	int		fd,
	char		*path,
	struct direntry	**dep)
{
	struct direntry	*de, *de1 = NULL;
	struct dinode	*dp = NULL;
	fs_u64_t	offset = 0, inum = MNTPT_INO;
	char		*buf = NULL;
	int		i = 0, j, nentries;
	int		error = 0, buflen, low, high;

	buf = (char *)malloc(INDIR_BLKSZ);
	if (!buf) {
		return ENOMEM;
	}
	if (dep) {
		de1 = (struct direntry *)malloc(sizeof(struct direntry));
		if (!de1) {
			free(buf);
			return ENOMEM;
		}
	}
	for (;;) {
		if ((error = iget(fd, inum, &dp)) != 0) {
			goto out;
		}
		if (dp->mode != IFDIR) {
			error = ENOTDIR;
			goto out;
		}
		low = i;
		while (path[i] != '/' && path[i] != '\0') {
			i++;
		}
		high = i - 1;
		offset = 0;
		buflen = (int)((dp->size - offset > INDIR_BLKSZ) ?
			INDIR_BLKSZ  : (dp->size - offset));
		while (1) {
			if ((error = read_dir(fd, dp, offset, buf,
					      buflen)) != 0) {
				goto out;
			}
			de = (struct direntry *)buf;
			nentries = buflen/sizeof(struct direntry);
			for (j = 0; j < nentries; j++) {
				assert(de->inum != 0);
				if (bcmp(path + low, de->name,
					 high - low + 1) == 0) {
					if (dep) {
						bcopy(de, de1,
						      sizeof(struct direntry));
					}
					break;
				}
				de++;
			}
			if (j < nentries) {
				break;
			}
			offset += buflen;
			if (offset == dp->size) {
				error = ENOENT:
				goto out;
			}
			buflen = (int)((dp->size - offset > INDIR_BLKSZ) ?
					INDIR_BLKSZ  : (dp->size - offset));
		}
		inum = de->inumber;
		if (path[i] == '\0') {
			break;
		}
	}

out:
	free(buf);
	if (dp) {
		free(dp);
	}
	return error;
}

/* a very simple read routine.
 */

int
fsread(
	int		fd,
	fs_u64_t	offset,
	int		len,
	char		*buf)
{
	fs_u64_t	off, foff, sz, blkno;
	int		remain = len, readlen;
	int		nread = 0, error = 0;

	/* TODO: add a code to check whether offset
	   is greater than maximum supported file size
	   set errno to EOVERFLOW if that turns out to be true.
	 */

	if (!buffer || !len) {
		errno = EINVAL;
		return 0;
	}
	while (nread < len) {
		if ((error = bmap(fd, &blkno, &sz, &off, offset)) != 0) {
			errno = error;
			goto out;
		}
		readlen = MIN(remain, (int)sz);
		foff = (blkno * ONE_K) + off;
		lseek(fd, foff, SEEK_SET);
		if (read(fd, (buf + nread), readlen) != readlen) {
			goto out;
		}
		offset += (fs_u64_t)readlen;
		nread += readlen;
	}

out:
	return nread;
}
