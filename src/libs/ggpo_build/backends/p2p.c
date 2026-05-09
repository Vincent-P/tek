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

#include "p2p.h"

static const int RECOMMENDATION_INTERVAL = 240;
static const int DEFAULT_DISCONNECT_TIMEOUT = 5000;
static const int DEFAULT_DISCONNECT_NOTIFY_START = 750;

static void p2p_OnMsg(conn_Address from, UdpMsg* msg, int len, void* user_data);



#ifndef GGPO_STEAM

void p2p_ctor(Peer2PeerBackend *p2p, GGPOSessionCallbacks *cb, const char *gamename, uint16 localport, int num_players, int input_size)
{
	p2p->_num_players = num_players;
	p2p->_input_size = input_size;
	p2p->_disconnect_timeout = DEFAULT_DISCONNECT_TIMEOUT;
	p2p->_disconnect_notify_start = DEFAULT_DISCONNECT_NOTIFY_START;
	p2p->_num_spectators = 0;
	p2p->_next_spectator_frame = 0;


	sync_ctor(&p2p->_sync, p2p->_local_connect_status);
	p2p->_header._session_type = SESSION_P2P;
	p2p->_header._callbacks = *cb;
	p2p->_synchronizing = true;
	p2p->_next_recommended_sleep = 0;

	/*
	 * Initialize the synchronziation layer
	 */
	sync_Config config = { 0 };
	config.num_players = num_players;
	config.input_size = input_size;
	config.callbacks = p2p->_header._callbacks;
	config.num_prediction_frames = MAX_PREDICTION_FRAMES;
	sync_Init(&p2p->_sync, &config);

	/*
	 * Initialize the UDP port
	 */
	udp_ctor(&p2p->_udp);
	udp_Init(&p2p->_udp, localport, p2p_OnMsg, p2p);

	p2p->_endpoints = calloc(p2p->_num_players, sizeof(UdpProtocol));
	for (int i = 0; i < p2p->_num_players; ++i) {
		UdpProtocol_ctor(&p2p->_endpoints[i]);
	}
	for (int i = 0; i < ARRAY_SIZE(p2p->_spectators); i++) {
		UdpProtocol_ctor(&p2p->_spectators[i]);
	}
	memset(p2p->_local_connect_status, 0, sizeof(p2p->_local_connect_status));
	for (int i = 0; i < ARRAY_SIZE(p2p->_local_connect_status); i++) {
		p2p->_local_connect_status[i].last_frame = -1;
	}

	/*
	 * Preload the ROM
	 */
	p2p->_header._callbacks.begin_game(gamename);
}

void p2p_AddRemotePlayer(Peer2PeerBackend *p2p, char* ip, uint16 port, int queue)
{
	p2p->_synchronizing = true;

	ASSERT(conn_support_ip_port());
	conn_Address peer_addr = conn_address_from_ip_port(ip, port);

	UdpProtocol_Init(&p2p->_endpoints[queue], &p2p->_udp, queue, peer_addr, p2p->_local_connect_status);
	UdpProtocol_SetDisconnectTimeout(&p2p->_endpoints[queue], p2p->_disconnect_timeout);
	UdpProtocol_SetDisconnectNotifyStart(&p2p->_endpoints[queue], p2p->_disconnect_notify_start);
	UdpProtocol_Synchronize(&p2p->_endpoints[queue]);
}

GGPOErrorCode p2p_AddSpectator(Peer2PeerBackend *p2p, char* ip, uint16 port)
{
	if (p2p->_num_spectators == GGPO_MAX_SPECTATORS) {
		return GGPO_ERRORCODE_TOO_MANY_SPECTATORS;
	}
	if (!p2p->_synchronizing) {
		return GGPO_ERRORCODE_INVALID_REQUEST;
	}
	int queue = p2p->_num_spectators++;

	ASSERT(conn_support_ip_port());
	conn_Address peer_addr = conn_address_from_ip_port(ip, port);

	UdpProtocol_Init(&p2p->_spectators[queue], &p2p->_udp, queue + 1000, peer_addr, p2p->_local_connect_status);
	UdpProtocol_SetDisconnectTimeout(&p2p->_spectators[queue], p2p->_disconnect_timeout);
	UdpProtocol_SetDisconnectNotifyStart(&p2p->_spectators[queue], p2p->_disconnect_notify_start);
	UdpProtocol_Synchronize(&p2p->_spectators[queue]);

	return GGPO_OK;
}

