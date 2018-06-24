#ifndef _FSONFILE_H_
#define _FSONFILE_H_

typedef void *	FSHANDLE;
typedef void *	FHANDLE;
extern int	create_fs(char *, int);
extern void	*fsmount(char *, char *);

/*
extern int	fsopen(void *, char *, fs_u16_t);
extern int	fslseek(void *, fs_u64_t, int);
extern int	fsread(void *, int, char *);
extern int	fswrite(void *, char *, int);
extern int	fslookup(void *, char *);
extern int	fsread_dir(void *, char *, int);
extern int	fsremove(void *);
extern int	fsclose(void *);
extern int	fsumount(void *);
*/

#endif	/*_FSONFILE_H_*/
