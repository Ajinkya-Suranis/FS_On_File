#include "types.h"
#include "layout.h"
#include "fs.h"
#include "inode.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>

static int		validate_sb(struct super_block *);
static int		fill_inodes(struct fsmem *);

static int
validate_sb(
	struct super_block	*sb)
{
	assert(sb != NULL);

	if (sb->magic != FS_MAGIC || sb->version != FS_VERSION1 ||
	    sb->ilistblk == 0 || sb->size == 0) {
		return EINVAL;
	}
	return 0;
}

static int
fill_inodes(
	struct fsmem	*fsm)
{
	int		error = 1;

	if ((fsm->fsm_ilip = iget(fsm, ILIST_INO)) == NULL) {
		fprintf(stderr, "ERROR: Failed to get ilist inode\n");
		goto out;
	}
	if ((fsm->fsm_emapip = iget(fsm, EMAP_INO)) == NULL) {
		fprintf(stderr, "ERROR: Failed to get emap inode\n");
		goto out;
	}
	if ((fsm->fsm_imapip = iget(fsm, IMAP_INO)) == NULL) {
		fprintf(stderr, "ERROR: Failed to get imap inode\n");
		goto out;
	}
	if ((fsm->fsm_mntip = iget(fsm, MNTPT_INO)) == NULL) {
		fprintf(stderr, "ERROR: Failed to get mntpt inode\n");
		goto out;
	}
	error = 0;

out:
	if (error) {
		if (fsm->fsm_ilip) {
			free(fsm->fsm_ilip);
		}
		if (fsm->fsm_emapip) {
			free(fsm->fsm_emapip);
		}
		if (fsm->fsm_imapip) {
			free(fsm->fsm_imapip);
		}
		if (fsm->fsm_mntip) {
			free(fsm->fsm_mntip);
		}
	}
	free(tmp);
	return error;
}

/*
 * Mount the file system.
 * Returns the file system handle (which is void *)
 * to the caller. The caller passes this pointer
 * to the further operations at file system level.
 * The input is device and mount point.
 */

void *
fsmount(
	char			*dev,
	char			*mntpt)
{
	struct stat		st;
	struct fs_handle	*fsh = NULL;
	struct fsmem		*fsm = NULL;
	struct super_block	*sb = NULL;
	int			devfd, mntfd, error = 1;

	if (!dev || !mntpt) {
		fprintf(stderr, "Device or mount point is not valid\n");
		return NULL;
	}
	if (*dev != '/' || *mntpt != '/') {
		fprintf(stderr, "Absolute path is required to mount\n");
		return NULL;
	}
	if (stat(dev, &st) != 0) {
		fprintf(stderr, "Failed to stat %s: %s\n", dev,
			strerror(errno));
		return NULL;
	}
	if (!S_ISREG(st.st_mode)) {
		fprintf(stderr, "%s is not a regular file\n");
		return NULL;
	}
	if (stat(mntpt, &st) != 0) {
		fprintf(stderr, "Failed to stat %s: %s\n", mntpt,
			strerror(errno));
		return NULL;
	}
	if (!S_ISDIR(st.st_mode)) {
		fprintf(stderr, "%s: Not a directory\n", mntpt);
		return NULL;
	}
	if ((devfd = open(dev, O_RDONLY)) < 0) {
		fprintf(stderr, "Failed to open file %s: %s\n", dev,
			strerror(errno));
		return NULL;
	}
	fsh = (struct fs_handle *) malloc(sizeof(struct fs_handle));
	if (!fsh) {
		fprintf(stderr, "Failed to allocate memory for fs handle\n");
		goto out;
	}
	fsm = (struct fsmem *) malloc(sizeof(struct fsmem));
	if (!fsm) {
		fprintf(stderr, "Failed to allocate memory for fs in-memory"
			" structure\n");
		goto out;
	}
	fsh->fsh_mem = fsm;
	bzero((caddr_t)fsm, sizeof(struct fsmem));
	sb = (struct super_block *) malloc(sizeof(struct super_block));
	if (!sb) {
		fprintf(stderr, "Failed to allocate memory for superblock\n");
		goto out;
	}
	(void) lseek(devfd, SB_OFFSET, SEEK_SET);
	if (read(devfd, sb, sizeof(struct super_block)) !=
	    sizeof(struct super_block)) {
		fprintf(stderr, "Failed to read superblock\n");
		goto out;
	}
	if (validate_sb(sb) != 0) {
		fprintf(stderr, "%s: Not a valid file system\n", dev);
		goto out;
	}
	fsm->fsm_devfd = devfd;
	fsm->fsm_devf = (char *)malloc(strlen(dev) + 1);
	if (!fsm->fsm_devf) {
		fprintf(stderr, "Failed to allocate memory for device "
			"file name\n");
		goto out;
	}
	fsm->fsm_mntpt = (char *)malloc(strlen(mntpt) + 1);
	if (!fsm->fsm_mntpt) {
		fprintf(stderr, "Failed to allocate memory for mount point\n");
		goto out;
	}
	strcpy(fsm->fsm_devf, dev);
	strcpy(fsm->fsm_mntpt, mntpt);
	fsm->fsm_sb = sb;
	error = fill_inodes(fsm);

out:
	if (error) {
		if (fsm->fsm_sb) {
			free(fsm->fsm_sb);
		}
		if (fsm->fsm_devf) {
			free(fsm->fsm_devf);
		}
		if (fsm->fsm_mntpt) {
			free(fsm->fsm_mntpt);
		}
		free(fsh);
		close(devfd);
		free(fsm);
		return NULL;
	}
	return (void *)fsh;
}
