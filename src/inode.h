#ifndef _FS_INODE_H_
#define _FS_INODE_H_

struct minode {
        struct dinode           mino_dip;
	struct fsmem		*mino_fsm;
        fs_u64_t                mino_number;
	fs_u64_t		mino_bno;
};

#define mino_type	mino_dip.type
#define mino_size	mino_dip.size
#define mino_nblocks	mino_dip.nblocks
#define mino_orgtype	mino_dip.orgtype
#define mino_orgarea	mino_dip.orgarea
#define mino_typespec	mino_dip.spec
#define mino_dirspec	mino_typespec.ts_dir
#define mino_ndirents	mino_dirspec.ds_ndirents

extern struct minode	*iget(struct fsmem *, fs_u64_t);
extern int		iwrite(struct minode *);
extern int		inode_alloc(struct fsmem *, fs_u32_t, fs_u64_t *);

#endif
