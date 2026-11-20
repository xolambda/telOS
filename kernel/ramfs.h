#ifndef RAMFS_H
#define RAMFS_H

#include <stddef.h>
#include <stdint.h>

#define RAMFS_MAX_FILES 64
#define RAMFS_MAX_NAME  32

typedef struct {
    int   used;
    char  name[RAMFS_MAX_NAME];
    char *data;
    size_t size;
} ramfs_file_t;

void  ramfs_init(void);

int   ramfs_create(const char *name);
int   ramfs_write(const char *name, const void *data, size_t len);
int   ramfs_append(const char *name, const void *data, size_t len);
int   ramfs_read(const char *name, void *out, size_t max);
int   ramfs_size(const char *name);
int   ramfs_remove(const char *name);
int   ramfs_exists(const char *name);

int   ramfs_count(void);
ramfs_file_t *ramfs_get(int index);

#endif
