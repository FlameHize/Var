#ifndef UTIL_MACROS_H
#define UTIL_MACROS_H

#define DISALLOW_COPY_MOVE_AND_ASSIGN(Typename) \
    Typename(const Typename&) = delete;         \
    Typename(Typename&&) = delete;              \
    void operator=(const Typename&) = delete;   \
    void operator=(Typename&&) = delete

#endif