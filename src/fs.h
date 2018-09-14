#ifndef _FS_H_
#define _FS_H_

struct fsmem {
	int			fsm_devfd;
	char			*fsm_devf;
	char			*fsm_mntpt;
	struct super_block	*fsm_sb;
	struct minode		*fsm_ilip;
	struct minode		*fsm_emapip;
	struct minode		*fsm_imapip;
	struct minode		*fsm_mntip;
};

struct fs_handle {
	struct fsmem		*fsh_mem;
};

struct file_handle {
	struct fs_handle	*fh_fsh;
	fs_u64_t		fh_curoffset;
	struct minode		*fh_inode;
};

#define FTYPE_MASK		0x03
#define FTYPE_FILE		0x01
#define FTYPE_DIR		0x02

#endif