#endif /* !GGPO_STEAM */

void p2p_dtor(Peer2PeerBackend *p2p)
{
	for (int i = 0; i < p2p->_num_players; ++i) {
		UdpProtocol_dtor(&p2p->_endpoints[i]);
	}
	free(p2p->_endpoints);
	sync_dtor(&p2p->_sync);
	udp_dtor(&p2p->_udp);
}

#if defined(GGPO_STEAM)

/*
 * p2p_ctor_steam --
 *
 * Constructor for Steam Networking Messages sessions.
 * Uses a virtual channel number instead of a UDP port.
 */
void p2p_ctor_steam(Peer2PeerBackend *p2p, GGPOSessionCallbacks *cb, const char *gamename, int local_channel, int num_players, int input_size)
{
	p2p->_num_players = num_players;
	p2p->_input_size = input_size;
	p2p->_disconnect_timeout = DEFAULT_DISCONNECT_TIMEOUT;
	p2p->_disconnect_notify_start = DEFAULT_DISCONNECT_NOTIFY_START;
	p2p->_num_spectators = 0;
	p2p->_next_spectator_frame = 0;

	sync_ctor(&p2p->_sync, p2p->_local_connect_status);
	p2p->_header._session_type = SESSION_P2P;
	p2p->_header._callbacks = *cb;
	p2p->_synchronizing = true;
	p2p->_next_recommended_sleep = 0;

	/*
	 * Initialize the synchronization layer
	 */
	sync_Config config = { 0 };
	config.num_players = num_players;
	config.input_size = input_size;
	config.callbacks = p2p->_header._callbacks;
	config.num_prediction_frames = MAX_PREDICTION_FRAMES;
	sync_Init(&p2p->_sync, &config);

	/*
	 * Initialize the Steam Networking Messages layer.
	 * local_channel is used as the virtual channel number.
	 */
	udp_ctor(&p2p->_udp);
	udp_Init(&p2p->_udp, (uint16)local_channel, p2p_OnMsg, p2p);

	p2p->_endpoints = calloc(p2p->_num_players, sizeof(UdpProtocol));
	for (int i = 0; i < p2p->_num_players; ++i) {
		UdpProtocol_ctor(&p2p->_endpoints[i]);
	}
	for (int i = 0; i < ARRAY_SIZE(p2p->_spectators); i++) {
		UdpProtocol_ctor(&p2p->_spectators[i]);
	}
	memset(p2p->_local_connect_status, 0, sizeof(p2p->_local_connect_status));
	for (int i = 0; i < ARRAY_SIZE(p2p->_local_connect_status); i++) {
		p2p->_local_connect_status[i].last_frame = -1;
	}

	/*
	 * Preload the ROM
	 */
	p2p->_header._callbacks.begin_game(gamename);
}

/*
 * p2p_AddRemotePlayerSteam --
 *
 * Add a remote player identified by their Steam ID.
 */
void p2p_AddRemotePlayerSteam(Peer2PeerBackend *p2p, uint64 steam_id, int queue)
{
	p2p->_synchronizing = true;

	/* Register as a known peer for auto-accept in the connection layer */
	conn_add_known_peer(steam_id);

	conn_Address peer_addr = conn_address_from_steam_id(steam_id);

	UdpProtocol_Init(&p2p->_endpoints[queue], &p2p->_udp, queue, peer_addr, p2p->_local_connect_status);
	UdpProtocol_SetDisconnectTimeout(&p2p->_endpoints[queue], p2p->_disconnect_timeout);
	UdpProtocol_SetDisconnectNotifyStart(&p2p->_endpoints[queue], p2p->_disconnect_notify_start);
	UdpProtocol_Synchronize(&p2p->_endpoints[queue]);
}

