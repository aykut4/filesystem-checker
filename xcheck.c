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

/*
 * Check #8: Consistency of inodes that are used and their references
 * For every inode, we keep track of if they are used and also how many times
 * they are referenced in directories via their dirent structures.
 * We need to make sure that for an inode that is used, it has a non-zero ref,
 * and for a non used inode it has a zero ref.
 * Also, for every used inode, if they are a file, their nlink must be equal to
 * the ref count, and if they are a directory, their ref must be 1.
 */
void check8(struct superblock *sb, struct dinode *inode_table, uint *inode_used, uint *inode_refd) {
    // Check #8 used inode is also referenced 
    for (uint i = 0; i < sb->ninodes; ++i) {
        if (inode_used[i] && inode_refd[i] == 0) {
            die("inode marked use but not found in a directory");
        }
        if (!inode_used[i] && inode_refd[i] > 0) {
            die("inode referred to in directory but marked free");
        }
        if (inode_used[i]) {
            struct dinode *inode = &inode_table[i];
            if (inode->type == T_FILE && inode->nlink != inode_refd[i]) {
                die("bad reference count for file");
            }
            if (inode->type == T_DIR && inode_refd[i] != 1) {
                die("directory appears more than once in file system");
            }
        }
    }
}

/*
 * Check #7: Blocks used and bitmap consistency
 * For every inode, we check their direct and indirect entries and mark the
 * blocks that are used as used in block_used data structure. This should be
 * equivalent to the bitmap that's written in the filesystem image. Thus, we
 * check for every bit (which corresponds to each block in fs img), if the
 * corresponding block was actually used or not.
 */
void check7(uint8 *bitmap, uint *block_used, uint8 nbitmaps) {
    for (uint8 i = 0; i < nbitmaps; ++i) {
        for (uint8 j = 0; j < 8; ++j) {
            uint8 bit = (bitmap[i] >> j) & 1;
            if (bit && !block_used[(i * 8) + j]) {
                die("bitmap marks block in use but it is not in use");
            }
            if (!bit && block_used[(i * 8) + j]) {
                die("address used by inode but marked free in bitmap");
            }
        }
    }
}

/*
 * Check #6v2: Directory not properly formatted
 * For every inode that is in use and is of type T_DIR, we check if they had
 * "." and ".." dirents as they must.
 */
void check6v2(struct dinode *inode, uint current_path_found, uint parent_path_found) {
    if (inode->type == T_DIR && (!current_path_found || !parent_path_found)) {
        die("directory not properly formatted1");
    }
}

/*
 * Check #6: Directory not properly formatted
 * Every inode that has a type T_DIR is a directory and they contain dirent
 * structures. We extract these dirent structures and check if they have
 * mandatory dirents with path "." and "..". We also make sure that the inum
 * of the dirent with "." path has the same inode as the directory itself.
 * Finally, we count references to the inodes that are referred to by dirents
 * with non-zero inums.
 */
void check6 (void *map, short type, uint addr, uint i, uint *inode_refd, uint *current_path_found, uint *parent_path_found) {
    if (type == T_DIR) {
        struct dirent *dirents = (struct dirent *)((char*) map + (addr * BSIZE));
        for (uint k = 0; k < BSIZE / sizeof(struct dirent); ++k) {
            if (dirents[k].inum != 0) {
                if (strcmp(dirents[k].name, ".") == 0) {
                    *current_path_found = 1;
                    if (dirents[k].inum != i) {
                        die("directory not properly formatted");
                    }
                } else if (strcmp(dirents[k].name, "..") == 0) {
                    *parent_path_found = 1;
                } else {
                    inode_refd[dirents[k].inum]++;
                }
            }
        }
    }
}

/*
 * Check #5v2: Indirect address used more than once
 * Same as #5 but for indirect addresses.
 */
void check5v2(uint addr, uint *block_used) {
    if (addr != 0) {
        if (block_used[addr]) {
            die("indirect address used more than once");
        }
        block_used[addr] = 1;
    }
}

/*
 * Check #5: Direct address used more than once
 * We keep track of direct addresses to see if they are really used.
 * As we keep track of them, we also need to make sure that they are not
 * being used more than once as this would make the filesystem inconsistent.
 * So we check if any non-zero address is used before and set it to used.
 */
