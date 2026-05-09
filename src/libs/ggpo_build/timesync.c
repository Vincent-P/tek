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

#include "timesync.h"

void timesync_init(TimeSync* timesync)
{
	memset(timesync->_local, 0, sizeof(timesync->_local));
	memset(timesync->_remote, 0, sizeof(timesync->_remote));
	timesync->_next_prediction = FRAME_WINDOW_SIZE * 3;
}


void timesync_advance_frame(TimeSync* timesync, GameInput* input, int advantage, int radvantage)
{

	// Remember the last frame and frame advantage
	timesync->_last_inputs[input->frame % ARRAY_SIZE(timesync->_last_inputs)] = *input;
	timesync->_local[input->frame % ARRAY_SIZE(timesync->_local)] = advantage;
	timesync->_remote[input->frame % ARRAY_SIZE(timesync->_remote)] = radvantage;

}

int timesync_recommend_frame_wait_duration(TimeSync const* timesync, bool require_idle_input)
{
	// Average our local and remote frame advantages
	int i, sum = 0;
	float advantage, radvantage;
	for (i = 0; i < ARRAY_SIZE(timesync->_local); i++) {
		sum += timesync->_local[i];
	}
	advantage = sum / (float)ARRAY_SIZE(timesync->_local);

	sum = 0;
	for (i = 0; i < ARRAY_SIZE(timesync->_remote); i++) {
		sum += timesync->_remote[i];
	}
	radvantage = sum / (float)ARRAY_SIZE(timesync->_remote);

	static int count = 0;
	count++;

	// See if someone should take action.  The person furthest ahead
	// needs to slow down so the other user can catch up.
	// Only do this if both clients agree on who's ahead!!
	if (advantage >= radvantage) {
		return 0;
	}

	// Both clients agree that we're the one ahead.  Split
	// the difference between the two to figure out how long to
	// sleep for.
	int sleep_frames = (int)(((radvantage - advantage) / 2) + 0.5);

	Log("iteration %d:  sleep frames is %d\n", count, sleep_frames);

	// Some things just aren't worth correcting for.  Make sure
	// the difference is relevant before proceeding.
	if (sleep_frames < MIN_FRAME_ADVANTAGE) {
		return 0;
	}

	// Make sure our input had been "idle enough" before recommending
	// a sleep.  This tries to make the emulator sleep while the
	// user's input isn't sweeping in arcs (e.g. fireball motions in
	// Street Fighter), which could cause the player to miss moves.
	if (require_idle_input) {
		for (i = 1; i < ARRAY_SIZE(timesync->_last_inputs); i++) {
			if (!gameinput_equal(&timesync->_last_inputs[i], &timesync->_last_inputs[0], true)) {
				Log("iteration %d:  rejecting due to input stuff at position %d...!!!\n", count, i);
				return 0;
			}
		}
	}

	// Success!!! Recommend the number of frames to sleep and adjust
	return MIN(sleep_frames, MAX_FRAME_ADVANTAGE);

}
