/*
 * Chocolate Duke3D retains a few original Build renderer paths that store
 * pointers in 32-bit integers. Keep game allocations in the executable's
 * low-address BSS while leaving Newlib free to manage its own heap.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define DUKE_ARENA_SIZE (24U * 1024U * 1024U)
#define ALIGNMENT 16U

typedef union block_header block_header;
union block_header {
    struct {
        size_t size;
        block_header *next;
        int free;
    } value;
    long double alignment;
};

static unsigned char arena[DUKE_ARENA_SIZE]
    __attribute__((aligned(ALIGNMENT)));
static block_header *first_block;

static size_t align_size(size_t size)
{
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

static void initialize_arena(void)
{
    if (first_block != NULL)
        return;
    first_block = (block_header *)arena;
    first_block->value.size = sizeof(arena) - sizeof(block_header);
    first_block->value.next = NULL;
    first_block->value.free = 1;
}

void *duke_malloc(size_t size)
{
    block_header *block;
    size_t wanted;

    if (size == 0)
        return NULL;
    if (size > SIZE_MAX - ALIGNMENT)
        return NULL;
    wanted = align_size(size);
    initialize_arena();

    for (block = first_block; block != NULL; block = block->value.next) {
        if (!block->value.free || block->value.size < wanted)
            continue;
        if (block->value.size >= wanted + sizeof(block_header) + ALIGNMENT) {
            block_header *remainder = (block_header *)
                ((unsigned char *)(block + 1) + wanted);
            remainder->value.size = block->value.size - wanted -
                                    sizeof(block_header);
            remainder->value.next = block->value.next;
            remainder->value.free = 1;
            block->value.next = remainder;
            block->value.size = wanted;
        }
        block->value.free = 0;
        return block + 1;
    }
    return NULL;
}

void duke_free(void *pointer)
{
    block_header *block;

    if (pointer == NULL)
        return;
    if ((unsigned char *)pointer < arena + sizeof(block_header) ||
        (unsigned char *)pointer >= arena + sizeof(arena))
        return;
    block = (block_header *)pointer - 1;
    block->value.free = 1;

    for (block = first_block; block != NULL && block->value.next != NULL; ) {
        if (block->value.free && block->value.next->value.free) {
            block->value.size += sizeof(block_header) +
                                 block->value.next->value.size;
            block->value.next = block->value.next->value.next;
        } else {
            block = block->value.next;
        }
    }
}

void *duke_realloc(void *pointer, size_t size)
{
    block_header *block;
    void *replacement;
    size_t copy_size;

    if (pointer == NULL)
        return duke_malloc(size);
    if (size == 0) {
        duke_free(pointer);
        return NULL;
    }
    block = (block_header *)pointer - 1;
    if (block->value.size >= size)
        return pointer;
    replacement = duke_malloc(size);
    if (replacement == NULL)
        return NULL;
    copy_size = block->value.size < size ? block->value.size : size;
    memcpy(replacement, pointer, copy_size);
    duke_free(pointer);
    return replacement;
}

void *duke_calloc(size_t count, size_t size)
{
    void *pointer;
    size_t total;

    if (size != 0 && count > SIZE_MAX / size)
        return NULL;
    total = count * size;
    pointer = duke_malloc(total);
    if (pointer != NULL)
        memset(pointer, 0, total);
    return pointer;
}
