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
#include "input_queue.h"

#define PREVIOUS_FRAME(offset)   (((offset) == 0) ? (INPUT_QUEUE_LENGTH - 1) : ((offset) - 1))

void
input_queue_Init(InputQueue* queue, int id, int input_size)
{
   queue->_id = id;
   queue->_head = 0;
   queue->_tail = 0;
   queue->_length = 0;
   queue->_frame_delay = 0;
   queue->_first_frame = true;
   queue->_last_user_added_frame = GAMEINPUT_NULL_FRAME;
   queue->_first_incorrect_frame = GAMEINPUT_NULL_FRAME;
   queue->_last_frame_requested = GAMEINPUT_NULL_FRAME;
   queue->_last_added_frame = GAMEINPUT_NULL_FRAME;

   gameinput_init(&queue->_prediction, GAMEINPUT_NULL_FRAME, NULL, input_size);

   /*
    * This is safe because we know the GameInput is a proper structure (as in,
    * no virtual methods, no contained classes, etc.).
    */
   memset(queue->_inputs, 0, sizeof(queue->_inputs));
   for (int i = 0; i < ARRAY_SIZE(queue->_inputs); i++) {
      queue->_inputs[i].size = input_size;
   }
}

int
input_queue_GetLastConfirmedFrame(InputQueue* queue)
{
   Log("returning last confirmed frame %d.\n", queue->_last_added_frame);
   return queue->_last_added_frame;
}

int
input_queue_GetFirstIncorrectFrame(InputQueue* queue)
{
   return queue->_first_incorrect_frame;
}

void
input_queue_DiscardConfirmedFrames(InputQueue* queue, int frame)
{
   ASSERT(frame >= 0);

   if (queue->_last_frame_requested != GAMEINPUT_NULL_FRAME) {
      frame = MIN(frame, queue->_last_frame_requested);
   }

   Log("discarding confirmed frames up to %d (last_added:%d length:%d [head:%d tail:%d]).\n",
       frame, queue->_last_added_frame, queue->_length, queue->_head, queue->_tail);
   if (frame >= queue->_last_added_frame) {
      queue->_tail = queue->_head;
   } else {
      int offset = frame - queue->_inputs[queue->_tail].frame + 1;

      Log("difference of %d frames.\n", offset);
      ASSERT(offset >= 0);

      queue->_tail = (queue->_tail + offset) % INPUT_QUEUE_LENGTH;
      queue->_length -= offset;
   }

   Log("after discarding, new tail is %d (frame:%d).\n", queue->_tail, queue->_inputs[queue->_tail].frame);
   ASSERT(queue->_length >= 0);
}

void
input_queue_ResetPrediction(InputQueue* queue, int frame)
{
   ASSERT(queue->_first_incorrect_frame == GAMEINPUT_NULL_FRAME || frame <= queue->_first_incorrect_frame);

   Log("resetting all prediction errors back to frame %d.\n", frame);

   /*
    * There's nothing really to do other than reset our prediction
    * state and the incorrect frame counter...
    */
   queue->_prediction.frame = GAMEINPUT_NULL_FRAME;
   queue->_first_incorrect_frame = GAMEINPUT_NULL_FRAME;
   queue->_last_frame_requested = GAMEINPUT_NULL_FRAME;
}

bool
input_queue_GetConfirmedInput(InputQueue* queue, int requested_frame, GameInput *input)
{
   ASSERT(queue->_first_incorrect_frame == GAMEINPUT_NULL_FRAME || requested_frame < queue->_first_incorrect_frame);
   int offset = requested_frame % INPUT_QUEUE_LENGTH;
   if (queue->_inputs[offset].frame != requested_frame) {
      return false;
   }
   *input = queue->_inputs[offset];
   return true;
}

bool
input_queue_GetInput(InputQueue* queue, int requested_frame, GameInput *input)
{
   Log("requesting input frame %d.\n", requested_frame);

   /*
    * No one should ever try to grab any input when we have a prediction
    * error.  Doing so means that we're just going further down the wrong
    * path.  ASSERT this to verify that it's true.
    */
   ASSERT(queue->_first_incorrect_frame == GAMEINPUT_NULL_FRAME);

   /*
    * Remember the last requested frame number for later.  We'll need
    * this in AddInput() to drop out of prediction mode.
    */
   queue->_last_frame_requested = requested_frame;

   ASSERT(requested_frame >= queue->_inputs[queue->_tail].frame);

   if (queue->_prediction.frame == GAMEINPUT_NULL_FRAME) {
      /*
       * If the frame requested is in our range, fetch it out of the queue and
       * return it.
       */
      int offset = requested_frame - queue->_inputs[queue->_tail].frame;

      if (offset < queue->_length) {
         offset = (offset + queue->_tail) % INPUT_QUEUE_LENGTH;
         ASSERT(queue->_inputs[offset].frame == requested_frame);
         *input = queue->_inputs[offset];
         Log("returning confirmed frame number %d.\n", input->frame);
         return true;
      }

      /*
       * The requested frame isn't in the queue.  Bummer.  This means we need
       * to return a prediction frame.  Predict that the user will do the
       * same thing they did last time.
       */
      if (requested_frame == 0) {
         Log("basing new prediction frame from nothing, you're client wants frame 0.\n");
         gameinput_erase(&queue->_prediction);
      } else if (queue->_last_added_frame == GAMEINPUT_NULL_FRAME) {
         Log("basing new prediction frame from nothing, since we have no frames yet.\n");
         gameinput_erase(&queue->_prediction);
      } else {
         Log("basing new prediction frame from previously added frame (queue entry:%d, frame:%d).\n",
              PREVIOUS_FRAME(queue->_head), queue->_inputs[PREVIOUS_FRAME(queue->_head)].frame);
         queue->_prediction = queue->_inputs[PREVIOUS_FRAME(queue->_head)];
      }
      queue->_prediction.frame++;
   }

   ASSERT(queue->_prediction.frame >= 0);

   /*
    * If we've made it this far, we must be predicting.  Go ahead and
    * forward the prediction frame contents.  Be sure to return the
    * frame number requested by the client, though.
    */
   *input = queue->_prediction;
   input->frame = requested_frame;
   Log("returning prediction frame number %d (%d).\n", input->frame, queue->_prediction.frame);

   return false;
}