/*
 * p2p_AddSpectatorSteam --
 *
 * Add a spectator identified by their Steam ID.
 */
GGPOErrorCode p2p_AddSpectatorSteam(Peer2PeerBackend *p2p, uint64 steam_id)
{
	if (p2p->_num_spectators == GGPO_MAX_SPECTATORS) {
		return GGPO_ERRORCODE_TOO_MANY_SPECTATORS;
	}
	if (!p2p->_synchronizing) {
		return GGPO_ERRORCODE_INVALID_REQUEST;
	}
	int queue = p2p->_num_spectators++;

	conn_add_known_peer(steam_id);
	conn_Address peer_addr = conn_address_from_steam_id(steam_id);

	UdpProtocol_Init(&p2p->_spectators[queue], &p2p->_udp, queue + 1000, peer_addr, p2p->_local_connect_status);
	UdpProtocol_SetDisconnectTimeout(&p2p->_spectators[queue], p2p->_disconnect_timeout);
	UdpProtocol_SetDisconnectNotifyStart(&p2p->_spectators[queue], p2p->_disconnect_notify_start);
	UdpProtocol_Synchronize(&p2p->_spectators[queue]);

	return GGPO_OK;
}

#endif /* GGPO_STEAM */

GGPOErrorCode
p2p_DoPoll(Peer2PeerBackend *p2p, int timeout)
{
	if (!sync_InRollback(&p2p->_sync)) {

		udp_OnLoopPoll(&p2p->_udp);
		for (int i = 0; i < p2p->_num_players; i++) {
			UdpProtocol_OnLoopPoll(&p2p->_endpoints[i]);
		}
		for (int i = 0; i < p2p->_num_spectators; i++) {
			UdpProtocol_OnLoopPoll(&p2p->_spectators[i]);
		}

		p2p_PollUdpProtocolEvents(p2p);

		if (!p2p->_synchronizing) {
			sync_CheckSimulation(&p2p->_sync, timeout);

			// notify all of our endpoints of their local frame number for their
			// next connection quality report
			int current_frame = sync_GetFrameCount(&p2p->_sync);
			for (int i = 0; i < p2p->_num_players; i++) {
				UdpProtocol_SetLocalFrameNumber(&p2p->_endpoints[i], current_frame);
			}

			int total_min_confirmed;
			if (p2p->_num_players <= 2) {
				total_min_confirmed = p2p_Poll2Players(p2p, current_frame);
			}
			else {
				total_min_confirmed = p2p_PollNPlayers(p2p, current_frame);
			}

			Log("last confirmed frame in p2p backend is %d.\n", total_min_confirmed);
			if (total_min_confirmed >= 0) {
				ASSERT(total_min_confirmed != INT_MAX);
				if (p2p->_num_spectators > 0) {
					while (p2p->_next_spectator_frame <= total_min_confirmed) {
						Log("pushing frame %d to spectators.\n", p2p->_next_spectator_frame);

						GameInput input;
						input.frame = p2p->_next_spectator_frame;
						input.size = p2p->_input_size * p2p->_num_players;
						sync_GetConfirmedInputs(&p2p->_sync, input.bits, p2p->_input_size * p2p->_num_players, p2p->_next_spectator_frame);
						for (int i = 0; i < p2p->_num_spectators; i++) {
							UdpProtocol_SendInput(&p2p->_spectators[i], &input);
						}
						p2p->_next_spectator_frame++;
					}
				}
				Log("setting confirmed frame in sync to %d.\n", total_min_confirmed);
				sync_SetLastConfirmedFrame(&p2p->_sync, total_min_confirmed);
			}

			// send timesync notifications if now is the proper time
			if (current_frame > p2p->_next_recommended_sleep) {
				int interval = 0;
				for (int i = 0; i < p2p->_num_players; i++) {
					interval = MAX(interval, UdpProtocol_RecommendFrameDelay(&p2p->_endpoints[i]));
				}

				if (interval > 0) {
					GGPOEvent info;
					info.code = GGPO_EVENTCODE_TIMESYNC;
					info.u.timesync.frames_ahead = interval;
					p2p->_header._callbacks.on_event(&info);
					p2p->_next_recommended_sleep = current_frame + RECOMMENDATION_INTERVAL;
				}
			}
			// XXX: this is obviously a farce...
			if (timeout) {
				// ASSERT(false);
				// Sleep(1);
			}
		}
	}
	return GGPO_OK;
}

