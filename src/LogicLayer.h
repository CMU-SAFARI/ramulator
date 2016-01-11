#ifndef __LOGICLAYER_H
#define __LOGICLAYER_H

#include "Packet.h"
#include "Controller.h"

#include <cmath>
#include <string>
#include <vector>
#include <map>

namespace ramulator {
class LinkMaster {
 public:
  function<void<Packet&>> send;
  Link* link;
  LogicLayer* logic_layer;
  std::deque<Packet> output_buffer;
  int buffer_max = 32;
  int available_token_count; // available token count on the other side, TODO: initialize it when initialize the system
  long clk = 0;
  int next_packet_clk = 0;

  void send() {
    assert(available_token_count == slave.available_space());
    if (output_buffer.size() > 0 &&
        available_token_count >= output_buffer.front().packet_flit) {
      // send the head of output_buffer
      Packet& packet = output_buffer.front();
      output_buffer.pop_front();
      // 1. change the RTC field in tail (TODO: change the RTC field when sending the exact FLIT)
      int rtc = leftmostbit(link->slave.extracted_token_count);
      printf("link->slave.extracted_token_count: %d, RTC: %d\n",
          link->slave.extracted_token_count, rtc);
      link->slave.extracted_token_count -= rtc;
      if (link->type != Link::Type::HOST_SOURCE_MODE) {
        available_token_count -= packet.total_flits;
      }
      packet.tail.RTC.value = rtc;
      send(packet);
      // 3. update next_packet_clk
      next_packet_clk = clk +
          ceil(packet.total_flits * logic_layer->one_flit_cycles);
      // TODO if send only one flit packet by fastest speed (16lanes * 30Gb/s = 480Gb/s), then the time to send == 128/480, == (128/480)/0.8 = 0.333333 mem cycle, which means in one memory cycle, the bandwidth allow us to send at most 3 flit, but we can only send one. That makes every flow control packet's size == 3 flit
    } else {
      // send flow control packet
      if (link->slave.extracted_token_count > 0) {
        int rtc = leftmostbit(link->slave.extracted_token_count);
        printf("link->slave.extracted_token_count: %d, RTC: %d\n",
            link->slave.extracted_token_count, rtc);
        link->master.extracted_token_count -= rtc;
        TRETPacket tret_packet(rtc);
        send(tret_packet);
        next_packet_clk = clk +
            ceil(tret_packet.total_flits * logic_layer->one_flit_cycles);
      } else {
        // send NULL packet here: no need to do anything
        // NULL packet has one flit
        next_packet_clk = clk + ceil(logic_layer->one_flit_cycles);
      }
    }
  }

  // In vault controller, I assume it will know the remaining space of the target
  // on the same stack directly, so it doesn't need to do flow control by passing
  // tokens like across different links
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

class LinkSlave {
 public:
  Link* link;
  int extracted_token_count = 0;
  std::deque<Packet> input_buffer;
  int buffer_max = 32;
  void receive(Packet& packet) {
    if (packet.flow_control) {
      int rtc = packet.tail.RTC.value;
      link->master.available_token_count += rtc;
      // drop this packet
    } else {
      int rtc = packet.tail.RTC.value;
      link->master.available_token_count += rtc;
      input_buffer.push_back(packet);
    }
  }
  // In vault controller, I assume it will know the remaining space of the target
  // on the same stack directly, so it doesn't need to do flow control by passing
  // tokens like across different links
  int available_space() {
    return buffer_max - input_buffer.size();
  }
};

template<class T>
class Link {
 public:
  enum class Type {
    HOST_SOURCE_MODE,
    HOST,
    PASSTHRU,
    MAX
  } type;

  int id;
  LinkMaster master;
  LinkSlave slave;
};

template<class T>
class Switch<T> {
 public:
  LogicLayer* logic_layer;
  vector<Controller<T>*> vault_ctrls;
  void tick() {
    // one controller can only receive one packet per cycle
    std::set<int> used_vaults;
    for (auto link : logic_layer->host_links) {
      Packet& packet = link->slave.input_buffer.front();
      Request& req = packet.req;
      // from links to vaults
      if (packet.header.CUB.value == logic_layer->cub) {
        int vault_id = req.addr_vec[int(HMC::Level::Vault)];
        if (used_vaults.find(vault_id) != used_vaults.end()) {
          continue; // This port has been occupied in this cycle
        }
        used_vaults.insert(vault_id);
        auto ctrl = vault_ctrls[vault_id];
        if(ctrl->receive(packet)) {
          link->slave.extracted_token_count += packet.total_flits;
          link->slave.input_buffer.pop_front();
        }
      } else { // from links to other stacks
        // TODO: support multiple stacks
        assert((packet.header.CUB.value != logic_layer->cub) &&
            "Only support one stack");
      }
    }
    // from vaults to links
    // currently only according to SLID field
    // TODO: support multiple stacks
    // one link can only receive one packet per cycle
    std::set<int> used_links;
    for (auto vault_ctrl : vault_ctrls) {
      Packet& packet = vault_ctrl->response_packets_buffer.q.front();
      int slid = packet.header.slid; // identify the target of transmission
      Link* link = logic_layer->host_links[slid];
      // TODO support multiple stacks
      assert(link->type == Link::Type::HOST_SOURCE_MODE);
      if (used_links.find(slid) != used_links.end()) {
        continue; // This port has been occupied in this cycle
      }
      used_links.insert(slid);
      if (packet.total_flits <= link->master.available_space()) {
        link->output_buffer.push_back(packet);
        vault_ctrl->response_packets_buffer.q.pop();
      }
    }
  }
 private:
  // TODO loner delay for different quadrants
  const int delay = 1;
};

template<class T>
class LogicLayer<T> {
 public:
  T* spec;
  Memory<T> mem;
  int cub;
  double one_flit_cycles; // = ceil(128/(30/# of lane) / 0.8)
  Switch<T> xbar;
  vector<Link<T>*> host_links;
  vector<Link<T>*> pass_thru_links;

  LogicLayer(const Config& configs) {
    one_flit_cycles =
        (128.0/(spec->link_speed/spec->link_width))/mem->clk_ns();
    printf("one_flit_cycles: %lf\n", one_flit_cycles);
    // XXX: the time to transfer one flit may not be exactly multiple memory
    // cycles, will round up when calculate time to transmit a packet
  }
  void tick() {
    for (auto link : links) {
    // all link master: send one packet
      link->master.tick();
    // all link slave: receive (driven by corresponding master)
    }
    // switch: extract packets from both sides
    xbar.tick();
  }
};
