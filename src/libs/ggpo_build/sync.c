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

#include "sync.h"
#include "network/udp_msg.h"

static int _sync_FindSavedFrameIndex(Sync* sync, int frame);
static bool _sync_CreateQueues(Sync* sync, sync_Config* config);
static bool _sync_CheckSimulationConsistency(Sync* sync, int* seekTo);
static void _sync_ResetPrediction(Sync* sync, int frameNumber);

void sync_ctor(Sync* sync, UdpMsg_connect_status* connect_status)
{
    memset(sync, 0, sizeof(Sync));
	sync->_local_connect_status = connect_status;
	sync->_input_queues = NULL;

   sync->_framecount = 0;
   sync->_last_confirmed_frame = -1;
   sync->_max_prediction_frames = 0;
   memset(&sync->_savedstate, 0, sizeof(sync->_savedstate));

   ring_ctor(&sync->_event_queue_ring, ARRAY_SIZE(sync->_event_queue));
}

void sync_dtor(Sync* sync)
{
   /*
    * Delete frames manually here rather than in a destructor of the SavedFrame
    * structure so we can efficently copy frames via weak references.
    */
   for (int i = 0; i < ARRAY_SIZE(sync->_savedstate.frames); i++) {
      sync->_callbacks.free_buffer(sync->_savedstate.frames[i].buf);
   }
   free(sync->_input_queues);
   sync->_input_queues = NULL;
}

void sync_Init(Sync* sync, sync_Config* config)
{
   sync->_config = *config;
   sync->_callbacks = config->callbacks;
   sync->_framecount = 0;
   sync->_rollingback = false;

   sync->_max_prediction_frames = config->num_prediction_frames;

   _sync_CreateQueues(sync, config);
}

void sync_SetLastConfirmedFrame(Sync* sync, int frame)
{
   sync->_last_confirmed_frame = frame;
   if (sync->_last_confirmed_frame > 0) {
      for (int i = 0; i < sync->_config.num_players; i++) {
         input_queue_DiscardConfirmedFrames(&sync->_input_queues[i], frame - 1);
      }
   }
}

void sync_SetFrameDelay(Sync* sync, int queue, int delay)
{
        input_queue_SetFrameDelay(&sync->_input_queues[queue], delay);
}

bool sync_AddLocalInput(Sync* sync, int queue, GameInput* input)
{
   int frames_behind = sync->_framecount - sync->_last_confirmed_frame;
   if (sync->_framecount >= sync->_max_prediction_frames && frames_behind >= sync->_max_prediction_frames) {
      Log("Rejecting input from emulator: reached prediction barrier.\n");
      return false;
   }

   if (sync->_framecount == 0) {
      sync_SaveCurrentFrame(sync);
   }

   Log("Sending undelayed local frame %d to queue %d.\n", sync->_framecount, queue);
   input->frame = sync->_framecount;
   input_queue_AddInput(&sync->_input_queues[queue], input);

   return true;
}

void sync_AddRemoteInput(Sync* sync, int queue, GameInput* input)
{
    input_queue_AddInput(&sync->_input_queues[queue], input);
}

int sync_GetConfirmedInputs(Sync* sync, void* values, int size, int frame)
{
   int disconnect_flags = 0;
   char *output = (char *)values;

   ASSERT(size >= sync->_config.num_players * sync->_config.input_size);

   memset(output, 0, size);
   for (int i = 0; i < sync->_config.num_players; i++) {
      GameInput input;
      if (sync->_local_connect_status[i].disconnected && frame > sync->_local_connect_status[i].last_frame) {
         disconnect_flags |= (1 << i);
         gameinput_erase(&input);
      } else {
         input_queue_GetConfirmedInput(&sync->_input_queues[i], frame, &input);
      }
      memcpy(output + (i * sync->_config.input_size), input.bits, sync->_config.input_size);
   }
   return disconnect_flags;
}

int sync_SynchronizeInputs(Sync* sync, void* values, int size)
{
   int disconnect_flags = 0;
   char *output = (char *)values;

   ASSERT(size >= sync->_config.num_players * sync->_config.input_size);

   memset(output, 0, size);
   for (int i = 0; i < sync->_config.num_players; i++) {
      GameInput input;
      if (sync->_local_connect_status[i].disconnected && sync->_framecount > sync->_local_connect_status[i].last_frame) {
         disconnect_flags |= (1 << i);
         gameinput_erase(&input);
      } else {
         input_queue_GetInput(&sync->_input_queues[i], sync->_framecount, &input);
      }
      memcpy(output + (i * sync->_config.input_size), input.bits, sync->_config.input_size);
   }
   return disconnect_flags;
}

void sync_CheckSimulation(Sync* sync, int timeout)
{
   int seek_to;
   if (!_sync_CheckSimulationConsistency(sync, &seek_to)) {
      sync_AdjustSimulation(sync, seek_to);
   }
}

