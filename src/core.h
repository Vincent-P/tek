#pragma once

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
