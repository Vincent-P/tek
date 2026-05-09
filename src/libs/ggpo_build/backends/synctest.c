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

#include "synctest.h"


void synctest_ctor(SyncTestBackend *synctest, GGPOSessionCallbacks *cb, char *gamename, int frames, int num_players)
{
    sync_ctor(&synctest->_sync, NULL);

	synctest->_header._session_type = SESSION_SYNCTEST;
   synctest->_header._callbacks = *cb;
   synctest->_num_players = num_players;
   synctest->_check_distance = frames;
   synctest->_last_verified = 0;
   synctest->_rollingback = false;
   synctest->_running = false;
   synctest->_logfp = NULL;
   gameinput_erase(&synctest->_current_input);
   strcpy(synctest->_game, gamename);
   ring_ctor(&synctest->_saved_frames_ring, ARRAY_SIZE(synctest->_saved_frames));

   /*
    * Initialize the synchronziation layer
    */
   sync_Config config = { 0 };
   config.callbacks = synctest->_header._callbacks;
   config.num_prediction_frames = MAX_PREDICTION_FRAMES;
   sync_Init(&synctest->_sync, &config);

   /*
    * Preload the ROM
    */
   synctest->_header._callbacks.begin_game(gamename);
}


void synctest_dtor(SyncTestBackend* synctest)
{

}

GGPOErrorCode
synctest_DoPoll(SyncTestBackend *synctest,int timeout)
{
   if (!synctest->_running) {
      GGPOEvent info;

      info.code = GGPO_EVENTCODE_RUNNING;
      synctest->_header._callbacks.on_event(&info);
      synctest->_running = true;
   }
   return GGPO_OK;
}

GGPOErrorCode
synctest_AddPlayer(SyncTestBackend *synctest,GGPOPlayer *player, GGPOPlayerHandle *handle)
{
   if (player->player_num < 1 || player->player_num > synctest->_num_players) {
      return GGPO_ERRORCODE_PLAYER_OUT_OF_RANGE;
   }
   *handle = (GGPOPlayerHandle)(player->player_num - 1);
   return GGPO_OK;
}

GGPOErrorCode
synctest_AddLocalInput(SyncTestBackend *synctest,GGPOPlayerHandle player, void *values, int size)
{
   if (!synctest->_running) {
      return GGPO_ERRORCODE_NOT_SYNCHRONIZED;
   }

   int index = (int)player;
   for (int i = 0; i < size; i++) {
      synctest->_current_input.bits[(index * size) + i] |= ((char *)values)[i];
   }
   return GGPO_OK;
}

GGPOErrorCode
synctest_SyncInput(SyncTestBackend *synctest,void *values,
                           int size,
                           int *disconnect_flags)
{
   synctest_BeginLog(synctest, false);
   if (synctest->_rollingback) {
      synctest->_last_input = synctest->_saved_frames[ring_front(&synctest->_saved_frames_ring)].input;
   } else {
      if (sync_GetFrameCount(&synctest->_sync) == 0) {
         sync_SaveCurrentFrame(&synctest->_sync);
      }
      synctest->_last_input = synctest->_current_input;
   }
   memcpy(values, synctest->_last_input.bits, size);
   if (disconnect_flags) {
      *disconnect_flags = 0;
   }
   return GGPO_OK;
}

