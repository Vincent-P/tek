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
#include "connection.h"
#include "ggponet.h" // for GGPO_MAX_PLAYERS
#include "types.h"
#include "steam_api_c.h"

struct _conn_Socket
{
	int unused; // Channel number is stored in the pointer value itself.
};

struct _conn_Address
{
	SteamNetworkingIdentity identity;
};

static struct _conn_Address g_address_pool[32];
static size_t g_address_pool_size = 0;

static uint64 g_known_peers[GGPO_MAX_PLAYERS];
static int g_known_peers_count = 0;
static int g_local_channel = 0;
static bool g_steam_initialized = false;

void conn_add_known_peer(uint64 steam_id)
{
	for (int i = 0; i < g_known_peers_count; i++) {
		if (g_known_peers[i] == steam_id) {
			return;
		}
	}
	if (g_known_peers_count < GGPO_MAX_PLAYERS) {
		g_known_peers[g_known_peers_count++] = steam_id;
	}
}

static bool conn_is_known_peer(uint64 steam_id)
{
	for (int i = 0; i < g_known_peers_count; i++) {
		if (g_known_peers[i] == steam_id) {
			return true;
		}
	}
	return false;
}

static conn_Address conn_intern_identity(const SteamNetworkingIdentity* identity)
{
	for (size_t i = 0; i < g_address_pool_size; i++) {
		if (memcmp(&g_address_pool[i].identity, identity, sizeof(SteamNetworkingIdentity)) == 0) {
			return &g_address_pool[i];
		}
	}

	ASSERT(g_address_pool_size < ARRAY_SIZE(g_address_pool));
	size_t idx = g_address_pool_size++;
	g_address_pool[idx].identity = *identity;
	return &g_address_pool[idx];
}

void ggpo_steam_callback(GGPOSession* session, int callback_type, void* callback_data, int callback_datasize)
{
	if (callback_type == k_iSteamNetworkingMessagesSessionRequestCallback) {
		ASSERT(callback_datasize == sizeof(SteamNetworkingMessagesSessionRequest_t));
		SteamNetworkingMessagesSessionRequest_t* session_request = (SteamNetworkingMessagesSessionRequest_t*)callback_data;



		// if someone wants to connect, accept the session ONLY if they are in our known peers list.
		for (int i = 0; i < g_known_peers_count; i++) {
			if (session_request->m_identityRemote.m_eType == k_ESteamNetworkingIdentityType_SteamID &&
				session_request->m_identityRemote.m_steamID64 == g_known_peers[i]) {
				SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser(SteamAPI_SteamNetworkingMessages_SteamAPI(), &session_request->m_identityRemote);
			}
		}

		Log("conn_send: Accepting session with user %llu.\n", session_request->m_identityRemote.m_steamID64);
	}
	else if (callback_type == k_iSteamNetworkingSteamNetworkingMessagesSessionFailedCallback) {
		ASSERT(callback_datasize == sizeof(SteamNetworkingMessagesSessionFailed_t));
		SteamNetworkingMessagesSessionFailed_t* session_failed = (SteamNetworkingMessagesSessionFailed_t*)callback_data;

		Log("conn_send: Session failed with user %llu.\n", session_failed->m_info.m_identityRemote.m_steamID64);
	}
}

conn_Socket conn_open(uint16 port)
{
	if (!SteamAPI_SteamNetworkingMessages_SteamAPI()) {
		Log("conn_open: SteamNetworkingMessages() not available.\n");
		return NULL;
	}

	g_local_channel = (int)port;
	g_steam_initialized = true;

	Log("Steam Networking Messages initialized on channel %d.\n", g_local_channel);

	return (conn_Socket)(uptr)g_local_channel;
}

void conn_close(conn_Socket socket)
{
	if (!g_steam_initialized) {
		return;
	}

	for (int i = 0; i < g_known_peers_count; i++) {
		SteamNetworkingIdentity identity;
		memset(&identity, 0, sizeof(identity));
		identity.m_eType = k_ESteamNetworkingIdentityType_SteamID;
		identity.m_cbSize = sizeof(uint64);
		identity.m_steamID64 = g_known_peers[i];

		SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser(SteamAPI_SteamNetworkingMessages_SteamAPI(), &identity);
	}

	g_known_peers_count = 0;
	g_address_pool_size = 0;
	g_steam_initialized = false;

	Log("Steam Networking Messages connection closed.\n");
}

