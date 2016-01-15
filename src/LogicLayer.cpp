#ifndef __LOGICLAYER_CPP
#define __LOGICLAYER_CPP

#include "LogicLayer.h"

#include <cmath>
#include <vector>
#include <set>

namespace ramulator {

template<typename T>
void LinkMaster<T>::send() {
  debug_hmc("link master send");
  if (output_buffer.size() > 0 &&
      available_token_count >= output_buffer.front().total_flits) {
    debug_hmc("send data packet @ link %d master", link->id);
    // send the head of output_buffer
    Packet packet = output_buffer.front();
    output_buffer.pop_front();
    // change the RTC field in tail (TODO: change the RTC field when sending the exact FLIT)
    int rtc = leftmostbit(link->slave.extracted_token_count);
    link->slave.extracted_token_count -= rtc;
    debug_hmc("link->slave.extracted_token_count: %d, RTC: %d",
        link->slave.extracted_token_count, rtc);
    packet.tail.RTC.value = rtc;

    debug_hmc("packet.total_flits: %d", packet.total_flits);
    if (link->type != Link<T>::Type::HOST_SOURCE_MODE) {
      available_token_count -= packet.total_flits;
    }
    send_via_link(packet);
    // update next_packet_clk
    next_packet_clk = clk +
        ceil(packet.total_flits * logic_layer->one_flit_cycles);
    debug_hmc("clk %ld", clk);
    debug_hmc("next_packet_clk %ld", next_packet_clk);
  } else {
    // send flow control packet
    debug_hmc("send flow control packet @ master");
    if (link->slave.extracted_token_count > 0) {
      int rtc = leftmostbit(link->slave.extracted_token_count);
      debug_hmc("link->slave.extracted_token_count: %d, RTC: %d",
          link->slave.extracted_token_count, rtc);
      link->slave.extracted_token_count -= rtc;
      Packet tret_packet(Packet::Type::TRET, rtc);
      send_via_link(tret_packet);
      next_packet_clk = clk +
          ceil(tret_packet.total_flits * logic_layer->one_flit_cycles);
      debug_hmc("clk: %ld", clk);
      debug_hmc("next_packet_clk %ld", next_packet_clk);
    } else {
      // send NULL packet here: no need to do anything
      // NULL packet has one flit
      debug_hmc("send NULL packet @ link %d master", link->id);
      next_packet_clk = clk + ceil(logic_layer->one_flit_cycles);
      debug_hmc("clk: %ld", clk);
      debug_hmc("next_packet_clk %ld", next_packet_clk);
    }
  }
}

template<typename T>
void LinkSlave<T>::receive(Packet& packet) {
  if (packet.flow_control) {
    debug_hmc("receive flow control packet @ link %d slave", link->id);
    int rtc = packet.tail.RTC.value;
    link->master.available_token_count += rtc;
    // drop this packet
  } else {
    debug_hmc("receive data packet @ slave");
    int rtc = packet.tail.RTC.value;
    link->master.available_token_count += rtc;
    input_buffer.push_back(packet);
    debug_hmc("input_buffer.size() %ld @ link %d slave",
        input_buffer.size(), link->id);
  }
}

template<typename T>
void Switch<T>::tick() {
  clk++;
  debug_hmc("@ clk: %ld stack %d", clk, logic_layer->cub);
  // one controller can only receive one packet per cycle
  std::set<int> used_vaults;
  for (auto link : logic_layer->host_links) {
    if (link->slave.input_buffer.empty()) {
      continue;
    }
    Packet& packet = link->slave.input_buffer.front();
    Request& req = packet.req;
    // from links to vaults
    // TODO maybe reorder the requests (inter links) in case one vault is more busy
    if (packet.header.CUB.value == logic_layer->cub) {
      int vault_id = req.addr_vec[int(HMC::Level::Vault)];
      if (used_vaults.find(vault_id) != used_vaults.end()) {
        continue; // This port has been occupied in this cycle
      }
      used_vaults.insert(vault_id);
      auto ctrl = vault_ctrls[vault_id];
      if(ctrl->receive(packet)) {
        link->slave.extracted_token_count += packet.total_flits;
        debug_hmc("extracted_token_count %d", link->slave.extracted_token_count);
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
    if (vault_ctrl->response_packets_buffer.empty()) {
      continue;
    }
    Packet& packet = vault_ctrl->response_packets_buffer.front();
    int slid = packet.header.SLID.value; // identify the target of transmission
    Link<T>* link = logic_layer->host_links[slid].get();
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
