//
// Created by sebas on 10/21/25.
//

#define FUSE_USE_VERSION 26

#include <fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/mman.h>

#include "tosfs.h"

#define EXAMPLE_FILE_PATH "test_tosfs_files"

#define SYSTEM_CALL_ERROR (-1)
#define MAX_INODE_NUMBER (TOSFS_BLOCK_SIZE / sizeof(struct tosfs_inode))
#define MAX_INODE_ENTRY_NUMBER (TOSFS_BLOCK_SIZE / sizeof(struct tosfs_inode))
#define MAX_NB_DATA_BLOCKS (29)


struct data_block_structure {
    char data[TOSFS_BLOCK_SIZE];
};

struct mapped_file_struct {
    int fd;
    void* mapped_file;
    struct stat file_info;

    struct tosfs_superblock* superblock;
    struct tosfs_inode* inodes;
    struct tosfs_dentry* root_block;
    struct data_block_structure* data_blocks;
};

static struct mapped_file_struct* mapped_file;


static struct mapped_file_struct* map_example_file() {
    mapped_file = malloc(sizeof(struct mapped_file_struct));

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

static void close_mapped_file() {
    if (munmap(mapped_file->mapped_file, mapped_file->file_info.st_size) == SYSTEM_CALL_ERROR) {
        close(mapped_file->fd);
        perror("close_mapped_file: munmap");
        exit(EXIT_FAILURE);
    }
    close(mapped_file->fd);
    free(mapped_file);
}

static void read_mapped_file_as_tosfs_file() {
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
    mapped_file->root_block = (struct tosfs_dentry*) tmp_ptr;

    tmp_ptr += TOSFS_BLOCK_SIZE;
    mapped_file->data_blocks = (struct data_block_structure*) tmp_ptr;
}


static int ensea_ll_stat(fuse_ino_t ino, struct stat *stbuf) {
    struct tosfs_inode* inode = &mapped_file->inodes[ino];
    if (inode == NULL) {
        return SYSTEM_CALL_ERROR;
    }

    stbuf->st_ino = (ino_t) inode->inode;
    stbuf->st_uid = (uid_t) inode->uid;
    stbuf->st_gid = (gid_t) inode->gid;
    stbuf->st_nlink = (nlink_t) inode->nlink;

    stbuf->st_size = (off_t) inode->size;
    stbuf->st_blksize = TOSFS_BLOCK_SIZE;
    stbuf->st_blocks = 1;

    mode_t perms = (mode_t)(inode->perm & 0777);
    stbuf->st_mode = S_IFREG | perms;

    return EXIT_SUCCESS;
}


static void ensea_ll_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
    /// TODO find inode corresponding to name
}

static void ensea_ll_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    struct stat stbuf = {0};

    if (ensea_ll_stat(ino, &stbuf) == SYSTEM_CALL_ERROR) {
        fuse_reply_err(req, ENOENT);
    } else {
        fuse_reply_attr(req, &stbuf, 1.0);
    }
}

static void ensea_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi) {
    /// TODO list all files in dir
}

static void ensea_ll_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    /// TODO
}

static void ensea_ll_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi) {
    /// TODO output file content
}


static struct fuse_lowlevel_ops ensea_ll_oper = {
    .lookup		= ensea_ll_lookup,
    .getattr	= ensea_ll_getattr,
    .readdir	= ensea_ll_readdir,
    .open		= ensea_ll_open,
    .read		= ensea_ll_read,
};


int main(int argc, char *argv[]) {
    map_example_file();
    read_mapped_file_as_tosfs_file();

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_chan *ch;
    char *mountpoint;
    int errors = -1;

    if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
        (ch = fuse_mount(mountpoint, &args)) != NULL) {
        struct fuse_session *se;

        se = fuse_lowlevel_new(&args, &ensea_ll_oper,
                       sizeof(ensea_ll_oper), NULL);
        if (se != NULL) {
            if (fuse_set_signal_handlers(se) != -1) {
                fuse_session_add_chan(se, ch);
                errors = fuse_session_loop(se);
                fuse_remove_signal_handlers(se);
                fuse_session_remove_chan(ch);
            }
            fuse_session_destroy(se);
        }
        fuse_unmount(mountpoint, ch);
    }
    fuse_opt_free_args(&args);

    close_mapped_file();
	return errors ? EXIT_FAILURE : EXIT_SUCCESS;
}










