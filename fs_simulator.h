#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

typedef struct {
    uint32_t inode_number;
    char type;
} __attribute__((packed)) InodeEntry;

int currentDirectoryInode = 0;

typedef struct {
    uint32_t inode_number;
    char name[32];
} __attribute__((packed)) DirectoryEntry;
