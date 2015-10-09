#ifndef __SPEEDYCONTROLLER_H
#define __SPEEDYCONTROLLER_H

#include "Config.h"
#include "DRAM.h"
#include "Request.h"
#include "Statistics.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cassert>
#include <utility>
#include <queue>

using namespace std;

namespace ramulator
{

template <typename T>
class SpeedyController
// A FR-FCFS Open Row Controller, optimized for simulation speed.
// Not For SALP-2
{
protected:
  ScalarStat row_hits;
  ScalarStat row_misses;
private:
    class compair_depart_clk{
    public:
        bool operator()(const Request& lhs, const Request& rhs) {
            return lhs.depart > rhs.depart;
        }
    };
public:
    /* Command trace for DRAMPower 3.1 */
    string cmd_trace_prefix = "cmd-trace-";
    vector<ofstream> cmd_trace_files;
    bool record_cmd_trace = false;
    /* Commands to stdout */
    bool print_cmd_trace = false;
    /* Member Variables */
    const unsigned int queue_capacity = 32;
    long clk = 0;
    DRAM<T>* channel;

    double write_hi = 0.875;
    double write_low = 0.5;

    // request, first command, earliest clk
    typedef tuple<Request, typename T::Command, long> request_info;
    typedef vector<request_info> request_queue;
    request_queue readq;   // queue for read requests
    request_queue writeq;  // queue for write requests
    request_queue otherq;  // queue for all "other" requests (e.g., refresh)

    // read requests that are about to receive data from DRAM
    priority_queue<Request, vector<Request>, compair_depart_clk> pending;

    bool write_mode = false;  // whether write requests should be prioritized over reads
    long refreshed = 0;  // last time refresh requests were generated

    /* Constructor */
    SpeedyController(const Config& configs, DRAM<T>* channel) :
        channel(channel)
    {
        record_cmd_trace = configs.record_cmd_trace();
        print_cmd_trace = configs.print_cmd_trace();
        if (record_cmd_trace){
            string prefix = cmd_trace_prefix + "chan-" + to_string(channel->id) + "-rank-";
            string suffix = ".cmdtrace";
            for (unsigned int i = 0; i < channel->children.size(); i++)
                cmd_trace_files.emplace_back(prefix + to_string(i) + suffix);
        }
        readq.reserve(queue_capacity);
        writeq.reserve(queue_capacity);
        otherq.reserve(queue_capacity);

        // regStats

        row_hits
            .name("row_hits_channel_"+to_string(channel->id))
            .desc("Number of row hits")
            .precision(0)
            ;
        row_misses
            .name("row_misses_channel_"+to_string(channel->id))
            .desc("Number of row misses")
            .precision(0)
            ;
    }

    ~SpeedyController(){
        delete channel;
        for (auto& file : cmd_trace_files)
            file.close();
    }

    /* Member Functions */

    void finish(int read_req, int write_req, int dram_cycles) {
      // call finish function of each channel
      channel->finish(dram_cycles);
    }

    bool enqueue(Request& req)
    {
        request_queue& q =
            req.type == Request::Type::READ? readq:
            req.type == Request::Type::WRITE? writeq:
                                             otherq;
        if (queue_capacity == q.size())
            return false;

        req.arrive = clk;
        if (req.type == Request::Type::READ){
            for (auto& info : writeq)
                if (req.addr == get<0>(info).addr){
                    req.depart = clk + 1;
                    pending.push(req);
                    return true;
                }
        }
        typename T::Command first_cmd = get_first_cmd(req);
        long first_clk = channel->get_next(first_cmd, req.addr_vec.data());
        q.emplace_back(req, first_cmd, first_clk);
        push_heap(q.begin(), q.end(), compair_first_clk);;
        return true;
    }

    void tick()
    {
        clk++;

        /*** 1. Serve completed reads ***/
        if (pending.size()) {
            Request req = pending.top();
            if (req.depart <= clk) {
                req.depart = clk; // actual depart clk
                req.callback(req);
                pending.pop();
            }
        }

        /*** 2. Should we schedule refreshes? ***/
        int refresh_interval = channel->spec->speed_entry.nREFI;
        if (clk - refreshed >= refresh_interval) {
            auto req_type = Request::Type::REFRESH;
            vector<int> addr_vec(int(T::Level::MAX), -1);
            addr_vec[0] = channel->id;
            for (auto child : channel->children) {
                addr_vec[1] = child->id;
                Request req(addr_vec, req_type, NULL);
                bool res = enqueue(req);
                assert(res);
            }

            refreshed = clk;
        }

        /*** 3. Should we schedule writes? ***/
        if (!write_mode) {
            // yes -- write queue is almost full or read queue is empty
            if (writeq.size() >= (unsigned int)(write_hi * queue_capacity) || readq.size() == 0)
                write_mode = true;
        }
        else {
            // no -- write queue is almost empty and read queue is not empty
            if (writeq.size() <= (unsigned int)(write_low * queue_capacity) && readq.size() != 0)
                write_mode = false;
        }

        /*** 4. Find the best command to schedule, if any ***/
        request_queue& q = otherq.size()? otherq: write_mode ? writeq : readq;

        schedule(q);
    }

    bool is_row_hit(Request& req)
    {
        typename T::Command cmd = get_first_cmd(req);
        return channel->check_row_hit(cmd, req.addr_vec.data());
    }

private:

    static bool compair_first_clk(const request_info& lhs, const request_info& rhs) {
        return (get<2>(lhs) > get<2>(rhs));
    }

    typename T::Command get_first_cmd(Request& req)
    {
        typename T::Command cmd = channel->spec->translate[int(req.type)];
        switch (int(req.type)){
            case int(Request::Type::READ):
            case int(Request::Type::WRITE):{
                auto node = channel;
                for (int i = 1; i < int(T::Level::Row); i++)
                    node = node->children[req.addr_vec[i]];
                assert(int(node->level) == int(T::Level::Row) - 1);
                if (node->state == T::State::Closed) return T::Command::ACT;
                else if (node->row_state.find(req.addr_vec[int(T::Level::Row)]) != node->row_state.end()) return cmd;
                else return T::Command::PRE;
            }
            case int(Request::Type::REFRESH):
                return channel->decode(cmd, req.addr_vec.data());
            default:
                assert(false);
        }
        // return channel->decode(cmd, req.addr_vec.data());
    }
    void update(typename T::Command cmd, bool state_change, vector<int>::iterator& begin, vector<int>::iterator& end, request_queue& q){
        if (q.empty()) return;

        for (auto& info : q) {
            bool addr_eq = equal(begin, end, get<0>(info).addr_vec.begin());
            if (state_change && addr_eq)
                get<1>(info) = get_first_cmd(get<0>(info));
            if ((cmd == T::Command::RD || cmd == T::Command::WR)
                && get<1>(info) == T::Command::ACT)
                continue;
            get<2>(info) = channel->get_next(get<1>(info), get<0>(info).addr_vec.data());
        }
        make_heap(q.begin(), q.end(), compair_first_clk);
    }

    void schedule(request_queue& q){
        if (q.empty()) return;

        Request& req = get<0>(q[0]);
        typename T::Command& first_cmd = get<1>(q[0]);
        long first_clk = get<2>(q[0]);

        if (first_clk > clk) return;

        if (req.is_first_command) {
            req.is_first_command = false;
            if (req.type == Request::Type::READ || req.type == Request::Type::WRITE) {
                if (is_row_hit(req))
                    ++row_hits;
                else
                    ++row_misses;
            }
        }

        issue_cmd(first_cmd, req.addr_vec.data());

        if (first_cmd == channel->spec->translate[int(req.type)]){
            if (req.type == Request::Type::READ) {
                req.depart = clk + channel->spec->read_latency;
                pending.push(req);
            }
            pop_heap(q.begin(), q.end(), compair_first_clk);
            q.pop_back();
        }

        bool state_change = channel->spec->is_opening(first_cmd)
                        || channel->spec->is_closing(first_cmd)
                        || channel->spec->is_refreshing(first_cmd);

        auto begin = req.addr_vec.begin();
        auto end = begin + 1;
        for (; end < begin + int(T::Level::Row) && *end >= 0; end++);

        update(first_cmd, state_change, begin, end, readq);
        update(first_cmd, state_change, begin, end, writeq);
        update(first_cmd, state_change, begin, end, otherq);
    }

    void issue_cmd(typename T::Command cmd, int* addr_vec)
    {
        // assert(channel->check(cmd, addr_vec, clk));
        channel->update(cmd, addr_vec, clk);

        if (record_cmd_trace){
            // select rank
            auto& file = cmd_trace_files[addr_vec[1]];
            string& cmd_name = channel->spec->command_name[int(cmd)];
            file<<clk<<','<<cmd_name;
            // TODO bad coding here
            if (cmd_name == "PREA" || cmd_name == "REF")
                file<<endl;
            else {
                int bank_id = addr_vec[int(T::Level::Bank)];
                if (channel->spec->standard_name == "DDR4" || channel->spec->standard_name == "GDDR5")
                    bank_id += addr_vec[int(T::Level::Bank) - 1] *
                        channel->spec->org_entry.count[int(T::Level::Bank)];
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
};

} /*namespace ramulator*/

#endif /*__SPEEDYCONTROLLER_H*/
