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

#ifndef _SYNCTEST_H
#define _SYNCTEST_H

#include "types.h"
#include "backend.h"
#include "sync.h"
#include "ring_buffer.h"


   struct synctest_SavedInfo {
      int         frame;
      int         checksum;
      char        *buf;
      int         cbuf;
      GameInput   input;
   };
   typedef struct synctest_SavedInfo synctest_SavedInfo;

struct SyncTestBackend {
	GGPOSessionHeader _header;
    
   Sync                  _sync;
   int                    _num_players;
   int                    _check_distance;
   int                    _last_verified;
   bool                   _rollingback;
   bool                   _running;
   FILE                   *_logfp;
   char                   _game[128];

   GameInput                  _current_input;
   GameInput                  _last_input;
   RingBuffer _saved_frames_ring;
   synctest_SavedInfo  _saved_frames[32];
};

   typedef struct SyncTestBackend SyncTestBackend;

   void synctest_ctor(SyncTestBackend *synctest, GGPOSessionCallbacks *cb, char *gamename, int frames, int num_players);
   void synctest_dtor(SyncTestBackend *synctest);

   GGPOErrorCode synctest_DoPoll(SyncTestBackend *synctest, int timeout);
   GGPOErrorCode synctest_AddPlayer(SyncTestBackend *synctest, GGPOPlayer *player, GGPOPlayerHandle *handle);
   GGPOErrorCode synctest_AddLocalInput(SyncTestBackend *synctest, GGPOPlayerHandle player, void *values, int size);
   GGPOErrorCode synctest_SyncInput(SyncTestBackend *synctest, void *values, int size, int *disconnect_flags);
   GGPOErrorCode synctest_IncrementFrame(SyncTestBackend *synctest);
   inline GGPOErrorCode synctest_DisconnectPlayer(SyncTestBackend *synctest,GGPOPlayerHandle handle) { return GGPO_OK; }
   inline GGPOErrorCode synctest_GetNetworkStats(SyncTestBackend *synctest,GGPONetworkStats* stats, GGPOPlayerHandle handle) { return GGPO_OK; }
   GGPOErrorCode synctest_Logv(SyncTestBackend *synctest, char const *fmt, va_list list);



   	inline GGPOErrorCode synctest_SetFrameDelay(SyncTestBackend *synctest,GGPOPlayerHandle player, int delay) { return GGPO_ERRORCODE_UNSUPPORTED; }
	inline GGPOErrorCode synctest_SetDisconnectTimeout(SyncTestBackend *synctest,int timeout) { return GGPO_ERRORCODE_UNSUPPORTED; }
	inline GGPOErrorCode synctest_SetDisconnectNotifyStart(SyncTestBackend *synctest,int timeout) { return GGPO_ERRORCODE_UNSUPPORTED; }
   
   void synctest_RaiseSyncError(SyncTestBackend *synctest, const char *fmt, ...);
   void synctest_BeginLog(SyncTestBackend *synctest, int saving);
   void synctest_EndLog(SyncTestBackend *synctest);
   void synctest_LogSaveStates(SyncTestBackend *synctest, synctest_SavedInfo *info);

#endif

