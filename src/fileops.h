#ifndef _FS_FILEOPS_H_
#define _FS_FILEOPS_H_

int     internal_read(int, struct minode *, char *, fs_u64_t, fs_u32_t);
int	metadata_write(struct fsmem *, fs_u64_t, char *, struct minode *);

#endif /*
