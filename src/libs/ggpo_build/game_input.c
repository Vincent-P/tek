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

#include "types.h"
#include "game_input.h"
#include "log.h"


void gameinput_init(GameInput* input, int iframe, char* ibits, int isize)
{
	ASSERT(isize);
	ASSERT(isize <= GAMEINPUT_MAX_BYTES * GAMEINPUT_MAX_PLAYERS);
	input->frame = iframe;
	input->size = isize;
	memset(input->bits, 0, sizeof(input->bits));
	if (ibits) {
		memcpy(input->bits, ibits, isize);
	}
}

void gameinput_desc(GameInput const* input, char* buf, size_t buf_size, bool show_frame)
{
	ASSERT(input->size);
	size_t remaining = buf_size;
	if (show_frame) {
		remaining -= snprintf(buf, buf_size, "(frame:%d size:%d ", input->frame, input->size);
	}
	else {
		remaining -= snprintf(buf, buf_size, "(size:%d ", input->size);
	}

	for (int i = 0; i < input->size * 8; i++) {
		char buf2[16];
		if (gameinput_value(input, i)) {
			int c = snprintf(buf2, ARRAY_SIZE(buf2), "%2d ", i);
			strncat_s(buf, remaining, buf2, ARRAY_SIZE(buf2));
			remaining -= c;
		}
	}
	strncat_s(buf, remaining, ")", 1);
}

void gameinput_log(GameInput const* input, char* prefix, bool show_frame)
{
	char buf[1024];
	size_t c = strlen(prefix);
	strncpy(buf, prefix, c);
	gameinput_desc(input, buf + c, ARRAY_SIZE(buf) - c, show_frame);
	strncat_s(buf, ARRAY_SIZE(buf) - strlen(buf), "\n", 1);
	Log(buf);
}

bool gameinput_equal(GameInput const* input, GameInput const* other, bool bitsonly)
{
	if (!bitsonly && input->frame != other->frame) {
		Log("frames don't match: %d, %d\n", input->frame, other->frame);
	}
	if (input->size != other->size) {
		Log("sizes don't match: %d, %d\n", input->size, other->size);
	}
	if (memcmp(input->bits, other->bits, input->size)) {
		Log("bits don't match\n");
	}
	ASSERT(input->size && other->size);
	return (bitsonly || input->frame == other->frame) &&
		input->size == other->size &&
		memcmp(input->bits, other->bits, input->size) == 0;
}
