#include "Config.h"

using namespace std;


void Config::parse(const string& fname)
{
    ifstream file(fname.c_str());
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

        if (tokens[0] == "size") {
          assert(isPowerOfTwo(atoi(tokens[1].c_str())) && "Bad config file. Size needs to be a power of two.");
          current->set_size(atoi(tokens[1].c_str()));
        } else if (tokens[0] == "assoc") {
          assert(isPowerOfTwo(atoi(tokens[1].c_str())) && "Bad config file. associativity needs to be a power of two.");
          current->set_assoc(atoi(tokens[1].c_str()));
        } else if (tokens[0] == "blocksize") {
          assert(isPowerOfTwo(atoi(tokens[1].c_str())) && "Bad config file. block size needs to be a power of two.");
          current->set_block_size(atoi(tokens[1].c_str()));
        }
        else if (tokens[0] == "level") {
          current = new CacheParams(atoi(tokens[1].c_str()),0,0,0);
          caches->push_back(current);
        }

    }
    file.close();
}
