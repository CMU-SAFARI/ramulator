#ifndef __CONTROLLER_H
#define __CONTROLLER_H

#include <cassert>
#include <cstdio>
#include <deque>
#include <fstream>
#include <list>
#include <string>
#include <vector>

#include "DRAM.h"
#include "Refresh.h"
#include "Request.h"
#include "Scheduler.h"
#include "SALP.h"
#include "TLDRAM.h"

using namespace std;
namespace ramulator
{

template <typename T>
class Controller
{
public:
    /* Command trace for DRAMPower 3.1 */
    string cmd_trace_prefix = "cmd-trace-";
    vector<ofstream> cmd_trace_files;
    bool record_cmd_trace = false;
    /* Commands to stdout */
    bool print_cmd_trace = false;
    /* Member Variables */
    long clk = 0;
    DRAM<T>* channel;

    Scheduler<T>* scheduler;  // determines the highest priority request whose commands will be issued
    RowPolicy<T>* rowpolicy;  // determines the row-policy (e.g., closed-row vs. open-row)
    RowTable<T>* rowtable;  // tracks metadata about rows (e.g., which are open and for how long)
    Refresh<T>* refresh;

    struct Queue {
        list<Request> q;
        int max = 32;
        int size() {return q.size();}
    }; 

    Queue readq;  // queue for read requests
    Queue writeq;  // queue for write requests
    Queue otherq;  // queue for all "other" requests (e.g., refresh)

    deque<Request> pending;  // read requests that are about to receive data from DRAM
    bool write_mode = false;  // whether write requests should be prioritized over reads
    //long refreshed = 0;  // last time refresh requests were generated

    /* Constructor */
    Controller(DRAM<T>* channel) :
        channel(channel),
        scheduler(new Scheduler<T>(this)),
        rowpolicy(new RowPolicy<T>(this)),
        rowtable(new RowTable<T>(this)),
		refresh(new Refresh<T>(this))
    {
        if (record_cmd_trace){
            string prefix = cmd_trace_prefix + "chan-" + to_string(channel->id) + "-rank-";
            string suffix = ".cmdtrace";
            for (int i = 0; i < channel->children.size(); i++)
                cmd_trace_files.emplace_back(prefix + to_string(i) + suffix);
        }
    }

    ~Controller(){
        delete scheduler;
        delete rowpolicy;
        delete rowtable;
        delete channel;
        delete refresh;
        for (auto& file : cmd_trace_files)
            file.close();
        cmd_trace_files.clear();
    }

    /* Member Functions */
    Queue& get_queue(Request::Type type)
    {
        switch (int(type)) {
            case int(Request::Type::READ): return readq;
            case int(Request::Type::WRITE): return writeq;
            default: return otherq;
        }
    }

    bool enqueue(Request& req)
    {
        Queue& queue = get_queue(req.type);
        if (queue.max == queue.size())
            return false;

        req.arrive = clk;
        queue.q.push_back(req);
        // shortcut for read requests, if a write to same addr exists
        // necessary for coherence
        if (req.type == Request::Type::READ && find_if(writeq.q.begin(), writeq.q.end(), 
                [req](Request& wreq){ return req.addr == wreq.addr;}) != writeq.q.end()){
            req.depart = clk + 1;
            pending.push_back(req);
            readq.q.pop_back();
        }
        return true;
    }

    void tick()
    {
        clk++;

        /*** 1. Serve completed reads ***/
        if (pending.size()) {
            Request& req = pending[0];
            if (req.depart <= clk) {
                req.callback(req);
                pending.pop_front();
            }
        }

        /*** 2. Refresh scheduler ***/
        refresh->tick_ref();

        /*** 3. Should we schedule writes? ***/
        if (!write_mode) {
            // yes -- write queue is almost full or read queue is empty
            if (writeq.size() >= int(0.8 * writeq.max) || readq.size() == 0)
                write_mode = true;
        }
        else {
            // no -- write queue is almost empty and read queue is not empty
            if (writeq.size() <= int(0.2 * writeq.max) && readq.size() != 0)
                write_mode = false;
        }

        /*** 4. Find the best command to schedule, if any ***/
        Queue* queue = !write_mode ? &readq : &writeq;
        if (otherq.size())
            queue = &otherq;  // "other" requests are rare, so we give them precedence over reads/writes

        auto req = scheduler->get_head(queue->q);
        if (req == queue->q.end() || !is_ready(req)) {
            // we couldn't find a command to schedule -- let's try to be speculative
            auto cmd = T::Command::PRE;
            vector<int> victim = rowpolicy->get_victim(cmd);
            if (!victim.empty()){
                issue_cmd(cmd, victim);
            }
            return;  // nothing more to be done this cycle
        }

        // issue command on behalf of request
        auto cmd = get_first_cmd(req);
        issue_cmd(cmd, get_addr_vec(cmd, req));

        // check whether this is the last command (which finishes the request)
        if (cmd != channel->spec->translate[int(req->type)])
            return;

        // set a future completion time for read requests
        if (req->type == Request::Type::READ) {
            req->depart = clk + channel->spec->read_latency;
            pending.push_back(*req);
        }

        // remove request from queue
        queue->q.erase(req);
    }

    bool is_ready(list<Request>::iterator req)
    {
        typename T::Command cmd = get_first_cmd(req);
        return channel->check(cmd, req->addr_vec.data(), clk);
    }

    bool is_ready(typename T::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check(cmd, addr_vec.data(), clk);
    }

    void update_temp(ALDRAM::Temp current_temperature)
    {
    }

private:
    typename T::Command get_first_cmd(list<Request>::iterator req)
    {
        typename T::Command cmd = channel->spec->translate[int(req->type)];
        return channel->decode(cmd, req->addr_vec.data());
    }

    void issue_cmd(typename T::Command cmd, const vector<int>& addr_vec)
    {
        assert(is_ready(cmd, addr_vec));
        channel->update(cmd, addr_vec.data(), clk);
        rowtable->update(cmd, addr_vec, clk);
        if (record_cmd_trace){
            // select rank
            auto& file = cmd_trace_files[addr_vec[1]];
            string& cmd_name = channel->spec->command_name[int(cmd)];
            file<<clk<<','<<cmd_name;
            // TODO bad coding here
            if (cmd_name == "PREA" || cmd_name == "REF")
                file<<endl;
            else{
                int bank_id = addr_vec[int(T::Level::Bank)];
                if (channel->spec->standard_name == "DDR4" || channel->spec->standard_name == "GDDR5")
                    bank_id += addr_vec[int(T::Level::Bank) - 1] * channel->spec->org_entry.count[int(T::Level::Bank)];
                file<<','<<bank_id<<endl;
            }
        }
        if (print_cmd_trace){
            printf("%5s %10ld:", channel->spec->command_name[int(cmd)].c_str(), clk);
            for (int lev = 0; lev < int(T::Level::MAX); lev++)
                printf(" %5d", addr_vec[lev]);
            printf("\n");
        }
    }
    vector<int> get_addr_vec(typename T::Command cmd, list<Request>::iterator req){
        return req->addr_vec;
    }
};

template <>
vector<int> Controller<SALP>::get_addr_vec(SALP::Command cmd, list<Request>::iterator req);
template <>
bool Controller<SALP>::is_ready(list<Request>::iterator req);
template <>
void Controller<ALDRAM>::update_temp(ALDRAM::Temp current_temperature);
template <>
void Controller<TLDRAM>::tick();

} /*namespace ramulator*/

#endif /*__CONTROLLER_H*/
