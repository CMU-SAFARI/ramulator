#ifndef __LOGICLAYER_H
#define __LOGICLAYER_H

#include "Config.h"
#include "Packet.h"
#include "HMC_Controller.h"
#include "Memory.h"

#include <memory>
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
  int available_token_count; // available token count on the other side
  long clk = 0;
  long next_packet_clk = 0;

  LinkMaster(const Config& configs, function<void(Packet&)> receive_from_link,
      Link<T>* link, LogicLayer<T>* logic_layer):
      send_via_link(receive_from_link), link(link), logic_layer(logic_layer),
      available_token_count(link->slave.buffer_max) {
  }

  void send();

  int available_space() {
    return buffer_max - output_buffer.size();
  }

  void tick() {
    clk++;
    if (clk >= next_packet_clk) {
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

  LinkSlave(const Config& configs, Link<T>* link): link(link) {}

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

  LinkSlave<T> slave;
  LinkMaster<T> master;

  Link(const Config& configs, Type type, int id, LogicLayer<T>* logic_layer,
       function<void(Packet&)> receive_from_link):type(type), id(id),
      slave(configs, this),
      master(configs, receive_from_link, this, logic_layer) {}
};

template<typename T>
class Switch {
 public:
  LogicLayer<T>* logic_layer;
  std::vector<Controller<T>*> vault_ctrls;
  long clk = 0;

  Switch(const Config& configs, LogicLayer<T>* logic_layer,
         std::vector<Controller<T>*> vault_ctrls):
      logic_layer(logic_layer), vault_ctrls(vault_ctrls) {}

  void tick();

 private:
  // TODO longer delay for different quadrants
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
  std::vector<std::shared_ptr<Link<T>>> host_links;
  std::vector<std::shared_ptr<Link<T>>> pass_thru_links;

  LogicLayer(const Config& configs, int cub, T* spec,
      std::vector<Controller<T>*> vault_ctrls, MemoryBase* mem,
      function<void(Packet&)> host_ctrl_recv):
      spec(spec), mem(mem), xbar(configs, this, vault_ctrls) {
    // initialize some system parameters
    one_flit_cycles =
        (128.0/(spec->lane_speed * spec->link_width))/mem->clk_ns();

    int host_links_num = configs.get_int_value("source_mode_host_links");
    // FIXME: we shouldn't assume all host links are in source mode
    int pass_thru_links_num = configs.get_int_value("pass_thru_links");
    // FIXME: we shouldn't assume we only have one stack so we shouldn't assume
    // the number of pass thru link is zero.
    assert(pass_thru_links_num == 0);
    int link_id = 0;
    for (int i = 0 ; i < host_links_num ; ++i) {
      // FIXME: we shouldn't assume all host links are in source mode
      host_links.emplace_back(
          new Link<T>(configs, Link<T>::Type::HOST_SOURCE_MODE, link_id, this,
                      host_ctrl_recv));
      link_id++;
    }
    for (int i = 0 ; i < pass_thru_links_num ; ++i) {
      // TODO instantiate pass_thru_links here
      link_id++;
    }
  }

  void tick();
};

} /* namespace ramulator */
#endif /*__LOGICLAYER_H*/
