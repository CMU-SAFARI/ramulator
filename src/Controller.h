#ifndef __CONTROLLER_H
#define __CONTROLLER_H

#include <cassert>
#include <cstdio>
#include <deque>
#include <fstream>
#include <list>
#include <string>
#include <vector>

#include "Config.h"
#include "DRAM.h"
#include "Refresh.h"
#include "Request.h"
#include "Scheduler.h"
#include "Statistics.h"

using namespace std;

namespace ramulator
{

template <typename T>
class Controller
{
protected:
    // For counting bandwidth
    ScalarStat read_data_amount;
    ScalarStat write_data_amount;

    ScalarStat read_row_hit;
    ScalarStat read_row_miss;
    ScalarStat read_row_conflict;
    ScalarStat write_row_hit;
    ScalarStat write_row_miss;
    ScalarStat write_row_conflict;

    ScalarStat read_latency_sum;
    ScalarStat req_queue_length_sum;
    ScalarStat act_cmd_count;
    ScalarStat read_act_cmd_count;
    ScalarStat non_auto_precharge_count;
    ScalarStat read_non_auto_precharge_count;
    ScalarStat wait_issue_time_sum;
    ScalarStat read_wait_issue_time_sum;
    ScalarStat write_wait_issue_time_sum;
    ScalarStat read_req_queue_length_sum;
    ScalarStat write_req_queue_length_sum;
public:
    /* Member Variables */
    long clk = 0;
    DRAM<T>* channel;

    Scheduler<T>* scheduler;  // determines the highest priority request whose commands will be issued
    RowPolicy<T>* rowpolicy;  // determines the row-policy (e.g., closed-row vs. open-row)
    RowTable<T>* rowtable;  // tracks metadata about rows (e.g., which are open and for how long)
    Refresh<T>* refresh;

    struct Queue {
        list<Request> q;
        unsigned int max = 32;
        unsigned int size() {return q.size();}
    };

    Queue readq;  // queue for read requests
    Queue writeq;  // queue for write requests
    Queue otherq;  // queue for all "other" requests (e.g., refresh)

    deque<Request> pending;  // read requests that are about to receive data from DRAM
    bool write_mode = false;  // whether write requests should be prioritized over reads
    //long refreshed = 0;  // last time refresh requests were generated

    /* Command trace for DRAMPower 3.1 */
    string cmd_trace_prefix = "cmd-trace-";
    vector<ofstream> cmd_trace_files;
    bool record_cmd_trace = false;
    /* Commands to stdout */
    bool print_cmd_trace = false;

