
CFLAGS = -g
CC = gcc
OBJ_PATH_MKFS = ../src/mkfs.o
OBJ_PATH_MOUNT = ../src/mount.o
OBJ_PATH_INO = ../src/inode.o
OBJ_PATH_BMAP = ../src/bmap.o
OBJ_PATH_ALLOC = ../src/allocate.o
OBJ_PATH_DIR = ../src/dir.o
OBJ_PATH_FILEOPS = ../src/fileops.o
INCLUDE = -I../src/

all:
	$(CC) $(CFLAGS) $(INCLUDE) -o test_mkfs test_mkfs.c  $(OBJ_PATH_MKFS)
	$(CC) $(CFLAGS) $(INCLUDE) -o test_mount test_mount.c $(OBJ_PATH_MOUNT) $(OBJ_PATH_INO) $(OBJ_PATH_BMAP) $(OBJ_PATH_ALLOC) $(OBJ_PATH_DIR) $(OBJ_PATH_FILEOPS)
	$(CC) $(CFLAGS) $(INCLUDE) -o test_create test_create.c $(OBJ_PATH_MOUNT) $(OBJ_PATH_INO) $(OBJ_PATH_BMAP) $(OBJ_PATH_ALLOC) $(OBJ_PATH_DIR) $(OBJ_PATH_FILEOPS)
	$(CC) $(CFLAGS) $(INCLUDE) -o test_readdir test_readdir.c $(OBJ_PATH_MOUNT) $(OBJ_PATH_INO) $(OBJ_PATH_BMAP) $(OBJ_PATH_ALLOC) $(OBJ_PATH_DIR) $(OBJ_PATH_FILEOPS)

clean:
	rm -rf test_mkfs test_mount test_create test_readdir
