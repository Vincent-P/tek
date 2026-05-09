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

#ifndef _SPECTATOR_H
#define _SPECTATOR_H

#include "types.h"
#include "sync.h"
#include "backend.h"
#include "timesync.h"
#include "network/udp_proto.h"

#define SPECTATOR_FRAME_BUFFER_SIZE    64

struct SpectatorBackend  {
	GGPOSessionHeader _header;

   Udp                   _udp;
   UdpProtocol           _host;
   bool                  _synchronizing;
   int                   _input_size;
   int                   _num_players;
   int                   _next_input_to_send;
   GameInput             _inputs[SPECTATOR_FRAME_BUFFER_SIZE];
};

typedef struct SpectatorBackend SpectatorBackend;

#if defined(GGPO_STEAM)
   void spec_ctor_steam(SpectatorBackend *spec, GGPOSessionCallbacks *cb, const char *gamename, int local_channel, int num_players, int input_size, uint64 host_steam_id);
#else
   void spec_ctor(SpectatorBackend *spec, GGPOSessionCallbacks *cb, const char *gamename, uint16 localport, int num_players, int input_size, char *hostip, uint16 hostport);
#endif
   void spec_dtor(SpectatorBackend *spec);

   GGPOErrorCode spec_DoPoll(SpectatorBackend *spec, int timeout);
   inline GGPOErrorCode spec_AddPlayer(SpectatorBackend *spec, GGPOPlayer *player, GGPOPlayerHandle *handle) { return GGPO_ERRORCODE_UNSUPPORTED; }
   inline GGPOErrorCode spec_AddLocalInput(SpectatorBackend *spec, GGPOPlayerHandle player, void *values, int size) { return GGPO_OK; }
   GGPOErrorCode spec_SyncInput(SpectatorBackend *spec, void *values, int size, int *disconnect_flags);
   GGPOErrorCode spec_IncrementFrame(SpectatorBackend *spec);
   inline GGPOErrorCode spec_DisconnectPlayer(SpectatorBackend *spec, GGPOPlayerHandle handle) { return GGPO_ERRORCODE_UNSUPPORTED; }
   inline GGPOErrorCode spec_GetNetworkStats(SpectatorBackend *spec, GGPONetworkStats *stats, GGPOPlayerHandle handle) { return GGPO_ERRORCODE_UNSUPPORTED; }
   inline GGPOErrorCode spec_SetFrameDelay(SpectatorBackend *spec, GGPOPlayerHandle player, int delay) { return GGPO_ERRORCODE_UNSUPPORTED; }
   inline GGPOErrorCode spec_SetDisconnectTimeout(SpectatorBackend *spec, int timeout) { return GGPO_ERRORCODE_UNSUPPORTED; }
   inline GGPOErrorCode spec_SetDisconnectNotifyStart(SpectatorBackend *spec, int timeout) { return GGPO_ERRORCODE_UNSUPPORTED; }

   void spec_PollUdpProtocolEvents(SpectatorBackend *spec);
   void spec_CheckInitialSync(SpectatorBackend *spec);

   void spec_OnUdpProtocolEvent(SpectatorBackend *spec, udp_protocol_Event *e);

#endif
