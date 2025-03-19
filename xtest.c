#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "xv6-riscv/kernel/fs.h"
#include "xv6-riscv/kernel/param.h"
#include "xv6-riscv/kernel/types.h"

#define die(msg) do { fprintf(stderr, "ERROR: %s\n", msg); exit(1); } while (0)

#define NAMESZ 64

static uint test_counter = 0;
static char testname[NAMESZ];

// ERROR: directory appears more than once in file system
void test21(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)test_map + sb->inodestart * BSIZE);
    uint inum_cp = 0;
    for (uint i = 1; i < sb->ninodes; ++i) {
        if (inode_table[i].type == T_DIR) {
            inum_cp = i;
            break;
        }
    }

    for (uint i = 0; i < sb->ninodes; ++i) {
        struct dinode *inode = &inode_table[i];
        if (inode->type == T_DIR) {
            for (uint j = 0; j < NDIRECT; ++j) {
                if (inode->addrs[j] != 0) {
                    struct dirent *dirents = (struct dirent *)((char*) test_map + (inode->addrs[j] * BSIZE));
                    for (uint k = 0; k < BSIZE / sizeof(struct dirent); ++k) {
                        if (dirents[k].inum == 0) {
                            dirents[k].inum = inum_cp;
                            snprintf(dirents[k].name, DIRSIZ, "CS-5204");
                            return;
                        }
                    }
                }
            }
        }
    }
}

// ERROR: bad reference count for file
void test20(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)test_map + sb->inodestart * BSIZE);
    for (uint i = 1; i < sb->ninodes; ++i) {
        if (inode_table[i].type == T_FILE) {
            inode_table[i].nlink += 50;
            return;
        }
    }
}

// problematic
// ERROR: inode referred to in directory but marked free
void test19(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)test_map + sb->inodestart * BSIZE);
    uint inum_cp = 0;
    for (uint i = 1; i < sb->ninodes; ++i) {
        if (inode_table[i].type == 0) {
            inum_cp = i;
            break;
        }
    }

    for (uint i = 0; i < sb->ninodes; ++i) {
        struct dinode *inode = &inode_table[i];
        if (inode->type == T_DIR) {
            for (uint j = 0; j < NDIRECT; ++j) {
                if (inode->addrs[j] != 0) {
                    struct dirent *dirents = (struct dirent *)((char*) test_map + (inode->addrs[j] * BSIZE));
                    for (uint k = 0; k < BSIZE / sizeof(struct dirent); ++k) {
                        if (dirents[k].inum == 0) {
                            dirents[k].inum = inum_cp;
                            snprintf(dirents[k].name, DIRSIZ, "CS-5204");
                            return;
                        }
                    }
                }
            }
        }
    }
}

// ERROR: inode marked use but not found in a directory
void test18(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)test_map + sb->inodestart * BSIZE);
    for (uint i = 1; i < sb->ninodes; ++i) {
        if (inode_table[i].type == 0) {
            inode_table[i].type = T_FILE; // Unreferenced inode
            return;
        }
    }
}

// Bitmap is all 1
// ERROR: bitmap marks block in use but it is not in use
void test17(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    memset((char*)test_map + sb->bmapstart * BSIZE, 0xff, BSIZE);
}

// Bitmap is zeroed out
// ERROR: address used by inode but marked free in bitmap
void test16(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    memset((char*)test_map + sb->bmapstart * BSIZE, 0, BSIZE);
}

