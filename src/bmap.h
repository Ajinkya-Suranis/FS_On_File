#ifndef _FS_EXTERNS_H_
#define _FS_EXTERNS_H_

extern int	bmap(int, struct minode *, fs_u64_t *, fs_u64_t *,
		     fs_u64_t *, fs_u64_t);
extern int	bmap_alloc(struct fsmem *, struct minode *, fs_u32_t,
			   fs_u64_t *);

#endif /*_FS_EXTERNS_H_*/
