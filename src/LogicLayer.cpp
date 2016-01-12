#ifndef __LOGICLAYER_CPP
#define __LOGICLAYER_CPP

#include "LogicLayer.h"

#include <cmath>
#include <vector>
#include <set>

namespace ramulator {

template<typename T>
void LinkMaster<T>::send() {
  if (output_buffer.size() > 0 &&
      available_token_count >= output_buffer.front().total_flits) {
    // send the head of output_buffer
    Packet& packet = output_buffer.front();
    output_buffer.pop_front();
    // 1. change the RTC field in tail (TODO: change the RTC field when sending the exact FLIT)
    int rtc = leftmostbit(link->slave.extracted_token_count);
    printf("link->slave.extracted_token_count: %d, RTC: %d\n",
        link->slave.extracted_token_count, rtc);
    link->slave.extracted_token_count -= rtc;
    if (link->type != Link<T>::Type::HOST_SOURCE_MODE) {
      available_token_count -= packet.total_flits;
    }
    packet.tail.RTC.value = rtc;
    send_via_link(packet);
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
      link->slave.extracted_token_count -= rtc;
      Packet tret_packet(Packet::Type::TRET, rtc);
      send_via_link(tret_packet);
      next_packet_clk = clk +
          ceil(tret_packet.total_flits * logic_layer->one_flit_cycles);
    } else {
      // send NULL packet here: no need to do anything
      // NULL packet has one flit
      next_packet_clk = clk + ceil(logic_layer->one_flit_cycles);
    }
  }
}

template<typename T>
void LinkSlave<T>::receive(Packet& packet) {
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

template<typename T>
void Switch<T>::tick() {
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
    Packet& packet = vault_ctrl->response_packets_buffer.front();
    int slid = packet.header.SLID.value; // identify the target of transmission
    Link<T>* link = logic_layer->host_links[slid];
    // TODO support multiple stacks
    assert(link->type == Link<T>::Type::HOST_SOURCE_MODE);
    if (used_links.find(slid) != used_links.end()) {
      continue; // This port has been occupied in this cycle
    }
    used_links.insert(slid);
    if (packet.total_flits <= link->master.available_space()) {
      link->master.output_buffer.push_back(packet);
      vault_ctrl->response_packets_buffer.pop_front();
    }
  }
}

template<typename T>
void LogicLayer<T>::tick() {
  for (auto link : host_links) {
  // all link master: send one packet
    link->master.tick();
  // all link slave: receive (driven by corresponding master)
  }
  for (auto link : pass_thru_links) {
  // all link master: send one packet
    link->master.tick();
  // all link slave: receive (driven by corresponding master)
  }
  // switch: extract packets from both sides
  xbar.tick();
}

} /* namespace ramulator */
#endif /*__LOGICLAYER_CPP*/
