#ifndef __HMC_CONTROLLER_H
#define __HMC_CONTROLLER_H

#include <cassert>
#include <cstdio>
#include <deque>
#include <fstream>
#include <list>
#include <string>
#include <vector>

#include "Controller.h"
#include "Scheduler.h"

#include "HMC.h"
#include "Packet.h"

using namespace std;

namespace ramulator
{

template <>
class Controller<HMC>
{
public:
    // For counting bandwidth
    ScalarStat* read_transaction_bytes;
    ScalarStat* write_transaction_bytes;

    ScalarStat* row_hits;
    ScalarStat* row_misses;
    ScalarStat* row_conflicts;
    VectorStat* read_row_hits;
    VectorStat* read_row_misses;
    VectorStat* read_row_conflicts;
    VectorStat* write_row_hits;
    VectorStat* write_row_misses;
    VectorStat* write_row_conflicts;

    ScalarStat* queueing_latency_sum;

    ScalarStat* req_queue_length_sum;
    ScalarStat* read_req_queue_length_sum;
    ScalarStat* write_req_queue_length_sum;

    VectorStat* record_read_hits;
    VectorStat* record_read_misses;
    VectorStat* record_read_conflicts;
    VectorStat* record_write_hits;
    VectorStat* record_write_misses;
    VectorStat* record_write_conflicts;

public:
    /* Member Variables */
    long clk = 0;
    DRAM<HMC>* channel;

    Scheduler<HMC>* scheduler;  // determines the highest priority request whose commands will be issued
    RowPolicy<HMC>* rowpolicy;  // determines the row-policy (e.g., closed-row vs. open-row)
    RowTable<HMC>* rowtable;  // tracks metadata about rows (e.g., which are open and for how long)
    Refresh<HMC>* refresh;

    struct Queue {
        list<Request> q;
        unsigned int max = 32; // TODO queue qize
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

    // ideal DRAM
    bool no_DRAM_latency = false;
    bool unlimit_bandwidth = false;

    // HMC
    deque<Packet> response_packets_buffer;
    map<long, Packet> incoming_packets_buffer;

