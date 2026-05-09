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
#include "udp_proto.h"
#include "bitvector.h"
#include "udp_msg.h"

#define UDP_HEADER_SIZE 28     /* Size of IP + UDP headers */
#define NUM_SYNC_PACKETS 5
#define SYNC_RETRY_INTERVAL 2000
#define SYNC_FIRST_RETRY_INTERVAL 500
#define RUNNING_RETRY_INTERVAL 200
#define KEEP_ALIVE_INTERVAL 200
#define QUALITY_REPORT_INTERVAL 1000
#define NETWORK_STATS_INTERVAL 1000
#define UDP_SHUTDOWN_TIMER 5000
#define MAX_SEQ_DISTANCE (1 << 15)



static bool UdpProtocol_OnInvalid(UdpProtocol *protocol, UdpMsg* msg, int len);
static bool UdpProtocol_OnSyncRequest(UdpProtocol *protocol, UdpMsg* msg, int len);
static bool UdpProtocol_OnSyncReply(UdpProtocol *protocol, UdpMsg* msg, int len);
static bool UdpProtocol_OnInput(UdpProtocol *protocol, UdpMsg* msg, int len);
static bool UdpProtocol_OnInputAck(UdpProtocol *protocol, UdpMsg* msg, int len);
static bool UdpProtocol_OnQualityReport(UdpProtocol *protocol, UdpMsg* msg, int len);
static bool UdpProtocol_OnQualityReply(UdpProtocol *protocol, UdpMsg* msg, int len);
static bool UdpProtocol_OnKeepAlive(UdpProtocol *protocol, UdpMsg* msg, int len);

void UdpProtocol_ctor(UdpProtocol* protocol)
{
	memset(protocol, 0, sizeof(UdpProtocol));
	protocol->_queue = -1;

	gameinput_init(&protocol->_last_sent_input, -1, NULL, 1);
	gameinput_init(&protocol->_last_received_input, -1, NULL, 1);
	gameinput_init(&protocol->_last_acked_input, -1, NULL, 1);

	memset(&protocol->_state, 0, sizeof protocol->_state);
	memset(protocol->_peer_connect_status, 0, sizeof(protocol->_peer_connect_status));
	for (int i = 0; i < ARRAY_SIZE(protocol->_peer_connect_status); i++) {
		protocol->_peer_connect_status[i].last_frame = -1;
	}
	memset(&protocol->_peer_addr, 0, sizeof protocol->_peer_addr);
	protocol->_oo_packet.msg = NULL;

	protocol->_send_latency = Platform_GetConfigInt("ggpo.network.delay");
	protocol->_oop_percent = Platform_GetConfigInt("ggpo.oop.percent");

	timesync_init(&protocol->_timesync);

	ring_ctor(&protocol->_send_queue_ring, ARRAY_SIZE(protocol->_send_queue));
	ring_ctor(&protocol->_pending_output_ring, ARRAY_SIZE(protocol->_pending_output));
	ring_ctor(&protocol->_event_queue_ring, ARRAY_SIZE(protocol->_event_queue));
}

void UdpProtocol_dtor(UdpProtocol* protocol)
{
	UdpProtocol_ClearSendQueue(protocol);
}

void UdpProtocol_Init(UdpProtocol* protocol,
	Udp* udp,
	int queue,
	conn_Address addr,
	UdpMsg_connect_status* status)
{
	protocol->_udp = udp;
	protocol->_queue = queue;
	protocol->_local_connect_status = status;

	protocol->_peer_addr = addr;
	// protocol->_peer_addr.sin_family = AF_INET;
	// protocol->_peer_addr.sin_port = htons(port);
	// inet_pton(AF_INET, ip, &protocol->_peer_addr.sin_addr.s_addr);

	do {
		protocol->_magic_number = (uint16)rand();
	} while (protocol->_magic_number == 0);
}