    /* Constructor */
    Controller(const Config& configs, DRAM<T>* channel) :
        channel(channel),
        scheduler(new Scheduler<T>(this)),
        rowpolicy(new RowPolicy<T>(this)),
        rowtable(new RowTable<T>(this)),
        refresh(new Refresh<T>(this)),
        cmd_trace_files(channel->children.size())
    {
        record_cmd_trace = configs.record_cmd_trace();
        print_cmd_trace = configs.print_cmd_trace();
        if (record_cmd_trace){
            if (configs["cmd_trace_prefix"] != "") {
              cmd_trace_prefix = configs["cmd_trace_prefix"];
            }
            string prefix = cmd_trace_prefix + "chan-" + to_string(channel->id) + "-rank-";
            string suffix = ".cmdtrace";
            for (unsigned int i = 0; i < channel->children.size(); i++)
                cmd_trace_files[i].open(prefix + to_string(i) + suffix);
        }

        // regStats

        read_row_hit
            .name("read_row_hit_channel_"+to_string(channel->id))
            .desc("Number of row hits for read request per channel")
            .precision(0)
            ;
        read_row_miss
            .name("read_row_miss_channel_"+to_string(channel->id))
            .desc("Number of row misses for read request per channel")
            .precision(0)
            ;
        read_row_conflict
            .name("read_row_conflict_channel_"+to_string(channel->id))
            .desc("Number of row conflicts for read request per channel")
            .precision(0)
            ;

        write_row_hit
            .name("write_row_hit_channel_"+to_string(channel->id))
            .desc("Number of row hits for write request per channel")
            .precision(0)
            ;
        write_row_miss
            .name("write_row_miss_channel_"+to_string(channel->id))
            .desc("Number of row misses for write request per channel")
            .precision(0)
            ;
        write_row_conflict
            .name("write_row_conflict_channel_"+to_string(channel->id))
            .desc("Number of row conflicts for write request per channel")
            .precision(0)
            ;

        read_data_amount
            .name("read_data_amount_"+to_string(channel->id))
            .desc("The total read data amount per channel")
            .precision(0)
            ;
        write_data_amount
            .name("write_data_amount_"+to_string(channel->id))
            .desc("The total write data amount per channel")
            .precision(0)
            ;

        read_latency_sum
            .name("read_latency_sum_"+to_string(channel->id))
            .desc("The memory latency sum for all read requests in this channel")
            .precision(0)
            ;

        req_queue_length_sum
            .name("req_queue_length_sum_"+to_string(channel->id))
            .desc("Sum of read and write queue length per cycle per channel.")
            .precision(0)
            ;

        read_req_queue_length_sum
            .name("read_req_queue_length_sum_"+to_string(channel->id))
            .desc("Sum of read queue length per cycle per channel.")
            .precision(0)
            ;

        write_req_queue_length_sum
            .name("write_req_queue_length_sum_"+to_string(channel->id))
            .desc("Sum of write queue length per cycle per channel.")
            .precision(0)
            ;

        act_cmd_count
            .name("act_cmd_count_"+to_string(channel->id))
            .desc("Number of ACT command per channel")
            .precision(0)
            ;

        read_act_cmd_count
            .name("read_act_cmd_count_"+to_string(channel->id))
            .desc("Number of ACT command by read request per channel")
            .precision(0)
            ;

        non_auto_precharge_count
            .name("non_auto_precharge_count"+to_string(channel->id))
            .desc("Number of non auto precharge count per channel")
            .precision(0)
            ;

        read_non_auto_precharge_count
            .name("read_non_auto_precharge_count"+to_string(channel->id))
            .desc("Number of non auto precharge by read requests per channel")
            .precision(0)
            ;

        wait_issue_time_sum
            .name("wait_issue_time_sum"+to_string(channel->id))
            .desc("The total time that a R/W request is waiting to be issued per channel.")
            .precision(0)
            ;

        read_wait_issue_time_sum
            .name("read_wait_issue_time_sum"+to_string(channel->id))
            .desc("The total time that a READ request is waiting to be issued per channel.")
            .precision(0)
            ;

        write_wait_issue_time_sum
            .name("write_wait_issue_time_sum"+to_string(channel->id))
            .desc("The total time that a WRITE request is waiting to be issued per channel.")
            .precision(0)
            ;

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
        req_queue_length_sum += readq.size() + writeq.size();
        read_req_queue_length_sum += readq.size();
        write_req_queue_length_sum += writeq.size();

        /*** 1. Serve completed reads ***/
        if (pending.size()) {
            Request& req = pending[0];
            if (req.depart <= clk) {
                if (req.depart - req.arrive > 1) { // this request really accessed a row
                  read_latency_sum += req.depart - req.arrive;
                  channel->update_serving_requests(
                      req.addr_vec.data(), -1, clk);
                }
                // FIXME update req.depart with clk?
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

        if (req->is_first_command) {
            req->is_first_command = false;
            if (req->type == Request::Type::READ || req->type == Request::Type::WRITE) {
              wait_issue_time_sum += clk - req->arrive;
              if (req->type == Request::Type::READ) {
                read_wait_issue_time_sum += clk - req->arrive;
              } else {
                write_wait_issue_time_sum += clk - req->arrive;
              }
              channel->update_serving_requests(req->addr_vec.data(), 1, clk);
            }
            int tx = (channel->spec->prefetch_size * channel->spec->channel_width / 8);
            if (req->type == Request::Type::READ) {
                if (is_row_hit(req)) {
                    ++read_row_hit;
                } else if (is_row_open(req)) {
                    ++read_row_conflict;
                } else {
                    ++read_row_miss;
                }
              read_data_amount += tx;
            } else if (req->type == Request::Type::WRITE) {
              if (is_row_hit(req)) {
                  ++write_row_hit;
              } else if (is_row_open(req)) {
                  ++write_row_conflict;
              } else {
                  ++write_row_miss;
              }
              write_data_amount += tx;
            }
        }

        // issue command on behalf of request
        auto cmd = get_first_cmd(req);
        if (cmd == T::Command::PRE) {
          non_auto_precharge_count++;
          if (req->type == Request::Type::READ) {
            read_non_auto_precharge_count++;
          }
        } else if (cmd == T::Command::ACT) {
          act_cmd_count++;
          if (req->type == Request::Type::READ) {
            read_act_cmd_count++;
          }
        }
        issue_cmd(cmd, get_addr_vec(cmd, req));

        // check whether this is the last command (which finishes the request)
        if (cmd != channel->spec->translate[int(req->type)])
            return;

        // set a future completion time for read requests
        if (req->type == Request::Type::READ) {
            req->depart = clk + channel->spec->read_latency;
            pending.push_back(*req);
        }

        if (req->type == Request::Type::WRITE) {
            channel->update_serving_requests(req->addr_vec.data(), -1, clk);
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

    bool is_row_hit(list<Request>::iterator req)
    {
        // cmd must be decided by the request type, not the first cmd
        typename T::Command cmd = channel->spec->translate[int(req->type)];
        return channel->check_row_hit(cmd, req->addr_vec.data());
    }

    bool is_row_hit(typename T::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check_row_hit(cmd, addr_vec.data());
    }

    bool is_row_open(list<Request>::iterator req)
    {
        // cmd must be decided by the request type, not the first cmd
        typename T::Command cmd = channel->spec->translate[int(req->type)];
        return channel->check_row_open(cmd, req->addr_vec.data());
    }

    bool is_row_open(typename T::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check_row_open(cmd, addr_vec.data());
    }

    void update_temp(ALDRAM::Temp current_temperature)
    {
    }

    // For telling whether this channel is busying in processing read or write
    bool is_active() {
      return (channel->cur_serving_requests > 0);
    }

    // For telling whether this channel is under refresh
    bool is_refresh() {
      return clk <= channel->end_of_refreshing;
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

} /*namespace ramulator*/

#endif /*__CONTROLLER_H*/
