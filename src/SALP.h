#ifndef __SALP_H
#define __SALP_H

#include "DRAM.h"
#include "Request.h"
#include <vector>
#include <functional>

using namespace std;

namespace ramulator
{

class SALP
{
public:
    string standard_name;
    enum class Org;
    enum class Speed;
    enum class Type;
    SALP(Org org, Speed speed, Type type = Type::MASA, int n_sa = 8);
    SALP(const string& org_str, const string& speed_str, const string& type_str = "SALP-MASA", int n_sa = 8);
    
    static map<string, enum Org> org_map;
    static map<string, enum Speed> speed_map;
    static map<string, enum Type> type_map;

    enum class Type : int
    {
        SALP_1, SALP_2, MASA, MAX
    } type;
    /*** Level ***/
    enum class Level : int
    { 
        Channel, Rank, Bank, SubArray, Row, Column, MAX
    };

    /*** Command ***/
    enum class Command : int
    { 
        ACT, SASEL, PRE, PRER, // precharge bank/bank/rank(SALP-1), subarray/bank/rank(SALP-2, MASA)
        RD,  WR,    RDA, WRA, // auto-precharge: bank(SALP-1), subarray(SALP-2, MASA)
        REF, PDE,   PDX, SRE, SRX, 
        PRE_OTHER,
        MAX
    };

    string command_name[int(Command::MAX)] = {
        "ACT", "SASEL", "PRE", "PRER",
        "RD",  "WR",    "RDA", "WRA",
        "REF", "PDE",   "PDX", "SRE", "SRX", "PRE_OTHER"
    };

    // The scope of each command
    Level scope[int(Command::MAX)] = {
        Level::Row,    Level::SubArray, Level::SubArray, Level::Rank,
        Level::Column, Level::Column,   Level::Column,   Level::Column,
        Level::Rank,   Level::Rank,     Level::Rank,     Level::Rank,   Level::Rank, Level::SubArray
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
            case int(Command::PRER):
            case int(Command::PRE_OTHER):
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
        Opened, Closed, Selected, PowerUp, ActPowerDown, PrePowerDown, SelfRefresh, MAX
    } start[int(Level::MAX)] = {
        State::MAX, State::PowerUp, State::Closed, State::Closed, State::Closed, State::MAX
    };

    /* Translate */
    Command translate[int(Request::Type::MAX)] = {
        Command::RD,  Command::WR,
        Command::REF, Command::PDE, Command::SRE
    };