void UdpProtocol_SendInput(UdpProtocol* protocol, GameInput* input)
{
	if (protocol->_udp) {
		if (protocol->_current_state == UdpProtocol_Running) {
			/*
			 * Check to see if this is a good time to adjust for the rift...
			 */
			timesync_advance_frame(&protocol->_timesync, input, protocol->_local_frame_advantage, protocol->_remote_frame_advantage);

			/*
			 * Save this input packet
			 *
			 * XXX: This queue may fill up for spectators who do not ack input packets in a timely
			 * manner.  When this happens, we can either resize the queue (ug) or disconnect them
			 * (better, but still ug).  For the meantime, make this queue really big to decrease
			 * the odds of this happening...
			 */
			protocol->_pending_output[ring_push(&protocol->_pending_output_ring)] = *input;
		}
		UdpProtocol_SendPendingOutput(protocol);
	}
}

void UdpProtocol_SendPendingOutput(UdpProtocol* protocol)
{
	UdpMsg* msg = calloc(1, sizeof(UdpMsg));  udp_msg_ctor(msg, UdpMsg_Input);
	int i, j, offset = 0;
	uint8* bits;
	GameInput last;

	if (ring_size(&protocol->_pending_output_ring)) {
		last = protocol->_last_acked_input;
		bits = msg->u.input.bits;

		msg->u.input.start_frame = protocol->_pending_output[ring_front(&protocol->_pending_output_ring)].frame;
		msg->u.input.input_size = (uint8)protocol->_pending_output[ring_front(&protocol->_pending_output_ring)].size;

		ASSERT(last.frame == -1 || last.frame + 1 == msg->u.input.start_frame);
		for (j = 0; j < ring_size(&protocol->_pending_output_ring); j++) {
			GameInput current = protocol->_pending_output[ring_item(&protocol->_pending_output_ring, j)];
			if (memcmp(current.bits, last.bits, current.size) != 0) {
				ASSERT((GAMEINPUT_MAX_BYTES * GAMEINPUT_MAX_PLAYERS * 8) < (1 << BITVECTOR_NIBBLE_SIZE));
				for (i = 0; i < current.size * 8; i++) {
					ASSERT(i < (1 << BITVECTOR_NIBBLE_SIZE));
					if (gameinput_value(&current, i) != gameinput_value(&last, i)) {
						BitVector_SetBit(msg->u.input.bits, &offset);
						(gameinput_value(&current, i) ? BitVector_SetBit : BitVector_ClearBit)(bits, &offset);
						BitVector_WriteNibblet(bits, i, &offset);
					}
				}
			}
			BitVector_ClearBit(msg->u.input.bits, &offset);
			last = protocol->_last_sent_input = current;
		}
	}
	else {
		msg->u.input.start_frame = 0;
		msg->u.input.input_size = 0;
	}
	msg->u.input.ack_frame = protocol->_last_received_input.frame;
	msg->u.input.num_bits = (uint16)offset;

	msg->u.input.disconnect_requested = protocol->_current_state == UdpProtocol_Disconnected;
	if (protocol->_local_connect_status) {
		memcpy(msg->u.input.peer_connect_status, protocol->_local_connect_status, sizeof(UdpMsg_connect_status) * UDP_MSG_MAX_PLAYERS);
	}
	else {
		memset(msg->u.input.peer_connect_status, 0, sizeof(UdpMsg_connect_status) * UDP_MSG_MAX_PLAYERS);
	}

	ASSERT(offset < MAX_COMPRESSED_BITS);

	UdpProtocol_SendMsg(protocol, msg);
}

void UdpProtocol_SendInputAck(UdpProtocol* protocol)
{
	UdpMsg* msg = calloc(1, sizeof(UdpMsg));  udp_msg_ctor(msg, UdpMsg_InputAck);
	msg->u.input_ack.ack_frame = protocol->_last_received_input.frame;
	UdpProtocol_SendMsg(protocol, msg);
}

bool UdpProtocol_GetEvent(UdpProtocol* protocol, udp_protocol_Event* e)
{
	if (ring_size(&protocol->_event_queue_ring) == 0) {
		return false;
	}
	*e = protocol->_event_queue[ring_front(&protocol->_event_queue_ring)];
	ring_pop(&protocol->_event_queue_ring);
	return true;
}


