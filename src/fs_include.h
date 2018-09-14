#ifndef _FSONFILE_H_
#define _FSONFILE_H_

struct dir_entry {
	char			dir_name[256];
	unsigned long long	dir_ino;
};

typedef void *	FSHANDLE;
typedef void *	FHANDLE;
extern int	create_fs(char *, int);
extern void	*fsmount(char *, char *);
extern void	*fsopen(void *, char *, unsigned short);
extern void	*fscreate(void *, char *, unsigned int);

/*
 * File type (used as argument to fscreate())
 */

#define	FTYPE_FILE	0x01
#define FTYPE_DIR	0x02

/*
extern int	fslseek(void *, fs_u64_t, int);
extern int	fsread(void *, char *, fs_u32_t);
extern int	fswrite(void *, char *, int);
extern int	fslookup(void *, char *);
extern int	fsread_dir(void *, char *, int);
extern void	fsreset_dir(void *);
extern int	fsremove(void *);
extern int	fsclose(void *);
extern int	fsumount(void *);
*/

#endif	/*_FSONFILE_H_*/