    /* Constructor */
    Controller(const Config& configs, DRAM<HMC>* channel) :
        channel(channel),
        scheduler(new Scheduler<HMC>(this)),
        rowpolicy(new RowPolicy<HMC>(this)),
        rowtable(new RowTable<HMC>(this)),
        refresh(new Refresh<HMC>(this)),
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
        if (configs["no_DRAM_latency"] == "true") {
          no_DRAM_latency = true;
          scheduler->type = Scheduler<HMC>::Type::FRFCFS;
        }
        if (configs["unlimit_bandwidth"] == "true") {
          unlimit_bandwidth = true;
          printf("nBL: %d\n", channel->spec->speed_entry.nBL);
          channel->spec->speed_entry.nBL = 0;
          channel->spec->read_latency = channel->spec->speed_entry.nCL;
          channel->spec->speed_entry.nCCDS = 1;
          channel->spec->speed_entry.nCCDL = 1;
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

    bool receive (Packet& packet) {
      assert(packet.type == Packet::Type::REQUEST);
      Request& req = packet.req;
      req.burst_count = channel->spec->burst_count;
      req.transaction_bytes = channel->spec->payload_flits * 16;
      debug_hmc("req.burst_count %d", req.burst_count);
      debug_hmc("req.reqid %d, req.coreid %d", req.reqid, req.coreid);
      // buffer packet, for future response packet
      incoming_packets_buffer[req.reqid] = packet;
      return enqueue(req);
    }

    void finish(long dram_cycles) {
      channel->finish(dram_cycles);
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

    Packet form_response_packet(Request& req) {
      // All packets sent from host controller are Request packets
      assert(incoming_packets_buffer.find(req.reqid) !=
          incoming_packets_buffer.end());
      Packet req_packet = incoming_packets_buffer[req.reqid];
      int cub = req_packet.header.CUB.value;
      int tag = req_packet.header.TAG.value;
      int slid = req_packet.tail.SLID.value;
      int lng = req.type == Request::Type::WRITE ?
                1 : 1 + channel->spec->payload_flits;
      Packet::Command cmd = req_packet.header.CMD.value;
      Packet packet(Packet::Type::RESPONSE, cub, tag, lng, slid, cmd);
      packet.req = req;
      debug_hmc("cub: %d", cub);
      debug_hmc("slid: %d", slid);
      debug_hmc("lng: %d", lng);
      debug_hmc("cmd: %d", int(cmd));
      // DEBUG:
      assert(packet.header.CUB.valid());
      assert(packet.header.TAG.valid()); // -1 also considered valid here...
      assert(packet.header.SLID.valid());
      assert(packet.header.CMD.valid());
      // Don't forget to release the space for incoming packet
      incoming_packets_buffer.erase(req.reqid);
      return packet;
    }

    void tick()
    {
        // FIXME back to back command (add back-to-back buffer)
        clk++;
        (*req_queue_length_sum) += readq.size() + writeq.size() + pending.size();
        (*read_req_queue_length_sum) += readq.size() + pending.size();
        (*write_req_queue_length_sum) += writeq.size();

        /*** 1. Serve completed reads ***/
        if (pending.size()) {
          Request& req = pending[0];
          if (req.depart <= clk) {
            if (req.depart - req.arrive > 1) {
              channel->update_serving_requests(req.addr_vec.data(), -1, clk);
            }
            Packet packet = form_response_packet(req);
            response_packets_buffer.push_back(packet);
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
          if (!no_DRAM_latency) {
            // we couldn't find a command to schedule -- let's try to be speculative
            auto cmd = HMC::Command::PRE;
            vector<int> victim = rowpolicy->get_victim(cmd);
            if (!victim.empty()){
                issue_cmd(cmd, victim);
            }
            return;  // nothing more to be done this cycle
          } else {
            return;
          }
        }

        if (req->is_first_command) {
          req->is_first_command = false;
          int coreid = req->coreid;
          if (req->type == Request::Type::READ || req->type == Request::Type::WRITE) {
            channel->update_serving_requests(req->addr_vec.data(), 1, clk);
          }
          if (req->type == Request::Type::READ) {
            (*queueing_latency_sum) += clk - req->arrive;
            if (is_row_hit(req)) {
                ++(*read_row_hits)[coreid];
                ++(*row_hits);
                debug_hmc("row hit");
            } else if (is_row_open(req)) {
                ++(*read_row_conflicts)[coreid];
                ++(*row_conflicts);
                debug_hmc("row conlict");
            } else {
                ++(*read_row_misses)[coreid];
                ++(*row_misses);
                debug_hmc("row miss");
            }
            (*read_transaction_bytes) += req->transaction_bytes;
          } else if (req->type == Request::Type::WRITE) {
            if (is_row_hit(req)) {
                ++(*write_row_hits)[coreid];
                ++(*row_hits);
            } else if (is_row_open(req)) {
                ++(*write_row_conflicts)[coreid];
                ++(*row_conflicts);
            } else {
                ++(*write_row_misses)[coreid];
                ++(*row_misses);
            }
            (*write_transaction_bytes) += req->transaction_bytes;
          }
        }

        // issue command on behalf of request
        auto cmd = get_first_cmd(req);
        issue_cmd(cmd, get_addr_vec(cmd, req));

        // check whether this is the last command (which finishes the request)
        if (cmd != channel->spec->translate[int(req->type)])
            return;

        // set a future completion time for read requests
        if (req->type == Request::Type::READ) {
            --req->burst_count;
            if (req->burst_count == 0) {
              req->depart = clk + channel->spec->read_latency;
              debug_hmc("req->depart: %ld\n", req->depart);
              pending.push_back(*req);
            }
        } else if (req->type == Request::Type::WRITE) {
            --req->burst_count;
            if (req->burst_count == 0) {
              Packet packet = form_response_packet(*req);
              response_packets_buffer.push_back(packet);
              channel->update_serving_requests(req->addr_vec.data(), -1, clk);
            }
        }

        // remove request from queue
        if (req->burst_count == 0) {
          queue->q.erase(req);
        }
    }

    bool is_ready(list<Request>::iterator req)
    {
        typename HMC::Command cmd = get_first_cmd(req);
        return channel->check(cmd, req->addr_vec.data(), clk);
    }

    bool is_ready(typename HMC::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check(cmd, addr_vec.data(), clk);
    }

    bool is_row_hit(list<Request>::iterator req)
    {
        // cmd must be decided by the request type, not the first cmd
        typename HMC::Command cmd = channel->spec->translate[int(req->type)];
        return channel->check_row_hit(cmd, req->addr_vec.data());
    }

    bool is_row_hit(typename HMC::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check_row_hit(cmd, addr_vec.data());
    }

    bool is_row_open(list<Request>::iterator req)
    {
        // cmd must be decided by the request type, not the first cmd
        typename HMC::Command cmd = channel->spec->translate[int(req->type)];
        return channel->check_row_open(cmd, req->addr_vec.data());
    }

    bool is_row_open(typename HMC::Command cmd, const vector<int>& addr_vec)
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

    void record_core(int coreid) {
      (*record_read_hits)[coreid] = (*read_row_hits)[coreid];
      (*record_read_misses)[coreid] = (*read_row_misses)[coreid];
      (*record_read_conflicts)[coreid] = (*read_row_conflicts)[coreid];
      (*record_write_hits)[coreid] = (*write_row_hits)[coreid];
      (*record_write_misses)[coreid] = (*write_row_misses)[coreid];
      (*record_write_conflicts)[coreid] = (*write_row_conflicts)[coreid];
    }

private:
    typename HMC::Command get_first_cmd(list<Request>::iterator req)
    {
        typename HMC::Command cmd = channel->spec->translate[int(req->type)];
        if (!no_DRAM_latency) {
          return channel->decode(cmd, req->addr_vec.data());
        } else {
          return cmd;
        }
    }

    void issue_cmd(typename HMC::Command cmd, const vector<int>& addr_vec)
    {
        if (print_cmd_trace){
            printf("%5s %10ld:", channel->spec->command_name[int(cmd)].c_str(), clk);
            for (int lev = 0; lev < int(HMC::Level::MAX); lev++)
                printf(" %5d", addr_vec[lev]);
            printf("\n");
        }
        assert(is_ready(cmd, addr_vec));
        if (!no_DRAM_latency) {
          channel->update(cmd, addr_vec.data(), clk);
          rowtable->update(cmd, addr_vec, clk);
        } else {
          // still have bandwidth restriction (update timing for RD/WR requets)
          channel->update_timing(cmd, addr_vec.data(), clk);
        }
        if (record_cmd_trace){
            // select rank
            auto& file = cmd_trace_files[addr_vec[1]];
            string& cmd_name = channel->spec->command_name[int(cmd)];
            file<<clk<<','<<cmd_name;
            // TODO bad coding here
            if (cmd_name == "PREA" || cmd_name == "REF")
                file<<endl;
            else{
                int bank_id = addr_vec[int(HMC::Level::Bank)];
                if (channel->spec->standard_name == "DDR4" || channel->spec->standard_name == "GDDR5")
                    bank_id += addr_vec[int(HMC::Level::Bank) - 1] * channel->spec->org_entry.count[int(HMC::Level::Bank)];
                file<<','<<bank_id<<endl;
            }
        }
    }
    vector<int> get_addr_vec(typename HMC::Command cmd, list<Request>::iterator req){
        return req->addr_vec;
    }
};

} /*namespace ramulator*/

#endif /*__HMC_CONTROLLER_H*/