bool UdpProtocol_OnLoopPoll(UdpProtocol* protocol)
{
	if (!protocol->_udp) {
		return true;
	}

	unsigned int now = Platform_GetCurrentTimeMS();
	unsigned int next_interval;

	UdpProtocol_PumpSendQueue(protocol);
	switch (protocol->_current_state) {
	case UdpProtocol_Syncing:
		next_interval = (protocol->_state.sync.roundtrips_remaining == NUM_SYNC_PACKETS) ? SYNC_FIRST_RETRY_INTERVAL : SYNC_RETRY_INTERVAL;
		if (protocol->_last_send_time && protocol->_last_send_time + next_interval < now) {
			Log("No luck syncing after %d ms... Re-queueing sync packet.\n", next_interval);
			UdpProtocol_SendSyncRequest(protocol);
		}
		break;

	case UdpProtocol_Running:
		// xxx: rig all this up with a timer wrapper
		if (!protocol->_state.running.last_input_packet_recv_time || protocol->_state.running.last_input_packet_recv_time + RUNNING_RETRY_INTERVAL < now) {
			Log("Haven't exchanged packets in a while (last received:%d  last sent:%d).  Resending.\n", protocol->_last_received_input.frame, protocol->_last_sent_input.frame);
			UdpProtocol_SendPendingOutput(protocol);
			protocol->_state.running.last_input_packet_recv_time = now;
		}

		if (!protocol->_state.running.last_quality_report_time || protocol->_state.running.last_quality_report_time + QUALITY_REPORT_INTERVAL < now) {
			UdpMsg* msg = calloc(1, sizeof(UdpMsg));   udp_msg_ctor(msg, UdpMsg_QualityReport);
			msg->u.quality_report.ping = Platform_GetCurrentTimeMS();
			msg->u.quality_report.frame_advantage = (uint8)protocol->_local_frame_advantage;
			UdpProtocol_SendMsg(protocol, msg);
			protocol->_state.running.last_quality_report_time = now;
		}

		if (!protocol->_state.running.last_network_stats_interval || protocol->_state.running.last_network_stats_interval + NETWORK_STATS_INTERVAL < now) {
			UdpProtocol_UpdateNetworkStats(protocol);
			protocol->_state.running.last_network_stats_interval = now;
		}

		if (protocol->_last_send_time && protocol->_last_send_time + KEEP_ALIVE_INTERVAL < now) {
			Log("Sending keep alive packet\n");
			UdpMsg* msg = calloc(1, sizeof(UdpMsg));   udp_msg_ctor(msg, UdpMsg_KeepAlive);
			UdpProtocol_SendMsg(protocol, msg);
		}

		if (protocol->_disconnect_timeout && protocol->_disconnect_notify_start &&
			!protocol->_disconnect_notify_sent && (protocol->_last_recv_time + protocol->_disconnect_notify_start < now)) {
			Log("Endpoint has stopped receiving packets for %d ms.  Sending notification.\n", protocol->_disconnect_notify_start);
			udp_protocol_Event e = { UdpProtocol_Event_NetworkInterrupted };
			e.u.network_interrupted.disconnect_timeout = protocol->_disconnect_timeout - protocol->_disconnect_notify_start;
			UdpProtocol_QueueEvent(protocol, &e);
			protocol->_disconnect_notify_sent = true;
		}

		if (protocol->_disconnect_timeout && (protocol->_last_recv_time + protocol->_disconnect_timeout < now)) {
			if (!protocol->_disconnect_event_sent) {
				Log("Endpoint has stopped receiving packets for %d ms.  Disconnecting.\n", protocol->_disconnect_timeout);
				UdpProtocol_QueueEvent(protocol, &(udp_protocol_Event){ UdpProtocol_Event_Disconnected });
				protocol->_disconnect_event_sent = true;
			}
		}
		break;

	case UdpProtocol_Disconnected:
		if (protocol->_shutdown_timeout < now) {
			Log("Shutting down udp connection.\n");
			protocol->_udp = NULL;
			protocol->_shutdown_timeout = 0;
		}
		break;

	case UdpProtocol_Synchronzied:
		break;
	}


	return true;
}

void UdpProtocol_Disconnect(UdpProtocol* protocol)
{
	protocol->_current_state = UdpProtocol_Disconnected;
	protocol->_shutdown_timeout = Platform_GetCurrentTimeMS() + UDP_SHUTDOWN_TIMER;
}

