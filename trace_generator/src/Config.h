#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <iostream>
#include <cassert>
#include <list>

class CacheParams {
private:
  int level;
  int size;
  int assoc;
  int block_size;
public:
  CacheParams() {}
  CacheParams(int level, int size, int assoc, int block_size):
  level(level), size(size), assoc(assoc), block_size(block_size) {}
  void set_size(int sz) { size=sz;}
  void set_block_size(int bs) {block_size=bs;}
  void set_assoc(int _assoc) {assoc=_assoc;}
  int get_level() const {return level;}
  int get_size() const {return size;}
  int get_assoc() const {return assoc;}
  int get_block_size() const {return block_size;}
};
class Config {

private:
    std::map<std::string, std::string> options;

public:
    std::list<CacheParams *> * caches = new std::list<CacheParams *>();
    CacheParams * current;
    Config() {}
    Config(const std::string& fname)  { Config::parse(fname); }
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
        printf("Config::add options[%s] already set.\n", name.c_str());
      }
    }

    std::list<CacheParams *> * get_caches() {return caches;}
    bool isPowerOfTwo(int n)
    {
       return (ceil(log2(n)) == floor(log2(n)));
    }
};
#endif
