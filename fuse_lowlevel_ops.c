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
#include <assert.h>

#include "tosfs.h"

#define EXAMPLE_FILE_PATH "test_tosfs_files"

#define SYSTEM_CALL_ERROR (-1)
#define MAX_INODE_NUMBER (TOSFS_BLOCK_SIZE / sizeof(struct tosfs_inode))
#define MAX_INODE_ENTRY_NUMBER (TOSFS_BLOCK_SIZE / sizeof(struct tosfs_inode))
#define MAX_NB_DATA_BLOCKS (29)

#define min_macro(x, y) ((x) < (y) ? (x) : (y))
#define max_macro(x, y) ((x) > (y) ? (x) : (y))

static const char *tmp_str = "Megalo Panzer Salad!!!\n";


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

struct directory_buffer {
    char* data_pointer;
    size_t size;
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
    if (inode->inode == mapped_file->superblock->root_inode) {
        stbuf->st_ino = (ino_t) inode->inode;
        stbuf->st_nlink = (nlink_t) inode->nlink;
        stbuf->st_mode = S_IFDIR | 0755;
        return EXIT_SUCCESS;
    }

    if (inode->inode < mapped_file->superblock->root_inode || inode->inode >= MAX_INODE_NUMBER) {
        return SYSTEM_CALL_ERROR;
    }

    stbuf->st_ino = (ino_t) inode->inode;
    stbuf->st_uid = (uid_t) inode->uid;
    stbuf->st_gid = (gid_t) inode->gid;
    stbuf->st_nlink = (nlink_t) inode->nlink;

    if (ino != mapped_file->superblock->root_inode) {
        stbuf->st_size = (off_t) inode->size;
    }

    stbuf->st_mode = S_IFREG | (inode->perm & 0777);

    return EXIT_SUCCESS;
}

static void dirbuf_add(fuse_req_t req, struct directory_buffer *buf, const char *name, fuse_ino_t ino) {
    struct stat stbuf;
    const size_t old_size = buf->size;
    buf->size += fuse_add_direntry(req, NULL, 0, name, NULL, 0);
	buf->data_pointer = (char *) realloc(buf->data_pointer, buf->size);
    memset(&stbuf, 0, sizeof(stbuf));
    stbuf.st_ino = ino;
    fuse_add_direntry(req, buf->data_pointer + old_size, buf->size - old_size, name, &stbuf, buf->size);
}

static int reply_buf_limited(fuse_req_t req, const char *buf, const size_t buffer_size, const off_t offset, size_t maxsize) {
    fprintf(stderr, "reply_buf_limited: maxsize: %lu\n", maxsize);
    fflush(stderr);

    if (offset < buffer_size) {
        return fuse_reply_buf(req, buf + offset, min_macro(buffer_size - offset, maxsize));
    }

    return fuse_reply_buf(req, NULL, 0);
}


static void ensea_ll_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
    struct fuse_entry_param entry_param;
    struct tosfs_dentry* disk_entry = mapped_file->root_block;

    for (unsigned int entry_number = 0; entry_number < MAX_INODE_ENTRY_NUMBER; entry_number++) {
        if (disk_entry->inode == 0) {
            // We don't break the loop because (depending on how we remove inodes) all inodes might not be at the start
            // of the inode map
            disk_entry++;
            continue;
        }

        if (parent != 1 || strcmp(name, disk_entry->name) != 0) {
            disk_entry++;
            continue;
        }

        memset(&entry_param, 0, sizeof(entry_param));
        entry_param.ino = disk_entry->inode;
        entry_param.attr_timeout = 1.0;
        entry_param.entry_timeout = 1.0;
        ensea_ll_stat(entry_param.ino, &entry_param.attr);

        fuse_reply_entry(req, &entry_param);
        return;
    }

    fuse_reply_err(req, ENOENT);
}

static void ensea_ll_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    struct stat stbuf = {0};
    (void) fi;

    if (ensea_ll_stat(ino, &stbuf) == SYSTEM_CALL_ERROR) {
        fuse_reply_err(req, ENOENT);
    } else {
        fuse_reply_attr(req, &stbuf, 1.0);
    }
}

static void ensea_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi) {
    (void) fi;

    if (ino != mapped_file->superblock->root_inode) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }

    struct directory_buffer buf = {0};

    struct tosfs_dentry* disk_entry = mapped_file->root_block;
    for (unsigned int entry_number = 0; entry_number < MAX_INODE_ENTRY_NUMBER; entry_number++) {
        if (disk_entry->inode == 0) {
            disk_entry++;
            continue;
        }

        dirbuf_add(req, &buf, disk_entry->name, disk_entry->inode);
        disk_entry++;
    }

    reply_buf_limited(req, buf.data_pointer, buf.size, off, size);
    free(buf.data_pointer);
}

static void ensea_ll_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    /// TODO open file
    if (ino != 2) {
        fuse_reply_err(req, EISDIR);
    } else if ((fi->flags & 3) != O_RDONLY) {
        fuse_reply_err(req, EACCES);
    } else {
        fuse_reply_open(req, fi);
    }
}

static void ensea_ll_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi) {
    /// TODO output file content
    (void) fi;

    assert(ino == 2);
    reply_buf_limited(req, tmp_str, strlen(tmp_str), off, size);
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