void UdpProtocol_SendSyncRequest(UdpProtocol* protocol)
{
	protocol->_state.sync.random = rand() & 0xFFFF;
	UdpMsg* msg = calloc(1, sizeof(UdpMsg));   udp_msg_ctor(msg, UdpMsg_SyncRequest);
	msg->u.sync_request.random_request = protocol->_state.sync.random;
	UdpProtocol_SendMsg(protocol, msg);
}

void UdpProtocol_SendMsg(UdpProtocol* protocol, UdpMsg* msg)
{
	UdpProtocol_LogMsg(protocol, "send", msg);

	protocol->_packets_sent++;
	protocol->_last_send_time = Platform_GetCurrentTimeMS();
	protocol->_bytes_sent += udp_msg_PacketSize(msg);

	msg->hdr.magic = protocol->_magic_number;
	msg->hdr.sequence_number = protocol->_next_send_seq++;

	protocol->_send_queue[ring_push(&protocol->_send_queue_ring)] = (udp_protocol_QueueEntry){(int)Platform_GetCurrentTimeMS(), protocol->_peer_addr, msg};
	UdpProtocol_PumpSendQueue(protocol);
}

bool UdpProtocol_HandlesMsg(UdpProtocol* protocol, conn_Address from, UdpMsg* msg)
{
	if (!protocol->_udp) {
		return false;
	}
	return conn_addr_is_equal(protocol->_peer_addr, from);
	//	return protocol->_peer_addr.sin_addr.S_un.S_addr == from->sin_addr.S_un.S_addr &&
	//		protocol->_peer_addr.sin_port == from->sin_port;
}


typedef bool (* DispatchFn)(UdpProtocol* protocol, UdpMsg* msg, int len);
static const DispatchFn table[] = {
	   UdpProtocol_OnInvalid,             /* Invalid */
	   UdpProtocol_OnSyncRequest,         /* SyncRequest */
	   UdpProtocol_OnSyncReply,           /* SyncReply */
	   UdpProtocol_OnInput,               /* Input */
	   UdpProtocol_OnQualityReport,       /* QualityReport */
	   UdpProtocol_OnQualityReply,        /* QualityReply */
	   UdpProtocol_OnKeepAlive,           /* KeepAlive */
	   UdpProtocol_OnInputAck,            /* InputAck */
};

void UdpProtocol_OnMsg(UdpProtocol* protocol, UdpMsg* msg, int len)
{
	bool handled = false;

	// filter out messages that don't match what we expect
	uint16 seq = msg->hdr.sequence_number;
	if (msg->hdr.type != UdpMsg_SyncRequest &&
		msg->hdr.type != UdpMsg_SyncReply) {
		if (msg->hdr.magic != protocol->_remote_magic_number) {
			UdpProtocol_LogMsg(protocol, "recv rejecting", msg);
			return;
		}

		// filter out out-of-order packets
		uint16 skipped = (uint16)((int)seq - (int)protocol->_next_recv_seq);
		// Log("checking sequence number -> next - seq : %d - %d = %d\n", seq, protocol->_next_recv_seq, skipped);
		if (skipped > MAX_SEQ_DISTANCE) {
			Log("dropping out of order packet (seq: %d, last seq:%d)\n", seq, protocol->_next_recv_seq);
			return;
		}
	}

	protocol->_next_recv_seq = seq;
	UdpProtocol_LogMsg(protocol, "recv", msg);
	if (msg->hdr.type >= ARRAY_SIZE(table)) {
		UdpProtocol_OnInvalid(protocol, msg, len);
	}
	else {
		handled = (*(table[msg->hdr.type]))(protocol, msg, len);
	}
	if (handled) {
		protocol->_last_recv_time = Platform_GetCurrentTimeMS();
		if (protocol->_disconnect_notify_sent && protocol->_current_state == UdpProtocol_Running) {
			UdpProtocol_QueueEvent(protocol, &(udp_protocol_Event){ UdpProtocol_Event_NetworkResumed });
			protocol->_disconnect_notify_sent = false;
		}
	}
}

