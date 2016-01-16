#ifndef __PACKET_H
#define __PACKET_H

#include "Request.h"

#include <cassert>

#include <map>

namespace ramulator
{

static int cub_bits = 3;
static int slid_bits = 3;
static int adrs_bits = 34;
static int tag_bits = 11;
static int lng_bits = 5;
static int cmd_bits = 7;
static int rtc_bits = 3;

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

  template<typename ValueType>
  struct Datafield {
    Datafield<ValueType>() {}
    Datafield<ValueType>(ValueType value, int bit): value(value), bit(bit) {}
    ValueType value; // ValueType should be convertible to int
    int bit;
    bool valid() {
      return (long(value) < (1ll<<bit));
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
  struct Header {
    Datafield<int> CUB;
    Datafield<long> ADRS;
    Datafield<int> SLID;
    Datafield<int> TAG;
    Datafield<int> LNG;
    Datafield<Command> CMD;
  } header;

  struct Tail {
    Datafield<int> RTC;
    Datafield<int> SLID;
  } tail;

  int payload_flits = 0; // one FLIT = 16 bytes
  int total_flits = 1;
  bool flow_control = false;

  // keeps orginal req to facilitate further extraction
  Request req;

  Packet() {}
  Packet(Type type, int CUB, long ADRS, int TAG, int LNG, int SLID, Command CMD):
      type(type) {
    assert(type == Type::REQUEST);
    header.CUB = Datafield<int>(CUB, cub_bits);
    header.ADRS = Datafield<long>(ADRS, adrs_bits);
    header.TAG = Datafield<int>(TAG, tag_bits);
    header.LNG = Datafield<int>(LNG, lng_bits);
    header.CMD = Datafield<Command>(CMD, cmd_bits);

    tail.RTC = Datafield<int>(0, rtc_bits);
    tail.SLID = Datafield<int>(SLID, slid_bits);

    payload_flits = LNG - 1;
    total_flits = LNG;
    flow_control = false;
  }

  Packet(Type type, int CUB, int TAG, int LNG, int SLID, Command CMD):
      type(type) {
    assert(type == Type::RESPONSE);
    header.CUB = Datafield<int>(CUB, cub_bits);
    header.SLID = Datafield<int>(SLID, slid_bits);
    header.TAG = Datafield<int>(TAG, tag_bits);
    header.LNG = Datafield<int>(LNG, lng_bits);
    header.CMD = Datafield<Command>(CMD, cmd_bits);

    tail.RTC = Datafield<int>(0, rtc_bits);

    payload_flits = LNG - 1;
    total_flits = LNG;
    flow_control = false;
  }

  Packet(Type type, int RTC):type(type) {
    assert(type == Type::TRET);
    tail.RTC = Datafield<int>(RTC, rtc_bits);

    payload_flits = 0;
    total_flits = payload_flits + 1;
    flow_control = true;
  }
};

extern std::map<int, enum Packet::Command> write_cmd_map;

extern std::map<int, enum Packet::Command> read_cmd_map;
} /*namespace ramulator*/

#endif /*__PACKET_H*/
