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


struct mapped_file_struct {
    int fd;
    void* mapped_file;
    struct stat file_info;
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


void disp_structure_tosfs_superblock(const struct mapped_file_struct* mapped_file) {
    const struct tosfs_superblock* superblock = (struct tosfs_superblock*) mapped_file->mapped_file;

    printf(
        "Reading mapped file as tosfs_superblock:"
        "\n\tmagic: %d"
        "\n\tblock_bitmap: %d"
        "\n\tinode_bitmap: %d"
        "\n\tblock_size: %d"
        "\n\tblocks: %d"
        "\n\tinodes: %d"
        "\n\troot_inode: %d",
        superblock->magic,
        superblock->block_bitmap,
        superblock->inode_bitmap,
        superblock->block_size,
        superblock->blocks,
        superblock->inodes,
        superblock->root_inode
    );
}

void disp_structures_tosfs() {
    struct mapped_file_struct* mapped_file = map_example_file();
    disp_structure_tosfs_superblock(mapped_file);
    close_mapped_file(mapped_file);
}


int main(int argc, char *argv[]) {
    disp_structures_tosfs();
    return EXIT_SUCCESS;
}

















