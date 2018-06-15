#ifndef _FS_H_
#define _FS_H_

struct fsmem {
	int			fd;
	char			*devf;
	char			*mntpt;
	struct super_block	*sb;
	struct minode		*ilip;
	struct minode		*emapip;
	struct minode		*imapip;
};

struct minode {
	struct dinode		dip;
	uint64_t		number;
};

#endif