void conn_send(conn_Socket socket, conn_Address remote, void const* data, uint32 size, int flags)
{
	if (!g_steam_initialized || !remote) {
		Log("conn_send: not initialized or null remote.\n");
		return;
	}

	int send_flags = k_nSteamNetworkingSend_UnreliableNoNagle;
	EResult result = SteamAPI_ISteamNetworkingMessages_SendMessageToUser(
		SteamAPI_SteamNetworkingMessages_SteamAPI(),
		&remote->identity,
		data,
		size,
		send_flags,
		g_local_channel);

	{

		SteamNetConnectionInfo_t conn_info = { 0 };
		SteamNetConnectionRealTimeStatus_t status = { 0 };
		ESteamNetworkingConnectionState state = SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo(
			SteamAPI_SteamNetworkingMessages_SteamAPI(),
			&remote->identity ,
			&conn_info,
			&status);
		Log("conn_send: state %d.\n", status.m_eState);
	}

	if (result == k_EResultOK) {

		ASSERT(remote->identity.m_eType == k_ESteamNetworkingIdentityType_SteamID);
		Log("conn_send: sent message length %u to Steam ID %llu\n", size, (unsigned long long)remote->identity.m_steamID64);

	} else if (result == k_EResultNoConnection) {

		SteamNetConnectionInfo_t conn_info = { 0 };
		SteamNetConnectionRealTimeStatus_t status = { 0 };
		ESteamNetworkingConnectionState state = SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo(
			SteamAPI_SteamNetworkingMessages_SteamAPI(),
			&remote->identity ,
			&conn_info,
			&status);
		Log("conn_send: the session has failed or was closed by the peer: %s.\n", conn_info.m_szEndDebug);

	} else if (result == k_EResultInvalidParam) {
		Log("conn_send: invalid connection handle, or the individual message is too big.\n");
	} else if (result == k_EResultInvalidState) {
		Log("conn_send: connection is in an invalid state.\n");
	} else if (result == k_EResultIgnored) {
		Log("conn_send: used k_nSteamNetworkingSend_NoDelay, and the message was dropped because we were not ready to send it.\n");
	} else if (result == k_EResultLimitExceeded) {
		Log("conn_send: there was already too much data queued to be sent.\n");
	} else if (result == k_EResultConnectFailed) {


		SteamNetConnectionInfo_t conn_info = { 0 };
		SteamNetConnectionRealTimeStatus_t status = { 0 };
		ESteamNetworkingConnectionState state = SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo(
			SteamAPI_SteamNetworkingMessages_SteamAPI(),
			&remote->identity ,
			&conn_info,
			&status);

		Log("conn_send: connection failed: %s.\n", conn_info.m_szEndDebug);

	} else {
		Log("conn_send: SendMessageToUser failed with result %d.\n", result);
	}
}

int conn_receive(conn_Socket socket, uint8* buf, uint32 size, conn_Address* out_address)
{
	if (!g_steam_initialized) {
		*out_address = NULL;
		return 0;
	}

	*out_address = NULL;

	SteamNetworkingMessage_t* messages[1];
	int num_messages = SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel(
		SteamAPI_SteamNetworkingMessages_SteamAPI(),
		g_local_channel,
		messages,
		1);

	if (num_messages <= 0) {
		return 0;
	}

	SteamNetworkingMessage_t* msg = messages[0];
	int len = msg->m_cbSize;

	if (len > 0 && (uint32)len <= size) {
		memcpy(buf, msg->m_pData, len);

		*out_address = conn_intern_identity(&msg->m_identityPeer);

		if (msg->m_identityPeer.m_eType == k_ESteamNetworkingIdentityType_SteamID) {
			Log("received message length %d from Steam ID %llu.\n",
				len,
				(unsigned long long)msg->m_identityPeer.m_steamID64);
		}
	}
	else if (len > (int)size) {
		Log("conn_receive: message too large (%d > %u).\n", len, size);
		len = -1;
	}

	SteamAPI_SteamNetworkingMessage_t_Release(msg);
	return len;
}

bool conn_support_ip_port()
{
	return false;
}

conn_Address conn_address_from_steam_id(uint64 steam_id)
{
	ASSERT(g_address_pool_size < ARRAY_SIZE(g_address_pool));
	ASSERT(steam_id != 0 && "Invalid Steam ID (zero)");

	SteamNetworkingIdentity identity;
	memset(&identity, 0, sizeof(identity));
	identity.m_eType = k_ESteamNetworkingIdentityType_SteamID;
	identity.m_cbSize = sizeof(uint64);
	identity.m_steamID64 = steam_id;

	Log("conn_address_from_steam_id: created address for %llu.\n",
		(unsigned long long)steam_id);

	return conn_intern_identity(&identity);
}

bool conn_addr_is_equal(conn_Address a, conn_Address b)
{
	if (a == b) {
		return true;
	}
	if (a == NULL || b == NULL) {
		return false;
	}
	return memcmp(&a->identity, &b->identity, sizeof(SteamNetworkingIdentity)) == 0;
}