int p2p_Poll2Players(Peer2PeerBackend *p2p, int current_frame)
{
	int i;

	// discard confirmed frames as appropriate
	int total_min_confirmed = MAX_INT;
	for (i = 0; i < p2p->_num_players; i++) {
		bool queue_connected = true;
		if (UdpProtocol_IsRunning(&p2p->_endpoints[i])) {
			int ignore;
			queue_connected = UdpProtocol_GetPeerConnectStatus(&p2p->_endpoints[i], i, &ignore);
		}
		if (!p2p->_local_connect_status[i].disconnected) {
			total_min_confirmed = MIN(p2p->_local_connect_status[i].last_frame, total_min_confirmed);
		}
		Log("  local endp: connected = %d, last_received = %d, total_min_confirmed = %d.\n", !p2p->_local_connect_status[i].disconnected, p2p->_local_connect_status[i].last_frame, total_min_confirmed);
		if (!queue_connected && !p2p->_local_connect_status[i].disconnected) {
			Log("disconnecting i %d by remote request.\n", i);
			p2p_DisconnectPlayerQueue(p2p, i, total_min_confirmed);
		}
		Log("  total_min_confirmed = %d.\n", total_min_confirmed);
	}
	return total_min_confirmed;
}

int p2p_PollNPlayers(Peer2PeerBackend *p2p, int current_frame)
{
	int i, queue, last_received;

	// discard confirmed frames as appropriate
	int total_min_confirmed = MAX_INT;
	for (queue = 0; queue < p2p->_num_players; queue++) {
		bool queue_connected = true;
		int queue_min_confirmed = MAX_INT;
		Log("considering queue %d.\n", queue);
		for (i = 0; i < p2p->_num_players; i++) {
			// we're going to do a lot of logic here in consideration of endpoint i.
			// keep accumulating the minimum confirmed point for all n*n packets and
			// throw away the rest.
			if (UdpProtocol_IsRunning(&p2p->_endpoints[i])) {
				bool connected = UdpProtocol_GetPeerConnectStatus(&p2p->_endpoints[i], queue, &last_received);

				queue_connected = queue_connected && connected;
				queue_min_confirmed = MIN(last_received, queue_min_confirmed);
				Log("  endpoint %d: connected = %d, last_received = %d, queue_min_confirmed = %d.\n", i, connected, last_received, queue_min_confirmed);
			}
			else {
				Log("  endpoint %d: ignoring... not running.\n", i);
			}
		}
		// merge in our local status only if we're still connected!
		if (!p2p->_local_connect_status[queue].disconnected) {
			queue_min_confirmed = MIN(p2p->_local_connect_status[queue].last_frame, queue_min_confirmed);
		}
		Log("  local endp: connected = %d, last_received = %d, queue_min_confirmed = %d.\n", !p2p->_local_connect_status[queue].disconnected, p2p->_local_connect_status[queue].last_frame, queue_min_confirmed);

		if (queue_connected) {
			total_min_confirmed = MIN(queue_min_confirmed, total_min_confirmed);
		}
		else {
			// check to see if this disconnect notification is further back than we've been before.  If
			// so, we need to re-adjust.  This can happen when we detect our own disconnect at frame n
			// and later receive a disconnect notification for frame n-1.
			if (!p2p->_local_connect_status[queue].disconnected || p2p->_local_connect_status[queue].last_frame > queue_min_confirmed) {
				Log("disconnecting queue %d by remote request.\n", queue);
				p2p_DisconnectPlayerQueue(p2p, queue, queue_min_confirmed);
			}
		}
		Log("  total_min_confirmed = %d.\n", total_min_confirmed);
	}
	return total_min_confirmed;
}