void UdpProtocol_UpdateNetworkStats(UdpProtocol *protocol)
{
	int now = Platform_GetCurrentTimeMS();

	if (protocol->_stats_start_time == 0) {
		protocol->_stats_start_time = now;
	}

	int total_bytes_sent = protocol->_bytes_sent + (UDP_HEADER_SIZE * protocol->_packets_sent);
	float seconds = (float)((now - protocol->_stats_start_time) / 1000.0);
	float Bps = total_bytes_sent / seconds;
	float udp_overhead = (float)(100.0 * (UDP_HEADER_SIZE * protocol->_packets_sent) / protocol->_bytes_sent);

	protocol->_kbps_sent = (int)(Bps / 1024);

	Log("Network Stats -- Bandwidth: %.2f KBps   Packets Sent: %5d (%.2f pps)   "
		"KB Sent: %.2f    UDP Overhead: %.2f %%.\n",
		protocol->_kbps_sent,
		protocol->_packets_sent,
		(float)protocol->_packets_sent * 1000 / (now - protocol->_stats_start_time),
		total_bytes_sent / 1024.0,
		udp_overhead);
}


void UdpProtocol_QueueEvent(UdpProtocol *protocol, const udp_protocol_Event* evt)
{
	UdpProtocol_LogEvent(protocol, "Queuing event", evt);
	protocol->_event_queue[ring_push(&protocol->_event_queue_ring)] = *evt;
}

void UdpProtocol_Synchronize(UdpProtocol *protocol)
{
	if (protocol->_udp) {
		protocol->_current_state = UdpProtocol_Syncing;
		protocol->_state.sync.roundtrips_remaining = NUM_SYNC_PACKETS;
		UdpProtocol_SendSyncRequest(protocol);
	}
}

bool UdpProtocol_GetPeerConnectStatus(UdpProtocol *protocol, int id, int* frame)
{
	*frame = protocol->_peer_connect_status[id].last_frame;
	return !protocol->_peer_connect_status[id].disconnected;
}

void UdpProtocol_Log(UdpProtocol *protocol, const char* fmt, ...)
{
	char buf[1024];
	size_t offset;
	va_list args;

	snprintf(buf, ARRAY_SIZE(buf), "udpproto%d | ", protocol->_queue);
	offset = strlen(buf);
	va_start(args, fmt);
	vsnprintf(buf + offset, ARRAY_SIZE(buf) - offset - 1, fmt, args);
	buf[ARRAY_SIZE(buf) - 1] = '\0';
	Log(buf);
	va_end(args);
}

void UdpProtocol_LogMsg(UdpProtocol *protocol, const char* prefix, UdpMsg* msg)
{
	switch (msg->hdr.type) {
	case UdpMsg_SyncRequest:
		UdpProtocol_Log(protocol, "%s sync-request (%d).\n", prefix,
			msg->u.sync_request.random_request);
		break;
	case UdpMsg_SyncReply:
		UdpProtocol_Log(protocol, "%s sync-reply (%d).\n", prefix,
			msg->u.sync_reply.random_reply);
		break;
	case UdpMsg_QualityReport:
		UdpProtocol_Log(protocol, "%s quality report.\n", prefix);
		break;
	case UdpMsg_QualityReply:
		UdpProtocol_Log(protocol, "%s quality reply.\n", prefix);
		break;
	case UdpMsg_KeepAlive:
		UdpProtocol_Log(protocol, "%s keep alive.\n", prefix);
		break;
	case UdpMsg_Input:
		UdpProtocol_Log(protocol, "%s game-compressed-input %d (+ %d bits).\n", prefix, msg->u.input.start_frame, msg->u.input.num_bits);
		break;
	case UdpMsg_InputAck:
		UdpProtocol_Log(protocol, "%s input ack.\n", prefix);
		break;
	default:
		ASSERT(false && "Unknown UdpMsg type.");
	}
}

void UdpProtocol_LogEvent(UdpProtocol *protocol, const char* prefix, const udp_protocol_Event* evt)
{
	if (evt->type == UdpProtocol_Event_Synchronzied) {
		UdpProtocol_Log(protocol, "%s (event: Synchronzied).\n", prefix);
	}
}

bool UdpProtocol_OnInvalid(UdpProtocol *protocol, UdpMsg* msg, int len)
{
	ASSERT(false && "Invalid msg in UdpProtocol");
	return false;
}

