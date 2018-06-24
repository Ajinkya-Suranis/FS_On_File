
OBJS = mkfs.c mount.c
CFLAG = -g
CC = gcc

all:
	for obj in $(OBJS); do			\
		$(CC) $(CFLAG) -c $$obj;	\
	done

clean:
	rm -rf mkfs.o mount.o