    /* Prerequisite */
    function<Command(DRAM<SALP>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    // SAUGATA: added function object container for row hit status
    /* Row hit */
    function<bool(DRAM<SALP>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];
    function<bool(DRAM<SALP>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

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
    function<void(DRAM<SALP>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    /* Organization */
    enum class Org : int
    {
        SALP_512Mb_x4, SALP_512Mb_x8, SALP_512Mb_x16,
        SALP_1Gb_x4,   SALP_1Gb_x8,   SALP_1Gb_x16,
        SALP_2Gb_x4,   SALP_2Gb_x8,   SALP_2Gb_x16,
        SALP_4Gb_x4,   SALP_4Gb_x8,   SALP_4Gb_x16,
        SALP_8Gb_x4,   SALP_8Gb_x8,   SALP_8Gb_x16,
        MAX
    };

    int n_sa; // number of subarrays per bank

    struct OrgEntry {
        int size;
        int dq;
        int count[int(Level::MAX)];
    } org_table[int(Org::MAX)] = {
        {  512,  4, {0, 0, 8, 0, 0, 1<<11}}, {  512,  8, {0, 0, 8, 0, 0, 1<<10}}, {  512, 16, {0, 0, 8, 0, 0, 1<<10}},
        {1<<10,  4, {0, 0, 8, 0, 0, 1<<11}}, {1<<10,  8, {0, 0, 8, 0, 0, 1<<10}}, {1<<10, 16, {0, 0, 8, 0, 0, 1<<10}},
        {2<<10,  4, {0, 0, 8, 0, 0, 1<<11}}, {2<<10,  8, {0, 0, 8, 0, 0, 1<<10}}, {2<<10, 16, {0, 0, 8, 0, 0, 1<<10}},
        {4<<10,  4, {0, 0, 8, 0, 0, 1<<11}}, {4<<10,  8, {0, 0, 8, 0, 0, 1<<10}}, {4<<10, 16, {0, 0, 8, 0, 0, 1<<10}},
        {8<<10,  4, {0, 0, 8, 0, 0, 1<<12}}, {8<<10,  8, {0, 0, 8, 0, 0, 1<<11}}, {8<<10, 16, {0, 0, 8, 0, 0, 1<<10}}
    }, org_entry;

    void set_channel_number(int channel);
    void set_rank_number(int rank);

    /* Speed */
    enum class Speed : int
    {
        SALP_800D,  SALP_800E,
        SALP_1066E, SALP_1066F, SALP_1066G,
        SALP_1333G, SALP_1333H,
        SALP_1600H, SALP_1600J, SALP_1600K,
        SALP_1866K, SALP_1866L,
        SALP_2133L, SALP_2133M,
        MAX
    };

    int prefetch_size = 8; // 8n prefetch DDR
    int channel_width = 64;

    struct SpeedEntry {
        int rate;
        double freq, tCK;
        int nBL, nCCD, nRTRS;
        int nCL, nRCD, nRP, nPA, nCWL; // nRP for pre2act same sa, nPA for pre2act diff sa (1 cycle)
        int nRAS, nRC;
        int nRTP, nWTR, nWR, nRA, nWA; // nRA = nCL/2, nWA = nCWL + nBL + nWR/2
        int nRRD, nFAW;
        int nRFC, nREFI;
        int nPD, nXP, nXPDLL;
        int nCKESR, nXS, nXSDLL;
        int nSCD;
    } speed_table[int(Speed::MAX)] = {
        {800,  (400.0/3)*3, (3/0.4)/3, 4, 4, 2,  5,  5,  5, 1,  5, 15, 20, 4, 4,  6, 3, 12, 0, 0, 0, 3120, 3, 3, 10, 4, 0, 512, 1},
        {800,  (400.0/3)*3, (3/0.4)/3, 4, 4, 2,  6,  6,  6, 1,  5, 15, 21, 4, 4,  6, 3, 12, 0, 0, 0, 3120, 3, 3, 10, 4, 0, 512, 1},
        {1066, (400.0/3)*4, (3/0.4)/4, 4, 4, 2,  6,  6,  6, 1,  6, 20, 26, 4, 4,  8, 3, 14, 0, 0, 0, 4160, 3, 4, 13, 4, 0, 512, 1},
        {1066, (400.0/3)*4, (3/0.4)/4, 4, 4, 2,  7,  7,  7, 1,  6, 20, 27, 4, 4,  8, 4, 14, 0, 0, 0, 4160, 3, 4, 13, 4, 0, 512, 1},
        {1066, (400.0/3)*4, (3/0.4)/4, 4, 4, 2,  8,  8,  8, 1,  6, 20, 28, 4, 4,  8, 4, 14, 0, 0, 0, 4160, 3, 4, 13, 4, 0, 512, 1},
        {1333, (400.0/3)*5, (3/0.4)/5, 4, 4, 2,  8,  8,  8, 1,  7, 24, 32, 5, 5, 10, 4, 16, 0, 0, 0, 5200, 4, 4, 16, 5, 0, 512, 1},
        {1333, (400.0/3)*5, (3/0.4)/5, 4, 4, 2,  9,  9,  9, 1,  7, 24, 33, 5, 5, 10, 5, 16, 0, 0, 0, 5200, 4, 4, 16, 5, 0, 512, 1},
        {1600, (400.0/3)*6, (3/0.4)/6, 4, 4, 2,  9,  9,  9, 1,  8, 28, 37, 6, 6, 12, 5, 18, 0, 0, 0, 6240, 4, 5, 20, 5, 0, 512, 1},
        {1600, (400.0/3)*6, (3/0.4)/6, 4, 4, 2, 10, 10, 10, 1,  8, 28, 38, 6, 6, 12, 5, 18, 0, 0, 0, 6240, 4, 5, 20, 5, 0, 512, 1},
        {1600, (400.0/3)*6, (3/0.4)/6, 4, 4, 2, 11, 11, 11, 1,  8, 28, 39, 6, 6, 12, 6, 18, 0, 0, 0, 6240, 4, 5, 20, 5, 0, 512, 1},
        {1866, (400.0/3)*7, (3/0.4)/7, 4, 4, 2, 11, 11, 11, 1,  9, 32, 43, 7, 7, 14, 6, 20, 0, 0, 0, 7280, 5, 6, 23, 6, 0, 512, 1},
        {1866, (400.0/3)*7, (3/0.4)/7, 4, 4, 2, 12, 12, 12, 1,  9, 32, 44, 7, 7, 14, 6, 20, 0, 0, 0, 7280, 5, 6, 23, 6, 0, 512, 1},
        {2133, (400.0/3)*8, (3/0.4)/8, 4, 4, 2, 12, 12, 12, 1, 10, 36, 48, 8, 8, 16, 6, 22, 0, 0, 0, 8320, 6, 7, 26, 7, 0, 512, 1},
        {2133, (400.0/3)*8, (3/0.4)/8, 4, 4, 2, 13, 13, 13, 1, 10, 36, 49, 8, 8, 16, 7, 22, 0, 0, 0, 8320, 6, 7, 26, 7, 0, 512, 1}
    }, speed_entry;

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

#endif /*__SALP_H*/
