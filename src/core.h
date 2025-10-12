#pragma once

//-----------------------------------------------------------------------------
// [SECTION] Maths
//-----------------------------------------------------------------------------

bool is_pow2(uint32_t x)
{
    return (x & (x-1)) == 0;
}

uint32_t next_pow2(uint32_t v)
{
    v--;
    v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

//-----------------------------------------------------------------------------
// [SECTION] Memory
//-----------------------------------------------------------------------------
typedef struct Arena {
    char *beg;
    char *end;
} Arena;

#define arena_new(a, t, n)  (t *)arena_alloc(a, sizeof(t), _Alignof(t), n)

void *arena_alloc(Arena *a, ptrdiff_t size, ptrdiff_t align, ptrdiff_t count)
{
    ptrdiff_t padding = -(uintptr_t)a->beg & (align - 1);
    ptrdiff_t available = a->end - a->beg - padding;
    if (available < 0 || count > available/size) {
        abort();  // one possible out-of-memory policy
    }
    void *p = a->beg + padding;
    a->beg += padding + count*size;
    return memset(p, 0, count*size);
}