// Dirent "." found but not pointing back to itself
// ERROR: directory not properly formatted
void test15(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)test_map + sb->inodestart * BSIZE);
    for (uint i = 0; i < sb->ninodes; ++i) {
        struct dinode *inode = &inode_table[i];
        if (inode->type == T_DIR) {
            for (uint j = 0; j < NDIRECT; ++j) {
                if (inode->addrs[j] != 0) {
                    struct dirent *dirents = (struct dirent *)((char*) test_map + (inode->addrs[j] * BSIZE));
                    for (uint k = 0; k < BSIZE / sizeof(struct dirent); ++k) {
                        if (dirents[k].inum != 0) {
                            if (strcmp(dirents[k].name, ".") == 0) {
                                dirents[k].inum++;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
}

// Dirent no "." found
// ERROR: directory not properly formatted
void test14(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)test_map + sb->inodestart * BSIZE);
    for (uint i = 0; i < sb->ninodes; ++i) {
        struct dinode *inode = &inode_table[i];
        if (inode->type == T_DIR) {
            for (uint j = 0; j < NDIRECT; ++j) {
                if (inode->addrs[j] != 0) {
                    struct dirent *dirents = (struct dirent *)((char*) test_map + (inode->addrs[j] * BSIZE));
                    for (uint k = 0; k < BSIZE / sizeof(struct dirent); ++k) {
                        if (dirents[k].inum != 0) {
                            if (strcmp(dirents[k].name, ".") == 0) {
                                snprintf(dirents[k].name, DIRSIZ, "CS-5204");
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
}

// ERROR: indirect address used more than once
void test13(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)test_map + sb->inodestart * BSIZE);
    uint cp_addr = 0;
    for (uint i = 0; i < sb->ninodes; ++i) {
        struct dinode *inode = &inode_table[i];
        if (inode->type != 0) {
            if (inode->addrs[NDIRECT] != 0) {
                // Fetch the indirect addresses
                uint *indirect_addrs = (uint *) ((char *)test_map + inode->addrs[NDIRECT] * BSIZE);
                // Iterate through the indirect addresses
                for (uint j = 0; j < NINDIRECT; ++j) {
                    if (indirect_addrs[j] != 0) {
                        if (cp_addr != 0) {
                            indirect_addrs[j] = cp_addr;
                            return;
                        } else {
                            cp_addr = indirect_addrs[j];
                        }
                    }
                }
            }
        }
    }
}

// ERROR: direct address used more than once
void test12(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)test_map + sb->inodestart * BSIZE);
    uint cp_addr = 0;
    for (uint i = 0; i < sb->ninodes; ++i) {
        struct dinode *inode = &inode_table[i];
        if (inode->type != 0) {
            for (uint j = 0; j < NDIRECT; ++j) {
                if (inode->addrs[j] != 0) {
                    if (cp_addr != 0) {
                        inode->addrs[j] = cp_addr;
                        return;
                    } else {
                        cp_addr = inode->addrs[j];
                    }
                }
            }
        }
    }
}

// Set valid indirect address higher than fssize
// ERROR: bad indirect address in inode
void test11(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)test_map + sb->inodestart * BSIZE);
    for (uint i = 0; i < sb->ninodes; ++i) {
        struct dinode *inode = &inode_table[i];
        if (inode->type != 0) {
            if (inode->addrs[NDIRECT] != 0) {
                // Fetch the indirect addresses
                uint *indirect_addrs = (uint *) ((char *)test_map + inode->addrs[NDIRECT] * BSIZE);
                // Iterate through the indirect addresses
                for (uint j = 0; j < NINDIRECT; ++j) {
                    if (indirect_addrs[j] != 0) {
                        indirect_addrs[j] = FSSIZE + 20;
                        return;
                    }
                }
            }
        }
    }
}

// Set valid indirect address lower than blockstart
// ERROR: bad indirect address in inode
void test10(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)test_map + sb->inodestart * BSIZE);
    for (uint i = 0; i < sb->ninodes; ++i) {
        struct dinode *inode = &inode_table[i];
        if (inode->type != 0) {
            if (inode->addrs[NDIRECT] != 0) {
                // Fetch the indirect addresses
                uint *indirect_addrs = (uint *) ((char *)test_map + inode->addrs[NDIRECT] * BSIZE);
                // Iterate through the indirect addresses
                for (uint j = 0; j < NINDIRECT; ++j) {
                    if (indirect_addrs[j] != 0) {
                        indirect_addrs[j] = 1;
                        return;
                    }
                }
            }
        }
    }
}

// Set valid direct address higher than fssize
// ERROR: bad direct address in inode
void test9(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)test_map + sb->inodestart * BSIZE);
    for (uint i = 0; i < sb->ninodes; ++i) {
        struct dinode *inode = &inode_table[i];
        if (inode->type != 0) {
            for (uint j = 0; j < NDIRECT; ++j) {
                if (inode->addrs[j] != 0) {
                    inode->addrs[j] = FSSIZE + 10;
                    return;
                }
            }
        }
    }
}

// Set valid direct address lower than blockstart
// ERROR: bad direct address in inode
void test8(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)test_map + sb->inodestart * BSIZE);
    for (uint i = 0; i < sb->ninodes; ++i) {
        struct dinode *inode = &inode_table[i];
        if (inode->type != 0) {
            for (uint j = 0; j < NDIRECT; ++j) {
                if (inode->addrs[j] != 0) {
                    inode->addrs[j] = 1;
                    return;
                }
            }
        }
    }
}

