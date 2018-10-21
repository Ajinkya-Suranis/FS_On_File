
OBJS = mkfs.c mount.c inode.c bmap.c allocate.c inode.c fileops.c dir.c
CFLAG = -g
CC = gcc

all:
	for obj in $(OBJS); do			\
		$(CC) $(CFLAG) -c $$obj;	\
	done

clean:
	rm -rf mkfs.o mount.o inode.o bmap.o allocate.o inode.o fileops.o dir.o
