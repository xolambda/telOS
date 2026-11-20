#ifndef PANIC_H
#define PANIC_H

void panic(const char *msg);
void panic_at(const char *msg, const char *file, int line);

#define ASSERT(cond) \
    do { \
        if (!(cond)) panic_at("assertion failed: " #cond, __FILE__, __LINE__); \
    } while (0)

#endif
