#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t len;
    uint8_t *data;
} blob_t;

typedef union {
    int32_t i;
    char T;
    float f;
    blob_t b;
} arg_t;

size_t sosc(char   *buffer,
        size_t      len,
        const char *address,
        const char *arguments,
        ...);

size_t vsosc(char   *buffer,
        size_t      len,
        const char *address,
        const char *arguments,
        va_list va);

unsigned nargs(const char *msg);
char type(const char *msg, unsigned i);

arg_t argument(const char *msg, unsigned i);

//Finds the offset of a given argument or 0 for invalid requests
unsigned arg_off(const char *msg, unsigned i);

#ifdef __cplusplus
};
#endif
