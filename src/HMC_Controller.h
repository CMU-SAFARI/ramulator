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

#include "libdrampower/LibDRAMPower.h"
#include "xmlparser/MemSpecParser.h"

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
    // DRAM power estimation statistics

    ScalarStat act_energy;
    ScalarStat pre_energy;
    ScalarStat read_energy;
    ScalarStat write_energy;

    ScalarStat act_stdby_energy;
    ScalarStat pre_stdby_energy;
    ScalarStat idle_energy_act;
    ScalarStat idle_energy_pre;

    ScalarStat f_act_pd_energy;
    ScalarStat f_pre_pd_energy;
    ScalarStat s_act_pd_energy;
    ScalarStat s_pre_pd_energy;
    ScalarStat sref_energy;
    ScalarStat sref_ref_energy;
    ScalarStat sref_ref_act_energy;
    ScalarStat sref_ref_pre_energy;

    ScalarStat spup_energy;
    ScalarStat spup_ref_energy;
    ScalarStat spup_ref_act_energy;
    ScalarStat spup_ref_pre_energy;
    ScalarStat pup_act_energy;
    ScalarStat pup_pre_energy;

    ScalarStat IO_power;
    ScalarStat WR_ODT_power;
    ScalarStat TermRD_power;
    ScalarStat TermWR_power;

    ScalarStat read_io_energy;
    ScalarStat write_term_energy;
    ScalarStat read_oterm_energy;
    ScalarStat write_oterm_energy;
    ScalarStat io_term_energy;

    ScalarStat ref_energy;

    ScalarStat total_energy;
    ScalarStat average_power;

    // drampower counter

    // Number of activate commands
    ScalarStat numberofacts_s;
    // Number of precharge commands
    ScalarStat numberofpres_s;
    // Number of reads commands
    ScalarStat numberofreads_s;
    // Number of writes commands
    ScalarStat numberofwrites_s;
    // Number of refresh commands
    ScalarStat numberofrefs_s;
    // Number of precharge cycles
    ScalarStat precycles_s;
    // Number of active cycles
    ScalarStat actcycles_s;
    // Number of Idle cycles in the active state
    ScalarStat idlecycles_act_s;
    // Number of Idle cycles in the precharge state
    ScalarStat idlecycles_pre_s;
    // Number of fast-exit activate power-downs
    ScalarStat f_act_pdns_s;
    // Number of slow-exit activate power-downs
    ScalarStat s_act_pdns_s;
    // Number of fast-exit precharged power-downs
    ScalarStat f_pre_pdns_s;
    // Number of slow-exit activate power-downs
    ScalarStat s_pre_pdns_s;
    // Number of self-refresh commands
    ScalarStat numberofsrefs_s;
    // Number of clock cycles in fast-exit activate power-down mode
    ScalarStat f_act_pdcycles_s;
    // Number of clock cycles in slow-exit activate power-down mode
    ScalarStat s_act_pdcycles_s;
    // Number of clock cycles in fast-exit precharged power-down mode
    ScalarStat f_pre_pdcycles_s;
    // Number of clock cycles in slow-exit precharged power-down mode
    ScalarStat s_pre_pdcycles_s;
    // Number of clock cycles in self-refresh mode
    ScalarStat sref_cycles_s;
    // Number of clock cycles in activate power-up mode
    ScalarStat pup_act_cycles_s;
    // Number of clock cycles in precharged power-up mode
    ScalarStat pup_pre_cycles_s;
    // Number of clock cycles in self-refresh power-up mode
    ScalarStat spup_cycles_s;

    // Number of active auto-refresh cycles in self-refresh mode
    ScalarStat sref_ref_act_cycles_s;
    // Number of precharged auto-refresh cycles in self-refresh mode
    ScalarStat sref_ref_pre_cycles_s;
    // Number of active auto-refresh cycles during self-refresh exit
    ScalarStat spup_ref_act_cycles_s;
    // Number of precharged auto-refresh cycles during self-refresh exit
    ScalarStat spup_ref_pre_cycles_s;

    libDRAMPower* drampower;
    long update_counter = 0;

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
    ofstream cmd_trace_file;
    bool record_cmd_trace = false;
    /* Commands to stdout */
    bool print_cmd_trace = false;
    bool with_drampower = false;

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
        refresh(new Refresh<HMC>(this))
    {
        record_cmd_trace = configs.record_cmd_trace();
        print_cmd_trace = configs.print_cmd_trace();
        if (record_cmd_trace){
            if (configs["cmd_trace_prefix"] != "") {
              cmd_trace_prefix = configs["cmd_trace_prefix"];
            }
            cmd_trace_file.open(
                cmd_trace_prefix + "chan-" + to_string(channel->id)
                + ".cmdtrace");
        }
        if (configs["drampower_memspecs"] != "") {
          with_drampower = true;
          drampower = new libDRAMPower(
              Data::MemSpecParser::getMemSpecFromXML(
                  configs["drampower_memspecs"]),
              true);
        }
        if (configs["no_DRAM_latency"] == "true") {
          no_DRAM_latency = true;
          scheduler->type = Scheduler<HMC>::Type::FRFCFS;
        }
        if (configs["unlimit_bandwidth"] == "true") {
          unlimit_bandwidth = true;
          printf("nBL: %d\n", channel->spec->speed_entry.nBL);
          assert(channel->spec->speed_entry.nBL == 0);
          assert(channel->spec->read_latency == channel->spec->speed_entry.nCL);
          assert(channel->spec->speed_entry.nCCDS == 1);
          assert(channel->spec->speed_entry.nCCDL == 1);
        }

        if (with_drampower) {
          // init DRAMPower stats
          act_energy
              .name("act_energy_" + to_string(channel->id))
              .desc("act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          pre_energy
              .name("pre_energy_" + to_string(channel->id))
              .desc("pre_energy_" + to_string(channel->id))
              .precision(6)
              ;
          read_energy
              .name("read_energy_" + to_string(channel->id))
              .desc("read_energy_" + to_string(channel->id))
              .precision(6)
              ;
          write_energy
              .name("write_energy_" + to_string(channel->id))
              .desc("write_energy_" + to_string(channel->id))
              .precision(6)
              ;

          act_stdby_energy
              .name("act_stdby_energy_" + to_string(channel->id))
              .desc("act_stdby_energy_" + to_string(channel->id))
              .precision(6)
              ;

          pre_stdby_energy
              .name("pre_stdby_energy_" + to_string(channel->id))
              .desc("pre_stdby_energy_" + to_string(channel->id))
              .precision(6)
              ;

          idle_energy_act
              .name("idle_energy_act_" + to_string(channel->id))
              .desc("idle_energy_act_" + to_string(channel->id))
              .precision(6)
              ;

          idle_energy_pre
              .name("idle_energy_pre_" + to_string(channel->id))
              .desc("idle_energy_pre_" + to_string(channel->id))
              .precision(6)
              ;

          f_act_pd_energy
              .name("f_act_pd_energy_" + to_string(channel->id))
              .desc("f_act_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          f_pre_pd_energy
              .name("f_pre_pd_energy_" + to_string(channel->id))
              .desc("f_pre_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          s_act_pd_energy
              .name("s_act_pd_energy_" + to_string(channel->id))
              .desc("s_act_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          s_pre_pd_energy
              .name("s_pre_pd_energy_" + to_string(channel->id))
              .desc("s_pre_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_energy
              .name("sref_energy_" + to_string(channel->id))
              .desc("sref_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_ref_energy
              .name("sref_ref_energy_" + to_string(channel->id))
              .desc("sref_ref_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_ref_act_energy
              .name("sref_ref_act_energy_" + to_string(channel->id))
              .desc("sref_ref_act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_ref_pre_energy
              .name("sref_ref_pre_energy_" + to_string(channel->id))
              .desc("sref_ref_pre_energy_" + to_string(channel->id))
              .precision(6)
              ;

          spup_energy
              .name("spup_energy_" + to_string(channel->id))
              .desc("spup_energy_" + to_string(channel->id))
              .precision(6)
              ;
          spup_ref_energy
              .name("spup_ref_energy_" + to_string(channel->id))
              .desc("spup_ref_energy_" + to_string(channel->id))
              .precision(6)
              ;
          spup_ref_act_energy
              .name("spup_ref_act_energy_" + to_string(channel->id))
              .desc("spup_ref_act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          spup_ref_pre_energy
              .name("spup_ref_pre_energy_" + to_string(channel->id))
              .desc("spup_ref_pre_energy_" + to_string(channel->id))
              .precision(6)
              ;
          pup_act_energy
              .name("pup_act_energy_" + to_string(channel->id))
              .desc("pup_act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          pup_pre_energy
              .name("pup_pre_energy_" + to_string(channel->id))
              .desc("pup_pre_energy_" + to_string(channel->id))
              .precision(6)
              ;

          IO_power
              .name("IO_power_" + to_string(channel->id))
              .desc("IO_power_" + to_string(channel->id))
              .precision(6)
              ;
          WR_ODT_power
              .name("WR_ODT_power_" + to_string(channel->id))
              .desc("WR_ODT_power_" + to_string(channel->id))
              .precision(6)
              ;
          TermRD_power
              .name("TermRD_power_" + to_string(channel->id))
              .desc("TermRD_power_" + to_string(channel->id))
              .precision(6)
              ;
          TermWR_power
              .name("TermWR_power_" + to_string(channel->id))
              .desc("TermWR_power_" + to_string(channel->id))
              .precision(6)
              ;

          read_io_energy
              .name("read_io_energy_" + to_string(channel->id))
              .desc("read_io_energy_" + to_string(channel->id))
              .precision(6)
              ;
          write_term_energy
              .name("write_term_energy_" + to_string(channel->id))
              .desc("write_term_energy_" + to_string(channel->id))
              .precision(6)
              ;
          read_oterm_energy
              .name("read_oterm_energy_" + to_string(channel->id))
              .desc("read_oterm_energy_" + to_string(channel->id))
              .precision(6)
              ;
          write_oterm_energy
              .name("write_oterm_energy_" + to_string(channel->id))
              .desc("write_oterm_energy_" + to_string(channel->id))
              .precision(6)
              ;
          io_term_energy
              .name("io_term_energy_" + to_string(channel->id))
              .desc("io_term_energy_" + to_string(channel->id))
              .precision(6)
              ;

          ref_energy
              .name("ref_energy_" + to_string(channel->id))
              .desc("ref_energy_" + to_string(channel->id))
              .precision(6)
              ;

          total_energy
              .name("total_energy_" + to_string(channel->id))
              .desc("total_energy_" + to_string(channel->id))
              .precision(6)
              ;
          average_power
              .name("average_power_" + to_string(channel->id))
              .desc("average_power_" + to_string(channel->id))
              .precision(6)
              ;

          numberofacts_s
              .name("numberofacts_s_" + to_string(channel->id))
              .desc("Number of activate commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofpres_s
              .name("numberofpres_s_" + to_string(channel->id))
              .desc("Number of precharge commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofreads_s
              .name("numberofreads_s_" + to_string(channel->id))
              .desc("Number of reads commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofwrites_s
              .name("numberofwrites_s_" + to_string(channel->id))
              .desc("Number of writes commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofrefs_s
              .name("numberofrefs_s_" + to_string(channel->id))
              .desc("Number of refresh commands_" + to_string(channel->id))
              .precision(0)
              ;
          precycles_s
              .name("precycles_s_" + to_string(channel->id))
              .desc("Number of precharge cycles_" + to_string(channel->id))
              .precision(0)
              ;
          actcycles_s
              .name("actcycles_s_" + to_string(channel->id))
              .desc("Number of active cycles_" + to_string(channel->id))
              .precision(0)
              ;
          idlecycles_act_s
              .name("idlecycles_act_s_" + to_string(channel->id))
              .desc("Number of Idle cycles in the active state_" + to_string(channel->id))
              .precision(0)
              ;
          idlecycles_pre_s
              .name("idlecycles_pre_s_" + to_string(channel->id))
              .desc("Number of Idle cycles in the precharge state_" + to_string(channel->id))
              .precision(0)
              ;
          f_act_pdns_s
              .name("f_act_pdns_s_" + to_string(channel->id))
              .desc("Number of fast-exit activate power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          s_act_pdns_s
              .name("s_act_pdns_s_" + to_string(channel->id))
              .desc("Number of slow-exit activate power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          f_pre_pdns_s
              .name("f_pre_pdns_s_" + to_string(channel->id))
              .desc("Number of fast-exit precharged power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          s_pre_pdns_s
              .name("s_pre_pdns_s_" + to_string(channel->id))
              .desc("Number of slow-exit activate power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          numberofsrefs_s
              .name("numberofsrefs_s_" + to_string(channel->id))
              .desc("Number of self-refresh commands_" + to_string(channel->id))
              .precision(0)
              ;
          f_act_pdcycles_s
              .name("f_act_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in fast-exit activate power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          s_act_pdcycles_s
              .name("s_act_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in slow-exit activate power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          f_pre_pdcycles_s
              .name("f_pre_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in fast-exit precharged power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          s_pre_pdcycles_s
              .name("s_pre_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in slow-exit precharged power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          sref_cycles_s
              .name("sref_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in self-refresh mode_" + to_string(channel->id))
              .precision(0)
              ;
          pup_act_cycles_s
              .name("pup_act_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in activate power-up mode_" + to_string(channel->id))
              .precision(0)
              ;
          pup_pre_cycles_s
              .name("pup_pre_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in precharged power-up mode_" + to_string(channel->id))
              .precision(0)
              ;
          spup_cycles_s
              .name("spup_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in self-refresh power-up mode_" + to_string(channel->id))
              .precision(0)
              ;
          sref_ref_act_cycles_s
              .name("sref_ref_act_cycles_s_" + to_string(channel->id))
              .desc("Number of active auto-refresh cycles in self-refresh mode_" + to_string(channel->id))
              .precision(0)
              ;
          sref_ref_pre_cycles_s
              .name("sref_ref_pre_cycles_s_" + to_string(channel->id))
              .desc("Number of precharged auto-refresh cycles in self-refresh mode_" + to_string(channel->id))
              .precision(0)
              ;
          spup_ref_act_cycles_s
              .name("spup_ref_act_cycles_s_" + to_string(channel->id))
              .desc("Number of active auto-refresh cycles during self-refresh exit_" + to_string(channel->id))
              .precision(0)
              ;
          spup_ref_pre_cycles_s
              .name("spup_ref_pre_cycles_s_" + to_string(channel->id))
              .desc("Number of precharged auto-refresh cycles during self-refresh exit_" + to_string(channel->id))
              .precision(0)
              ;
        }
    }

    ~Controller(){
        delete scheduler;
        delete rowpolicy;
        delete rowtable;
        delete channel;
        delete refresh;
        cmd_trace_file.close();
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
      // finalize DRAMPower
      if (with_drampower) {
        drampower->updateCounters(true); // last update
        drampower->calcEnergy();
        act_energy = drampower->getEnergy().act_energy;
        pre_energy = drampower->getEnergy().pre_energy;
        read_energy = drampower->getEnergy().read_energy;
        write_energy = drampower->getEnergy().write_energy;

        act_stdby_energy = drampower->getEnergy().act_stdby_energy;
        pre_stdby_energy = drampower->getEnergy().pre_stdby_energy;
        idle_energy_act = drampower->getEnergy().idle_energy_act;
        idle_energy_pre = drampower->getEnergy().idle_energy_pre;

        f_act_pd_energy = drampower->getEnergy().f_act_pd_energy;
        f_pre_pd_energy = drampower->getEnergy().f_pre_pd_energy;
        s_act_pd_energy = drampower->getEnergy().s_act_pd_energy;
        s_pre_pd_energy = drampower->getEnergy().s_pre_pd_energy;
        sref_energy = drampower->getEnergy().sref_energy;
        sref_ref_energy = drampower->getEnergy().sref_ref_energy;
        sref_ref_act_energy = drampower->getEnergy().sref_ref_act_energy;
        sref_ref_pre_energy = drampower->getEnergy().sref_ref_pre_energy;

        spup_energy = drampower->getEnergy().spup_energy;
        spup_ref_energy = drampower->getEnergy().spup_ref_energy;
        spup_ref_act_energy = drampower->getEnergy().spup_ref_act_energy;
        spup_ref_pre_energy = drampower->getEnergy().spup_ref_pre_energy;
        pup_act_energy = drampower->getEnergy().pup_act_energy;
        pup_pre_energy = drampower->getEnergy().pup_pre_energy;

        IO_power = drampower->getPower().IO_power;
        WR_ODT_power = drampower->getPower().WR_ODT_power;
        TermRD_power = drampower->getPower().TermRD_power;
        TermWR_power = drampower->getPower().TermWR_power;

        read_io_energy = drampower->getEnergy().read_io_energy;
        write_term_energy = drampower->getEnergy().write_term_energy;
        read_oterm_energy = drampower->getEnergy().read_oterm_energy;
        write_oterm_energy = drampower->getEnergy().write_oterm_energy;
        io_term_energy = drampower->getEnergy().io_term_energy;

        ref_energy = drampower->getEnergy().ref_energy;

        total_energy = drampower->getEnergy().total_energy;
        average_power = drampower->getPower().average_power;
        //drampower counter
        numberofacts_s = drampower->counters.numberofacts;
        numberofpres_s = drampower->counters.numberofpres;
        numberofreads_s = drampower->counters.numberofreads;
        numberofwrites_s = drampower->counters.numberofwrites;
        numberofrefs_s = drampower->counters.numberofrefs;
        precycles_s = drampower->counters.precycles;
        actcycles_s = drampower->counters.actcycles;
        idlecycles_act_s = drampower->counters.idlecycles_act;
        idlecycles_pre_s = drampower->counters.idlecycles_pre;
        f_act_pdns_s = drampower->counters.f_act_pdns;
        s_act_pdns_s = drampower->counters.s_act_pdns;
        f_pre_pdns_s = drampower->counters.f_pre_pdns;
        s_pre_pdns_s = drampower->counters.s_pre_pdns;
        numberofsrefs_s = drampower->counters.numberofsrefs;
        f_act_pdcycles_s = drampower->counters.f_act_pdcycles;
        s_act_pdcycles_s = drampower->counters.s_act_pdcycles;
        f_pre_pdcycles_s = drampower->counters.f_pre_pdcycles;
        s_pre_pdcycles_s = drampower->counters.s_pre_pdcycles;
        sref_cycles_s = drampower->counters.sref_cycles;
        pup_act_cycles_s = drampower->counters.pup_act_cycles;
        pup_pre_cycles_s = drampower->counters.pup_pre_cycles;
        spup_cycles_s = drampower->counters.spup_cycles;
        sref_ref_act_cycles_s = drampower->counters.sref_ref_act_cycles;
        sref_ref_pre_cycles_s = drampower->counters.sref_ref_pre_cycles;
        spup_ref_act_cycles_s = drampower->counters.spup_ref_act_cycles;
        spup_ref_pre_cycles_s = drampower->counters.spup_ref_pre_cycles;
      }

      // finalize DRAM status
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
        // update power estimation
        if (with_drampower) {

          const string& cmd_name = channel->spec->command_name[int(cmd)];
          int bank_id = addr_vec[int(HMC::Level::Bank)];
          bank_id += addr_vec[int(HMC::Level::Bank) - 1] * channel->spec->org_entry.count[int(HMC::Level::Bank)];
          printf("%ld %d\n", clk, bank_id);
          drampower->doCommand(Data::MemCommand::getTypeFromName(cmd_name), bank_id, clk);

          update_counter++;
          if (update_counter == 1000000) {
              drampower->updateCounters(false); // not the last update
              update_counter = 0;
          }
        }

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
            string& cmd_name = channel->spec->command_name[int(cmd)];
            cmd_trace_file<<clk<<','<<cmd_name;
            // TODO bad coding here
            if (cmd_name == "PREA" || cmd_name == "REF")
                cmd_trace_file<<endl;
            else{
                int bank_id = addr_vec[int(HMC::Level::Bank)];
                bank_id += addr_vec[int(HMC::Level::Bank) - 1] * channel->spec->org_entry.count[int(HMC::Level::Bank)];
                cmd_trace_file<<','<<bank_id<<endl;
            }
        }
    }
    vector<int> get_addr_vec(typename HMC::Command cmd, list<Request>::iterator req){
        return req->addr_vec;
    }
};

} /*namespace ramulator*/

#endif /*__HMC_CONTROLLER_H*/
