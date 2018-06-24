gcc -c mkfs.c
gcc -c mount.c
gcc -o test_mkfs test_mkfs.c mkfs.o
gcc -o test_mount test_mount.c mount.o
touch testfs
mkdir mnt1
