#include "types.h"
#include "layout.h"
#include "fs.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>

static int	validate_sb(struct super_block *);

static int
validate_sb(
	struct super_block	*sb)
{
	assert(sb != NULL);

	if (sb->magic != FS_MAGIC || sb->version != FS_VERSION1) {
		return EINVAL;
	}
	return 0;
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
	int			devfd, mntfd, error = 0;

	if ((devfd = open(dev, O_RDONLY)) < 0) {
		fprintf(stderr, "Failed to open file %s: %s\n", dev,
			strerror(errno));
		return NULL;
	}
	if (stat(mntpt, &st) != 0) {
		fprintf(stderr, "Failed to stat %s: %s\n", mntpt,
			strerror(errno));
		close(devfd);
		return NULL;
	}
	if (!S_ISDIR(st.st_mode)) {
		fprintf(stderr, "%s: Not a directory\n", mntpt);
		goto out;
	}
	fsh = (struct fs_handle *) malloc(sizeof(struct fs_handle));
	if (!fsh) {
		fprintf(stderr, "Failed to allocate memory for fs handle\n");
		error = 1;
		goto out;
	}
	fsm = (struct fsmem *) malloc(sizeof(struct fsmem));
	if (!fsm) {
		fprintf(stderr, "Failed to allocate memory for fs in-memory"
			" structure\n");
		error = 1;
		goto out;
	}
	sb = (struct super_block *) malloc(sizeof(struct super_block));
	if (!sb) {
		fprintf(stderr, "Failed to allocate memory for superblock\n");
		error = 1;
		goto out;
	}
	(void) lseek(devfd, SB_OFFSET, SEEK_SET);
	if (read(devfd, sb, sizeof(struct super_block)) !=
	    sizeof(struct super_block)) {
		fprintf(stderr, "Failed to read superblock\n");
		error = 1;
		goto out;
	}
	if (validate_sb(sb) != 0) {
		fprintf(stderr, "%s: Not a valid file system\n", dev);
		error = 1;
	}

out:
	if (error) {
		if (fsm->sb) {
			free(fsm->sb);
		}
		if (fsm->fsm_ilip) {
			free(fsm->fsm_ilip);
		}
		if (fsm->fsm_emapip) {
			free(fsm->fsm_emapip);
		}
		if (fsm->fsm_imapip) {
			free(fsm->fsm_imapip);
		}
		free(fsh);
		close(devfd);
		free(fsm);
		return NULL;
	}
	return (void *)fsh;
}
