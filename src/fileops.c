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

int		internal_read(int, struct minode *, char *, fs_u64_t, fs_u32_t);
static int	lookup_path(struct fsmem *, char *, struct direntry *);

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
	void			*vfh,
	char			*buf,
	fs_u32_t		nentries)
{
	struct file_handle	*fh;
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
	if (!buf || !vfh) {
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
	fh = (struct file_handle *)vfh;
	mino = fh->fh_inode;
	offset = fh->fh_curoffset;
	assert(offset % DIRENTRY_LEN == 0);
	len = nentries * DIRENTRY_LEN;

	printf("error string is: %s\n", strerror(errno));
	if ((rd = internal_read(fh->fh_fsh->fsh_mem->fsm_devfd, mino, intbuf,
				offset, len)) != (int)len && errno) {
		fprintf(stderr, "Failed to read directory inode %llu: %s\n",
			mino->mino_number, strerror(errno));
	}
	nentries = rd/DIRENTRY_LEN;
	for (i = 0; i < nentries; i++) {
		dir = (struct direntry *)intbuf;
		udir = (struct udirentry *)buf;
		strcpy(udir->udir_name, dir->name);
		udir->udir_inum = dir->inumber;
		buf += UDIRENTRY_LEN;
		intbuf += DIRENTRY_LEN;
	}

	/*
	 * Update the current offset of file after
	 * successful/partial read.
	 */

	fh->fh_curoffset += (fs_u64_t)rd;

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

	return (int)rd/DIRENTRY_LEN;
}

static int
lookup_path(
	struct fsmem	*fsm,
	char		*path,
	struct direntry	*entp)
{
	struct direntry	*dirp = NULL;
	struct minode	*mino = NULL;
	fs_u64_t	inum = MNTPT_INO, offset = 0;
	char		*buf = NULL;
	int		start = 1, end = 1;
	int		i = 1, j, nent, found = 0;

	if (entp) {
		memset(entp, 0, sizeof(struct direntry));
	}
	if (strlen(path) == 1) {
		/*
		 * '/' is being looked up.
		 * This is special case.
		 * Set the 'name' in direntry to null
		 * and inode number to MNTPT_INO(3)
		 */
		if (entp) {
			entp->name[0] = '\0';
			entp->inumber = MNTPT_INO;
		}
		return 1;
	}

	/*
	 * Allocate buffer for directory entries.
	 * We'll be reading 16 directory entries
	 * in each iteration.
	 */

	buf = (char *)malloc(16 * DIRENTRY_LEN);
	if (!buf) {
		fprintf(stderr, "Failed to allocate memory for lookup "
			"buffer for %s\n", fsm->fsm_mntpt);
		return 0;
	}

	for (;;) {
		if (path[i] != '/' && path[i] != '\0') {
			end++;
			i++;
			continue;
		}
		fprintf(stdout, "Component: ");
		for (j = start; j <= end; j++) {
			fprintf(stdout, "%c", path[j]);
		}
		fprintf(stdout, "\n");
		mino = iget(fsm, inum);
		if (mino == NULL) {
			fprintf(stderr, "lookup_path: Failed to read inode %llu"
				" for %s\n", inum, fsm->fsm_mntpt);
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
				if (strlen(dirp->name) == (end - start) &&
				    strncmp(path + start, dirp->name,
				    (end - start)) == 0) {
					fprintf(stdout, "Found matching entry:"
						" %s\n", dirp->name);
					found = 1;
					break;
				}
				dirp++;
			}
			if (found) {
				if (entp) {
					memcpy(entp, dirp,
					       sizeof(struct direntry));
				}
				break;
			}
			offset += nent * DIRENTRY_LEN;
		}
		if (!found) {
			fprintf(stdout, "component %s not found\n",
				path + start);
			errno = ENOENT;
			goto out;
		}
		if (path[i] == '\0') {
			break;
		}
		start = end = ++i;
		inum = dirp->inumber;
		fprintf(stdout, "Next inode number is %llu\n", inum);
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

	errno = 0;
	printf("inode size iss: %llu\n", mino->mino_size);
	while (nread < len) {
		printf("curoff is: %llu\n", curoff);
		if (remain == 0 || curoff >= mino->mino_size) {
			printf("We're breaking properly\n");
			break;
		}
                if ((error = bmap(fd, mino, &blkno, &sz, &off, curoff)) != 0) {
                        errno = error;
                        goto out;
                }
		printf("Returned length is: %llu\n", sz);
                readlen = (fs_u32_t)MIN(remain,
					MIN(sz, mino->mino_size - curoff));
		printf("readlen is %u\n", readlen);
                foff = (blkno << LOG_ONE_K) + off;
                lseek(fd, foff, SEEK_SET);
		printf("internal_read: Reading from blkno %llu\n", blkno);
                if (read(fd, (buf + nread), (int)readlen) != (int)readlen) {
                        fprintf(stderr, "Failed to read from inode %llu at"
                                " offset %llu\n", mino->mino_number, foff);
                        goto out;
                }
                curoff += (fs_u64_t)readlen;
                nread += readlen;
		remain -= readlen;
        }

out:
        return (int)nread;
}

/*
 * Write to the metadata inode.
 * This isn't a generic write routine to
 * a structural inode; it has some restrictions,
 * like: the write area must be inside the
 * allocated blocks for the inode.
 */

int
metadata_write(
	struct fsmem	*fsm,
	fs_u64_t	offset,
	char		*buf,
	int		len,
	struct minode	*ino)
{
	fs_u64_t	off, sz, blkno, foff;
	int		error = 0, nwrite = 0;

	error = bmap(fsm->fsm_devfd, ino, &blkno, &sz, &off, offset);
	if (error) {
		errno = error;
		return 0;
	}
	foff = (blkno << LOG_ONE_K) + off;
	lseek(fsm->fsm_devfd, foff, SEEK_SET);
	if ((nwrite = write(fsm->fsm_devfd, buf, len)) != len) {
		fprintf(stderr, "Failed to write metadata inode %llu at offset"
			" %llu for %s\n", ino->mino_number, foff,
			fsm->fsm_mntpt);
		return 0;
	}

	return nwrite;
}

int
fsread(
	void			*vfh,
	char			*buf,
	fs_u32_t		len)
{
	struct file_handle	*fh;
	struct minode		*mino = NULL;
	struct fsmem		*fsm = NULL;
	int			nread;
	int			error = 0, fd;

	if (len == 0) {
		return 0;
	}
	if (vfh == NULL || buf == NULL) {
		errno = EINVAL;
		return 0;
	}
	fh = (struct file_handle *)vfh;
	fsm = fh->fh_fsh->fsh_mem;
	assert(fsm->fsm_devfd != 0);
	assert(fsm->fsm_sb && fsm->fsm_ilip && fsm->fsm_emapip &&
	       fsm->fsm_imapip && fsm->fsm_mntip);
	mino = fh->fh_inode;
	fd = fsm->fsm_devfd;
	assert(mino != NULL);
	nread = internal_read(fd, mino, buf, fh->fh_curoffset, len);
	fh->fh_curoffset += (fs_u64_t)nread;

out:
	return nread;
}

/*
 * Create a new file or directory.
 */

void *
fscreate(
	void			*vfsh,
	char			*path,
	fs_u32_t		flags)
{
	struct direntry		ent;
	struct file_handle	*fh = NULL;
	struct fs_handle	*fsh;
	struct minode		*parent = NULL;
	struct fsmem		*fsm = NULL;
	fs_u64_t		inum;
	int			i, len = strlen(path);
	int			error = 0, last;

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
/*	if ((flags & FTYPE_MASK == FTYPE_MASK) || (flags & ~FTYPE_MASK)) {
		fprintf(stderr, "ERROR: Invalid flags\n");
		errno = EINVAL;
		return NULL;
	}*/

	/*
	 * First, do lookup on full path and see if the file/directory
	 * is already present. If so, return NULL.
	 * If it doesn't, lookup again for its to-be parent directory
	 * if parent isn't the root directory.
	 */

	fsh = (struct fs_handle *)vfsh;
	if (lookup_path(fsh->fsh_mem, path, NULL) != 0) {
		fprintf(stderr, "ERROR: The file %s already exists\n", path);
		return NULL;
	}
	fsm = fsh->fsh_mem;
	assert(fsm != NULL);
	for (i = 1, last = 0; i < len; i++) {
		if (path[i] == '/') {
			last = i;
		}
	}
	if (last != 0) {
		path[last] = '\0';
		if(lookup_path(fsh->fsh_mem, path, &ent) == 0) {
			fprintf(stderr, "ERROR: %s doesn't exist\n", path);
			errno = ENOENT;
			path[last] = '/';
			return NULL;
		}
		path[last] = '/';
	} else {
		ent.inumber = MNTPT_INO;
	}
	printf("ent.inumber: %llu\n", ent.inumber);

	/*
	 * The file doesn't exist.
	 * Allocate a new inode for it and create a new directory entry.
	 */

	assert(ent.inumber != 0);
	if ((parent = iget(fsm, ent.inumber)) == NULL) {
		fprintf(stderr, "Failed to get inode %llu of %s\n", ent.inumber,
			fsm->fsm_mntpt);
		return NULL;
	}
	printf("Parent inode num: %llu, type: %u, size: %llu\n", parent->mino_number, parent->mino_type, parent->mino_size);
	assert(parent->mino_type == IFDIR);
	if ((error = inode_alloc(fsm, flags, &inum)) != 0) {
		return NULL;
	}

	for (i = len - 1; path[i] != '/'; i--);
	fprintf(stdout, "INFO: Passing file name %s to add_direntry()\n",
		path + i + 1);
	if ((error = add_direntry(fsm, parent, path + i + 1, inum)) != 0) {
		fprintf(stderr, "ERROR: Failed to add direntry: name-%s, "
			"inum: %llu for %s\n", path + i + 1, inum,
			fsm->fsm_mntpt);
		return NULL;
	}

	/*
	 * Inode is successfully created and necessary metadata
	 * is also written. Now fill the file handle structure
	 * and return it to the caller.
	 */

	fh = (struct file_handle *)malloc(sizeof(struct file_handle));
	if (fh == NULL) {
		fprintf(stderr, "ERROR: Failed to allocate memory to "
			"file handle for %s\n", fsm->fsm_mntpt);
		errno = ENOMEM;
		return NULL;
	}
	if ((fh->fh_inode = iget(fsm, inum)) == NULL) {
		free(fh);
		return NULL;
	}
	fh->fh_fsh = fsh;
	fh->fh_curoffset = 0;

	return fh;
}

/*
 * Open a file.
 * Returns a file handle (void *) which will be used
 * by the caller for further operations on the file.
 */

void *
fsopen(
	void			*vfsh,
	char			*path,
	fs_u32_t		flags)
{
	struct file_handle	*fh = NULL;
	struct fs_handle	*fsh = NULL;
	struct direntry		de;
	struct minode		*mino = NULL;

	if (!vfsh || !path) {
		errno = EINVAL;
		fprintf(stderr, "Failed to open file: %s\n", strerror(errno));
		return NULL;
	}

	/*
	 * lookup if the file in 'path' exists.
	 * For now we're returning an error if
	 * the file doesn't exist.
	 * TODO: if file doesn't exist and 'flags'
	 * specifies creating one, then we'll create
	 * a new file.
	 */

	fsh = (struct fs_handle *)vfsh;
	if (lookup_path(fsh->fsh_mem, path, &de) == 0) {
		fprintf(stderr, "The path %s doesn't exist\n", path);
		return NULL;
	}
	fprintf(stdout, "Got inode number %llu from lookup\n", de.inumber);
	mino = iget(fsh->fsh_mem, de.inumber);
	if (mino == NULL) {
		fprintf(stderr, "Failed to get inode %llu for %s\n",
			fsh->fsh_mem, path);
		return NULL;
	}
	fh = (struct file_handle *)malloc(sizeof(struct file_handle));
	if (fh == NULL) {
		fprintf(stderr, "ERROR: Failed to allocate memory to "
			"file handle for %s\n", fsh->fsh_mem->fsm_mntpt);
		errno = ENOMEM;
		return NULL;
	}
	fh->fh_inode = mino;
	fh->fh_fsh = fsh;
	fh->fh_curoffset = 0;

	fprintf(stdin, "Opened file %s successfully\n", path);
	return fh;
}