GGPOErrorCode
p2p_AddPlayer(Peer2PeerBackend *p2p, GGPOPlayer* player,
	GGPOPlayerHandle* handle)
{
	if (player->type == GGPO_PLAYERTYPE_SPECTATOR) {
#if defined(GGPO_STEAM)
		return p2p_AddSpectatorSteam(p2p, player->u.steam_remote.steam_id);
#else
		return p2p_AddSpectator(p2p, player->u.remote.ip_address, player->u.remote.port);
#endif
	}

	int queue = player->player_num - 1;
	if (player->player_num < 1 || player->player_num > p2p->_num_players) {
		return GGPO_ERRORCODE_PLAYER_OUT_OF_RANGE;
	}
	*handle = p2p_QueueToPlayerHandle(p2p, queue);

	if (player->type == GGPO_PLAYERTYPE_REMOTE) {
#if defined(GGPO_STEAM)
		p2p_AddRemotePlayerSteam(p2p, player->u.steam_remote.steam_id, queue);
#else
		p2p_AddRemotePlayer(p2p, player->u.remote.ip_address, player->u.remote.port, queue);
#endif
	}
	return GGPO_OK;
}

GGPOErrorCode
p2p_AddLocalInput(Peer2PeerBackend *p2p, GGPOPlayerHandle player,
	void* values,
	int size)
{
	int queue;
	GameInput input;
	GGPOErrorCode result;

	if (sync_InRollback(&p2p->_sync)) {
		return GGPO_ERRORCODE_IN_ROLLBACK;
	}
	if (p2p->_synchronizing) {
		return GGPO_ERRORCODE_NOT_SYNCHRONIZED;
	}

	result = p2p_PlayerHandleToQueue(p2p, player, &queue);
	if (!GGPO_SUCCEEDED(result)) {
		return result;
	}

	gameinput_init(&input, -1, (char*)values, size);

	// Feed the input for the current frame into the synchronzation layer.
	if (!sync_AddLocalInput(&p2p->_sync, queue, &input)) {
		return GGPO_ERRORCODE_PREDICTION_THRESHOLD;
	}

	if (input.frame != GAMEINPUT_NULL_FRAME) { // xxx: <- comment why this is the case
		// Update the local connect status state to indicate that we've got a
		// confirmed local frame for this player.  this must come first so it
		// gets incorporated into the next packet we send.

		Log("setting local connect status for local queue %d to %d", queue, input.frame);
		p2p->_local_connect_status[queue].last_frame = input.frame;

		// Send the input to all the remote players.
		for (int i = 0; i < p2p->_num_players; i++) {
			if (UdpProtocol_IsInitialized(&p2p->_endpoints[i])) {
				UdpProtocol_SendInput(&p2p->_endpoints[i], &input);
			}
		}
	}

	return GGPO_OK;
}


GGPOErrorCode
p2p_SyncInput(Peer2PeerBackend *p2p, void* values,
	int size,
	int* disconnect_flags)
{
	int flags;

	// Wait until we've started to return inputs.
	if (p2p->_synchronizing) {
		return GGPO_ERRORCODE_NOT_SYNCHRONIZED;
	}
	flags = sync_SynchronizeInputs(&p2p->_sync, values, size);
	if (disconnect_flags) {
		*disconnect_flags = flags;
	}
	return GGPO_OK;
}

GGPOErrorCode
p2p_IncrementFrame(Peer2PeerBackend *p2p)
{
	Log("End of frame (%d)...\n", sync_GetFrameCount(&p2p->_sync));
	sync_IncrementFrame(&p2p->_sync);
	p2p_DoPoll(p2p, 0);
	p2p_PollSyncEvents(p2p);

	return GGPO_OK;
}


