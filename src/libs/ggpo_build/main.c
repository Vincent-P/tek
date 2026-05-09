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
#include "backends/p2p.h"
#include "backends/synctest.h"
#include "backends/spectator.h"
#include "ggponet.h"

#if defined(_WINDOWS)
BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
   srand(Platform_GetCurrentTimeMS() + Platform_GetProcessID());
   return TRUE;
}
#endif

void
ggpo_log(GGPOSession *ggpo, const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   ggpo_logv(ggpo, fmt, args);
   va_end(args);
}

void
ggpo_logv(GGPOSession *ggpo, const char *fmt, va_list args)
{
   if (ggpo) {
       GGPOSessionHeader* header = (GGPOSessionHeader*)ggpo;
       if (header->_session_type == SESSION_SYNCTEST) {
           synctest_Logv((SyncTestBackend*)ggpo, fmt, args);
       }
       else {
           Logv(fmt, args);
       }
   }
}

#if defined(GGPO_STEAM)
GGPOErrorCode
ggpo_start_session(GGPOSession **session,
                   GGPOSessionCallbacks *cb,
                   const char *game,
                   int num_players,
                   int input_size,
                   int local_channel)
{
    void* p2p = calloc(sizeof(Peer2PeerBackend), 1);
    p2p_ctor_steam((Peer2PeerBackend*)p2p, cb,
        game,
        local_channel,
        num_players,
        input_size);
    *session = (GGPOSession*)p2p;
    return GGPO_OK;
}
#else
GGPOErrorCode
ggpo_start_session(GGPOSession **session,
                   GGPOSessionCallbacks *cb,
                   const char *game,
                   int num_players,
                   int input_size,
                   unsigned short localport)
{
    void* p2p = calloc(sizeof(Peer2PeerBackend), 1);
    p2p_ctor((Peer2PeerBackend*)p2p, cb,
        game,
        localport,
        num_players,
        input_size);
    *session = (GGPOSession*)p2p;
    return GGPO_OK;
}
#endif

GGPOErrorCode
ggpo_add_player(GGPOSession *ggpo,
                GGPOPlayer *player,
                GGPOPlayerHandle *handle)
{
   if (!ggpo) {
      return GGPO_ERRORCODE_INVALID_SESSION;
   }
   GGPOSessionHeader* header = (GGPOSessionHeader*)ggpo;
   switch (header->_session_type) {
   case SESSION_P2P: return p2p_AddPlayer((Peer2PeerBackend*)ggpo, player, handle);
   case SESSION_SPECTATOR: return spec_AddPlayer((SpectatorBackend*)ggpo, player, handle);
   case SESSION_SYNCTEST: return synctest_AddPlayer((SyncTestBackend*)ggpo, player, handle);
   }

   return GGPO_ERRORCODE_INVALID_SESSION;
}



GGPOErrorCode
ggpo_start_synctest(GGPOSession **ggpo,
                    GGPOSessionCallbacks *cb,
                    char *game,
                    int num_players,
                    int input_size,
                    int frames)
{
	void* synctest = calloc(sizeof(SyncTestBackend), 1);
	synctest_ctor((SyncTestBackend*)synctest, cb, game, frames, num_players);
	*ggpo = (GGPOSession*)synctest;
	return GGPO_OK;
}

GGPOErrorCode
ggpo_set_frame_delay(GGPOSession *ggpo,
                     GGPOPlayerHandle player,
                     int frame_delay)
{
   if (!ggpo) {
	   return GGPO_ERRORCODE_INVALID_SESSION;
   }
   GGPOSessionHeader* header = (GGPOSessionHeader*)ggpo;
   switch (header->_session_type) {
   case SESSION_P2P: return p2p_SetFrameDelay((Peer2PeerBackend*)ggpo, player, frame_delay);
   case SESSION_SPECTATOR: return spec_SetFrameDelay((SpectatorBackend*)ggpo, player, frame_delay);
   case SESSION_SYNCTEST: return synctest_SetFrameDelay((SyncTestBackend*)ggpo, player, frame_delay);
   }

   return GGPO_ERRORCODE_INVALID_SESSION;
}

