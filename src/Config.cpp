#include "Config.h"

using namespace std;
using namespace ramulator;

Config::Config(const std::string& fname) {
  parse(fname);
}

void Config::parse_to_const(const string& name, const string& value) {
  if (name == "stacks") {
    stacks = atoi(value.c_str());
  } else if (name == "channels") {
    channels = atoi(value.c_str());
  } else if (name == "ranks") {
    ranks = atoi(value.c_str());
  } else if (name == "subarrays") {
    subarrays = atoi(value.c_str());
  } else if (name == "cpu_frequency") {
    cpu_frequency = atoi(value.c_str());
  } else if (name == "expected_limit_insts") {
    expected_limit_insts = atoi(value.c_str());
  }
}

void Config::parse(const string& fname)
{
    ifstream file(fname);
    assert(file.good() && "Bad config file");
    string line;
    while (getline(file, line)) {
        char delim[] = " \t=";
        vector<string> tokens;

        while (true) {
            size_t start = line.find_first_not_of(delim);
            if (start == string::npos) 
                break;

            size_t end = line.find_first_of(delim, start);
            if (end == string::npos) {
                tokens.push_back(line.substr(start));
                break;
            }

            tokens.push_back(line.substr(start, end - start));
            line = line.substr(end);
        }

        // empty line
        if (!tokens.size())
            continue;

        // comment line
        if (tokens[0][0] == '#')
            continue;

        // parameter line
        assert(tokens.size() == 2 && "Only allow two tokens in one line");

        options[tokens[0]] = tokens[1];
        parse_to_const(tokens[0], tokens[1]);
    }
    file.close();
}


