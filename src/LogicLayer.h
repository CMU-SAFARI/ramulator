#ifndef __LOGICLAYER_H
#define __LOGICLAYER_H

#include "Packet.h"
#include "HMC_Controller.h"
#include "Memory.h"

#include <vector>

namespace ramulator {

template<typename T>
class Link;

template<typename T>
class LogicLayer;

template<typename T>
class LinkMaster {
 public:
  function<void(Packet&)> send_via_link;
  Link<T>* link;
  LogicLayer<T>* logic_layer;
  std::deque<Packet> output_buffer;
  int buffer_max = 32;
  int available_token_count; // available token count on the other side, TODO: initialize it when initialize the system
  long clk = 0;
  int next_packet_clk = 0;

  void send();

  int available_space() {
    return buffer_max - output_buffer.size();
  }

  void tick() {
    clk++;
    if (clk == next_packet_clk) {
      send();
    }
  }
 private:
  // returns 0 if val == 0
  // returns 1<<leftmostbit if val > 0
  int leftmostbit(int val) {
    if (val == 0) {
      return 0;
    }
    int n = 0;
    while ((val >>= 1)) {
      n++;
    }
    return 1<<n;
  }
};

template<typename T>
class LinkSlave {
 public:
  Link<T>* link;
  int extracted_token_count = 0;
  std::deque<Packet> input_buffer;
  int buffer_max = 32;

  void receive(Packet& packet);

  int available_space() {
    return buffer_max - input_buffer.size();
  }
};

template<typename T>
class Link {
 public:
  enum class Type {
    HOST_SOURCE_MODE,
    HOST,
    PASSTHRU,
    MAX
  } type;

  int id;
  LinkMaster<T> master;
  LinkSlave<T> slave;
};

template<typename T>
class Switch {
 public:
  LogicLayer<T>* logic_layer;
  std::vector<Controller<T>*> vault_ctrls;
  void tick();

 private:
  // TODO loner delay for different quadrants
  const int delay = 1;
};

template<typename T>
class LogicLayer {
 public:
  T* spec;
  MemoryBase* mem;
  int cub;
  double one_flit_cycles; // = ceil(128/(30/# of lane) / 0.8)
  Switch<T> xbar;
  std::vector<Link<T>*> host_links;
  std::vector<Link<T>*> pass_thru_links;

  LogicLayer(const Config& configs) {
    one_flit_cycles =
        (128.0/(spec->link_speed/spec->link_width))/mem->clk_ns();
    printf("one_flit_cycles: %lf\n", one_flit_cycles);
    // XXX: the time to transfer one flit may not be exactly multiple memory
    // cycles, will round up when calculate time to transmit a packet
  }

  void tick();
};

} /* namespace ramulator */
#endif /*__LOGICLAYER_H*/
