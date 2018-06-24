
CFLAGS = -g
CC = gcc
OBJ_PATH_MKFS = ../src/mkfs.o
OBJ_PATH_MOUNT = ../src/mount.o
INCLUDE = -I../src/

all:
	$(CC) $(CFLAGS) $(INCLUDE) -o test_mkfs test_mkfs.c  $(OBJ_PATH_MKFS)
	$(CC) $(CFLAGS) $(INCLUDE) -o test_mount test_mount.c $(OBJ_PATH_MOUNT)

clean:
	rm -rf test_mkfs test_mount
