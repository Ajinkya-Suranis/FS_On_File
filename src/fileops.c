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

	if ((rd = internal_read(fh->fh_fsh->fsh_mem->fsm_devfd, mino, intbuf, offset,
				len)) != (int)len) {
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
				len)) != len) {
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