void input_queue_AddInput(InputQueue* queue, GameInput *input)
{
   int new_frame;

   Log("adding input frame number %d to queue.\n", input->frame);

   /*
    * These next two lines simply verify that inputs are passed in
    * sequentially by the user, regardless of frame delay.
    */
   ASSERT(queue->_last_user_added_frame == GAMEINPUT_NULL_FRAME ||
          input->frame == queue->_last_user_added_frame + 1);
   queue->_last_user_added_frame = input->frame;

   /*
    * Move the queue head to the correct point in preparation to
    * input the frame into the queue.
    */
   new_frame = input_queue_AdvanceQueueHead(queue, input->frame);
   if (new_frame != GAMEINPUT_NULL_FRAME) {
      input_queue_AddDelayedInputToQueue(queue, input, new_frame);
   }

   /*
    * Update the frame number for the input.  This will also set the
    * frame to GAMEINPUT_NULL_FRAME for frames that get dropped (by
    * design).
    */
   input->frame = new_frame;
}

void
input_queue_AddDelayedInputToQueue(InputQueue* queue, GameInput *input, int frame_number)
{
   Log("adding delayed input frame number %d to queue.\n", frame_number);

   ASSERT(input->size == queue->_prediction.size);

   ASSERT(queue->_last_added_frame == GAMEINPUT_NULL_FRAME || frame_number == queue->_last_added_frame + 1);

   ASSERT(frame_number == 0 || queue->_inputs[PREVIOUS_FRAME(queue->_head)].frame == frame_number - 1);

   /*
    * Add the frame to the back of the queue
    */
   queue->_inputs[queue->_head] = *input;
   queue->_inputs[queue->_head].frame = frame_number;
   queue->_head = (queue->_head + 1) % INPUT_QUEUE_LENGTH;
   queue->_length++;
   queue->_first_frame = false;

   queue->_last_added_frame = frame_number;

   if (queue->_prediction.frame != GAMEINPUT_NULL_FRAME) {
      ASSERT(frame_number == queue->_prediction.frame);

      /*
       * We've been predicting...  See if the inputs we've gotten match
       * what we've been predicting.  If so, don't worry about it.  If not,
       * remember the first input which was incorrect so we can report it
       * in GetFirstIncorrectFrame()
       */
      if (queue->_first_incorrect_frame == GAMEINPUT_NULL_FRAME && !gameinput_equal(&queue->_prediction, input, true)) {
         Log("frame %d does not match prediction.  marking error.\n", frame_number);
         queue->_first_incorrect_frame = frame_number;
      }

      /*
       * If this input is the same frame as the last one requested and we
       * still haven't found any mis-predicted inputs, we can dump out
       * of predition mode entirely!  Otherwise, advance the prediction frame
       * count up.
       */
      if (queue->_prediction.frame == queue->_last_frame_requested && queue->_first_incorrect_frame == GAMEINPUT_NULL_FRAME) {
         Log("prediction is correct!  dumping out of prediction mode.\n");
         queue->_prediction.frame = GAMEINPUT_NULL_FRAME;
      } else {
              queue->_prediction.frame++;
      }
   }
   ASSERT(queue->_length <= INPUT_QUEUE_LENGTH);
}

int
input_queue_AdvanceQueueHead(InputQueue* queue, int frame)
{
   Log("advancing queue head to frame %d.\n", frame);

   int expected_frame = queue->_first_frame ? 0 : queue->_inputs[PREVIOUS_FRAME(queue->_head)].frame + 1;

   frame += queue->_frame_delay;

   if (expected_frame > frame) {
      /*
       * This can occur when the frame delay has dropped since the last
       * time we shoved a frame into the system.  In this case, there's
       * no room on the queue.  Toss it.
       */
      Log("Dropping input frame %d (expected next frame to be %d).\n",
          frame, expected_frame);
      return GAMEINPUT_NULL_FRAME;
   }

   while (expected_frame < frame) {
      /*
       * This can occur when the frame delay has been increased since the last
       * time we shoved a frame into the system.  We need to replicate the
       * last frame in the queue several times in order to fill the space
       * left.
       */
      Log("Adding padding frame %d to account for change in frame delay.\n",
          expected_frame);
      GameInput *last_frame = &queue->_inputs[PREVIOUS_FRAME(queue->_head)];
      input_queue_AddDelayedInputToQueue(queue, last_frame, expected_frame);
      expected_frame++;
   }

   ASSERT(frame == 0 || frame == queue->_inputs[PREVIOUS_FRAME(queue->_head)].frame + 1);
   return frame;
}


void
input_queue_Log(InputQueue* queue, const char *fmt, ...)
{
   char buf[1024];
   size_t offset;
   va_list args;

   offset = snprintf(buf, ARRAY_SIZE(buf), "input q%d | ", queue->_id);
   va_start(args, fmt);
   vsnprintf(buf + offset, ARRAY_SIZE(buf) - offset - 1, fmt, args);
   buf[ARRAY_SIZE(buf)-1] = '\0';
   Log(buf);
   va_end(args);
}