void check5(uint addr, uint *block_used) {
    if (addr != 0) {
        if (block_used[addr]) {
            die("direct address used more than once");
        }
        block_used[addr] = 1;
    }
}

/*
 * Check #4v3: Bad indirect address in inode
 * When an inode->addrs[NDIRECT] is valid, it means we have indirect addresses,
 * then we access the pointer that points to the indirect addresses of an inode
 * we need to make sure that this pointer is also valid.
 */
void check4v3(uint *indirect_addrs) {
    if (!indirect_addrs) {
        die("bad indirect address in inode");
    }
}

/*
 * Check #4v2: Bad indirect address in inode
 * The same as #4 but this time check for indirect addresses.
 */
void check4v2(struct superblock *sb, uint addr, uint blockstart) {
    if (addr != 0) {
        if (addr > sb->size || addr < blockstart) {
            die("bad indirect address in inode");
        }
    }
}

/*
 * Check #4: Bad direct address in inode
 * For every inode that is in use, if their direct addresses are in use,
 * which is indicated with the addr field being non-zero, then make sure,
 * this addr is within the boundaries of the filesystem image and it has
 * a valid value.
 */
void check4(struct superblock *sb, uint addr, uint blockstart) {
    if (addr != 0) {
        if (addr > sb->size || addr < blockstart) {
            die("bad direct address in inode");
        }
    }
}

/*
 * Check #3: Bad inode
 * Make sure that any inode is either not used, type 0, or if it's used,
 * the type is set to one of three valid values.
 */
void check3(struct dinode *inode) {
    if (inode->type != 0 && inode->type != T_FILE && inode->type != T_DIR && inode->type != T_DEVICE) {
        die("bad inode");
    }
}

/*
 * Check #2: Root validation
 * Make sure that the root directory exists, it exists at where it should,
 * and it's type is also set properly.
 */
void check2(struct dinode *inode_table) {
    struct dinode *root_dir = &inode_table[ROOTINO];
    if (root_dir->type != T_DIR) {
        die("root directory does not exist");
    }
}

/*
 * Check #1: Superblock consistency
 * Make sure that the fields of the superblock are consistent with each other
 * and the filesize image.
 */
void check1(struct superblock *sb, uint inodes_block_size, uint bitmaps_block_size) {
    if (sb->size != 2 + sb->nlog + inodes_block_size + bitmaps_block_size + sb->nblocks) {
        die("bad superblock1");
    }
    if (sb->logstart != 2) {
        die("bad superblock2");
    }
    if (sb->inodestart != sb->logstart + sb->nlog) {
        die("bad superblock3");
    }
    if (sb->bmapstart != sb->inodestart + inodes_block_size) {
        die("bad superblock4");
    }
    if (sb->magic != FSMAGIC) {
        die("bad superblock magic");
    }
}

void sb_log(struct superblock *sb, uint inodes_block_size, uint bitmaps_block_size, uint blockstart, uint8 nbitmaps) {
    // logs
    printf("sb->magic: 0x%x\n", sb->magic);
    printf("sb->size: 0x%x\n", sb->size);
    printf("sb->nblocks: 0x%x\n", sb->nblocks);
    printf("sb->ninodes: 0x%x\n", sb->ninodes);
    printf("sb->nlog: 0x%x\n", sb->nlog);
    printf("sb->logstart: 0x%x\n", sb->logstart);
    printf("sb->inodestart: 0x%x\n", sb->inodestart);
    printf("sb->bmapstart: 0x%x\n", sb->bmapstart);
    printf("nbitmaps: 0x%x\n", nbitmaps);
    printf("bitmaps_block_size: 0x%x\n", bitmaps_block_size);
    printf("blockstart: 0x%x\n", blockstart);
    printf("inodes_block_size: 0x%x\n", inodes_block_size);
    printf("Expected data blocks start at: %d\n", sb->bmapstart + bitmaps_block_size);
    printf("Computed blockstart: %d\n", blockstart);
}

