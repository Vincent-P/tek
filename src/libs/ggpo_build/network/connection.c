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

#if defined(_WINDOWS)
#include <WinSock2.h>
#else
#include "sys/socket.h"
#include <fcntl.h> // to set nonblocking socket
#include <arpa/inet.h> // htonl
#endif

struct _conn_Socket
{
	// empty, socket descriptor is stored directly in the pointer to this struct.
	int unused;
};

struct _conn_Address
{
	struct sockaddr_in sa;
};

struct _conn_Address g_adress_pool[32];
size_t g_address_pool_size;

conn_Socket conn_open(uint16 port)
{
	// Create socket
#if defined(_WINDOWS)
	SOCKET s = INVALID_SOCKET;
#else
	int s = 0;
#endif
	s = socket(AF_INET, SOCK_DGRAM, 0);
	int optval = 1;
	int iresult = 0;
	iresult = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));
	ASSERT(iresult == 0);
	LINGER dont_linger = { 0 };
	iresult = setsockopt(s, SOL_SOCKET, SO_LINGER, (const char*)&dont_linger, sizeof(dont_linger));
	// int error = WSAGetLastError();
	// ASSERT(iresult == 0);

	// Set it to non-blocking
#if defined(_WINDOWS)
	u_long iMode = 1;
	iresult = ioctlsocket(s, FIONBIO, &iMode);
	ASSERT(iresult == 0);
#else
	int flags = fcntl(s, F_GETFL, 0);
	ASSERT(flags != -1);
	flags = (flags | O_NONBLOCK);
	fcntl(s, F_SETFL, flags);
#endif

	// Bind it to the specified port
	struct sockaddr_in sin = { 0 };
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(port);
	if (bind(s, (struct sockaddr*)&sin, sizeof sin) < 0) {
#if defined(_WINDOWS)
		closesocket(s);
#else
		close(s);
#endif
		return NULL;
	}

	Log("Udp bound to port: %d.\n", port);
	return (conn_Socket)(uptr)s;
}

void conn_close(conn_Socket socket)
{
#if defined(_WINDOWS)
	SOCKET s = (uptr)socket;
	closesocket(s);
#else
	int s = (int)(uptr)socket;
	close(s);
#endif
}

void conn_send(conn_Socket socket, conn_Address remote, void const* data, uint32 size, int flags)
{
#if defined(_WINDOWS)
	SOCKET s = (uptr)socket;
#else
	int s = (int)(uptr)socket;
#endif

	// NOTE: sockaddr_in and sockaddr have the same length by design.
	int res = sendto(s, data, size, flags, (struct sockaddr*)&remote->sa, sizeof(remote->sa));
	if (res < 0) {
#if defined(_WINDOWS)
		DWORD err = WSAGetLastError();
		Log("unknown error in sendto (erro: %d  wsaerr: %d).\n", res, err);
#endif
		ASSERT(false && "Unknown error in sendto");
	}
	char dst_ip[1024];
	Log("sent packet length %d to %s:%d (ret:%d).\n",
		size,
		inet_ntop(AF_INET, (void*)&remote->sa, dst_ip, ARRAY_SIZE(dst_ip)),
		ntohs(remote->sa.sin_port),
		res);
}

static void conn_fill_out_address(struct sockaddr_in const* sender_addr, conn_Address* out_address)
{
	for (size_t i = 0; i < g_address_pool_size; ++i) {
		if (memcmp(sender_addr, &g_adress_pool[i].sa, sizeof(struct sockaddr_in)) == 0) {
			*out_address = &g_adress_pool[i];
			return;
		}
	}


	ASSERT(g_address_pool_size < ARRAY_SIZE(g_adress_pool));
	size_t iaddress = g_address_pool_size++;
	g_adress_pool[iaddress].sa = *sender_addr;
	*out_address = &g_adress_pool[iaddress];
	return;
}

int conn_receive(conn_Socket socket, uint8* buf, uint32 size, conn_Address* out_address)
{
#if defined(_WINDOWS)
	SOCKET s = (uptr)socket;
#else
	int s = (int)(uptr)socket;
#endif

	// TODO: handle len == 0... indicates a disconnect.
	struct sockaddr_in sender_addr = { 0 };
	*out_address = NULL;

	uint32 sender_addr_len = sizeof(sender_addr);
	int len = recvfrom(s, (char*)buf, size, 0, (struct sockaddr*)&sender_addr, &sender_addr_len);
	if (len == -1) {
#if defined(_WINDOWS)
		int error = WSAGetLastError();
		if (error != WSAEWOULDBLOCK) {
			Log("recvfrom WSAGetLastError returned %d (%x).\n", error, error);
		}
#endif
	}
	else if (len > 0) {
		char src_ip[1024];
		Log("recvfrom returned (len:%d  from:%s:%d).\n",
			len,
			inet_ntop(AF_INET, (void*)&sender_addr.sin_addr, src_ip, ARRAY_SIZE(src_ip)),
			ntohs(sender_addr.sin_port));

		conn_fill_out_address(&sender_addr, out_address);
	}

	return len;
}

bool conn_support_ip_port()
{
	return true;
}

conn_Address conn_address_from_ip_port(char* ip, uint16 port)
{
	ASSERT(g_address_pool_size < ARRAY_SIZE(g_adress_pool));

	size_t iaddress = g_address_pool_size++;
	g_adress_pool[iaddress].sa.sin_family = AF_INET; // IPv4
	g_adress_pool[iaddress].sa.sin_port = htons(port);
	inet_pton(AF_INET, ip, &g_adress_pool[iaddress].sa.sin_addr.s_addr);

	return &g_adress_pool[iaddress];
}

bool conn_addr_is_equal(conn_Address a, conn_Address b)
{
	bool address_match = a == b;
	bool content_match = (memcmp(&a->sa.sin_addr, &b->sa.sin_addr, sizeof(a->sa.sin_addr)) == 0)
		&& a->sa.sin_port == b->sa.sin_port;
	return address_match || content_match;
}
