#include <stdint.h>
#include <stddef.h>
#include <sys/uio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int32_t magic;
    int32_t version;
    // mah: see [1],[4]: shouldnt head+tail be volatile?
    size_t size;

    /* volatile */ size_t head __attribute__((aligned(16)));
    /* volatile */ uint64_t generation;

    /* volatile */ size_t tail __attribute__((aligned(16)));;
} ring_header_t;

#define _S(x, s) (((int32_t) (x)) << 8 * (s))
static const int32_t ring_magic = _S('r', 3) | _S('i', 2) | _S('n', 1) | 'g';
#undef _S

typedef int32_t ring_size_t; // Negative numbers are needed for skips

typedef struct
{
    ring_header_t * header;
    char * buf;
} ringbuffer_t;

typedef struct
{
    const ringbuffer_t *ring;
    uint64_t generation;
    size_t offset;
} ringiter_t;


int ring_init(ringbuffer_t *ring, size_t size, void * memory);
int ring_init_file(ringbuffer_t *ring, size_t size, int fd);

int ring_write_start(ringbuffer_t *ring, void ** data, size_t size);
int ring_write_end(ringbuffer_t *ring, void * data, size_t size);
int ring_write(ringbuffer_t *ring, const void * data, size_t size);

int ring_read(const ringbuffer_t *ring, const void **data, size_t *size);
void ring_shift(ringbuffer_t *ring);

size_t ring_available(const ringbuffer_t *ring);


// Wrappers, not needed
const void * ring_next(ringbuffer_t *ring);
ring_size_t ring_next_size(ringbuffer_t *ring);

// Bad design :(
struct iovec ring_next_iovec(ringbuffer_t *ring);

int ring_iter_init(const ringbuffer_t *ring, ringiter_t *iter);
int ring_iter_shift(ringiter_t *iter);
int ring_iter_read(const ringiter_t *iter, const void **data, size_t *size);

void ring_dump(ringbuffer_t *ring, const char *name);

#ifdef __cplusplus
}; //extern "C"
#endif