bool UdpProtocol_OnSyncRequest(UdpProtocol *protocol, UdpMsg* msg, int len)
{
	if (protocol->_remote_magic_number != 0 && msg->hdr.magic != protocol->_remote_magic_number) {
		Log("Ignoring sync request from unknown endpoint (%d != %d).\n",
			msg->hdr.magic, protocol->_remote_magic_number);
		return false;
	}
	UdpMsg* reply = calloc(1, sizeof(UdpMsg));   udp_msg_ctor(reply, UdpMsg_SyncReply);
	reply->u.sync_reply.random_reply = msg->u.sync_request.random_request;
	UdpProtocol_SendMsg(protocol, reply);
	return true;
}

bool UdpProtocol_OnSyncReply(UdpProtocol *protocol, UdpMsg* msg, int len)
{
	if (protocol->_current_state != UdpProtocol_Syncing) {
		Log("Ignoring SyncReply while not synching.\n");
		return msg->hdr.magic == protocol->_remote_magic_number;
	}

	if (msg->u.sync_reply.random_reply != protocol->_state.sync.random) {
		Log("sync reply %d != %d.  Keep looking...\n",
			msg->u.sync_reply.random_reply, protocol->_state.sync.random);
		return false;
	}

	if (!protocol->_connected) {
		UdpProtocol_QueueEvent(protocol, &(udp_protocol_Event){ UdpProtocol_Event_Connected });
		protocol->_connected = true;
	}

	Log("Checking sync state (%d round trips remaining).\n", protocol->_state.sync.roundtrips_remaining);
	if (--protocol->_state.sync.roundtrips_remaining == 0) {
		Log("Synchronized!\n");
		UdpProtocol_QueueEvent(protocol, &(udp_protocol_Event){ UdpProtocol_Event_Synchronzied });
		protocol->_current_state = UdpProtocol_Running;
		protocol->_last_received_input.frame = -1;
		protocol->_remote_magic_number = msg->hdr.magic;
	}
	else {
		udp_protocol_Event evt = { UdpProtocol_Event_Synchronizing };
		evt.u.synchronizing.total = NUM_SYNC_PACKETS;
		evt.u.synchronizing.count = NUM_SYNC_PACKETS - protocol->_state.sync.roundtrips_remaining;
		UdpProtocol_QueueEvent(protocol, &evt);
		UdpProtocol_SendSyncRequest(protocol);
	}
	return true;
}

