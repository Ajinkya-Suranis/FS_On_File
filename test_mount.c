#include <stdio.h>
#include <stdlib.h>
#include "fs_include.h"
#include "layout.h"
#include "inode.h"
#include "fs.h"

int
main(
        int    	 		argc,
        char    		*argv[])
{
	struct super_block	*sb;
	struct fsmem		*fsm;
	FSHANDLE		fsh = NULL;

        if (argc != 3) {
                fprintf(stderr, "Usage: %s <file> <mount dir>\n", argv[0]);
                return 1;
        }
	if ((fsh = fsmount(argv[1], argv[2])) == NULL) {
		fprintf(stderr, "Failed to mount file system\n");
		return 1;
	}
	printf("FS mounted successfully\n");
	fsm = ((struct fs_handle *)fsh)->fsh_mem;
	printf("Dev: %s, mntpt: %s\n", fsm->fsm_devf, fsm->fsm_mntpt);
	sb = fsm->fsm_sb;
	printf("magic: %x, version: %u, freeblks: %llu\n", sb->magic,
	       sb->version, sb->freeblks);
	printf("ip0- type: %u, nblocks: %llu, size: %llu, number: %llu\n",
	       fsm->fsm_ilip->mino_type, fsm->fsm_ilip->mino_nblocks,
	       fsm->fsm_ilip->mino_size, fsm->fsm_ilip->mino_number);
	printf("ip1- type: %u, nblocks: %llu, size: %llu, number: %llu\n",
	       fsm->fsm_emapip->mino_type, fsm->fsm_emapip->mino_nblocks,
	       fsm->fsm_emapip->mino_size, fsm->fsm_emapip->mino_number);
	printf("ip2- type: %u, nblocks: %llu, size: %llu, number: %llu\n",
	       fsm->fsm_imapip->mino_type, fsm->fsm_imapip->mino_nblocks,
	       fsm->fsm_imapip->mino_size, fsm->fsm_imapip->mino_number);
	printf("ip3- type: %u, nblocks: %llu, size: %llu, number: %llu\n",
	       fsm->fsm_mntip->mino_type, fsm->fsm_mntip->mino_nblocks,
	       fsm->fsm_mntip->mino_size, fsm->fsm_mntip->mino_number);

	return 0;
}
