
OBJS = mkfs.c mount.c inode.c bmap.c
CFLAG = -g
CC = gcc

all:
	for obj in $(OBJS); do			\
		$(CC) $(CFLAG) -c $$obj;	\
	done

clean:
	rm -rf mkfs.o mount.o inode.o bmap.o
