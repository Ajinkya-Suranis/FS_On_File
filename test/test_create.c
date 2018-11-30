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
	unsigned int		type;

	if (argc != 5) {
		fprintf(stderr, "Usage: %s <device file> <mntpt> <path>"
			" <type>\n", argv[0]);
		return 1;
	}

	if ((fsh = fsmount(argv[1], argv[2])) == NULL) {
                fprintf(stderr, "Failed to mount file system\n");
                return 1;
        }
	printf("FS mounted successfully\n");
	type = (unsigned int)atoi(argv[4]);
	fh = fscreate(fsh, argv[3], type);
	if (fh == NULL) {
		fprintf(stderr, "Failed to create file %s\n", argv[3]);
		return 1;
	}
	printf("Created file %s successfully\n", argv[3]);

	return 0;
}
