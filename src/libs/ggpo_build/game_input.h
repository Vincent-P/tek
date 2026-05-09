/**
 * Copyright (C) 2025 Vincent Parizet
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <https://www.gnu.org/licenses/>.
**/
/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _GAMEINPUT_H
#define _GAMEINPUT_H

#include <stdio.h>
#include <memory.h>

 // GAMEINPUT_MAX_BYTES * GAMEINPUT_MAX_PLAYERS * 8 must be less than
 // 2^BITVECTOR_NIBBLE_SIZE (see bitvector.h)

#define GAMEINPUT_MAX_BYTES      9
#define GAMEINPUT_MAX_PLAYERS    2
#define GAMEINPUT_NULL_FRAME -1

struct GameInput 
{
	int      frame;
	int      size; /* size in bytes of the entire input for all players */
	char     bits[GAMEINPUT_MAX_BYTES * GAMEINPUT_MAX_PLAYERS];
};

typedef struct GameInput GameInput;

void gameinput_init(GameInput* input, int frame, char* bits, int size);
inline bool gameinput_value(GameInput const* input, int i) { return (input->bits[i / 8] & (1 << (i % 8))) != 0; }
inline void gameinput_set(GameInput* input, int i) { input->bits[i / 8] |= (1 << (i % 8)); }
inline void gameinput_clear(GameInput* input, int i) { input->bits[i / 8] &= ~(1 << (i % 8)); }
inline void gameinput_erase(GameInput* input) { memset(input->bits, 0, sizeof(input->bits)); }
void gameinput_desc(GameInput const* input, char* buf, size_t buf_size, bool show_frame/*= true*/);
void gameinput_log(GameInput const* input, char* prefix, bool show_frame/* = true */);
bool gameinput_equal(GameInput const* a, GameInput const* b, bool bitsonly/* = false*/);

#endif
