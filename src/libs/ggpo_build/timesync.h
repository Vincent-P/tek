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

#ifndef _TIMESYNC_H
#define _TIMESYNC_H

#include "types.h"
#include "game_input.h"

#define FRAME_WINDOW_SIZE           40
#define MIN_UNIQUE_FRAMES           10
#define MIN_FRAME_ADVANTAGE          3
#define MAX_FRAME_ADVANTAGE          9


struct TimeSync
{
	int         _local[FRAME_WINDOW_SIZE];
	int         _remote[FRAME_WINDOW_SIZE];
	GameInput   _last_inputs[MIN_UNIQUE_FRAMES];
	int         _next_prediction;
};
typedef struct TimeSync TimeSync;

void timesync_init(TimeSync* timesync);
void timesync_advance_frame(TimeSync* timesync, GameInput* input, int advantage, int radvantage);
int timesync_recommend_frame_wait_duration(TimeSync const* timesync, bool require_idle_input);

#endif