GGPOErrorCode
synctest_IncrementFrame(SyncTestBackend *synctest)
{
   sync_IncrementFrame(&synctest->_sync);
   gameinput_erase(&synctest->_current_input);

   Log("End of frame(%d)...\n", sync_GetFrameCount(&synctest->_sync));
   synctest_EndLog(synctest);

   if (synctest->_rollingback) {
      return GGPO_OK;
   }

   int frame = sync_GetFrameCount(&synctest->_sync);
   // Hold onto the current frame in our queue of saved states.  We'll need
   // the checksum later to verify that our replay of the same frame got the
   // same results.
   synctest_SavedInfo info;
   info.frame = frame;
   info.input = synctest->_last_input;
   info.cbuf = sync_GetLastSavedFrame(&synctest->_sync)->cbuf;
   info.buf = (char *)malloc(info.cbuf);
   memcpy(info.buf, sync_GetLastSavedFrame(&synctest->_sync)->buf, info.cbuf);
   info.checksum = sync_GetLastSavedFrame(&synctest->_sync)->checksum;
   synctest->_saved_frames[ring_push(&synctest->_saved_frames_ring)] = info;

   if (frame - synctest->_last_verified == synctest->_check_distance) {
      // We've gone far enough ahead and should now start replaying frames.
      // Load the last verified frame and set the rollback flag to true.
      sync_LoadFrame(&synctest->_sync, synctest->_last_verified);

      synctest->_rollingback = true;
      while(!ring_empty(&synctest->_saved_frames_ring)) {
         synctest->_header._callbacks.advance_frame(0);

         // Verify that the checksumn of this frame is the same as the one in our
         // list.
         info = synctest->_saved_frames[ring_front(&synctest->_saved_frames_ring)];
         ring_pop(&synctest->_saved_frames_ring);

         if (info.frame != sync_GetFrameCount(&synctest->_sync)) {
            synctest_RaiseSyncError(synctest, "Frame number %d does not match saved frame number %d", info.frame, frame);
         }
         int checksum = sync_GetLastSavedFrame(&synctest->_sync)->checksum;
         if (info.checksum != checksum) {
            synctest_LogSaveStates(synctest, &info);
            synctest_RaiseSyncError(synctest, "Checksum for frame %d does not match saved (%d != %d)", frame, checksum, info.checksum);
         }
         printf("Checksum %08d for frame %d matches.\n", checksum, info.frame);
         free(info.buf);
      }
      synctest->_last_verified = frame;
      synctest->_rollingback = false;
   }

   return GGPO_OK;
}

void
synctest_RaiseSyncError(SyncTestBackend *synctest, const char *fmt, ...)
{
   char buf[1024];
   va_list args;
   va_start(args, fmt);
   vsnprintf(buf, ARRAY_SIZE(buf), fmt, args);
   va_end(args);

   puts(buf);
#if defined(_WINDOWS)
   OutputDebugStringA(buf);
#endif
   synctest_EndLog(synctest);
   DebugBreak();
}

GGPOErrorCode
synctest_Logv(SyncTestBackend *synctest, char const *fmt, va_list list)
{
   if (synctest->_logfp) {
      vfprintf(synctest->_logfp, fmt, list);
   }
   return GGPO_OK;
}

void
synctest_BeginLog(SyncTestBackend *synctest, int saving)
{
   synctest_EndLog(synctest);

   char filename[MAX_PATH];
#if defined(_WINDOWS)
   CreateDirectoryA("synclogs", NULL);
#else
   ASSERT(false);
#endif
   snprintf(filename, ARRAY_SIZE(filename), "synclogs\\%s-%04d-%s.log",
           saving ? "state" : "log",
           sync_GetFrameCount(&synctest->_sync),
           synctest->_rollingback ? "replay" : "original");

   synctest->_logfp = fopen(filename, "w");
}

void
synctest_EndLog(SyncTestBackend *synctest)
{
   if (synctest->_logfp) {
      fprintf(synctest->_logfp, "Closing log file.\n");
      fclose(synctest->_logfp);
      synctest->_logfp = NULL;
   }
}
void
synctest_LogSaveStates(SyncTestBackend *synctest, synctest_SavedInfo *info)
{
   char filename[MAX_PATH];
   snprintf(filename, ARRAY_SIZE(filename), "synclogs\\state-%04d-original.log", sync_GetFrameCount(&synctest->_sync));
   synctest->_header._callbacks.log_game_state(filename, (unsigned char *)info->buf, info->cbuf);

   snprintf(filename, ARRAY_SIZE(filename), "synclogs\\state-%04d-replay.log", sync_GetFrameCount(&synctest->_sync));
   synctest->_header._callbacks.log_game_state(filename, sync_GetLastSavedFrame(&synctest->_sync)->buf, sync_GetLastSavedFrame(&synctest->_sync)->cbuf);
}
