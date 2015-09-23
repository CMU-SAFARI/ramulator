#ifndef __LPDDR3_H
#define __LPDDR3_H

#include "DRAM.h"
#include "Request.h"
#include <vector>
#include <functional>

using namespace std;

namespace ramulator
{

class LPDDR3
{
public:
    static string standard_name;
    enum class Org;
    enum class Speed;
    LPDDR3(Org org, Speed speed);
    LPDDR3(const string& org_str, const string& speed_str);
    
    static map<string, enum Org> org_map;
    static map<string, enum Speed> speed_map;

    /* Level */
    enum class Level : int
    { 
        Channel, Rank, Bank, Row, Column, MAX
    };

    /* Command */
    enum class Command : int
    { 
        ACT, PRE, PRA, 
        RD,  WR,  RDA,  WRA, 
        REF, REFPB, PD, PDX,  SREF, SREFX, 
        MAX
    };

    string command_name[int(Command::MAX)] = {
        "ACT", "PRE", "PRA", 
        "RD",  "WR",  "RDA",  "WRA", 
        "REF", "REFPB", "PD", "PDX",  "SREF", "SREFX"
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
    function<Command(DRAM<LPDDR3>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    // SAUGATA: added function object container for row hit status
    /* Row hit */
    function<bool(DRAM<LPDDR3>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];
    function<bool(DRAM<LPDDR3>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

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
    function<void(DRAM<LPDDR3>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    /* Organization */
    enum class Org : int
    {
        LPDDR3_4Gb_x16, LPDDR3_4Gb_x32,
        LPDDR3_6Gb_x16, LPDDR3_6Gb_x32,
        LPDDR3_8Gb_x16, LPDDR3_8Gb_x32,
        LPDDR3_12Gb_x16, LPDDR3_12Gb_x32,
        LPDDR3_16Gb_x16, LPDDR3_16Gb_x32,
        MAX
    };

    struct OrgEntry {
        int size;
        int dq;
        int count[int(Level::MAX)];
    } org_table[int(Org::MAX)] = {
        {4<<10, 16, {0, 0, 8, 1<<14, 1<<11}}, {4<<10, 32, {0, 0, 8, 1<<14, 1<<10}},
        {6<<10, 16, {0, 0, 8, 3<<13, 1<<11}}, {6<<10, 32, {0, 0, 8, 3<<13, 1<<10}},
        {8<<10, 16, {0, 0, 8, 1<<15, 1<<11}}, {8<<10, 32, {0, 0, 8, 1<<15, 1<<10}},
        {12<<10, 16, {0, 0, 8, 3<<13, 1<<12}}, {12<<10, 32, {0, 0, 8, 3<<13, 1<<11}},
        {16<<10, 16, {0, 0, 8, 1<<15, 1<<12}}, {16<<10, 32, {0, 0, 8, 1<<15, 1<<11}},
    }, org_entry;

    void set_channel_number(int channel);
    void set_rank_number(int rank);

    /* Speed */
    enum class Speed : int
    {
        LPDDR3_1333,
        LPDDR3_1600,
        LPDDR3_1866,
        LPDDR3_2133,
        MAX
    };

    int prefetch_size = 8; // 16n prefetch DDR
    int channel_width = 64;

    struct SpeedEntry {
        int rate;
        double freq, tCK;
        int nBL, nCCD, nRTRS, nDQSCK;
        int nCL, nRCD, nRPpb, nRPab, nCWL;
        int nRAS, nRC;
        int nRTP, nWTR, nWR;
        int nRRD, nFAW;
        int nRFCab, nRFCpb, nREFI;
        int nCKE, nXP; // CKE is PD, LPDDR3 has no DLL
        int nCKESR, nXSR; // tXSR = tRFCab + 10ns
    } speed_table[int(Speed::MAX)] = {
        {1333, (400.0/3)*5, (3/0.4)/5, 4, 4, 2, 2, 10, 12, 12, 14, 6, 28, 40, 5, 5, 10,  7, 34, 0, 0, 2600, 5, 5, 10, 0},
        {1600, (400.0/3)*6, (3/0.4)/6, 4, 4, 2, 2, 12, 15, 15, 17, 6, 34, 48, 6, 6, 12,  8, 40, 0, 0, 3120, 6, 6, 12, 0},
        {1866, (400.0/3)*7, (3/0.4)/7, 4, 4, 2, 3, 14, 17, 17, 20, 8, 40, 56, 7, 7, 14, 10, 47, 0, 0, 3640, 7, 7, 14, 0},
        {2133, (400.0/3)*8, (3/0.4)/8, 4, 4, 2, 3, 16, 20, 20, 23, 8, 45, 64, 8, 8, 16, 11, 54, 0, 0, 4160, 8, 8, 16, 0}
    }, speed_entry;

    // LPDDR3 defines {fast, typical, slow} timing for tRCD and tRP. (typ)
    // WL as diff. values for set A/B (A)

    int read_latency;

private:
    void init_speed();
    void init_lambda();
    void init_prereq();
    void init_rowhit();  // SAUGATA: added function to check for row hits
    void init_rowopen();
    void init_timing();
};

} /*namespace ramulator*/

#endif /*__LPDDR3_H*/
