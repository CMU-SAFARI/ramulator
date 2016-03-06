#ifndef __CONFIG_H
#define __CONFIG_H

#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <iostream>
#include <cassert>

namespace ramulator
{

class Config {

private:
    std::map<std::string, std::string> options;
    int stacks;
    int channels;
    int ranks;
    int subarrays;
    int cpu_frequency;
    int core_num = 0;
    int cacheline_size = 64;
    long expected_limit_insts = 0;

public:
    Config() {}
    Config(const std::string& fname);
    void parse(const std::string& fname);
    std::string operator [] (const std::string& name) const {
      if (options.find(name) != options.end()) {
        return (options.find(name))->second;
      } else {
        return "";
      }
    }

    bool contains(const std::string& name) const {
      if (options.find(name) != options.end()) {
        return true;
      } else {
        return false;
      }
    }

    void add (const std::string& name, const std::string& value) {
      if (!contains(name)) {
        options.insert(make_pair(name, value));
      } else {
        printf("ramulator::Config::add options[%s] already set.\n", name.c_str());
      }
    }

    void set(const std::string& name, const std::string& value) {
      options[name] = value;
    }

    void set_core_num(int _core_num) {core_num = _core_num;}
    void set_cacheline_size(int _cacheline_size) {cacheline_size = _cacheline_size;}

    int get_int_value(const std::string& name) const {
      assert(options.find(name) != options.end() && "can't find this argument");
      return atoi(options.find(name)->second.c_str());
    }
    int get_stacks() const {return stacks;}
    int get_channels() const {return channels;}
    int get_subarrays() const {return subarrays;}
    int get_ranks() const {return ranks;}
    int get_cpu_tick() const {return int(1000000 / cpu_frequency);}
    int get_core_num() const {return core_num;}
    int get_cacheline_size() const {return cacheline_size;}
    long get_expected_limit_insts() const {return expected_limit_insts;}
    bool has_l3_cache() const {
      if (options.find("cache") != options.end()) {
        const std::string& cache_option = (options.find("cache"))->second;
        return (cache_option == "all") || (cache_option == "L3");
      } else {
        return false;
      }
    }
    bool has_core_caches() const {
      if (options.find("cache") != options.end()) {
        const std::string& cache_option = (options.find("cache"))->second;
        return (cache_option == "all" || cache_option == "L1L2");
      } else {
        return false;
      }
    }
    bool is_early_exit() const {
      // the default value is true
      if (options.find("early_exit") != options.end()) {
        if ((options.find("early_exit"))->second == "off") {
          return false;
        }
        return true;
      }
      return true;
    }
    bool calc_weighted_speedup() const {
      return (expected_limit_insts != 0);
    }
    bool record_cmd_trace() const {
      // the default value is false
      if (options.find("record_cmd_trace") != options.end()) {
        if ((options.find("record_cmd_trace"))->second == "on") {
          return true;
        }
        return false;
      }
      return false;
    }
    bool print_cmd_trace() const {
      // the default value is false
      if (options.find("print_cmd_trace") != options.end()) {
        if ((options.find("print_cmd_trace"))->second == "on") {
          return true;
        }
        return false;
      }
      return false;
    }
};


} /* namespace ramulator */

#endif /* _CONFIG_H */

