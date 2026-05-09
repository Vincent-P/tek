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

#include "spectator.h"


static void SpectatorBackend_OnMsg(conn_Address from, UdpMsg* msg, int len, void* user_data);

#ifndef GGPO_STEAM

void spec_ctor(SpectatorBackend* spec, GGPOSessionCallbacks* cb,
	const char* gamename,
	uint16 localport,
	int num_players,
	int input_size,
	char* hostip,
	uint16 hostport)
{

	spec->_num_players = num_players;
	spec->_input_size = input_size;
	spec->_next_input_to_send = 0;

	spec->_header._session_type = SESSION_SPECTATOR;
	spec->_header._callbacks = *cb;
	spec->_synchronizing = true;

	for (int i = 0; i < ARRAY_SIZE(spec->_inputs); i++) {
		spec->_inputs[i].frame = -1;
	}

	/*
	 * Initialize the UDP port
	 */
	udp_ctor(&spec->_udp);
	udp_Init(&spec->_udp, localport, SpectatorBackend_OnMsg, spec);

	/*
	 * Init the host endpoint
	 */
	ASSERT(conn_support_ip_port());
	conn_Address peer_addr =  conn_address_from_ip_port(hostip, hostport);

	UdpProtocol_ctor(&spec->_host);
	UdpProtocol_Init(&spec->_host, &spec->_udp, 0, peer_addr, NULL);
	UdpProtocol_Synchronize(&spec->_host);

	/*
	 * Preload the ROM
	 */
	spec->_header._callbacks.begin_game(gamename);
}

#endif /* !GGPO_STEAM */

#if defined(GGPO_STEAM)

/*
 * spec_ctor_steam --
 *
 * Constructor for spectator sessions using Steam Networking Messages.
 * The host is identified by their Steam ID instead of IP:port.
 */
void spec_ctor_steam(SpectatorBackend* spec, GGPOSessionCallbacks* cb,
	const char* gamename,
	int local_channel,
	int num_players,
	int input_size,
	uint64 host_steam_id)
{
	spec->_num_players = num_players;
	spec->_input_size = input_size;
	spec->_next_input_to_send = 0;

	spec->_header._session_type = SESSION_SPECTATOR;
	spec->_header._callbacks = *cb;
	spec->_synchronizing = true;

	for (int i = 0; i < ARRAY_SIZE(spec->_inputs); i++) {
		spec->_inputs[i].frame = -1;
	}

	/*
	 * Initialize the Steam Networking Messages layer
	 */
	udp_ctor(&spec->_udp);
	udp_Init(&spec->_udp, (uint16)local_channel, SpectatorBackend_OnMsg, spec);

	/*
	 * Init the host endpoint using Steam ID
	 */
	conn_add_known_peer(host_steam_id);
	conn_Address peer_addr = conn_address_from_steam_id(host_steam_id);

	UdpProtocol_ctor(&spec->_host);
	UdpProtocol_Init(&spec->_host, &spec->_udp, 0, peer_addr, NULL);
	UdpProtocol_Synchronize(&spec->_host);

	/*
	 * Preload the ROM
	 */
	spec->_header._callbacks.begin_game(gamename);
}

#endif /* GGPO_STEAM */

void spec_dtor(SpectatorBackend* spec)
{
	udp_dtor(&spec->_udp);
}

GGPOErrorCode
spec_DoPoll(SpectatorBackend* spec, int timeout)
{
	udp_OnLoopPoll(&spec->_udp);
	UdpProtocol_OnLoopPoll(&spec->_host);


	spec_PollUdpProtocolEvents(spec);
	return GGPO_OK;
}

