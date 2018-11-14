#ifndef __LPDDR4_H
#define __LPDDR4_H

#include "DRAM.h"
#include "Request.h"
#include <vector>
#include <functional>

using namespace std;

namespace ramulator
{

class LPDDR4
{
public:
    static string standard_name;
    enum class Org;
    enum class Speed;
    LPDDR4(Org org, Speed speed);
    LPDDR4(const string& org_str, const string& speed_str);
    
    static map<string, enum Org> org_map;
    static map<string, enum Speed> speed_map;

    /* Level */
    enum class Level : int
    { 
        Channel, Rank, Bank, Row, Column, MAX
    };
    
    static std::string level_str [int(Level::MAX)];

    /* Command */
    enum class Command : int
    { 
        ACT, PRE, PREA, 
        RD,  WR,  RDA,  WRA, 
        REF, REFPB, PDE, PDX, SREF, SREFX, 
        MAX
    };
    // Due to multiplexing on the cmd/addr bus:
    //      ACT, RD, WR, RDA, WRA take 4 cycles
    //      PRE, PREA, REF, REFPB, PDE, PDX, SREF, SREFX take 2 cycles
    string command_name[int(Command::MAX)] = {
        "ACT", "PRE", "PREA", 
        "RD",  "WR",  "RDA",  "WRA", 
        "REF", "REFPB", "PDE", "PDX", "SREF", "SREFX"
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
        Command::REF, Command::PDE, Command::SREF
    };

    /* Prerequisite */
    function<Command(DRAM<LPDDR4>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    // SAUGATA: added function object container for row hit status
    /* Row hit */
    function<bool(DRAM<LPDDR4>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];
    function<bool(DRAM<LPDDR4>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

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
    function<void(DRAM<LPDDR4>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    /* Organization */
    enum class Org : int
    {
        // this is per-die density, actual per-chan density is half
        LPDDR4_4Gb_x16,
        LPDDR4_6Gb_x16,
        LPDDR4_8Gb_x16,
        // LPDDR4_12Gb_x16, // tRFC TBD
        // LPDDR4_16Gb_x16, // tRFC TBD
        MAX
    };

    struct OrgEntry {
        int size;
        int dq;
        int count[int(Level::MAX)];
    } org_table[int(Org::MAX)] = {
        {2<<10, 16, {0, 0, 8, 1<<14, 1<<10}},
        {3<<10, 16, {0, 0, 8, 3<<13, 1<<10}},
        {4<<10, 16, {0, 0, 8, 1<<15, 1<<10}},
    }, org_entry;

    void set_channel_number(int channel);
    void set_rank_number(int rank);


    /* Speed */
    enum class Speed : int
    {
        LPDDR4_1600,
        LPDDR4_2400,
        LPDDR4_3200,
        MAX
    };

    enum class RefreshMode : int
    {
        Refresh_1X,
        Refresh_2X,
        Refresh_4X,
        MAX
    } refresh_mode = RefreshMode::Refresh_1X;

    int prefetch_size = 16; // 16n prefetch DDR
    int channel_width = 32;

    struct SpeedEntry {
        int rate;
        double freq, tCK;
        int nBL, nCCD, nRTRS, nDQSCK;
        int nCL, nRCD, nRPpb, nRPab, nCWL;
        int nRAS, nRC;
        int nRTP, nWTR, nWR;
        int nPPD, nRRD, nFAW;
        int nRFCab, nRFCpb, nREFI;
        int nCKE, nXP; // CKE value n/a
        int nSR, nXSR; // tXSR = tRFCab + 7.5ns
    } speed_table[int(Speed::MAX)] = {
        // LPDDR4 is 16n prefetch. Latencies in JESD209-4 counts from and to 
        // the end of each command, I've converted them as if all commands take
        // only 1 cycle like other standards
        // CL-RCD-RPpb are set to the same value althrough CL is not explicitly specified.
        // CWL is made up, half of CL.
        // calculated from 10.2 core timing table 89
        {1600, 400.0*2, 2.5/2, 8, 8, 2, 1, 15+3, 15, 15-2, 17-2,  8+3, 34, 47,  8+2,  8, 15-1, 4,  8, 32, 0, 0, 0, 0,  6, 12, 0},
        {2400, 400.0*3, 2.5/3, 8, 8, 2, 2, 22+3, 22, 22-2, 26-2, 11+3, 51, 71,  9+2, 12, 22-1, 4, 12, 48, 0, 0, 0, 0,  9, 18, 0},
        {3200, 400.0*4, 2.5/4, 8, 8, 2, 3, 29+3, 29, 29-2, 34-2, 15+3, 68, 95, 12+2, 16, 29-1, 4, 16, 64, 0, 0, 0, 0, 12, 24, 0}
    }, speed_entry;

    // LPDDR4 defines {fast, typical, slow} timing for tRCD and tRP. (typ)
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

#endif /*__LPDDR4_H*/
