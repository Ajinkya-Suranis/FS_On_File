#include "types.h"
#include "layout.h"
#include "fs.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

fs_u32_t        emap_sz;
fs_u64_t        emap_firstblk, imap_firstblk;
fs_u64_t	init_ilistblk;

static int	alloc_emap(struct super_block *, int, int);
static int	alloc_imap(struct super_block *, int, int);
static int	write_ilist(struct super_block *, int);

static int
alloc_emap(
        struct super_block      *sb,
        int                     fd,
        int                     size)
{
        char                    *buf = NULL;
        int                     alloc, nexts = INIT_FIXED_EXTS;

        emap_sz = (size % 8 == 0) ? (size/8) : (size/8 + 1);
        emap_sz = (emap_sz + ONE_K - 1) & ~(ONE_K - 1);
        nexts += emap_sz/ONE_K;
        buf = (char *)malloc(emap_sz);
        if (buf == NULL) {
                fprintf(stderr, "Failed to allocate memory for emap\n");
                return ENOMEM;
        }
        memset(buf, 0xff, emap_sz);
        nexts = (nexts % 8 == 0) ? (nexts/8) : (nexts/8 + 1);
        memset(buf, 0, nexts);
        (void) lseek(fd, sb->lastblk * 8192, SEEK_SET);
        if (write(fd, buf, emap_sz) < emap_sz) {
                fprintf(stderr, "Error writing emap\n");
                free(buf);
                return 1;
        }
        emap_firstblk = sb->lastblk;
        sb->lastblk += emap_sz/ONE_K;
        sb->freeblks -= emap_sz/ONE_K;
        free(buf);
        return 0;
}

static int
alloc_imap(
        struct super_block      *sb,
        int                     fd,
        int                     size)
{
        char            *buf = NULL;

        buf = (char *) malloc(8192);
        if (buf == NULL) {
                fprintf(stderr, "Failed to allocate memory for imap\n");
                return ENOMEM;
        }
        memset(buf, 0xff, 8192);
	buf[0] = 0xf0;
        (void)lseek(fd, sb->lastblk * ONE_K, SEEK_SET);
        if (write(fd, buf, 8192) < 8192) {
                fprintf(stderr, "Error writing imap\n");
                free(buf);
                return 1;
        }
        imap_firstblk = sb->lastblk;
        sb->lastblk += 8;
        sb->freeblks -= 8;
        free(buf);
        return 0;
}

static int
write_ilist(
        struct super_block      *sb,
        int                     fd)
{
        struct dinode   *dp = NULL;
        char            *ptr, *buf = NULL;

	buf = (char *) malloc(INIT_ILT_SIZE);
	if (buf == NULL) {
		fprintf(stderr, "Failed to allocate memory for ilist buffer\n");
		return 1;
	}
	ptr = buf;
        dp = (struct dinode *) buf;
        bzero((caddr_t)dp, INIT_ILT_SIZE);
        dp->type = IFILT;
        dp->size = INIT_NINODES * INOSIZE;
        dp->nblocks = INIT_ILT_SIZE/ONE_K;
        dp->orgtype = ORG_DIRECT;
        dp->orgarea.dir[0].blkno = sb->lastblk;
        dp->orgarea.dir[0].len = 4;
        ptr += INOSIZE;
        dp = (struct dinode *)ptr;
        dp->type = IFEMP;
        dp->size = emap_sz;
        dp->nblocks = emap_sz/ONE_K;
        dp->orgtype = ORG_DIRECT;
        dp->orgarea.dir[0].blkno = emap_firstblk;
        dp->orgarea.dir[0].len = emap_sz/ONE_K;
        ptr += INOSIZE;
        dp = (struct dinode *)ptr;

        dp->type = IFIMP;
        dp->size = 8192;
        dp->nblocks = 8;
        dp->orgtype = ORG_DIRECT;
        dp->orgarea.dir[0].blkno = imap_firstblk;
        dp->orgarea.dir[0].len = 8;
        ptr += INOSIZE;
        dp = (struct dinode *)ptr;

        dp->type = IFDIR;
        dp->size = 0;
        dp->nblocks = 0;
        dp->orgtype = ORG_DIRECT;

	init_ilistblk = (imap_firstblk + 8) << LOG_ONE_K;
        (void) lseek(fd, init_ilistblk, SEEK_SET);

        if (write(fd, buf, INIT_ILT_SIZE) < INIT_ILT_SIZE) {
                fprintf(stderr, "Error writing to ilist file\n");
                free(buf);
                return 1;
        }
        sb->lastblk += INIT_ILT_SIZE >> LOG_ONE_K;
        sb->freeblks -= INIT_ILT_SIZE >> LOG_ONE_K;

        free(buf);
        return 0;
}

int
create_fs(
        char                    *fname,
        int                     size)
{
        struct super_block      *sb = NULL;
        struct stat             st;
        int                     fd, error = 0;

        if (stat(fname, &st) != 0) {
                fprintf(stderr, "Couldn't stat %s\n", fname);
                return 1;
        }

        if ((fd = open(fname, O_RDWR)) < 0) {
                fprintf(stderr, "Couldn't open %s\n", fname);
                return 1;
        }

        if (fallocate(fd, 0, 0, size * ONE_K) == -1) {
                fprintf(stderr, "Couldn't fallocate %s\n", fname);
                return 1;
        }

        sb = (struct super_block *)malloc(sizeof(struct super_block));
        bzero((caddr_t)sb, sizeof(struct super_block));

        sb->magic = FS_MAGIC;
        sb->version = FS_VERSION1;
        sb->size = size;
        sb->freeblks = size - 1;
        sb->lastblk = 2;
	sb->iused = INIT_NINODES;

        if ((error = alloc_emap(sb, fd, size)) ||
                (error = alloc_imap(sb, fd, size))) {
                return 1;
        }

        if (error = write_ilist(sb, fd)) {
                fprintf(stderr, "Couldn't write to ilist file for %s\n",  fname);
                return 1;
        }

	sb->ilistblk = init_ilistblk;
        (void) lseek(fd, SB_OFFSET, SEEK_SET);
        if (write(fd, sb, sizeof(struct super_block)) !=
                sizeof(struct super_block)) {
                fprintf(stderr, "Error writing super block\n");
                return 1;
        }

        printf("File system created successfully\n");
        return 0;
}
