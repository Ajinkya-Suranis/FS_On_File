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

int	internal_read(int, struct minode *, char *, fs_u64_t, fs_u32_t);
int	lookup_path(void *, char *);

/*
 * Read the directory specified by 'fh' handle.
 * 'nentries' specifies number of entries to be
 * read.
 * Returns the number of directory entries
 * actually read.
 * 'buf' should be allocated enough to occupy
 * the required number of entries, otherwise
 * it could lead to segfault.
 */

int
fsread_dir(
	void			*fh,
	char			*buf,
	fs_u32_t		nentries)
{
	struct udirentry	*udir = NULL;
	struct direntry		*dir = NULL;
	struct minode		*mino = NULL;
	fs_u64_t		offset;
	fs_u32_t		len, i;
	char			*intbuf = NULL;
	char			*tmp = NULL;
	int			rd;

	if (nentries == 0) {
		return 0;
	}
	if (!buf || !fh) {
		errno = EINVAL;
		return 0;
	}
	memset(buf, 0, nentries * UDIRENTRY_LEN);
	tmp = intbuf = (char *) malloc(nentries * DIRENTRY_LEN);
	if (intbuf == NULL) {
		fprintf(stderr, "fsread_dir: Failed to allocate memory for "
			"internal buffer\n");
		return 0;
	}
	memset(intbuf, 0, nentries * DIRENTRY_LEN);
	mino = fh->fh_inode;
	offset = fh->fh_curoffset;
	assert(offset % DIRENTRY_LEN == 0);
	len = nentries * DIRENTRY_LEN;

	if ((rd = internal_read(fh->fh_fsh->fsh_mem->fsm_devfd, mino, intbuf,
				offset, len)) != (int)len && errno) {
		fprintf(stderr, "Failed to read directory inode %llu\n",
			mino->mino_number);
	}
	nentries = rd/DIRENTRY_LEN;
	for (i = 0; i < nentries; i++) {
		dir = (struct direntry *)intbuf;
		udir = (struct udirentry *)buf;
		strcpy(buf->udir_name, dir->name);
		buf->udir_inum = dir->inumber;
		buf += UDIRENTRY_LEN;
		intbuf += DIRENTRY_LEN;
	}

	return nentries;
}

int
internal_readdir(
	struct minode		*mino,
	char			*buf,
	fs_u64_t		offset,
	fs_u32_t		nentries)
{
	fs_u32_t		rd, len;

	assert(mino && buf && nentries);

	len = nentries * DIRENTRY_LEN;
	memset(buf, 0, len);
	if ((offset + len) > mino->mino_size) {
		len = (fs_u32_t)mino->mino_size - offset;
		nentries = len/DIRENTRY_LEN;
	}
	if (nentries == 0) {
		return 0;
	}
	if ((rd = internal_read(mino->mino_fsm->fsm_devfd, mino, buf, offset,
				len)) != len && errno) {
		fprintf(stderr, "internal_readdir failed for inode %llu\n",
			mino->mino_number);
	}

	return rd/DIRENTRY_LEN;
}

int
lookup_path(
	void		*fsh,
	char		*path)
{
	struct direntry	*dirp = NULL;
	struct minode	*mino = NULL;
	fs_u64_t	inum = MNTPT_INO, offset = 0;
	char		*buf = NULL;
	int		start = 1, end = strlen(path) - 1;
	int		i = 1, j, nent, found = 0;

	/*
	 * Allocate buffer for directory entries.
	 * We'll be reading 16 directory entries
	 * in each iteration.
	 */

	buf = (char *)malloc(16 * DIRENTRY_LEN);

	for (;;) {
		if (path[i] != '/' && path[i] != '\0') {
			end++;
			i++;
			continue;
		}
		if (start == end) {
			break;
		}
		fprintf(stdout, "Component: ");
		for (j = start; j <= end; j++) {
			fprintf(stdout, "%c", path[j]);
		}
		fprintf(stdout, "\n");
		mino = iget(fsh->fsh_mem, inum);
		if (mino == NULL) {
			fprintf(stderr, "lookup_path: Failed to read inode %llu"
				" for %s\n", inum, fsh->fsh_mem->fsm_mntpt);
			return 0;
		}
		found = 0;
		for (;;) {
			memset(buf, 0, 16 * DIRENTRY_LEN);
			nent = internal_readdir(mino, buf, offset, 16);
			if (nent == 0) {
				break;
			}
			dirp = (struct direntry *)buf;
			for (j = 0; j < nent; j++) {
				fprintf(stdout, "Trying to match with %s",
					dirp->name);
				if (strlen(dirp->name) == (end - start + 1) &&
				    strncmp(path + start, dirp->name,
				    (end - start + 1))) {
					fprintf(stdout, "Found matching entry:",
						dirp->name);
					found = 1;
					break;
				}
				dirp++;
			}
			if (!found) {
				goto out;
			}
			offset += nent * DIRENTRY_LEN;
		}
		start = end = ++i;
		inum = dirp->inumber;
	}

out:
	free(buf);
	return found;
}