void xcheck(void *map) {
    // Get the superblock
    struct superblock *sb = (struct superblock *) ((char *)map + BSIZE);

    // Get the number of bitmaps each of which is a byte
    uint8 nbitmaps = sb->nblocks / 8 + (sb->nblocks % 8 != 0);

    // Get the size of total bitmaps in blocks
    uint bitmaps_block_size = ((nbitmaps * sizeof(uint8)) / BSIZE) + ((nbitmaps * sizeof(uint8)) % BSIZE != 0);

    // Get the size of total inodes in blocks
    uint inodes_block_size = ((sb->ninodes * sizeof(struct dinode)) / BSIZE)
        + ((sb->ninodes * sizeof(struct dinode)) % BSIZE != 0);

    // Get the start of the datablocks in blocks
    uint blockstart = 2 + sb->nlog + inodes_block_size + bitmaps_block_size;

    // Get the inode_table which is a table of sb->ninodes many dinode
    struct dinode *inode_table = (struct dinode *)((char *)map + sb->inodestart * BSIZE);

    // Get the bitmap table which is a table of 4 byte bitmaps
    // The total number of bitmaps is 4*8=32 bit for each, nblocks many bits
    // nblocks divided by 32 gives us how many iters we need
    uint8 *bitmap = (uint8 *)((char *)map + sb->bmapstart * BSIZE);

    // Create a bitmap of used blocks from inodes
    uint block_used[sb->size];
    memset(block_used, 0, sizeof(block_used));

    // Record the used blocks until data blocks
    for (uint i = 0; i < blockstart; i++) {
        block_used[i] = 1;
    }

    // Create a bitmap of used inodes 
    uint inode_used[sb->ninodes];
    memset(inode_used, 0, sizeof(inode_used));

    // Create a bitmap of inodes referred to in a dir
    uint inode_refd[sb->ninodes];
    memset(inode_refd, 0, sizeof(inode_refd));
    inode_refd[ROOTINO] = 1;

    // sb_log(sb, inodes_block_size, bitmaps_block_size, blockstart, nbitmaps);

    check1(sb, inodes_block_size, bitmaps_block_size);
    check2(inode_table);
    
    for (uint i = 0; i < sb->ninodes; ++i) {
        struct dinode *inode = &inode_table[i];

        check3(inode);

        // In use inodes
        if (inode->type != 0) {

            // Mark as used inode
            inode_used[i] = 1;

            // Flags to mark "." and ".." dirents found if T_DIR
            uint current_path_found = 0;
            uint parent_path_found = 0;

            // Iterate through direct addresses of the inode
            for (uint j = 0; j < NDIRECT; ++j) {
                check4(sb, inode->addrs[j], blockstart);
                check5(inode->addrs[j], block_used);
                check6(map, inode->type, inode->addrs[j], i, inode_refd, &current_path_found, &parent_path_found);
            }

            check4(sb, inode->addrs[NDIRECT], blockstart);
            check5(inode->addrs[NDIRECT], block_used);

            // Check if inode has indirect addresses
            if (inode->addrs[NDIRECT] != 0) {

                // Fetch the indirect addresses
                uint *indirect_addrs = (uint *) ((char *)map + inode->addrs[NDIRECT] * BSIZE);
                check4v3(indirect_addrs);

                // Iterate through the indirect addresses
                for (uint j = 0; j < NINDIRECT; ++j) {
                    check4v2(sb, indirect_addrs[j], blockstart);
                    check5v2(indirect_addrs[j], block_used);
                    check6(map, inode->type, indirect_addrs[j], i, inode_refd, &current_path_found, &parent_path_found);
                }
            }

            check6v2(inode, current_path_found, parent_path_found);
        }
    }

    check7(bitmap, block_used, nbitmaps);
    check8(sb,inode_table, inode_used, inode_refd);
}

int main(int argc, char *argv[]) {

    // Read the optional repair flag
    int repair_flag = 0;
    int c;
    while ((c = getopt(argc, argv, "r")) != -1) {
        switch (c) {
        case 'r':
            printf("repair flag is set\n");
            repair_flag = 1;
            break;
        default:
            printf("repair flag is not set\n");
            break;
        }
    }

    // Validate number of args
    if (argc != 2 + repair_flag) {
        printf("usage: xcheck [-r | optional repair flag] [xv6 filesystem image]\n");
        exit(1);
    }

    // Open the filesystem image
    char *fs_img = argv[optind];
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
    xcheck(file_map);

    // Unmap
    if (munmap(file_map, stat.st_size) != 0) {
        printf("munmap failed with errno %d\n", errno);
        exit(1);
    }
    return 0;
}
