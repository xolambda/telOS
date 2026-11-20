#include "ramfs.h"
#include "heap.h"
#include "string.h"
#include "serial.h"
#include "panic.h"

static ramfs_file_t files[RAMFS_MAX_FILES];

static int find(const char *name) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0) return i;
    }
    return -1;
}

static int find_free(void) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used) return i;
    }
    return -1;
}

void ramfs_init(void) {
    memset(files, 0, sizeof(files));
    serial_puts("[ramfs] initialized\n");
}

int ramfs_create(const char *name) {
    if (!name || name[0] == '\0') return -1;
    if (strlen(name) >= RAMFS_MAX_NAME) return -1;
    if (find(name) >= 0) return -1;

    int i = find_free();
    if (i < 0) return -1;

    files[i].used = 1;
    strncpy(files[i].name, name, RAMFS_MAX_NAME - 1);
    files[i].name[RAMFS_MAX_NAME - 1] = '\0';
    files[i].data = 0;
    files[i].size = 0;
    return 0;
}

int ramfs_write(const char *name, const void *data, size_t len) {
    int i = find(name);
    if (i < 0) return -1;
    if (len == 0) {
        if (files[i].data) kfree(files[i].data);
        files[i].data = 0;
        files[i].size = 0;
        return 0;
    }

    char *buf = (char *)kmalloc(len);
    if (!buf) return -1;
    memcpy(buf, data, len);

    if (files[i].data) kfree(files[i].data);
    files[i].data = buf;
    files[i].size = len;
    return (int)len;
}

int ramfs_append(const char *name, const void *data, size_t len) {
    int i = find(name);
    if (i < 0) return -1;
    if (len == 0) return 0;

    char *buf = (char *)kmalloc(files[i].size + len);
    if (!buf) return -1;

    if (files[i].size > 0 && files[i].data) {
        memcpy(buf, files[i].data, files[i].size);
    }
    memcpy(buf + files[i].size, data, len);

    if (files[i].data) kfree(files[i].data);
    files[i].data = buf;
    files[i].size += len;
    return (int)files[i].size;
}

int ramfs_read(const char *name, void *out, size_t max) {
    int i = find(name);
    if (i < 0) return -1;

    size_t n = files[i].size < max ? files[i].size : max;
    if (n > 0) memcpy(out, files[i].data, n);
    return (int)n;
}

int ramfs_size(const char *name) {
    int i = find(name);
    if (i < 0) return -1;
    return (int)files[i].size;
}

int ramfs_remove(const char *name) {
    int i = find(name);
    if (i < 0) return -1;
    if (files[i].data) kfree(files[i].data);
    files[i].used = 0;
    files[i].data = 0;
    files[i].size = 0;
    return 0;
}

int ramfs_exists(const char *name) {
    return find(name) >= 0;
}

int ramfs_count(void) {
    int n = 0;
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used) n++;
    }
    return n;
}

ramfs_file_t *ramfs_get(int index) {
    int seen = 0;
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used) {
            if (seen == index) return &files[i];
            seen++;
        }
    }
    return 0;
}