int
internal_read(
	int		fd,
	struct minode	*mino,
	char		*buf,
	fs_u64_t	curoff,
	fs_u32_t	len)
{
	fs_u64_t	off, foff, sz, blkno;
	fs_u32_t	nread = 0, readlen;
	fs_u32_t	remain = len;
	int		error = 0;

	while (nread < len) {
                if ((error = bmap(fd, mino, &blkno, &sz, &off, curoff)) != 0) {
                        errno = error;
                        goto out;
                }
                readlen = MIN(remain, sz);
                foff = (blkno * ONE_K) + off;
                lseek(fd, foff, SEEK_SET);
                if (read(fd, (buf + nread), (int)readlen) != (int)readlen) {
                        fprintf(stderr, "Failed to read from inode %llu at"
                                " offset %llu\n", mino->mino_number, foff);
                        goto out;
                }
                curoff += (fs_u64_t)readlen;
                nread += readlen;
        }

out:
        return (int)nread;
}

int
fsread(
	void		*fh,
	char		*buf,
	fs_u32_t	len)
{
	struct minode	*mino = NULL;
	struct fsmem	*fsm = NULL;
	int		nread;
	int		error = 0, fd;

	if (len == 0) {
		return 0;
	}
	if (fh == NULL || buf == NULL) {
		errno = EINVAL;
		return 0;
	}
	fsm = fh->fh_fsh->fsh_mem;
	assert(fsm->fsm_devfd != 0);
	assert(fsm->fsm_sb && fsm->fsm_ilip && fsm->fsm_emapip &&
	       fsm->fsm_imapip && fsm->fsm_mntip);
	mino = fh->fh_inode;
	fd = fsm->fsm_devfd;
	assert(mino != NULL);
	nread = internal_read(fd, mino, buf, fh->fh_curoffset, len);
	fh->fh_curoffset += (fs_u64_t)readlen;

out:
	return nread;
}

/*
 * Create a new file or directory.
 */

void *
fscreate(
	void			*fsh,
	char			*path)
{
	struct file_handle	*fh = NULL;
	int			i, len = strlen(path), last;

	if (*path != '/') {
		fprintf(stderr, "ERROR: %s: path must be absolute i.e. "
			"must start with '/'\n", path);
		errno = EINVAL;
		return NULL;
	}
	if (len == 1) {
		fprintf(stderr, "ERROR: %s is invalid path\n", path);
		errno = EINVAL;
		return NULL;
	}

	/*
	 * First, do lookup on full path and see if the file/directory
	 * is already present. If so, return NULL.
	 * If it doesn't, lookup again for its to-be parent directory
	 * if parent isn't the root directory.
	 */

	if (lookup_path(fsh, path) != 0) {
		fprintf(stderr, "ERROR: The file %s already exists\n", path);
		return NULL;
	}
	for (i = 0, last = 0; i < len; i++) {
		if (path[i] == '/') {
			last = i;
		}
	}
	if (last != 0) {
		path[last] = '\0';
		if(lookup_path(fsh, path) == 0) {
			fprintf(stderr, "ERROR: %s doesn't exist\n", path);
			errno = ENOENT;
			return NULL;
		}
		path[last] = '/';
	}
	
