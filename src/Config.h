#ifndef __CONFIG_H
#define __CONFIG_H

#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <iostream>
#include <cassert>

using namespace std;
namespace ramulator
{

class Config {
public:
    std::map<std::string, std::string> options;
    void parse(const string& fname);
};


} /* namespace ramulator */

#endif /* _CONFIG_H */

