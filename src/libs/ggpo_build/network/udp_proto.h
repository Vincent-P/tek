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

#ifndef _UDP_PROTO_H_
#define _UDP_PROTO_H_

#include "udp.h"
#include "game_input.h"
#include "timesync.h"
#include "ggponet.h"
#include "ring_buffer.h"
#include "udp_msg.h"


struct udp_protocol_Stats {
		int                 ping;
		int                 remote_frame_advantage;
		int                 local_frame_advantage;
		int                 send_queue_len;
		udp_Stats          udp;
};
typedef struct udp_protocol_Stats udp_protocol_Stats;

enum udp_protocol_EventType {
			UdpProtocol_Event_Unknown = -1,
			UdpProtocol_Event_Connected,
			UdpProtocol_Event_Synchronizing,
			UdpProtocol_Event_Synchronzied,
			UdpProtocol_Event_Input,
			UdpProtocol_Event_Disconnected,
			UdpProtocol_Event_NetworkInterrupted,
			UdpProtocol_Event_NetworkResumed,
};
typedef enum udp_protocol_EventType udp_protocol_EventType;

struct udp_protocol_Event {
		udp_protocol_EventType      type;
		union {
			struct {
				GameInput   input;
			} input;
			struct {
				int         total;
				int         count;
			} synchronizing;
			struct {
				int         disconnect_timeout;
			} network_interrupted;
		} u;
};
typedef struct udp_protocol_Event udp_protocol_Event;

enum udp_protocol_State
{
		UdpProtocol_Syncing,
		UdpProtocol_Synchronzied,
		UdpProtocol_Running,
		UdpProtocol_Disconnected
};
typedef enum udp_protocol_State udp_protocol_State;

struct udp_protocol_QueueEntry
{
		int         queue_time;
		conn_Address dest_addr;
		UdpMsg* msg;
};
typedef struct udp_protocol_QueueEntry udp_protocol_QueueEntry;

struct UdpProtocol
{
	/*
	 * Network transmission information
	 */
	Udp* _udp;
	conn_Address    _peer_addr;
	uint16         _magic_number;
	int            _queue;
	uint16         _remote_magic_number;
	bool           _connected;
	int            _send_latency;
	int            _oop_percent;
	struct {
		int         send_time;
		conn_Address dest_addr;
		UdpMsg* msg;
	}              _oo_packet;
	RingBuffer _send_queue_ring;
	udp_protocol_QueueEntry _send_queue[64];

	/*
	 * Stats
	 */
	int            _round_trip_time;
	int            _packets_sent;
	int            _bytes_sent;
	int            _kbps_sent;
	int            _stats_start_time;

	/*
	 * The state machine
	 */
	UdpMsg_connect_status* _local_connect_status;
	UdpMsg_connect_status _peer_connect_status[UDP_MSG_MAX_PLAYERS];

	udp_protocol_State          _current_state;
	union {
		struct {
			uint32   roundtrips_remaining;
			uint32   random;
		} sync;
		struct {
			uint32   last_quality_report_time;
			uint32   last_network_stats_interval;
			uint32   last_input_packet_recv_time;
		} running;
	} _state;

	/*
	 * Fairness.
	 */
	int               _local_frame_advantage;
	int               _remote_frame_advantage;

	/*
	 * Packet loss...
	 */
	RingBuffer  _pending_output_ring;
	GameInput  _pending_output[64];
	GameInput                  _last_received_input;
	GameInput                  _last_sent_input;
	GameInput                  _last_acked_input;
	unsigned int               _last_send_time;
	unsigned int               _last_recv_time;
	unsigned int               _shutdown_timeout;
	unsigned int               _disconnect_event_sent;
	unsigned int               _disconnect_timeout;
	unsigned int               _disconnect_notify_start;
	bool                       _disconnect_notify_sent;

	uint16                     _next_send_seq;
	uint16                     _next_recv_seq;

	/*
	 * Rift synchronization.
	 */
	TimeSync                   _timesync;

	/*
	 * Event queue
	 */
	RingBuffer  _event_queue_ring;
	udp_protocol_Event  _event_queue[64];
};
typedef struct UdpProtocol UdpProtocol;


	void UdpProtocol_ctor(UdpProtocol *protocol);
	void UdpProtocol_dtor(UdpProtocol *protocol);

	bool UdpProtocol_OnLoopPoll(UdpProtocol *protocol);


	void UdpProtocol_Init(UdpProtocol *protocol, Udp* udp, int queue, conn_Address addr, UdpMsg_connect_status* status);

	void UdpProtocol_Synchronize(UdpProtocol *protocol);
	bool UdpProtocol_GetPeerConnectStatus(UdpProtocol *protocol, int id, int* frame);
	inline bool UdpProtocol_IsInitialized(UdpProtocol *protocol) { return protocol->_udp != NULL; }
	inline bool UdpProtocol_IsSynchronized(UdpProtocol *protocol) { return protocol->_current_state == UdpProtocol_Running; }
	inline bool UdpProtocol_IsRunning(UdpProtocol *protocol) { return protocol->_current_state == UdpProtocol_Running; }
	void UdpProtocol_SendInput(UdpProtocol *protocol, GameInput* input);
	void UdpProtocol_SendInputAck(UdpProtocol *protocol);
	bool UdpProtocol_HandlesMsg(UdpProtocol *protocol, conn_Address from, UdpMsg* msg);
	void UdpProtocol_OnMsg(UdpProtocol *protocol, UdpMsg* msg, int len);
	void UdpProtocol_Disconnect(UdpProtocol *protocol);

	void UdpProtocol_GetNetworkStats(UdpProtocol *protocol, struct GGPONetworkStats* stats);
	bool UdpProtocol_GetEvent(UdpProtocol *protocol, udp_protocol_Event* e);
	void UdpProtocol_GGPONetworkStats(UdpProtocol *protocol, udp_protocol_Stats* stats);
	void UdpProtocol_SetLocalFrameNumber(UdpProtocol *protocol, int num);
	int UdpProtocol_RecommendFrameDelay(UdpProtocol *protocol);

	void UdpProtocol_SetDisconnectTimeout(UdpProtocol *protocol, int timeout);
	void UdpProtocol_SetDisconnectNotifyStart(UdpProtocol *protocol, int timeout);

	bool UdpProtocol_CreateSocket(UdpProtocol *protocol, int retries);
	void UdpProtocol_UpdateNetworkStats(UdpProtocol *protocol);
	void UdpProtocol_QueueEvent(UdpProtocol *protocol, const udp_protocol_Event* evt);
	void UdpProtocol_ClearSendQueue(UdpProtocol *protocol);
	void UdpProtocol_Log(UdpProtocol *protocol, const char* fmt, ...);
	void UdpProtocol_LogMsg(UdpProtocol *protocol, const char* prefix, UdpMsg* msg);
	void UdpProtocol_LogEvent(UdpProtocol *protocol, const char* prefix, const udp_protocol_Event* evt);
	void UdpProtocol_SendSyncRequest(UdpProtocol *protocol);
	void UdpProtocol_SendMsg(UdpProtocol *protocol, UdpMsg* msg);
	void UdpProtocol_PumpSendQueue(UdpProtocol *protocol);
	void UdpProtocol_DispatchMsg(UdpProtocol *protocol, uint8* buffer, int len);
	void UdpProtocol_SendPendingOutput(UdpProtocol *protocol);
#endif
