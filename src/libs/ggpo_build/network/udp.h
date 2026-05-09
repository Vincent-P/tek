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

#ifndef _UDP_H
#define _UDP_H

#include "ggponet.h"
#include "ring_buffer.h"
#include "connection.h"

#define MAX_UDP_ENDPOINTS     16

#define MAX_UDP_PACKET_SIZE 4096

typedef struct UdpMsg UdpMsg;
typedef void (*UdpOnMsgFn)(conn_Address from, UdpMsg *msg, int len, void* user_data);


struct udp_Stats {
      int      bytes_sent;
      int      packets_sent;
      float    kbps_sent;
};
typedef struct udp_Stats udp_Stats;

struct Udp
{
   // Network transmission information
   conn_Socket         _socket;

   // state management
   void* _user_data;
   UdpOnMsgFn      _on_msg_callback;
};
typedef struct Udp Udp;

void udp_Log(const char *fmt, ...);

void udp_ctor(Udp* udp);
void udp_dtor(Udp* udp);
void udp_Init(Udp* udp, uint16 port, UdpOnMsgFn on_msg_callback, void *user_data);
void udp_SendTo(Udp* ud, char *buffer, int len, int flags, conn_Address to);
bool udp_OnLoopPoll(Udp* udp);


#endif
