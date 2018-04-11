// from https://gist.github.com/nicky-zs/7541169
// see also https://stackoverflow.com/questions/4032373/linking-against-an-old-version-of-libc-to-provide-greater-application-coverage
// see also https://stackoverflow.com/questions/8823267/linking-against-older-symbol-version-in-a-so-file

#include <string.h>

void *__memcpy_glibc_2_2_5(void *, const void *, size_t);

asm(".symver __memcpy_glibc_2_2_5, memcpy@GLIBC_2.2.5");

void *__wrap_memcpy(void *dest, const void *src, size_t n)
{
    return __memcpy_glibc_2_2_5(dest, src, n);
}
