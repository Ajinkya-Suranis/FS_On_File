#include <stdio.h>
#include <stdlib.h>
#include "fs_include.h"

int
main(
	int	argc,
	char	*argv[])
{
	int	size, res;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <file> <size>\n", argv[0]);
		return 1;
	}
	size = atoi(argv[2]);
	if ((res = create_fs(argv[1], size)) != 0) {
		fprintf(stderr, "Failed to create FS on %s\n", argv[1]);
		return 1;
	}

	return 0;
}