GGPOErrorCode
spec_SyncInput(SpectatorBackend* spec, void* values,
	int size,
	int* disconnect_flags)
{
	// Wait until we've started to return inputs.
	if (spec->_synchronizing) {
		return GGPO_ERRORCODE_NOT_SYNCHRONIZED;
	}

	GameInput const input = spec->_inputs[spec->_next_input_to_send % SPECTATOR_FRAME_BUFFER_SIZE];
	if (input.frame < spec->_next_input_to_send) {
		// Haven't received the input from the host yet.  Wait
		return GGPO_ERRORCODE_PREDICTION_THRESHOLD;
	}
	if (input.frame > spec->_next_input_to_send) {
		// The host is way way way far ahead of the spectator.  How'd this
		// happen?  Anyway, the input we need is gone forever.
		return GGPO_ERRORCODE_GENERAL_FAILURE;
	}

	ASSERT(size >= spec->_input_size * spec->_num_players);
	memcpy(values, input.bits, spec->_input_size * spec->_num_players);
	if (disconnect_flags) {
		*disconnect_flags = 0; // xxx: should get them from the host!
	}
	spec->_next_input_to_send++;

	return GGPO_OK;
}

GGPOErrorCode
spec_IncrementFrame(SpectatorBackend* spec)
{
	Log("End of frame (%d)...\n", spec->_next_input_to_send - 1);
	spec_DoPoll(spec, 0);
	spec_PollUdpProtocolEvents(spec);

	return GGPO_OK;
}

void
spec_PollUdpProtocolEvents(SpectatorBackend* spec)
{
	udp_protocol_Event evt;
	while (UdpProtocol_GetEvent(&spec->_host, &evt)) {
		spec_OnUdpProtocolEvent(spec, &evt);
	}
}

void
spec_OnUdpProtocolEvent(SpectatorBackend* spec, udp_protocol_Event* evt)
{
	GGPOEvent info;

	switch (evt->type) {
	case UdpProtocol_Event_Connected:
		info.code = GGPO_EVENTCODE_CONNECTED_TO_PEER;
		info.u.connected.player = 0;
		spec->_header._callbacks.on_event(&info);
		break;
	case UdpProtocol_Event_Synchronizing:
		info.code = GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER;
		info.u.synchronizing.player = 0;
		info.u.synchronizing.count = evt->u.synchronizing.count;
		info.u.synchronizing.total = evt->u.synchronizing.total;
		spec->_header._callbacks.on_event(&info);
		break;
	case UdpProtocol_Event_Synchronzied:
		if (spec->_synchronizing) {
			info.code = GGPO_EVENTCODE_SYNCHRONIZED_WITH_PEER;
			info.u.synchronized.player = 0;
			spec->_header._callbacks.on_event(&info);

			info.code = GGPO_EVENTCODE_RUNNING;
			spec->_header._callbacks.on_event(&info);
			spec->_synchronizing = false;
		}
		break;

	case UdpProtocol_Event_NetworkInterrupted:
		info.code = GGPO_EVENTCODE_CONNECTION_INTERRUPTED;
		info.u.connection_interrupted.player = 0;
		info.u.connection_interrupted.disconnect_timeout = evt->u.network_interrupted.disconnect_timeout;
		spec->_header._callbacks.on_event(&info);
		break;

	case UdpProtocol_Event_NetworkResumed:
		info.code = GGPO_EVENTCODE_CONNECTION_RESUMED;
		info.u.connection_resumed.player = 0;
		spec->_header._callbacks.on_event(&info);
		break;

	case UdpProtocol_Event_Disconnected:
		info.code = GGPO_EVENTCODE_DISCONNECTED_FROM_PEER;
		info.u.disconnected.player = 0;
		spec->_header._callbacks.on_event(&info);
		break;

	case UdpProtocol_Event_Input:
		GameInput const input = evt->u.input.input;

		UdpProtocol_SetLocalFrameNumber(&spec->_host, input.frame);
		UdpProtocol_SendInputAck(&spec->_host);
		spec->_inputs[input.frame % SPECTATOR_FRAME_BUFFER_SIZE] = input;
		break;

	case UdpProtocol_Event_Unknown:
		break;
	}
}

static void SpectatorBackend_OnMsg(conn_Address from, UdpMsg* msg, int len, void* user_data)
{
	SpectatorBackend* backend = (SpectatorBackend*)user_data;
	if (UdpProtocol_HandlesMsg(&backend->_host, from, msg)) {
		UdpProtocol_OnMsg(&backend->_host, msg, len);
	}
}