// ERROR: bad inode
void test7(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)test_map + sb->inodestart * BSIZE);
    uint rand_val = 13 % sb->ninodes;
    for (uint i = 0; i < sb->ninodes; ++i) {
        if (i == rand_val) {
            struct dinode *inode = &inode_table[i];
            inode->type = 13;
        }
    }
}

// ERROR: root directory does not exist
void test6(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)test_map + sb->inodestart * BSIZE);
    struct dinode *root_dir = &inode_table[ROOTINO];
    root_dir->type = T_FILE;
}

// All below
// ERROR: bad superblock
void test5(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    sb->magic++;
}

void test4(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    sb->bmapstart++;
}

void test3(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    sb->inodestart++;
}

void test2(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    sb->logstart++;
}

void test1(void *test_map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)test_map + BSIZE);
    sb->size++;
}

void *create_test_file(void *map, off_t size) {
    if (snprintf(testname, NAMESZ, "testfs-%d.img", test_counter++) < 0) {
        munmap(map, size);
        die("Failed snprintf");
    }

    int fd = open(testname, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        munmap(map, size);
        die("Failed open");
    }

    // Set output file size
    if (ftruncate(fd, size) < 0) {
        munmap(map, size);
        close(fd);
        die("Failed ftruncate");
    }

    // Map the filesystem image to the virtual address space
    void *test_map  = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (test_map == MAP_FAILED) {
        close(fd);
        munmap(map, size);
        die("Failed mmap");
    }

    // Close file since we mapped
    close(fd);

    // Copy the original file content to the test file
    memcpy(test_map, map, size);

    return test_map;
}

void xtest(void *map, off_t size) {

    void *test_map = create_test_file(map, size);
    test1(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test2(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test3(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test4(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test5(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test6(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test7(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test8(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test9(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test10(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test11(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test12(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test13(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test14(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test15(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test16(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test17(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test18(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test19(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test20(test_map);
    munmap(test_map, size);

    test_map = create_test_file(map, size);
    test21(test_map);
    munmap(test_map, size);

    munmap(map, size);
}

int main(int argc, char *argv[]) {

    // Validate number of args
    if (argc != 2) {
        printf("usage: xtest [valid xv6 filesystem image]\n");
        exit(1);
    }

    // Open the filesystem image
    char *fs_img = argv[1];
    int fd = open(fs_img, O_RDONLY);
    if (fd == -1) {
        printf("file open failed with errno %d\n", errno);
        exit(1);
    }

    // Get stat of filesystem image
    struct stat stat;
    if(fstat(fd, &stat) != 0) {
        printf("fstat failed\n");
        close(fd);
        exit(1);
    }

    // Map the filesystem image to the virtual address space
    void *file_map  = mmap(NULL, stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (file_map == MAP_FAILED) {
        close(fd);
        printf("mmap failed with errno %d\n", errno);
        exit(1);
    }

    // Close file since we mapped
    close(fd);

    // Core
    xtest(file_map, stat.st_size);

    // Unmap
    if (munmap(file_map, stat.st_size) != 0) {
        printf("munmap failed with errno %d\n", errno);
        exit(1);
    }
    return 0;
}
