#ifndef __HMC_H
#define __HMC_H

#include "DRAM.h"
#include "Request.h"
#include <vector>
#include <map>
#include <string>
#include <functional>

using namespace std;

namespace ramulator
{

class HMC
{
public:
    static string standard_name;
    enum class Org;
    enum class Speed;
    enum class MaxBlock;
    enum class LinkWidth;
    enum class LaneSpeed;
    HMC(Org org, Speed speed, MaxBlock maxblock, LinkWidth linkwidth, LaneSpeed lanespeed, int source_links, int payload_flits);
    HMC(const string& org_str, const string& speed_str, const string& maxblock_str, const string& linkwidth_str, const string& lanespeed_str, int source_links, int payload_flits);
    
    static map<string, enum Org> org_map;
    static map<string, enum Speed> speed_map;
    static map<string, enum MaxBlock> maxblock_map;
    static map<string, enum LinkWidth> linkwidth_map;
    static map<string, enum LaneSpeed> lanespeed_map;
    /*** Level ***/
    enum class Level : int
    { 
        Vault, BankGroup, Bank, Row, Column, MAX
    };

    /*** Command ***/
    enum class Command : int
    { 
        ACT, PRE, PREA, 
        RD,  WR,  RDA,  WRA, 
        REF, PDE, PDX,  SRE, SRX, 
        MAX
    };

    string command_name[int(Command::MAX)] = {
        "ACT", "PRE", "PREA", 
        "RD",  "WR",  "RDA",  "WRA", 
        "REF", "PDE", "PDX",  "SRE", "SRX"
    };

    // HMC doesn't have rank level[1] so every commands that end at rank level
    // are at vault level now.
    Level scope[int(Command::MAX)] = {
        Level::Row,    Level::Bank,   Level::Vault,   
        Level::Column, Level::Column, Level::Column, Level::Column,
        Level::Vault,   Level::Vault,   Level::Vault,   Level::Vault,   Level::Vault
    };

    bool is_opening(Command cmd) 
    {
        switch(int(cmd)) {
            case int(Command::ACT):
                return true;
            default:
                return false;
        }
    }

    bool is_accessing(Command cmd) 
    {
        switch(int(cmd)) {
            case int(Command::RD):
            case int(Command::WR):
            case int(Command::RDA):
            case int(Command::WRA):
                return true;
            default:
                return false;
        }
    }

    bool is_closing(Command cmd) 
    {
        switch(int(cmd)) {
            case int(Command::RDA):
            case int(Command::WRA):
            case int(Command::PRE):
            case int(Command::PREA):
                return true;
            default:
                return false;
        }
    }

    bool is_refreshing(Command cmd) 
    {
        switch(int(cmd)) {
            case int(Command::REF):
                return true;
            default:
                return false;
        }
    }


    /* State */
    enum class State : int
    {
        Opened, Closed, PowerUp, ActPowerDown, PrePowerDown, SelfRefresh, MAX
    } start[int(Level::MAX)] = {
        State::MAX, State::PowerUp, State::Closed, State::Closed, State::MAX
    };

    /* Translate */
    Command translate[int(Request::Type::MAX)] = {
        Command::RD,  Command::WR,
        Command::REF, Command::PDE, Command::SRE
    };

    /* Prerequisite */
    function<Command(DRAM<HMC>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    // SAUGATA: added function object container for row hit status
    /* Row hit */
    function<bool(DRAM<HMC>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];
    function<bool(DRAM<HMC>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

    /* Timing */
    struct TimingEntry
    {
        Command cmd;
        int dist;
        int val;
        bool sibling;
    }; 
    vector<TimingEntry> timing[int(Level::MAX)][int(Command::MAX)];

    /* Lambda */
    function<void(DRAM<HMC>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    /* Organization */
    enum class Org : int
    {
      HMC_4GB,
      HMC_8GB,
      MAX
    };

    struct OrgEntry {
        int size;
        int dq;
        int count[int(Level::MAX)];
    } org_table[int(Org::MAX)] = {
        {32<<10, 32, {32, 4, 2, 1<<11, 1<<16}},
        {32<<10, 32, {32, 8, 2, 1<<11, 1<<16}}
    }, org_entry;

    /* Speed */
    enum class Speed : int
    {
        HMC_2500,
        MAX
    };

    int prefetch_size = 8; // 8n prefetch DDR
    int channel_width = 32; // 32 TSV per vault [2]

    struct SpeedEntry {
        int rate;
        double freq, tCK;
        int nBL, nCCD;
        int nCL, nRCD, nRP, nCWL;
        int nRAS, nRC;
        int nRTP, nWTR, nWR;
        int nRRD, nFAW;
        int nRFC, nREFI;
        int nPD, nXP, nXPDLL;
        int nCKESR, nXS, nXSDLL;
    } speed_table[int(Speed::MAX)] = {
        {2500, 1250, 1.25, 4, 4, 11,  11,  11, -1, 22, 33, -1, -1, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    }, speed_entry;

    int read_latency;

    enum class MaxBlock: int {
      HMC_32B, HMC_64B, HMC_128B, HMC_256B, MAX
    };

    struct MaxBlockEntry {
      int max_block_size;
      int flit_num_bits;
    } maxblock_table[int(MaxBlock::MAX)] = {
      {32, 1}, {64, 2}, {128, 3}, {256, 4},
    }, maxblock_entry;

    enum class LinkWidth: int {
      HMC_Full_Width, HMC_Half_Width, HMC_Quarter_Width, MAX
    };

    int link_width_table[int(LinkWidth::MAX)] = { 16, 8, 4, };
    int link_width;

    enum class LaneSpeed: int {
      HMC_12_5_Gbps, HMC_15_Gbps, HMC_25_Gbps, HMC_28_Gbps, HMC_30_Gbps, MAX
    };

    double lane_speed_table[int(LaneSpeed::MAX)] = { 12.5, 15, 25, 28, 30, };
    double lane_speed;

    int source_links = 0; // number of host links in source mode, must be power of 2
    int pass_thru_links = 0;
    int payload_flits = 4; // TODO to make it flexible

    int burst_count = 0; // real_payload_flits / one_fetch_flits
    
    int max_tags = 1 << 11;

private:
    void init_speed();
    void init_lambda();
    void init_prereq();
    void init_rowhit();  // SAUGATA: added function to check for row hits
    void init_rowopen();
    void init_timing();
};

// [1] “Hybrid memory cube specification 2.1,” Hybrid Memory Cube Consortium, Tech. Rep., Oct. 2015.
// [2] Jeddeloh, Joe, and Brent Keeth. "Hybrid memory cube new DRAM architecture increases density and performance." 2012 Symposium on VLSI Technology (VLSIT). 2012.
// [3] Kim, Gwangsun, et al. "Memory-centric system interconnect design with hybrid memory cubes." Proceedings of the 22nd international conference on Parallel architectures and compilation techniques. IEEE Press, 2013.

} /*namespace ramulator*/

#endif /*__HMC_H*/