bool UdpProtocol_OnInput(UdpProtocol *protocol, UdpMsg* msg, int len)
{
	/*
	 * If a disconnect is requested, go ahead and disconnect now.
	 */
	bool disconnect_requested = msg->u.input.disconnect_requested;
	if (disconnect_requested) {
		if (protocol->_current_state != UdpProtocol_Disconnected && !protocol->_disconnect_event_sent) {
			Log("Disconnecting endpoint on remote request.\n");
			UdpProtocol_QueueEvent(protocol, &(udp_protocol_Event) { UdpProtocol_Event_Disconnected });
			protocol->_disconnect_event_sent = true;
		}
	}
	else {
		/*
		 * Update the peer connection status if this peer is still considered to be part
		 * of the network.
		 */
		UdpMsg_connect_status* remote_status = msg->u.input.peer_connect_status;
		for (int i = 0; i < ARRAY_SIZE(protocol->_peer_connect_status); i++) {
			ASSERT(remote_status[i].last_frame >= protocol->_peer_connect_status[i].last_frame);
			protocol->_peer_connect_status[i].disconnected = protocol->_peer_connect_status[i].disconnected || remote_status[i].disconnected;
			protocol->_peer_connect_status[i].last_frame = MAX(protocol->_peer_connect_status[i].last_frame, remote_status[i].last_frame);
		}
	}

	/*
	 * Decompress the input.
	 */
	int last_received_frame_number = protocol->_last_received_input.frame;
	if (msg->u.input.num_bits) {
		int offset = 0;
		uint8* bits = (uint8*)msg->u.input.bits;
		int numBits = msg->u.input.num_bits;
		int currentFrame = msg->u.input.start_frame;

		protocol->_last_received_input.size = msg->u.input.input_size;
		if (protocol->_last_received_input.frame < 0) {
			protocol->_last_received_input.frame = msg->u.input.start_frame - 1;
		}
		while (offset < numBits) {
			/*
			 * Keep walking through the frames (parsing bits) until we reach
			 * the inputs for the frame right after the one we're on.
			 */
			ASSERT(currentFrame <= (protocol->_last_received_input.frame + 1));
			bool useInputs = currentFrame == protocol->_last_received_input.frame + 1;

			while (BitVector_ReadBit(bits, &offset)) {
				int on = BitVector_ReadBit(bits, &offset);
				int button = BitVector_ReadNibblet(bits, &offset);
				if (useInputs) {
					if (on) {
						gameinput_set(&protocol->_last_received_input, button);
					}
					else {
						gameinput_clear(&protocol->_last_received_input, button);
					}
				}
			}
			ASSERT(offset <= numBits);

			/*
			 * Now if we want to use these inputs, go ahead and send them to
			 * the emulator.
			 */
			if (useInputs) {
				/*
				 * Move forward 1 frame in the stream.
				 */
				char desc[1024];
				ASSERT(currentFrame == protocol->_last_received_input.frame + 1);
				protocol->_last_received_input.frame = currentFrame;

				/*
				 * Send the event to the emualtor
				 */
				udp_protocol_Event evt = { UdpProtocol_Event_Input };
				evt.u.input.input = protocol->_last_received_input;

				gameinput_desc(&protocol->_last_received_input, desc, ARRAY_SIZE(desc), true);

				protocol->_state.running.last_input_packet_recv_time = Platform_GetCurrentTimeMS();

				Log("Sending frame %d to emu queue %d (%s).\n", protocol->_last_received_input.frame, protocol->_queue, desc);
				UdpProtocol_QueueEvent(protocol, &evt);

			}
			else {
				Log("Skipping past frame:(%d) current is %d.\n", currentFrame, protocol->_last_received_input.frame);
			}

			/*
			 * Move forward 1 frame in the input stream.
			 */
			currentFrame++;
		}
	}
	ASSERT(protocol->_last_received_input.frame >= last_received_frame_number);

	/*
	 * Get rid of our buffered input
	 */
	while (ring_size(&protocol->_pending_output_ring) && protocol->_pending_output[ring_front(&protocol->_pending_output_ring)].frame < msg->u.input.ack_frame) {
		Log("Throwing away pending output frame %d\n", protocol->_pending_output[ring_front(&protocol->_pending_output_ring)].frame);
		protocol->_last_acked_input = protocol->_pending_output[ring_front(&protocol->_pending_output_ring)];
		ring_pop(&protocol->_pending_output_ring);
	}
	return true;
}


bool UdpProtocol_OnInputAck(UdpProtocol *protocol, UdpMsg* msg, int len)
{
	/*
	 * Get rid of our buffered input
	 */
	while (ring_size(&protocol->_pending_output_ring) && protocol->_pending_output[ring_front(&protocol->_pending_output_ring)].frame < msg->u.input_ack.ack_frame) {
		Log("Throwing away pending output frame %d\n", protocol->_pending_output[ring_front(&protocol->_pending_output_ring)].frame);
		protocol->_last_acked_input = protocol->_pending_output[ring_front(&protocol->_pending_output_ring)];
		ring_pop(&protocol->_pending_output_ring);
	}
	return true;
}

bool UdpProtocol_OnQualityReport(UdpProtocol *protocol, UdpMsg* msg, int len)
{
	// send a reply so the other side can compute the round trip transmit time.
	UdpMsg* reply = calloc(1, sizeof(UdpMsg));   udp_msg_ctor(reply, UdpMsg_QualityReply);
	reply->u.quality_reply.pong = msg->u.quality_report.ping;
	UdpProtocol_SendMsg(protocol, reply);

	protocol->_remote_frame_advantage = msg->u.quality_report.frame_advantage;
	return true;
}

bool UdpProtocol_OnQualityReply(UdpProtocol *protocol, UdpMsg* msg, int len)
{
	protocol->_round_trip_time = Platform_GetCurrentTimeMS() - msg->u.quality_reply.pong;
	return true;
}

