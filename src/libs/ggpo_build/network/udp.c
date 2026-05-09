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
#include "udp.h"

#if 0
static SOCKET CreateSocket(uint16 bind_port, int retries)
{
}
#endif

void udp_ctor(Udp* udp)
{
	memset(udp, 0, sizeof(Udp));
	// udp->_socket = INVALID_SOCKET;
}

void udp_dtor(Udp* udp)
{
	conn_close(udp->_socket);
	// closesocket(udp->_socket);
	// udp->_socket = INVALID_SOCKET;
}

void udp_Init(Udp* udp, uint16 port, UdpOnMsgFn on_msg_callback, void* user_data)
{
	udp->_user_data = user_data;
	udp->_on_msg_callback = on_msg_callback;

	udp_Log("binding udp socket to port %d.\n", port);
	// udp->_socket = CreateSocket(port, 0);
	udp->_socket = conn_open(port);
}

void udp_SendTo(Udp* udp, char* buffer, int len, int flags, conn_Address to)
{
	conn_send(udp->_socket, to, buffer, len, flags);
	// int res = sendto(udp->_socket, buffer, len, flags, dst, destlen);
	// if (res == SOCKET_ERROR) {
	// 	DWORD err = WSAGetLastError();
	// 	Log("unknown error in sendto (erro: %d  wsaerr: %d).\n", res, err);
	// 	ASSERT(false && "Unknown error in sendto");
	// }
	// char dst_ip[1024];
	// Log("sent packet length %d to %s:%d (ret:%d).\n", len, inet_ntop(AF_INET, (void*)&to->sin_addr, dst_ip, ARRAY_SIZE(dst_ip)), ntohs(to->sin_port), res);
}

bool udp_OnLoopPoll(Udp *udp)
{
	uint8          recv_buf[MAX_UDP_PACKET_SIZE];
	conn_Address    recv_addr;
	// int            recv_addr_len;

	for (;;) {
		// recv_addr_len = sizeof(recv_addr);

		int len = conn_receive(udp->_socket, recv_buf, MAX_UDP_PACKET_SIZE, &recv_addr);
		// TODO: handle len == 0... indicates a disconnect.
		// if (len == -1) {
		//	int error = WSAGetLastError();
		//	if (error != WSAEWOULDBLOCK) {
		//		Log("recvfrom WSAGetLastError returned %d (%x).\n", error, error);
		//	}
		//	break;
		//} else if (len > 0) {
		 if (len > 0) {
			 // char src_ip[1024];
			// Log("recvfrom returned (len:%d  from:%s:%d).\n", len, inet_ntop(AF_INET, (void*)&recv_addr.sin_addr, src_ip, ARRAY_SIZE(src_ip)), ntohs(recv_addr.sin_port));
			UdpMsg* msg = (UdpMsg*)recv_buf;
			udp->_on_msg_callback(recv_addr, msg, len, udp->_user_data);
		 }
		 else {
			 break;
		 }
	}
	return true;
}

void udp_Log(const char* fmt, ...)
{
	char buf[1024];
	size_t offset;
	va_list args;

	strcpy(buf, "udp | ");
	offset = strlen(buf);
	va_start(args, fmt);
	vsnprintf(buf + offset, ARRAY_SIZE(buf) - offset - 1, fmt, args);
	buf[ARRAY_SIZE(buf) - 1] = '\0';
	Log(buf);
	va_end(args);
}
