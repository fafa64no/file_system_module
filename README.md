# TP3 : File system

## English

The code used to display the file structure in the terminal can be found in
[file_mapping.c](file_mapping.c).

The fuse module is written in
[fuse_lowlevel_ops.c](fuse_lowlevel_ops.c).

## French

Le code permettant d'afficher le système de fichier est 
[file_mapping.c](file_mapping.c).

Le module fuse est quand à lui situé dans
[fuse_lowlevel_ops.c](fuse_lowlevel_ops.c).

## Commands

Compile the code:
```shell
# Compile the code
echo "gcc -Wall fuse_lowlevel_ops.c `pkg-config fuse --cflags --libs` -o fuse_lowlevel_ops" | bash

# Run the code
./fuse_lowlevel_ops /tmp/futosfs -d
```