void
p2p_PollSyncEvents(Peer2PeerBackend *p2p)
{
	sync_Event e;
	while (sync_GetEvent(&p2p->_sync, &e)) {
		p2p_OnSyncEvent(p2p, &e);
	}
	return;
}

void
p2p_PollUdpProtocolEvents(Peer2PeerBackend *p2p)
{
	udp_protocol_Event evt;
	for (int i = 0; i < p2p->_num_players; i++) {
		while (UdpProtocol_GetEvent(&p2p->_endpoints[i], &evt)) {
			p2p_OnUdpProtocolPeerEvent(p2p, &evt, i);
		}
	}
	for (int i = 0; i < p2p->_num_spectators; i++) {
		while (UdpProtocol_GetEvent(&p2p->_spectators[i], &evt)) {
			p2p_OnUdpProtocolSpectatorEvent(p2p, &evt, i);
		}
	}
}

void
p2p_OnUdpProtocolPeerEvent(Peer2PeerBackend *p2p, udp_protocol_Event* evt, int queue)
{
	p2p_OnUdpProtocolEvent(p2p, evt, p2p_QueueToPlayerHandle(p2p, queue));
	switch (evt->type) {
	case UdpProtocol_Event_Input:
		if (!p2p->_local_connect_status[queue].disconnected) {
			int current_remote_frame = p2p->_local_connect_status[queue].last_frame;
			int new_remote_frame = evt->u.input.input.frame;
			ASSERT(current_remote_frame == -1 || new_remote_frame == (current_remote_frame + 1));

			sync_AddRemoteInput(&p2p->_sync, queue, &evt->u.input.input);
			// Notify the other endpoints which frame we received from a peer
			Log("setting remote connect status for queue %d to %d\n", queue, evt->u.input.input.frame);
			p2p->_local_connect_status[queue].last_frame = evt->u.input.input.frame;
		}
		break;

	case UdpProtocol_Event_Disconnected:
		p2p_DisconnectPlayer(p2p, p2p_QueueToPlayerHandle(p2p, queue));
		break;


	case UdpProtocol_Event_Unknown:
	case UdpProtocol_Event_Connected:
	case UdpProtocol_Event_Synchronizing:
	case UdpProtocol_Event_Synchronzied:
	case UdpProtocol_Event_NetworkInterrupted:
	case UdpProtocol_Event_NetworkResumed:
		break;
	}
}


void
p2p_OnUdpProtocolSpectatorEvent(Peer2PeerBackend *p2p, udp_protocol_Event* evt, int queue)
{
	GGPOPlayerHandle handle = p2p_QueueToSpectatorHandle(p2p, queue);
	p2p_OnUdpProtocolEvent(p2p, evt, handle);

	GGPOEvent info;

	if (evt->type == UdpProtocol_Event_Disconnected) {
		UdpProtocol_Disconnect(&p2p->_spectators[queue]);

		info.code = GGPO_EVENTCODE_DISCONNECTED_FROM_PEER;
		info.u.disconnected.player = handle;
		p2p->_header._callbacks.on_event(&info);
	}
}

