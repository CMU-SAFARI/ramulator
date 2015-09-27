#include "Processor.h"
#include <cassert>

using namespace std;
using namespace ramulator;

Processor::Processor(const Config& configs, const char* trace_fname, function<bool(Request)> send)
    : send(send), callback(bind(&Processor::receive, this, placeholders::_1)), trace(trace_fname)
{
    more_reqs = trace.get_request(bubble_cnt, req_addr, req_type);

    // regStats
    memory_access_cycles.name("memory_access_cycles")
                        .desc("cycle number in memory clock domain that there is at least one request in the queue of memory controller")
                        .precision(0)
                        ;
    memory_access_cycles = 0;
    cpu_inst.name("cpu_instructions")
            .desc("commited cpu instruction number")
            .precision(0)
            ;
    cpu_inst = 0;
    cpu_cycles.name("cpu_cycles")
              .desc("CPU cycle of the whole execution")
              .precision(0)
              ;
    cpu_cycles = 0;
}


double Processor::calc_ipc()
{
    return (double) retired / clk;
}

void Processor::tick() 
{
    clk++;
    cpu_cycles++;

    retired += window.retire();

    if (!more_reqs) return;
    // bubbles (non-memory operations)
    int inserted = 0;
    while (bubble_cnt > 0) {
        if (inserted == window.ipc) return;
        if (window.is_full()) return;

        window.insert(true, -1);
        inserted++;
        bubble_cnt--;
        cpu_inst++;
    }

    if (req_type == Request::Type::READ) {
        // read request
        if (inserted == window.ipc) return;
        if (window.is_full()) return;

        Request req(req_addr, req_type, callback);
        if (!send(req)) return;

        //cout << "Inserted: " << clk << "\n";

        window.insert(false, req_addr);
        cpu_inst++;
        more_reqs = trace.get_request(bubble_cnt, req_addr, req_type);
        return;
    }
    else {
        // write request
        assert(req_type == Request::Type::WRITE);
        Request req(req_addr, req_type, callback);
        if (!send(req)) return;
        cpu_inst++;
    }

    more_reqs = trace.get_request(bubble_cnt, req_addr, req_type);
}

bool Processor::finished()
{
    return !more_reqs && window.is_empty();
}
void Processor::receive(Request& req) 
{
    window.set_ready(req.addr);
    if (req.arrive != -1 && req.depart > last) {
      memory_access_cycles += (req.depart - max(last, req.arrive));
      last = req.depart;
    }
}


bool Window::is_full()
{
    return load == depth;
}

bool Window::is_empty()
{
    return load == 0;
}


void Window::insert(bool ready, long addr)
{
    assert(load <= depth);

    ready_list.at(head) = ready;
    addr_list.at(head) = addr;

    head = (head + 1) % depth;
    load++;
}


long Window::retire()
{
    assert(load <= depth);

    if (load == 0) return 0;

    int retired = 0;
    while (load > 0 && retired < ipc) {
        if (!ready_list.at(tail)) 
            break;

        tail = (tail + 1) % depth;
        load--;
        retired++;
    }

    return retired;
}


void Window::set_ready(long addr)
{
    if (load == 0) return;

    for (int i = 0; i < load; i++) {
        int index = (tail + i) % depth;
        if (addr_list.at(index) != addr)
            continue;
        ready_list.at(index) = true;
    }
}



Trace::Trace(const char* trace_fname) : file(trace_fname)
{ 
    if (!file.good()) {
        std::cerr << "Bad trace file: " << trace_fname << std::endl;
        exit(1);
    }
}


bool Trace::get_request(long& bubble_cnt, long& req_addr, Request::Type& req_type)
{
    static bool has_write = false;
    static long write_addr;
    static int line_num = 0;
    if (has_write){
        bubble_cnt = 0;
        req_addr = write_addr;
        req_type = Request::Type::WRITE;
        has_write = false;
        return true;
    }
    string line;
    getline(file, line);
    line_num ++;
    if (file.eof() || line.size() == 0) {
        file.clear();
        file.seekg(0);
        // getline(file, line);
        line_num = 0;
        return false;
    }

    size_t pos, end;
    bubble_cnt = std::stoul(line, &pos, 10);
    //std::cout << "Bubble_Count: " << bubble_cnt << std::endl;

    pos = line.find_first_not_of(' ', pos+1);
    req_addr = stoul(line.substr(pos), &end, 0);
    //std::cout << "Req_Addr: " << req_addr << std::endl;
    req_type = Request::Type::READ;

    pos = line.find_first_not_of(' ', pos+end);
    if (pos != string::npos){
        has_write = true;
        write_addr = stoul(line.substr(pos), NULL, 0);
    }
    return true;
}

bool Trace::get_request(long& req_addr, Request::Type& req_type)
{
    string line;
    getline(file, line);
    if (file.eof()) {
        return false;
    }
    size_t pos;
    req_addr = std::stoul(line, &pos, 16);

    pos = line.find_first_not_of(' ', pos+1);

    if (pos == string::npos || line.substr(pos)[0] == 'R')
        req_type = Request::Type::READ;
    else if (line.substr(pos)[0] == 'W')
        req_type = Request::Type::WRITE;
    else assert(false);
    return true;
}
