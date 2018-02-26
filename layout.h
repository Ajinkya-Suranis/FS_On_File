#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/param.h>
#include <assert.h>

#define FS_MAGIC	0xf001cab
#define MAX_DIRECT	12
#define MAX_INDIRECT	24
#define INOSIZE		256
#define INDIR_BLKSZ	8192
#define SB_OFFSET	8192

#define MNTPT_INO	0
#define ILIST_INO	1
#define EMAP_INO	2
#define IMAP_INO	3

#define IFREG		0x0001
#define IFDIR		0x0002
#define IFILT		0x0004
#define IFEMP		0x0008
#define IFIMP		0x0010

#define ORG_DIRECT	1
#define ORG_INDIRECT	2
#define ORG_2INDIRECT	3

#define ONE_K		1024
#define LOG_ONE_K	10

struct super_block {
	uint32_t	magic;
	uint32_t	version;
	uint32_t	freeblks;
	uint32_t	size;
	uint32_t	lastblk;
	uint32_t	pad;
	uint64_t	lastino;
};

struct direntry {
	char		name[56];
	uint64_t	inumber;
};

struct direct {
	uint64_t	blkno;
	uint32_t	len;
};

union org {
	struct direct	dir[MAX_DIRECT];
};

struct dinode {
	uint32_t	type;
	uint32_t	size;
	uint32_t	nblocks;
	uint32_t	orgtype;
	union org	orgarea;
};
