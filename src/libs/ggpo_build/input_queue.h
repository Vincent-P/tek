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

#ifndef _INPUT_QUEUE_H
#define _INPUT_QUEUE_H

#include "game_input.h"

#define INPUT_QUEUE_LENGTH    128
#define DEFAULT_INPUT_SIZE      4

struct InputQueue
{
	int                  _id;
	int                  _head;
	int                  _tail;
	int                  _length;
	bool                 _first_frame;

	int                  _last_user_added_frame;
	int                  _last_added_frame;
	int                  _first_incorrect_frame;
	int                  _last_frame_requested;

	int                  _frame_delay;

	GameInput            _inputs[INPUT_QUEUE_LENGTH];
	GameInput            _prediction;
};
typedef struct InputQueue InputQueue;

void input_queue_Init(InputQueue* queue, int id, int input_size);
int input_queue_GetLastConfirmedFrame(InputQueue* queue);
int input_queue_GetFirstIncorrectFrame(InputQueue* queue);
inline int input_queue_GetLength(InputQueue* queue) { return queue->_length; }
inline void input_queue_SetFrameDelay(InputQueue* queue, int delay) { queue->_frame_delay = delay; }
void input_queue_ResetPrediction(InputQueue* queue, int frame);
void input_queue_DiscardConfirmedFrames(InputQueue* queue, int frame);
bool input_queue_GetConfirmedInput(InputQueue* queue, int frame, GameInput* input);
bool input_queue_GetInput(InputQueue* queue, int frame, GameInput* input);
void input_queue_AddInput(InputQueue* queue, GameInput* input);
int input_queue_AdvanceQueueHead(InputQueue* queue, int frame);
void input_queue_AddDelayedInputToQueue(InputQueue* queue, GameInput* input, int i);
void input_queue_Log(InputQueue* queue, const char* fmt, ...);

#endif



