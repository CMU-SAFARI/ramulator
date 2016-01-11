#ifndef __PACKET_H
#define __PACKET_H

#include "Request.h"

namespace ramulator
{
class Packet {
 public:
  enum class Type {
    // Transaction layer: request/response packets
    REQUEST,
    RESPONSE,
    // Link layer: flow control packets
    NULLPACKET,
    TRET,
    RETRY,
    MAX
  } type;

  enum class Command {
    // WRITE Requests
    WR16, WR32, WR48, WR64, WR80, WR96, WR112, WR128, WR256,
    // READ Requests
    RD16, RD32, RD48, RD64, RD80, RD96, RD112, RD128, RD256,
    MAX
  };
  class Datafield<ValueType> {
   public:
    Datafield<ValueType>(ValueType value, int bit): value(value), bit(bit) {}
    ValueType value; // ValueType should be convertible to int
    int bit;
    bool valid() {
      (int(value) < (1<<bit));
    }
  };

/*
 * Request Packet:
 * Header (LSBs on the left):
 * [ CUB | RES | ADRS | RES | TAG | LNG | CMD ]
 * Tail (LSBs on the left):
 * [ CRC | RTC | SLID | RES | Pb | SEQ | FRP | RRP ]
 * Response Packet:
 * Header (LSBs on the left):
 * [ CUB | RES | SLID | RES | AF | TAG | LNG | CMD ]
 * Tail (LSBs on the left):
 * [ CRC | RTC | ERRSTAT | DINV | SEQ | FRP | RRP ]
 */
  class Header {
    Datafield<int> CUB;
    Datafield<int> ADRS;
    Datafield<int> SLID;
    Datafield<int> TAG;
    Datafield<int> LNG;
    Datafield<Command> CMD;
  } header;

  class Tail {
    Datafield<int> RTC;
    Datafield<int> SLID;
  } tail;

  int payload_flits = 0; // one FLIT = 16 bytes
  int total_flits = 1;
  bool flow_control = false;

  // keeps orginal req to facilitate further extraction
  Request req;

  const int cub_bits = 3;
  const int slid_bits = 3;
  const int adrs_bits = 34;
  const int tag_bits = 11;
  const int lng_bits = 5;
  const int cmd_bits = 7;
  const int rtc_bits = 3;

  Packet() {}
  Packet(Type type, int CUB, int ADRS, int TAG, int LNG, int SLID, Command CMD):
      type(type) {
    assert(type == Type::Request);
    header.CUB = Datafield<int>(CUB, cub_bits);
    header.ADRS = Datafield<int>(ADRS, adrs_bits);
    header.TAG = Datafield<int>(TAG, tag_bits);
    header.LNG = Datafield<int>(LNG, lng_bits);
    header.CMD = Datafield<int>(LNG, cmd_bits);

    tail.RTC = Datafield<int>(0, rtc_bits);
    tail.SLID = Datafield<int>(SLID, slid_bits);

    payload_flits = LNG;
    total_flits = payload_flits + 1;
    flow_control = false;
  }

  Packet(Type type, int CUB, int TAG, int LNG, int SLID, Command CMD):
      type(type) {
    assert(type == Type::Response);
    header.CUB = Datafield<int>(CUB, cub_bits);
    header.SLID = Datafield<int>(SLID, slid_bits);
    header.TAG = Datafield<int>(TAG, tag_bits);
    header.LNG = Datafield<int>(LNG, lng_bits);
    header.CMD = Datafield<Command>(CMD, cmd_bits);

    tail.RTC = Datafield<int>(0, rtc_bits);

    payload_flits = LNG;
    total_flits = payload_flits + 1;
    flow_control = false;
  }

  Packet(Type type, int RTC):type(type) {
    assert(type == Type::TRET);
    tail.RTC.bits = rtc_bits;
    tail.RTC.value = RTC;
    tail.RTC = Datafield<int>(RTC, rtc_bits);

    payload_flits = 0;
    total_flits = payload_flits + 1;
    flow_control = true;
  }
};

map<int, Packet::Command> write_cmd_map = {
  {1, Packet::Command::WR16}, {2, Packet::Command::WR32},
  {3, Packet::Command::WR48}, {4, Packet::Command::WR64},
  {5, Packet::Command::WR80}, {6, Packet::Command::WR96},
  {7, Packet::Command::WR112}, {8, Packet::Command::WR128},
  {16, Packet::Command::WR256},
};

map<int, Packet::Command> read_cmd_map = {
  {1, Packet::Command::RD16}, {2, Packet::Command::RD32},
  {3, Packet::Command::RD48}, {4, Packet::Command::RD64},
  {5, Packet::Command::RD80}, {6, Packet::Command::RD96},
  {7, Packet::Command::RD112}, {8, Packet::Command::RD128},
  {16, Packet::Command::RD256},
};

} /*namespace ramulator*/

#endif /*__PACKET_H*/
