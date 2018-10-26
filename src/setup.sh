make clean
rm -rf abc test_mkfs test_mount
make
gcc -o test_mkfs test_mkfs.c mkfs.o
gcc -o test_mount test_mount.c allocate.o bmap.o dir.o fileops.o inode.o mount.o
touch abc
./test_mkfs `pwd`/abc 102400
