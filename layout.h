#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/param.h>
#include <assert.h>

/*
 * Magic number of our file system.
 * Useful to verify the identity of
 * superblock.
 */

#define FS_MAGIC	0xf001cab

/*
 * Maximum number of direct extents allocated
 * to a file/directory.
 */

#define MAX_DIRECT	12

/*
 * Maximum number of indirect extents allocated
 * to a file/directory.
 */

#define MAX_INDIRECT	24

/*
 * Size of metadata of a single inode.
 * This much bytes are allocated in the
 * ilist file for every inode
 */

#define INOSIZE		256

/*
 * Size of an indirect extent.
 */

#define INDIR_BLKSZ	8192

/*
 * Offset of a superblock from start of the file.
 */

#define SB_OFFSET	8192

/*
 * Serially:
 *  Inode number of root directory.
 *  Inode number of ilist inode
 *  Inode number of extent map inode
 *  Inode number of inode map inode
 */

#define ILIST_INO	0
#define EMAP_INO	1
#define IMAP_INO	2
#define MNTPT_INO	3

/*
 * Various inode formats
 */

#define IFREG		0x0001	/* regular file */
#define IFDIR		0x0002	/* directory inode */
#define IFILT		0x0004	/* ilist inode */
#define IFEMP		0x0008	/* extent map inode */
#define IFIMP		0x0010	/* inode map inode */

/*
 * Inode org type.
 * Org type tells how to interpret the org area
 * of inode.
 * Currently we support only four org types:
 * 1. ORG_IMMED: immediate area
 *    The inode data stored inside metadata itself.
 * 2. ORG_DIRECT: direct type
 *    The org area points to block numbers which
 *    contains inode data.
 * 3. ORG_INDIRECT: indirect (first-level) type
 *    The org area points to block numbers which refer
 *    to block numbers of direct block numbers.
 * 4. ORG_2INDIRECT: second level indirect
 *    The org area points to block numbers which contain
 *    block numbers of first-level indirect  block numbers.
 *
 * We currently don't support sparse files.
 */

#define ORG_IMMED	1
#define ORG_DIRECT	2
#define ORG_INDIRECT	3
#define ORG_2INDIRECT	4

#define FS_VERSION1	1

/*
 * Macros just to avoid numbers in the code.
 */

#define ONE_K		1024
#define LOG_ONE_K	10

#define INIT_FIXED_EXTS	42

/*
 * Super block structure.
 *
 * Apart from all obvious fields, we have:
 *
 * lastblk: last free block number in file system.
 * lastino: last free inode number.
 */

struct super_block {
	uint32_t	magic;
	uint32_t	version;
	uint32_t	freeblks;
	uint32_t	size;
	uint32_t	lastblk;
	uint32_t	pad;
	uint64_t	lastino;
};

/*
 * A directory entry.
 * Currently it's always 64 bytes size, even if
 * the file name isn't as long as 56 bytes.
 * I'll optimize it later.
 */

struct direntry {
	char		name[56];
	uint64_t	inumber;
};

/*
 * direct org type structure.
 * It just contains block number and length and hence
 * it's a single extent descriptor.
 */

struct direct {
	uint64_t	blkno;
	uint32_t	len;
};

union org {
	struct direct	dir[MAX_DIRECT];
};

/*
 * On-disk inode structure.
 */

struct dinode {
	uint32_t	type;
	uint32_t	size;
	uint32_t	nblocks;
	uint32_t	orgtype;
	union org	orgarea;
};
