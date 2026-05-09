/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _UDP_MSG_H
#define _UDP_MSG_H

#define MAX_COMPRESSED_BITS       4096
#define UDP_MSG_MAX_PLAYERS          4

#pragma pack(push, 1)

enum udp_msg_MsgType {
      UdpMsg_Invalid       = 0,
      UdpMsg_SyncRequest   = 1,
      UdpMsg_SyncReply     = 2,
      UdpMsg_Input         = 3,
      UdpMsg_QualityReport = 4,
      UdpMsg_QualityReply  = 5,
      UdpMsg_KeepAlive     = 6,
      UdpMsg_InputAck      = 7,
};
typedef enum udp_msg_MsgType udp_msg_MsgType;

struct UdpMsg_connect_status {
      unsigned int   disconnected:1;
      int            last_frame:31;
};
typedef struct UdpMsg_connect_status UdpMsg_connect_status;

struct UdpMsg
{
   struct {
      uint16         magic;
      uint16         sequence_number;
      uint8          type;            /* packet type */
   } hdr;
   union {
      struct {
         uint32      random_request;  /* please reply back with this random data */
         uint16      remote_magic;
         uint8       remote_endpoint;
      } sync_request;
      
      struct {
         uint32      random_reply;    /* OK, here's your random data back */
      } sync_reply;
      
      struct {
         int8        frame_advantage; /* what's the other guy's frame advantage? */
         uint32      ping;
      } quality_report;
      
      struct {
         uint32      pong;
      } quality_reply;

      struct {
         UdpMsg_connect_status    peer_connect_status[UDP_MSG_MAX_PLAYERS];

         uint32            start_frame;

         int               disconnect_requested:1;
         int               ack_frame:31;

         uint16            num_bits;
         uint8             input_size; // XXX: shouldn't be in every single packet!
         uint8             bits[MAX_COMPRESSED_BITS]; /* must be last */
      } input;

      struct {
         int               ack_frame:31;
      } input_ack;
   } u;
};
typedef struct UdpMsg UdpMsg;

inline void udp_msg_ctor(UdpMsg* msg, udp_msg_MsgType t) { memset(msg, 0, sizeof(UdpMsg)); msg->hdr.type = (uint8)t; }


inline int udp_msg_PayloadSize(UdpMsg* msg)
{
    int size;

    switch (msg->hdr.type) {
    case UdpMsg_SyncRequest:   return sizeof(msg->u.sync_request);
    case UdpMsg_SyncReply:     return sizeof(msg->u.sync_reply);
    case UdpMsg_QualityReport: return sizeof(msg->u.quality_report);
    case UdpMsg_QualityReply:  return sizeof(msg->u.quality_reply);
    case UdpMsg_InputAck:      return sizeof(msg->u.input_ack);
    case UdpMsg_KeepAlive:     return 0;
    case UdpMsg_Input:
        size = (int)((char *)&msg->u.input.bits - (char *)&msg->u.input);
        size += (msg->u.input.num_bits + 7) / 8;
        return size;
    }
    ASSERT(false);
    return 0;
}

inline int udp_msg_PacketSize(UdpMsg* msg)
{
    return sizeof(msg->hdr) + udp_msg_PayloadSize(msg);
}

#pragma pack(pop)

#endif   
