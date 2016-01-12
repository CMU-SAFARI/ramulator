#include "Packet.h"

namespace ramulator {

std::map<int, enum Packet::Command> write_cmd_map = {
  {1, Packet::Command::WR16}, {2, Packet::Command::WR32},
  {3, Packet::Command::WR48}, {4, Packet::Command::WR64},
  {5, Packet::Command::WR80}, {6, Packet::Command::WR96},
  {7, Packet::Command::WR112}, {8, Packet::Command::WR128},
  {16, Packet::Command::WR256},
};

std::map<int, enum Packet::Command> read_cmd_map = {
  {1, Packet::Command::RD16}, {2, Packet::Command::RD32},
  {3, Packet::Command::RD48}, {4, Packet::Command::RD64},
  {5, Packet::Command::RD80}, {6, Packet::Command::RD96},
  {7, Packet::Command::RD112}, {8, Packet::Command::RD128},
  {16, Packet::Command::RD256},
};

} /*namespace ramulator*/
