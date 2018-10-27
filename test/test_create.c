#include <stdio.h>
#include <stdlib.h>
#include "fs_include.h"
#include "layout.h"
#include "inode.h"
#include "fs.h"

int
main(
        int                     argc,
        char                    *argv[])
{
	struct file_handle	*fh = NULL;
	FSHANDLE		fsh = NULL;
	int			err = 0;
	unsigned int		type;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s <device file> <path> <type>\n", argv[0]);
		return 1;
	}

	if ((fsh = fsmount(argv[1], argv[2])) == NULL) {
                fprintf(stderr, "Failed to mount file system\n");
                return 1;
        }
	printf("FS mounted successfully\n");
	type = (unsigned int)atoi(argv[2]);
	fh = fscreate(fsh, argv[2], type);
	if (fh == NULL) {
		fprintf(stderr, "Failed to create file %s\n", argv[2]);
		err = 1;
	}
	printf("Created file %s successfully\n", argv[2]);

	return err;
}