bool UdpProtocol_OnKeepAlive(UdpProtocol *protocol, UdpMsg* msg, int len)
{
	return true;
}

void UdpProtocol_GetNetworkStats(UdpProtocol *protocol, struct GGPONetworkStats* s)
{
	s->network.ping = protocol->_round_trip_time;
	s->network.send_queue_len = ring_size(&protocol->_pending_output_ring);
	s->network.kbps_sent = protocol->_kbps_sent;
	s->timesync.remote_frames_behind = protocol->_remote_frame_advantage;
	s->timesync.local_frames_behind = protocol->_local_frame_advantage;
}

void UdpProtocol_SetLocalFrameNumber(UdpProtocol *protocol, int localFrame)
{
	/*
	 * Estimate which frame the other guy is one by looking at the
	 * last frame they gave us plus some delta for the one-way packet
	 * trip time.
	 */
	int remoteFrame = protocol->_last_received_input.frame + (protocol->_round_trip_time * 60 / 1000);

	/*
	 * Our frame advantage is how many frames *behind* the other guy
	 * we are.  Counter-intuative, I know.  It's an advantage because
	 * it means they'll have to predict more often and our moves will
	 * pop more frequenetly.
	 */
	protocol->_local_frame_advantage = remoteFrame - localFrame;
}

int UdpProtocol_RecommendFrameDelay(UdpProtocol *protocol)
{
	// XXX: require idle input should be a configuration parameter
	return timesync_recommend_frame_wait_duration(&protocol->_timesync, false);
}


void UdpProtocol_SetDisconnectTimeout(UdpProtocol *protocol, int timeout)
{
	protocol->_disconnect_timeout = timeout;
}

void UdpProtocol_SetDisconnectNotifyStart(UdpProtocol *protocol, int timeout)
{
	protocol->_disconnect_notify_start = timeout;
}

void UdpProtocol_PumpSendQueue(UdpProtocol *protocol)
{
	while (!ring_empty(&protocol->_send_queue_ring)) {
		udp_protocol_QueueEntry const entry = protocol->_send_queue[ring_front(&protocol->_send_queue_ring)];

		if (protocol->_send_latency) {
			// should really come up with a gaussian distributation based on the configured
			// value, but this will do for now.
			int jitter = (protocol->_send_latency * 2 / 3) + ((rand() % protocol->_send_latency) / 3);
			if (Platform_GetCurrentTimeMS() < protocol->_send_queue[ring_front(&protocol->_send_queue_ring)].queue_time + jitter) {
				break;
			}
		}
		if (protocol->_oop_percent && !protocol->_oo_packet.msg && ((rand() % 100) < protocol->_oop_percent)) {
			int delay = rand() % (protocol->_send_latency * 10 + 1000);
			Log("creating rogue oop (seq: %d  delay: %d)\n", entry.msg->hdr.sequence_number, delay);
			protocol->_oo_packet.send_time = Platform_GetCurrentTimeMS() + delay;
			protocol->_oo_packet.msg = entry.msg;
			protocol->_oo_packet.dest_addr = entry.dest_addr;
		}
		else {
			ASSERT(entry.dest_addr);

			udp_SendTo(protocol->_udp, (char*)entry.msg, udp_msg_PacketSize(entry.msg), 0, entry.dest_addr);

			free(entry.msg);
		}
		ring_pop(&protocol->_send_queue_ring);
	}
	if (protocol->_oo_packet.msg && protocol->_oo_packet.send_time < Platform_GetCurrentTimeMS()) {
		Log("sending rogue oop!");
		udp_SendTo(protocol->_udp, (char*)protocol->_oo_packet.msg, udp_msg_PacketSize(protocol->_oo_packet.msg), 0,
			   protocol->_oo_packet.dest_addr);

		free(protocol->_oo_packet.msg);
		protocol->_oo_packet.msg = NULL;
	}
}

void UdpProtocol_ClearSendQueue(UdpProtocol *protocol)
{
	while (!ring_empty(&protocol->_send_queue_ring)) {
		free(protocol->_send_queue[ring_front(&protocol->_send_queue_ring)].msg);
		ring_pop(&protocol->_send_queue_ring);
	}
}