void sync_AdjustSimulation(Sync* sync, int seek_to)
{
   int framecount = sync->_framecount;
   int count = sync->_framecount - seek_to;

   Log("Catching up\n");
   sync->_rollingback = true;

   /*
    * Flush our input queue and load the last frame.
    */
   sync_LoadFrame(sync, seek_to);
   ASSERT(sync->_framecount == seek_to);

   /*
    * Advance frame by frame (stuffing notifications back to
    * the master).
    */
   _sync_ResetPrediction(sync, sync->_framecount);
   for (int i = 0; i < count; i++) {
      sync->_callbacks.advance_frame(0);
   }
   ASSERT(sync->_framecount == framecount);

   sync->_rollingback = false;

   Log("---\n");
}

void sync_IncrementFrame(Sync* sync)
{
        sync->_framecount++;
        sync_SaveCurrentFrame(sync);
}

bool sync_GetEvent(Sync* sync, sync_Event* e)
{
        if (ring_size(&sync->_event_queue_ring)) {
                *e = sync->_event_queue[ring_front(&sync->_event_queue_ring)];
                ring_pop(&sync->_event_queue_ring);
                return true;
        }
        return false;
}

sync_SavedFrame* sync_GetLastSavedFrame(Sync *sync)
{
        int i = sync->_savedstate.head - 1;
        if (i < 0) {
                i = ARRAY_SIZE(sync->_savedstate.frames) - 1;
        }
        return &sync->_savedstate.frames[i];
}

void sync_SaveCurrentFrame(Sync* sync)
{
        /*
         * See StateCompress for the real save feature implemented by FinalBurn.
         * Write everything into the head, then advance the head pointer.
         */
        sync_SavedFrame* state = sync->_savedstate.frames + sync->_savedstate.head;
        if (state->buf) {
                sync->_callbacks.free_buffer(state->buf);
                state->buf = NULL;
        }
        state->frame = sync->_framecount;
        sync->_callbacks.save_game_state(&state->buf, &state->cbuf, &state->checksum, state->frame);

        Log("=== Saved frame info %d (size: %d  checksum: %08x).\n", state->frame, state->cbuf, state->checksum);
        sync->_savedstate.head = (sync->_savedstate.head + 1) % ARRAY_SIZE(sync->_savedstate.frames);
}

void sync_LoadFrame(Sync* sync, int frame)
{
   // find the frame in question
   if (frame == sync->_framecount) {
      Log("Skipping NOP.\n");
      return;
   }

   // Move the head pointer back and load it up
   sync->_savedstate.head = _sync_FindSavedFrameIndex(sync, frame);
   sync_SavedFrame *state = sync->_savedstate.frames + sync->_savedstate.head;

   Log("=== Loading frame info %d (size: %d  checksum: %08x).\n",
       state->frame, state->cbuf, state->checksum);

   ASSERT(state->buf && state->cbuf);
   sync->_callbacks.load_game_state(state->buf, state->cbuf);

   // Reset framecount and the head of the state ring-buffer to point in
   // advance of the current frame (as if we had just finished executing it).
   sync->_framecount = state->frame;
   sync->_savedstate.head = (sync->_savedstate.head + 1) % ARRAY_SIZE(sync->_savedstate.frames);
}

static int _sync_FindSavedFrameIndex(Sync* sync, int frame)
{
   int i, count = ARRAY_SIZE(sync->_savedstate.frames);
   for (i = 0; i < count; i++) {
      if (sync->_savedstate.frames[i].frame == frame) {
         break;
      }
   }
   if (i == count) {
      ASSERT(false);
   }
   return i;
}

static bool _sync_CreateQueues(Sync* sync, sync_Config *config)
{
   free(sync->_input_queues);
   sync->_input_queues = calloc(sync->_config.num_players, sizeof(InputQueue));
   for (int i = 0; i < sync->_config.num_players; i++) {
      input_queue_Init(&sync->_input_queues[i], i, sync->_config.input_size);
   }
   return true;
}

static bool _sync_CheckSimulationConsistency(Sync* sync, int *seekTo)
{
   int first_incorrect = GAMEINPUT_NULL_FRAME;
   for (int i = 0; i < sync->_config.num_players; i++) {
      int incorrect = input_queue_GetFirstIncorrectFrame(&sync->_input_queues[i]);
      Log("considering incorrect frame %d reported by queue %d.\n", incorrect, i);

      if (incorrect != GAMEINPUT_NULL_FRAME && (first_incorrect == GAMEINPUT_NULL_FRAME || incorrect < first_incorrect)) {
         first_incorrect = incorrect;
      }
   }

   if (first_incorrect == GAMEINPUT_NULL_FRAME) {
      Log("prediction ok.  proceeding.\n");
      return true;
   }
   *seekTo = first_incorrect;
   return false;
}


static void _sync_ResetPrediction(Sync* sync, int frameNumber)
{
   for (int i = 0; i < sync->_config.num_players; i++) {
      input_queue_ResetPrediction(&sync->_input_queues[i], frameNumber);
   }
}
