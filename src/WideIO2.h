#ifndef __WIDEIO2_H
#define __WIDEIO2_H

#include "DRAM.h"
#include "Request.h"
#include <vector>
#include <functional>

using namespace std;

namespace ramulator
{

class WideIO2
{
public:
    static string standard_name;
    enum class Org;
    enum class Speed;
    WideIO2(Org org, Speed speed, int channels = 4);
    WideIO2(const string& org_str, const string& speed_str, int channels = 4);
    
    static map<string, enum Org> org_map;
    static map<string, enum Speed> speed_map;

    /*** Level ***/
    enum class Level : int
    { 
        Channel, Rank, Bank, Row, Column, MAX
    };

    /*** Command ***/
    enum class Command : int
    { 
        ACT, PRE,   PRA, 
        RD,  WR,    RDA,  WRA, 
        REF, REFPB, PD,  PDX,  SREF, SREFX, 
        MAX
    };

    string command_name[int(Command::MAX)] = {
        "ACT", "PRE",   "PRA", 
        "RD",  "WR",    "RDA", "WRA", 
        "REF", "REFPB", "PD",  "PDX",  "SREF", "SREFX"
    };

    Level scope[int(Command::MAX)] = {
        Level::Row,    Level::Bank,   Level::Rank,   
        Level::Column, Level::Column, Level::Column, Level::Column,
        Level::Rank,   Level::Bank,   Level::Rank,   Level::Rank,   Level::Rank,   Level::Rank
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
            case int(Command::PRA):
                return true;
            default:
                return false;
        }
    }

    bool is_refreshing(Command cmd) 
    {
        switch(int(cmd)) {
            case int(Command::REF):
            case int(Command::REFPB):
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
        Command::REF, Command::PD, Command::SREF
    };

    /* Prerequisite */
    function<Command(DRAM<WideIO2>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    // SAUGATA: added function object container for row hit status
    /* Row hit */
    function<bool(DRAM<WideIO2>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];
    function<bool(DRAM<WideIO2>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

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
    function<void(DRAM<WideIO2>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    /* Organization */
    enum class Org : int
    {
        // per-die density
        WideIO2_8Gb,
        // WideIO2_12Gb, tRFC TBD
        // WideIO2_16Gb, tRFC TBD
        // WideIO2_24Gb, TBD
        // WideIO2_32Gb, TBD
        MAX
    };

    struct OrgEntry {
        int size; // per-channel density
        int dq;
        int count[int(Level::MAX)];
    } org_entry;

    void set_channel_number(int channel);
    void set_rank_number(int rank);

    /* Speed */
    enum class Speed : int
    {
        WideIO2_800,
        WideIO2_1066,
        MAX
    };
    // WideIO2 specified /4, /2, x1, x2, x4 refresh rates. x1 is used here

    int prefetch_size = 4;
    int channel_width = 64;

    struct SpeedEntry {
        int rate;
        double freq, tCK;
        int nBL, nCCD, nDQSCK, nRTRS; // 4n prefetch, DDR (although 8n is allowed?)
        int nCL, nRCD, nRPpb, nRP8b, nRPab, nCWL;
        int nRAS, nRC;
        int nRTP, nWTR, nWR;
        int nRRD, nFAW;
        int nRFCab, nRFCpb, nREFI;
        int nCKE, nXP;
        int nCKESR, nXSR;
    } speed_table[int(Speed::MAX)] = {
        { 800, 800.0/3*3, 2.5*3/3, 2, 2, 1, 2, 7,  8,  8,  9, 0, 5, 17, 25, 3, 4,  8, 4, 24, 72, 36, 1560, 3, 3, 6,  76},
        {1066, 800.0/3*4, 2.5*3/4, 2, 2, 1, 2, 9, 10, 10, 12, 0, 7, 23, 33, 4, 6, 11, 6, 32, 96, 48, 2080, 3, 4, 8, 102}
    }, speed_entry;

    int read_latency;

private:
    void init_lambda();
    void init_prereq();
    void init_rowhit();  // SAUGATA: added function to check for row hits
    void init_rowopen();
    void init_timing();
};

} /*namespace ramulator*/

#endif /*__WIDEIO2_H*/
