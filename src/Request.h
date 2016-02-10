#ifndef __REQUEST_H
#define __REQUEST_H

#include <vector>
#include <functional>

using namespace std;

namespace ramulator
{

class Request
{
public:
    bool is_first_command;
    long addr;
    // long addr_row;
    vector<int> addr_vec;
    long reqid = -1;
    // specify which core this request sent from, for virtual address translation
    int coreid = -1;

    enum class Type
    {
        READ,
        WRITE,
        REFRESH,
        POWERDOWN,
        SELFREFRESH,
        EXTENSION,
        MAX
    } type;

    long arrive = -1;
    long depart;
    long arrive_hmc;
    long depart_hmc;

    int burst_count = 0;
    int transaction_bytes = 0;
    function<void(Request&)> callback; // call back with more info

    Request(long addr, Type type, int coreid)
        : is_first_command(true), addr(addr), coreid(coreid), type(type),
      callback([](Request& req){}) {}

    Request(long addr, Type type, function<void(Request&)> callback, int coreid)
        : is_first_command(true), addr(addr), coreid(coreid), type(type), callback(callback) {}

    Request(vector<int>& addr_vec, Type type, function<void(Request&)> callback, int coreid)
        : is_first_command(true), addr_vec(addr_vec), coreid(coreid), type(type), callback(callback) {}

    Request() {}

};

} /*namespace ramulator*/

#endif /*__REQUEST_H*/

