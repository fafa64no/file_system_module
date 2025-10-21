//
// Created by sebas on 10/21/25.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "tosfs.h"

#define EXAMPLE_FILE_PATH "test_tosfs_files"

#define SYSTEM_CALL_ERROR (-1)
#define MAX_INODE_NUMBER (TOSFS_BLOCK_SIZE / sizeof(struct tosfs_inode))


struct mapped_file_struct {
    int fd;
    void* mapped_file;
    struct stat file_info;

    struct tosfs_superblock* superblock;
    struct tosfs_inode* inodes;
    struct tosfs_dentry* entries;
};


struct mapped_file_struct* map_example_file() {
    struct mapped_file_struct* mapped_file = malloc(sizeof(struct mapped_file_struct));

    mapped_file->fd = open(EXAMPLE_FILE_PATH, O_RDONLY);
    if (mapped_file->fd == SYSTEM_CALL_ERROR) {
        perror("map_example_file: open");
        exit(EXIT_FAILURE);
    }

    if (stat(EXAMPLE_FILE_PATH, &mapped_file->file_info) == SYSTEM_CALL_ERROR) {
        perror("map_example_file: stat");
        exit(EXIT_FAILURE);
    }

    mapped_file->mapped_file = mmap(
        NULL,
        mapped_file->file_info.st_size,
        PROT_READ,
        MAP_SHARED,
        mapped_file->fd,
        0
    );

    return mapped_file;
}

void close_mapped_file(struct mapped_file_struct* mapped_file) {
    if (munmap(mapped_file->mapped_file, mapped_file->file_info.st_size) == SYSTEM_CALL_ERROR) {
        close(mapped_file->fd);
        perror("close_mapped_file: munmap");
        exit(EXIT_FAILURE);
    }
    close(mapped_file->fd);
    free(mapped_file);
}


void read_mapped_file_as_tosfs_file(struct mapped_file_struct* mapped_file) {
    void* tmp_ptr = mapped_file->mapped_file;

    mapped_file->superblock = (struct tosfs_superblock*) tmp_ptr;
    if (mapped_file->superblock->magic != TOSFS_MAGIC) {
        perror("read_mapped_file_as_tosfs_file: bad magic");
        exit(EXIT_FAILURE);
    }
    if (mapped_file->superblock->inodes > MAX_INODE_NUMBER) {
        perror("read_mapped_file_as_tosfs_file: too many inodes");
        exit(EXIT_FAILURE);
    }

    tmp_ptr += TOSFS_BLOCK_SIZE;
    mapped_file->inodes = (struct tosfs_inode*) tmp_ptr;

    tmp_ptr += TOSFS_BLOCK_SIZE;
    mapped_file->entries = (struct tosfs_dentry*) tmp_ptr;
}


void disp_structure_tosfs_superblock(const struct mapped_file_struct* mapped_file) {
    const struct tosfs_superblock* superblock = mapped_file->superblock;

    printf(
        "Reading mapped file as tosfs_superblock:"
        "\n\tmagic: %d"     "\n\tblock_bitmap: %d"      "\n\tinode_bitmap: %d"      "\n\tblock_size: %d"
        "\n\tblocks: %d"    "\n\tinodes: %d"            "\n\troot_inode: %d"        "\n",
        superblock->magic,  superblock->block_bitmap,   superblock->inode_bitmap,   superblock->block_size,
        superblock->blocks, superblock->inodes,         superblock->root_inode
    );
}

void disp_structure_tosfs_inode(const struct mapped_file_struct* mapped_file) {
    const unsigned int root_inode_number = mapped_file->superblock->root_inode;
    const unsigned int last_inode_number = root_inode_number + mapped_file->superblock->inodes;

    for (unsigned int i = root_inode_number; i < last_inode_number; i++) {
        const struct tosfs_inode* inode = &mapped_file->inodes[i];
        printf(
            "Reading node nb %u:"
            "\n\tinode: %d" "\n\tblock_no: %d"  "\n\tuid: %d"   "\n\tgid: %d"   "\n\tmode: %d"
            "\n\tperm: %d"  "\n\tsize: %d"      "\n\tnlink: %d" "\n",
            i,
            inode->inode,   inode->block_no,    inode->uid,     inode->gid,
            inode->mode,    inode->perm,        inode->size,    inode->nlink
        );
    }
}

void disp_structures_tosfs() {
    struct mapped_file_struct* mapped_file = map_example_file();
    read_mapped_file_as_tosfs_file(mapped_file);

    printf("\n----------------------------------------------\n");
    disp_structure_tosfs_superblock(mapped_file);
    printf("\n");
    disp_structure_tosfs_inode(mapped_file);
    printf("----------------------------------------------\n");

    close_mapped_file(mapped_file);
}


int main(int argc, char *argv[]) {
    printf("\ntosfs can support %lu inodes\n", MAX_INODE_NUMBER);

    disp_structures_tosfs();

    return EXIT_SUCCESS;
}

















