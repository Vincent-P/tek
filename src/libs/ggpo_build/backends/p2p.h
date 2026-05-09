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

#ifndef _P2P_H
#define _P2P_H

#include "types.h"
#include "sync.h"
#include "backend.h"
#include "timesync.h"
#include "network/udp_proto.h"

struct UdpMsg;

struct Peer2PeerBackend {
	GGPOSessionHeader _header;

   Sync                  _sync;
   Udp                   _udp;
   UdpProtocol           *_endpoints;
   UdpProtocol           _spectators[GGPO_MAX_SPECTATORS];
   int                   _num_spectators;
   int                   _input_size;

   bool                  _synchronizing;
   int                   _num_players;
   int                   _next_recommended_sleep;

   int                   _next_spectator_frame;
   int                   _disconnect_timeout;
   int                   _disconnect_notify_start;

   UdpMsg_connect_status _local_connect_status[UDP_MSG_MAX_PLAYERS];
};
typedef struct Peer2PeerBackend Peer2PeerBackend;


#if defined(GGPO_STEAM)
void p2p_ctor_steam(Peer2PeerBackend *p2p, GGPOSessionCallbacks *cb, const char *gamename, int local_channel, int num_players, int input_size);
void p2p_AddRemotePlayerSteam(Peer2PeerBackend *p2p, uint64 steam_id, int queue);
GGPOErrorCode p2p_AddSpectatorSteam(Peer2PeerBackend *p2p, uint64 steam_id);
#else
void p2p_ctor(Peer2PeerBackend *p2p, GGPOSessionCallbacks *cb, const char *gamename, uint16 localport, int num_players, int input_size);
void p2p_AddRemotePlayer(Peer2PeerBackend *p2p, char *remoteip, uint16 reportport, int queue);
GGPOErrorCode p2p_AddSpectator(Peer2PeerBackend *p2p, char *remoteip, uint16 reportport);
#endif

void p2p_dtor(Peer2PeerBackend *p2p);

GGPOErrorCode p2p_DoPoll(Peer2PeerBackend *p2p, int timeout);
GGPOErrorCode p2p_AddPlayer(Peer2PeerBackend *p2p, GGPOPlayer *player, GGPOPlayerHandle *handle);
GGPOErrorCode p2p_AddLocalInput(Peer2PeerBackend *p2p, GGPOPlayerHandle player, void *values, int size);
GGPOErrorCode p2p_SyncInput(Peer2PeerBackend *p2p, void *values, int size, int *disconnect_flags);
GGPOErrorCode p2p_IncrementFrame(Peer2PeerBackend *p2p);
GGPOErrorCode p2p_DisconnectPlayer(Peer2PeerBackend *p2p, GGPOPlayerHandle handle);
GGPOErrorCode p2p_GetNetworkStats(Peer2PeerBackend *p2p, GGPONetworkStats *stats, GGPOPlayerHandle handle);
GGPOErrorCode p2p_SetFrameDelay(Peer2PeerBackend *p2p, GGPOPlayerHandle player, int delay);
GGPOErrorCode p2p_SetDisconnectTimeout(Peer2PeerBackend *p2p, int timeout);
GGPOErrorCode p2p_SetDisconnectNotifyStart(Peer2PeerBackend *p2p, int timeout);

GGPOErrorCode p2p_PlayerHandleToQueue(Peer2PeerBackend *p2p, GGPOPlayerHandle player, int *queue);
inline GGPOPlayerHandle p2p_QueueToPlayerHandle(Peer2PeerBackend *p2p, int queue) { return (GGPOPlayerHandle)(queue + 1); }
inline GGPOPlayerHandle p2p_QueueToSpectatorHandle(Peer2PeerBackend *p2p, int queue) { return (GGPOPlayerHandle)(queue + 1000); }
void p2p_DisconnectPlayerQueue(Peer2PeerBackend *p2p, int queue, int syncto);
void p2p_PollSyncEvents(Peer2PeerBackend *p2p);
void p2p_PollUdpProtocolEvents(Peer2PeerBackend *p2p);
void p2p_CheckInitialSync(Peer2PeerBackend *p2p);
int p2p_Poll2Players(Peer2PeerBackend *p2p, int current_frame);
int p2p_PollNPlayers(Peer2PeerBackend *p2p, int current_frame);
inline void p2p_OnSyncEvent(Peer2PeerBackend *p2p, sync_Event *e) { }
void p2p_OnUdpProtocolEvent(Peer2PeerBackend *p2p, udp_protocol_Event *e, GGPOPlayerHandle handle);
void p2p_OnUdpProtocolPeerEvent(Peer2PeerBackend *p2p, udp_protocol_Event *e, int queue);
void p2p_OnUdpProtocolSpectatorEvent(Peer2PeerBackend *p2p, udp_protocol_Event *e, int queue);

#endif
