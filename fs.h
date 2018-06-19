#ifndef _FS_H_
#define _FS_H_

struct fsmem {
	int			fsm_fd;
	char			*fsm_devf;
	char			*fsm_mntpt;
	struct super_block	*fsm_sb;
	struct minode		*fsm_ilip;
	struct minode		*fsm_emapip;
	struct minode		*fsm_imapip;
};

struct fs_handle {
	struct fsmem		*fsh_mem;
};

struct file_handle {
	struct fs_handle	*fh_fsh;
	fs_u64_t		fh_curoffset;
	struct minode		*fh_inode;
};

#endif
