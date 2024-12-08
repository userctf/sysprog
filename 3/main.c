#include "heap_help.h"
#include "userfs.h"

#include <stdio.h>

int main(void) {
	int fd1 = ufs_open("file", UFS_CREATE);
	int fd2 = ufs_open("file", 0);
    printf("fd1: %d; fd2: %d\n", fd1, fd2);

	char buffer[2048];
	ufs_write(fd1, "123###", 3); // returns 3
	ufs_read(fd2, buffer, sizeof(buffer)); // returns 3
	// memcmp(buffer, "123", 3) == 0;
    printf("data: %s\n", buffer); // print  "data: 123"

	ufs_close(fd1);
	ufs_close(fd2);

    ufs_destroy();
}