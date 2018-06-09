#include "fs.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

struct super_block		sb;
struct dinode			ildp;
uint32_t			emap_sz;

int
bmap_direct(
	struct dinode	*dp,
	uint64_t	*blknop,
	uint64_t	*lenp,
	uint64_t	*offp,
	uint64_t	offset)
{
	int		i;
	uint64_t	total = 0, blkno, len;

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

int
bmap_indirect(
	int		fd,
	struct dinode	*dp,
	uint64_t	*blknop,
	uint64_t	*lenp,
	uint64_t	*offp,
	uint64_t	offset)
{
	struct direct	*dir;
	int		i, j, ndirects, error = 0;
	char		*buf = NULL;
	uint64_t	total = 0, blkno, len;

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
		   this means either the passed offset
		   is dead wrong or there is fatal bug
		   in the bmap code itself.
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

int
bmap_2indirect(
	int		fd,
	struct dinode	*dp,
	uint64_t	*blknop,
	uint64_t	*lenp,
	uint64_t	*offp,
	uint64_t	offset)
{
	struct direct	*dir;
	uint64_t	blkno, *indir, total = 0;
	char		*indirbuf = NULL, *dirbuf = NULL;
	int		i, j, k, nindirs, ndirs, erro = 0;

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
	nindirs = INDIR_BLKSZ/(sizeof(uint64_t));
	ndirs = INDIR_BLKSZ/(sizeof(struct direct));
	for (i = 0; i < MAX_INDIRECT; i++) {
		blkno = dp->orgarea.indir[i].blkno;
		lseek(fd, (blkno * ONE_K), SEEK_SET);
		if (read(fd, indirbuf, INDIR_BLKSZ) != INDIR_BLKSZ) {
			error = errno;
			goto out;
		}
		indir = (uint64_t *)indirbuf;
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
	uint64_t	*blknop,
	uint64_t	*lenp,
	uint64_t	*offp,
	uint64_t	offset)
{
	int		error = 0;

	assert(dp->orgtype == ORG_DIRECT || dp->orgtype == ORG_INDIRECT ||
	       dp->orgtype == ORG_2INDIRECT);
	if (dp->orgtype == ORG_DIRECT) {
		error = bmap_direct(fd, dp, blknop, lenp,
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

int
iget(
	int		fd,
	uint64_t	inum,
	struct dinode	*ildp,
	struct dinode	**dpp)
{
	struct dinode	*dp = NULL;
	uint64_t	offset, off, blkno, len;
	int		error = 0;

	*dpp = NULL;
	if (inum > fs->lastino) {
		assert(0);
		fprintf(stderr, "iget: File system %s invalid inode "
			"number %llu access\n", fs->mntpt, inum);
		return EINVAL;
	}
	offset = inum * INOSIZE;
	dp = (struct dinode *)malloc(sizeof(struct dinode));
	if (!dp) {
		return ENOMEM;
	}
	if ((error = bmap(fd, fs->ildp, &blkno, &len, &off, offset)) != 0) {
		free(dp);
		return error;
	}
	offset = blkno * ONE_K + off;
	lseek(fd, offset, SEEK_SET);
	if (read(fd, (caddr_t)dp, INOSIZE) != INOSIZE) {
		fprintf(stderr, "iget: File system %s ilist read failed\n",
			fs->mntpt);
		error = errno;
		free(dp);
		return error;
	}
	*dpp = dp;
	return 0;
}

/*
 * if error is returned from this function, then the contents
 * of buffer 'buf' should not be trusted and be relied upon.
 */

int
read_dir(
	int		fd,
	struct dinode	*dp,
	uint64_t	offset,
	char		*buf,
	int		buflen)
{
	uint64_t	blkno, off, len;
	uint64_t	sz = dp->size, remain = buflen, count = 0;
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
	uint64_t	offset = 0, inum = MNTPT_INO;
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
	uint64_t	offset,
	int		len,
	char		*buf)
{
	uint64_t	off, foff, sz, blkno;
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
		offset += (uint64_t)readlen;
		nread += readlen;
	}

out:
	return nread;
}

int
read_sb(
	struct fsmem	*fs)
{
	lseek(fs->fd, SB_OFFSET, SEEK_SET);
	if (read(fs->fd, &fs->sb, sizeof(struct super_block)) !=
	    sizeof(struct super_block)) {
		fprintf(stderr, "Failed to read superblock for %s: %s\n",
			fs->devf, strerror(errno));
		return errno;
	}
	if (fs->sb.magic != 0xf001cab) {
		fprintf(stderr, "%s: Not a valid file system\n", fs->devf);
		return EINVAL;
	}

	return 0;
}

struct fsmem *
fsmount(
	char		*fsname,
	char		*mntpt)
{
	struct fsmem	*fs = NULL;
	int		fd, mntlen = strlen(mntpt);
	int		devlen = strlen(fsname);

	if (access(fsname, F_OK) != 0) {
		fprintf(stderr, "Failed to access %s: %s\n",
			fsname, strerror(errno));
		return NULL;
	}
	if ((fd = open(fsname, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n",
			fsname, strerror(errno));
		goto errout;
	}
	fs = (struct fsmem *)malloc(sizeof(struct fsmem));
	if (!fs) {
		fprintf(stderr, "Failed to allocate memory while mounting "
			"%s: %s\n", fsname, strerror(errno));
		goto errout;
	}
	bzero(fs, sizeof(struct fsmem));
	fs->fd = fd;
	fs->mntpt = (char *)malloc(mntlen + 1);
	if (!fs->mntpt) {
		fprintf(stderr, "Failed to allocate memory for mntpt %s: %s\n",
			mntpt, strerror(errno));
		goto errout;
	}
	bcopy(mntpt, fs->mntpt, mntlen + 1);
	fs->devf = (char *)malloc(devlen + 1);
	if (!fs->devlen) {
		fprintf(stderr, "Failed to allocate memory for fsname %s: %s\n",
			fsname, strerror(errno));
		goto errout;
	}
	if (read_sb(fs) != 0) {
		goto errout;
	}
	if (iget(fs, &fs->ilip, ILIST_INO) || iget(fs, &fs->emapip, EMAP_INO) ||
	    iget(fs, &fs->imapip, IMAP_INO)) {
		goto errout;
	}
	return fs;

errout:
	if (fd >= 0) {
		close(fd);
	}
	if (fs) {
		if (fs->mntpt) {
			free(fs->mntpt);
		}
		if (fs->devf) {
			free(fs->devf);
		}
		free(fs);
	}
	return NULL;
}

int
alloc_emap(
	int		fd,
	int		size)
{
	char		*buf = NULL;
	int		alloc, nexts = INIT_FIXED_EXTS;

	emap_sz = (size % 8 == 0) ? (size/8) : (size/8 + 1);
	emap_sz = (emap_sz + ONE_K - 1) & ~(ONE_K - 1);
	nexts += emap_sz/1024;
	buf = (char *)malloc(emap_sz);
	bzero(buf, emap_sz);
	memset(buf, 0xff, nexts);
	(void) lseek(fd, sb.lastblk * 8192, SEEK_SET);
	if (write(fd, buf, emap_sz) < emap_sz) {
		fprintf(stderr, "Error writing emap\n");
		free(buf);
		return 1;
	}
	sb.lastblk += emap_sz/1024;

	free(buf);
	return 0;
}

int
alloc_imap(
	int		fd,
	int		size)
{
	char		*buf = NULL;

	buf = (char *) malloc(1024);
	bzero(buf, 1024);
	buf[0] = 0xf;
	(void)lseek(fd, sb.lastblk * 1024, SEEK_SET);
	if (write(fd, buf, 1024) < 1024) {
		fprintf(stderr, "Error writing imap\n");
		free(buf);
		return 1;
	}
	sb.lastblk += 1;

	free(buf);
	return 0;
}

int
write_ilist(
	int		fd)
{
	struct dinode	*dp = NULL;
	char		*ptr;

	dp = (struct dinode *) malloc(INOSIZE * 32);
	ptr = dp;
	bzero(dp, INOSIZE * 32);
	dp->type = IFILT;
	dp->size = 4 * INOSIZE;
	dp->nblocks = 4;
	dp->orgtype = ORG_DIRECT;
	dp->orgarea.dir[0].blkno = 1;
	dp->orgarea.dir[0].len = 4;
	bcopy((void *)dp, (void *)&ildp, sizeof(struct dinode));
	ptr += INOSIZE;
	dp = ptr;

	dp->type = IFEMP;
	dp->size = emap_sz;
	dp->nblocks = emap_sz;
	dp->orgtype = ORG_DIRECT;
	ptr += INOSIZE;
	dp = ptr;

	dp->type = IFIMP;
	dp->size = 1024;
	dp->nblocks = 1;
	dp->orgtype = ORG_DIRECT;
	ptr += INOSIZE;
	dp = ptr;

	dp->type = IFDIR;
	dp->size = 0;
	dp->nblocks = 0;
	dp->orgtype = ORG_DIRECT;

	(void) lseek(fd, 1024, SEEK_SET);

	if (write(fd, dp, INOSIZE * 32) < (INOSIZE * 32)) {
		fprintf(stderr, "Error writing to ilist file\n");
		free(dp);
		return 1;
	}

	free(buf);
	return 0;
}

int
create_fs(
	char		*fname,
	int		size)
{
	struct stat	st;
	int		error = 0;

	if (stat(fname, &st) != 0) {
		fprintf(stderr, "Couldn't stat %s", fname);
		return 1;
	}

	if ((fd = open(fname, O_RDWR)) < 0) {
		fprintf(stderr, "Couldn't open %s\n", fname);
		return 1;
	}

	if (fallocate(fd, 0, 0, size * 1024) == -1) {
		fprintf(stderr, "Couldn't fallocate %s\n", fname);
		return 1;
	}

	sb.magic = FS_MAGIC;
	sb.version = FS_VERSION1;
	sb.size = size;
	sb.freeblks = size - 1;
	sb.lastblk = 2;

	if ((error = alloc_emap(fd, size)) ||
		(error = alloc_imap(fd, size))) {
		return 1;
	}

	if (error = write_ilist(fd, size)) {
		fprintf(stderr, "Couldn't write to ilist file for %s\n",  fname);
		return 1;
	}

	if (write(fd, &sb, sizeof(struct super_block)) !=
                sizeof(struct super_block)) {
		fprintf(stderr, "Error writing super block\n");
                return 1;
        }
