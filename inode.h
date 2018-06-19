#ifndef _FS_INODE_H_
#define _FS_INODE_H_

struct minode {
        struct dinode           mino_dip;
        uint64_t                mino_number;
};

extern struct minode	*iget(struct fsmem *, fs_u64_t);