GGPOErrorCode
ggpo_idle(GGPOSession *ggpo, int timeout)
{
   if (!ggpo) {
	   return GGPO_ERRORCODE_INVALID_SESSION;
   }
   GGPOSessionHeader* header = (GGPOSessionHeader*)ggpo;
   switch (header->_session_type) {
   case SESSION_P2P: return p2p_DoPoll((Peer2PeerBackend*)ggpo, timeout);
   case SESSION_SPECTATOR: return spec_DoPoll((SpectatorBackend*)ggpo, timeout);
   case SESSION_SYNCTEST: return synctest_DoPoll((SyncTestBackend*)ggpo, timeout);
   }

   return GGPO_ERRORCODE_INVALID_SESSION;
}

GGPOErrorCode
ggpo_add_local_input(GGPOSession *ggpo,
                     GGPOPlayerHandle player,
                     void *values,
                     int size)
{
   if (!ggpo) {
	   return GGPO_ERRORCODE_INVALID_SESSION;
   }
   GGPOSessionHeader* header = (GGPOSessionHeader*)ggpo;
   switch (header->_session_type) {
   case SESSION_P2P: return p2p_AddLocalInput((Peer2PeerBackend*)ggpo, player, values, size);
   case SESSION_SPECTATOR: return spec_AddLocalInput((SpectatorBackend*)ggpo, player, values, size);
   case SESSION_SYNCTEST: return synctest_AddLocalInput((SyncTestBackend*)ggpo, player, values, size);
   }

   return GGPO_ERRORCODE_INVALID_SESSION;
}

GGPOErrorCode
ggpo_synchronize_input(GGPOSession *ggpo,
                       void *values,
                       int size,
                       int *disconnect_flags)
{
   if (!ggpo) {
	   return GGPO_ERRORCODE_INVALID_SESSION;
   }
   GGPOSessionHeader* header = (GGPOSessionHeader*)ggpo;
   switch (header->_session_type) {
   case SESSION_P2P: return p2p_SyncInput((Peer2PeerBackend*)ggpo, values, size, disconnect_flags);
   case SESSION_SPECTATOR: return spec_SyncInput((SpectatorBackend*)ggpo, values, size, disconnect_flags);
   case SESSION_SYNCTEST: return synctest_SyncInput((SyncTestBackend*)ggpo, values, size, disconnect_flags);
   }

   return GGPO_ERRORCODE_INVALID_SESSION;
}

GGPOErrorCode ggpo_disconnect_player(GGPOSession *ggpo,
                                     GGPOPlayerHandle player)
{
   if (!ggpo) {
	   return GGPO_ERRORCODE_INVALID_SESSION;
   }
   GGPOSessionHeader* header = (GGPOSessionHeader*)ggpo;
   switch (header->_session_type) {
   case SESSION_P2P: return p2p_DisconnectPlayer((Peer2PeerBackend*)ggpo, player);
   case SESSION_SPECTATOR: return spec_DisconnectPlayer((SpectatorBackend*)ggpo, player);
   case SESSION_SYNCTEST: return synctest_DisconnectPlayer((SyncTestBackend*)ggpo, player);
   }

   return GGPO_ERRORCODE_INVALID_SESSION;
}

GGPOErrorCode
ggpo_advance_frame(GGPOSession *ggpo)
{
   if (!ggpo) {
	   return GGPO_ERRORCODE_INVALID_SESSION;
   }
   GGPOSessionHeader* header = (GGPOSessionHeader*)ggpo;
   switch (header->_session_type) {
   case SESSION_P2P: return p2p_IncrementFrame((Peer2PeerBackend*)ggpo);
   case SESSION_SPECTATOR: return spec_IncrementFrame((SpectatorBackend*)ggpo);
   case SESSION_SYNCTEST: return synctest_IncrementFrame((SyncTestBackend*)ggpo);
   }

   return GGPO_ERRORCODE_INVALID_SESSION;
}

