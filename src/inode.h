#ifndef _FS_INODE_H_
#define _FS_INODE_H_

struct minode {
        struct dinode           mino_dip;
        uint64_t                mino_number;
};

#define mino_type	mino_dip.type
#define mino_size	mino_dip.size
#define mino_nblocks	mino_dip.nblocks
#define mino_orgtype	mino_dip.orgtype
#define mino_orgarea	mino_dip.orgarea

extern struct minode	*iget(struct fsmem *, fs_u64_t);

#endif
