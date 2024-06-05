#ifndef UTIL_MACROS_H
#define UTIL_MACROS_H

#define DISALLOW_COPY_MOVE_AND_ASSIGN(Typename) \
    Typename(const Typename&) = delete;         \
    Typename(Typename&&) = delete;              \
    void operator=(const Typename&) = delete;   \
    void operator=(Typename&&) = delete
                
// The impl. of chrome does not work for offsetof(Object, private_filed)
#define COMPILE_ASSERT(expr, msg)  static_assert(expr, #msg)

#endif