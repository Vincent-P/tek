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
#pragma once
#include "types.h"

typedef struct _conn_Socket* conn_Socket;
typedef struct _conn_Address* conn_Address;

conn_Socket conn_open(uint16 port);
void conn_close(conn_Socket socket);

void conn_send(conn_Socket socket, conn_Address remote, void const *data, uint32 size, int flags);
int conn_receive(conn_Socket socket, uint8 *buf, uint32 size, conn_Address *out_address);

bool conn_support_ip_port();
bool conn_addr_is_equal(conn_Address a, conn_Address b);

#if defined(GGPO_STEAM)
conn_Address conn_address_from_steam_id(uint64 steam_id);
void conn_add_known_peer(uint64 steam_id);
#else
conn_Address conn_address_from_ip_port(char *ip, uint16 port);
#endif
