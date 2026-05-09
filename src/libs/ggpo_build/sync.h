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

#ifndef _SYNC_H
#define _SYNC_H

#include "types.h"
#include "ggponet.h"
#include "game_input.h"
#include "input_queue.h"
#include "ring_buffer.h"

#define MAX_PREDICTION_FRAMES    8

typedef struct SyncTestBackend SyncTestBackend;
typedef struct UdpMsg_connect_status UdpMsg_connect_status;

struct sync_Config
{
        GGPOSessionCallbacks    callbacks;
        int                     num_prediction_frames;
        int                     num_players;
        int                     input_size;
};
typedef struct sync_Config sync_Config;

struct sync_Event
{
        enum {
                ConfirmedInput,
        } type;
        union {
                struct {
                        GameInput   input;
                } confirmedInput;
        } u;
};
typedef struct sync_Event sync_Event;

struct sync_SavedFrame
{
        byte* buf;
        int      cbuf;
        int      frame; // -1
        int      checksum;
        // sync_SavedFrame() : buf(NULL), cbuf(0), frame(-1), checksum(0) {}
};
typedef struct sync_SavedFrame sync_SavedFrame;

struct sync_SavedState
{
        sync_SavedFrame frames[MAX_PREDICTION_FRAMES + 2];
        int head;
};
typedef struct sync_SavedState sync_SavedState;

struct Sync
{
        GGPOSessionCallbacks _callbacks;
        sync_SavedState     _savedstate;
        sync_Config         _config;

        bool           _rollingback;
        int            _last_confirmed_frame;
        int            _framecount;
        int            _max_prediction_frames;

        InputQueue* _input_queues;

        RingBuffer _event_queue_ring;
        sync_Event _event_queue[32];
        UdpMsg_connect_status* _local_connect_status;
};
typedef struct Sync Sync;

void sync_ctor(Sync* sync, UdpMsg_connect_status* connect_status);
void sync_dtor(Sync* sync);
void sync_Init(Sync *sync, sync_Config* config);

void sync_SetLastConfirmedFrame(Sync* sync, int frame);
void sync_SetFrameDelay(Sync* sync, int queue, int delay);
bool sync_AddLocalInput(Sync* sync, int queue, GameInput* input);
void sync_AddRemoteInput(Sync* sync, int queue, GameInput* input);
int sync_GetConfirmedInputs(Sync* sync, void* values, int size, int frame);
int sync_SynchronizeInputs(Sync* sync, void* values, int size);
void sync_CheckSimulation(Sync* sync, int timeout);
void sync_AdjustSimulation(Sync* sync, int seek_to);
void sync_IncrementFrame(Sync* sync);
inline int sync_GetFrameCount(Sync* sync) { return sync->_framecount; }
inline bool sync_InRollback(Sync* sync) { return sync->_rollingback; }
bool sync_GetEvent(Sync* sync, sync_Event* e);
sync_SavedFrame* sync_GetLastSavedFrame(Sync* sync);
void sync_SaveCurrentFrame(Sync* sync);
void sync_LoadFrame(Sync* sync, int frame);
#endif