void
p2p_OnUdpProtocolEvent(Peer2PeerBackend *p2p, udp_protocol_Event* evt, GGPOPlayerHandle handle)
{
	GGPOEvent info;

	switch (evt->type) {
	case UdpProtocol_Event_Connected:
		info.code = GGPO_EVENTCODE_CONNECTED_TO_PEER;
		info.u.connected.player = handle;
		p2p->_header._callbacks.on_event(&info);
		break;
	case UdpProtocol_Event_Synchronizing:
		info.code = GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER;
		info.u.synchronizing.player = handle;
		info.u.synchronizing.count = evt->u.synchronizing.count;
		info.u.synchronizing.total = evt->u.synchronizing.total;
		p2p->_header._callbacks.on_event(&info);
		break;
	case UdpProtocol_Event_Synchronzied:
		info.code = GGPO_EVENTCODE_SYNCHRONIZED_WITH_PEER;
		info.u.synchronized.player = handle;
		p2p->_header._callbacks.on_event(&info);

		p2p_CheckInitialSync(p2p);
		break;

	case UdpProtocol_Event_NetworkInterrupted:
		info.code = GGPO_EVENTCODE_CONNECTION_INTERRUPTED;
		info.u.connection_interrupted.player = handle;
		info.u.connection_interrupted.disconnect_timeout = evt->u.network_interrupted.disconnect_timeout;
		p2p->_header._callbacks.on_event(&info);
		break;

	case UdpProtocol_Event_NetworkResumed:
		info.code = GGPO_EVENTCODE_CONNECTION_RESUMED;
		info.u.connection_resumed.player = handle;
		p2p->_header._callbacks.on_event(&info);
		break;
	case UdpProtocol_Event_Unknown:
	case UdpProtocol_Event_Input:
	case UdpProtocol_Event_Disconnected:
		// default case
		break;
	}
}

/*
 * Called only as the result of a local decision to disconnect.  The remote
 * decisions to disconnect are a result of us parsing the peer_connect_settings
 * blob in every endpoint periodically.
 */
GGPOErrorCode
p2p_DisconnectPlayer(Peer2PeerBackend *p2p, GGPOPlayerHandle player)
{
	int queue;
	GGPOErrorCode result;

	result = p2p_PlayerHandleToQueue(p2p, player, &queue);
	if (!GGPO_SUCCEEDED(result)) {
		return result;
	}

	if (p2p->_local_connect_status[queue].disconnected) {
		return GGPO_ERRORCODE_PLAYER_DISCONNECTED;
	}

	if (!UdpProtocol_IsInitialized(&p2p->_endpoints[queue])) {
		int current_frame = sync_GetFrameCount(&p2p->_sync);
		// xxx: we should be tracking who the local player is, but for now assume
		// that if the endpoint is not initalized, this must be the local player.
		Log("Disconnecting local player %d at frame %d by user request.\n", queue, p2p->_local_connect_status[queue].last_frame);
		for (int i = 0; i < p2p->_num_players; i++) {
			if (UdpProtocol_IsInitialized(&p2p->_endpoints[i])) {
				p2p_DisconnectPlayerQueue(p2p, i, current_frame);
			}
		}
	}
	else {
		Log("Disconnecting queue %d at frame %d by user request.\n", queue, p2p->_local_connect_status[queue].last_frame);
		p2p_DisconnectPlayerQueue(p2p, queue, p2p->_local_connect_status[queue].last_frame);
	}
	return GGPO_OK;
}

void
p2p_DisconnectPlayerQueue(Peer2PeerBackend *p2p, int queue, int syncto)
{
	GGPOEvent info;
	int framecount = sync_GetFrameCount(&p2p->_sync);

	UdpProtocol_Disconnect(&p2p->_endpoints[queue]);

	Log("Changing queue %d local connect status for last frame from %d to %d on disconnect request (current: %d).\n",
		queue, p2p->_local_connect_status[queue].last_frame, syncto, framecount);

	p2p->_local_connect_status[queue].disconnected = 1;
	p2p->_local_connect_status[queue].last_frame = syncto;

	if (syncto < framecount) {
		Log("adjusting simulation to account for the fact that %d disconnected @ %d.\n", queue, syncto);
		sync_AdjustSimulation(&p2p->_sync, syncto);
		Log("finished adjusting simulation.\n");
	}

	info.code = GGPO_EVENTCODE_DISCONNECTED_FROM_PEER;
	info.u.disconnected.player = p2p_QueueToPlayerHandle(p2p, queue);
	p2p->_header._callbacks.on_event(&info);

	p2p_CheckInitialSync(p2p);
}