GGPOErrorCode
ggpo_get_network_stats(GGPOSession *ggpo,
                       GGPOPlayerHandle player,
                       GGPONetworkStats *stats)
{
   if (!ggpo) {
	   return GGPO_ERRORCODE_INVALID_SESSION;
   }
   GGPOSessionHeader* header = (GGPOSessionHeader*)ggpo;
   switch (header->_session_type) {
   case SESSION_P2P: return p2p_GetNetworkStats((Peer2PeerBackend*)ggpo, stats, player);
   case SESSION_SPECTATOR: return spec_GetNetworkStats((SpectatorBackend*)ggpo, stats, player);
   case SESSION_SYNCTEST: return synctest_GetNetworkStats((SyncTestBackend*)ggpo, stats, player);
   }

   return GGPO_ERRORCODE_INVALID_SESSION;
}


GGPOErrorCode
ggpo_close_session(GGPOSession *ggpo)
{
   if (!ggpo) {
	   return GGPO_ERRORCODE_INVALID_SESSION;
   }
   GGPOSessionHeader* header = (GGPOSessionHeader*)ggpo;
   switch (header->_session_type) {
   case SESSION_P2P: p2p_dtor((Peer2PeerBackend*)ggpo); break;
   case SESSION_SPECTATOR: spec_dtor((SpectatorBackend*)ggpo); break;
   case SESSION_SYNCTEST: synctest_dtor((SyncTestBackend*)ggpo); break;
   }
   free(ggpo);
   return GGPO_OK;
}

GGPOErrorCode
ggpo_set_disconnect_timeout(GGPOSession *ggpo, int timeout)
{
   if (!ggpo) {
	   return GGPO_ERRORCODE_INVALID_SESSION;
   }
   GGPOSessionHeader* header = (GGPOSessionHeader*)ggpo;
   switch (header->_session_type) {
   case SESSION_P2P: return p2p_SetDisconnectTimeout((Peer2PeerBackend*)ggpo, timeout);
   case SESSION_SPECTATOR: return spec_SetDisconnectTimeout((SpectatorBackend*)ggpo, timeout);
   case SESSION_SYNCTEST: return synctest_SetDisconnectTimeout((SyncTestBackend*)ggpo, timeout);
   }

   return GGPO_ERRORCODE_INVALID_SESSION;
}

GGPOErrorCode
ggpo_set_disconnect_notify_start(GGPOSession *ggpo, int timeout)
{
   if (!ggpo) {
	   return GGPO_ERRORCODE_INVALID_SESSION;
   }
   GGPOSessionHeader* header = (GGPOSessionHeader*)ggpo;
   switch (header->_session_type) {
   case SESSION_P2P: return p2p_SetDisconnectNotifyStart((Peer2PeerBackend*)ggpo, timeout);
   case SESSION_SPECTATOR: return spec_SetDisconnectNotifyStart((SpectatorBackend*)ggpo, timeout);
   case SESSION_SYNCTEST: return synctest_SetDisconnectNotifyStart((SyncTestBackend*)ggpo, timeout);
   }

   return GGPO_ERRORCODE_INVALID_SESSION;
}

#if defined(GGPO_STEAM)
GGPOErrorCode ggpo_start_spectating(GGPOSession **session,
                                    GGPOSessionCallbacks *cb,
                                    const char *game,
                                    int num_players,
                                    int input_size,
                                    int local_channel,
                                    uint64_t host_steam_id)
{
    void* spec = calloc(sizeof(SpectatorBackend), 1);
    spec_ctor_steam((SpectatorBackend*)spec, cb,
                    game,
                    local_channel,
                    num_players,
                    input_size,
                    host_steam_id);
    *session = (GGPOSession*)spec;
    return GGPO_OK;
}
#else
GGPOErrorCode ggpo_start_spectating(GGPOSession **session,
                                    GGPOSessionCallbacks *cb,
                                    const char *game,
                                    int num_players,
                                    int input_size,
                                    unsigned short local_port,
                                    char *host_ip,
                                    unsigned short host_port)
{
    void* spec = calloc(sizeof(SpectatorBackend), 1);
    spec_ctor((SpectatorBackend*)spec, cb,
                                                  game,
                                                  local_port,
                                                  num_players,
                                                  input_size,
                                                  host_ip,
                                                  host_port);
    *session = (GGPOSession*)spec;
    return GGPO_OK;
}
#endif