GGPOErrorCode
p2p_GetNetworkStats(Peer2PeerBackend *p2p, GGPONetworkStats* stats, GGPOPlayerHandle player)
{
	int queue;
	GGPOErrorCode result;

	result = p2p_PlayerHandleToQueue(p2p, player, &queue);
	if (!GGPO_SUCCEEDED(result)) {
		return result;
	}

	memset(stats, 0, sizeof * stats);
	UdpProtocol_GetNetworkStats(&p2p->_endpoints[queue], stats);

	return GGPO_OK;
}

GGPOErrorCode
p2p_SetFrameDelay(Peer2PeerBackend *p2p, GGPOPlayerHandle player, int delay)
{
	int queue;
	GGPOErrorCode result;

	result = p2p_PlayerHandleToQueue(p2p, player, &queue);
	if (!GGPO_SUCCEEDED(result)) {
		return result;
	}
	sync_SetFrameDelay(&p2p->_sync, queue, delay);
	return GGPO_OK;
}

GGPOErrorCode
p2p_SetDisconnectTimeout(Peer2PeerBackend *p2p, int timeout)
{
	p2p->_disconnect_timeout = timeout;
	for (int i = 0; i < p2p->_num_players; i++) {
		if (UdpProtocol_IsInitialized(&p2p->_endpoints[i])) {
			UdpProtocol_SetDisconnectTimeout(&p2p->_endpoints[i], p2p->_disconnect_timeout);
		}
	}
	return GGPO_OK;
}

GGPOErrorCode
p2p_SetDisconnectNotifyStart(Peer2PeerBackend *p2p, int timeout)
{
	p2p->_disconnect_notify_start = timeout;
	for (int i = 0; i < p2p->_num_players; i++) {
		if (UdpProtocol_IsInitialized(&p2p->_endpoints[i])) {
			UdpProtocol_SetDisconnectNotifyStart(&p2p->_endpoints[i], p2p->_disconnect_notify_start);
		}
	}
	return GGPO_OK;
}

GGPOErrorCode
p2p_PlayerHandleToQueue(Peer2PeerBackend *p2p, GGPOPlayerHandle player, int* queue)
{
	int offset = ((int)player - 1);
	if (offset < 0 || offset >= p2p->_num_players) {
		return GGPO_ERRORCODE_INVALID_PLAYER_HANDLE;
	}
	*queue = offset;
	return GGPO_OK;
}


static void p2p_OnMsg(conn_Address from, UdpMsg* msg, int len, void* user_data)
{
	Peer2PeerBackend* backend = (Peer2PeerBackend*)user_data;
	for (int i = 0; i < backend->_num_players; i++) {
		if (UdpProtocol_HandlesMsg(&backend->_endpoints[i], from, msg)) {
			UdpProtocol_OnMsg(&backend->_endpoints[i], msg, len);
			return;
		}
	}
	for (int i = 0; i < backend->_num_spectators; i++) {
		if (UdpProtocol_HandlesMsg(&backend->_spectators[i], from, msg)) {
			UdpProtocol_OnMsg(&backend->_spectators[i], msg, len);
			return;
		}
	}
}

void
p2p_CheckInitialSync(Peer2PeerBackend *p2p)
{
	int i;

	if (p2p->_synchronizing) {
		// Check to see if everyone is now synchronized.  If so,
		// go ahead and tell the client that we're ok to accept input.
		for (i = 0; i < p2p->_num_players; i++) {
			// xxx: IsInitialized() must go... we're actually using it as a proxy for "represents the local player"
			if (UdpProtocol_IsInitialized(&p2p->_endpoints[i]) && !UdpProtocol_IsSynchronized(&p2p->_endpoints[i]) && !p2p->_local_connect_status[i].disconnected) {
				return;
			}
		}
		for (i = 0; i < p2p->_num_spectators; i++) {
			if (UdpProtocol_IsInitialized(&p2p->_spectators[i]) && !UdpProtocol_IsSynchronized(&p2p->_spectators[i])) {
				return;
			}
		}

		GGPOEvent info;
		info.code = GGPO_EVENTCODE_RUNNING;
		p2p->_header._callbacks.on_event(&info);
		p2p->_synchronizing = false;
	}
}
