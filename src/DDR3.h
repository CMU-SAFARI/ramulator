#ifndef __DDR3_H
#define __DDR3_H

#include "DRAM.h"
#include "Request.h"
#include <vector>
#include <map>
#include <string>
#include <functional>

using namespace std;

namespace ramulator
{

class DDR3
{
public:
    static string standard_name;
    enum class Org;
    enum class Speed;
    DDR3(Org org, Speed speed);
    DDR3(const string& org_str, const string& speed_str);
    
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

    Level scope[int(Command::MAX)] = {
        Level::Row,    Level::Bank,   Level::Rank,   
        Level::Column, Level::Column, Level::Column, Level::Column,
        Level::Rank,   Level::Rank,   Level::Rank,   Level::Rank,   Level::Rank
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
    function<Command(DRAM<DDR3>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    // SAUGATA: added function object container for row hit status
    /* Row hit */
    function<bool(DRAM<DDR3>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];
    function<bool(DRAM<DDR3>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

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
    function<void(DRAM<DDR3>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    /* Organization */
    enum class Org : int
    {
        DDR3_512Mb_x4, DDR3_512Mb_x8, DDR3_512Mb_x16,
        DDR3_1Gb_x4,   DDR3_1Gb_x8,   DDR3_1Gb_x16,
        DDR3_2Gb_x4,   DDR3_2Gb_x8,   DDR3_2Gb_x16,
        DDR3_4Gb_x4,   DDR3_4Gb_x8,   DDR3_4Gb_x16,
        DDR3_8Gb_x4,   DDR3_8Gb_x8,   DDR3_8Gb_x16,
        DDR3_2Gb_x16_bank16,
        DDR3_2Gb_x16_bank32,
        DDR3_2Gb_x16_bank64,
        DDR3_2Gb_x16_bank128,
        DDR3_2Gb_x16_bank256,
        MAX
    };

    struct OrgEntry {
        int size;
        int dq;
        int count[int(Level::MAX)];
    } org_table[int(Org::MAX)] = {
        {  512,  4, {0, 0, 8, 1<<13, 1<<11}}, {  512,  8, {0, 0, 8, 1<<13, 1<<10}}, {  512, 16, {0, 0, 8, 1<<12, 1<<10}},
        {1<<10,  4, {0, 0, 8, 1<<14, 1<<11}}, {1<<10,  8, {0, 0, 8, 1<<14, 1<<10}}, {1<<10, 16, {0, 0, 8, 1<<13, 1<<10}},
        {2<<10,  4, {0, 0, 8, 1<<15, 1<<11}}, {2<<10,  8, {0, 0, 8, 1<<15, 1<<10}}, {2<<10, 16, {0, 0, 8, 1<<14, 1<<10}},
        {4<<10,  4, {0, 0, 8, 1<<16, 1<<11}}, {4<<10,  8, {0, 0, 8, 1<<16, 1<<10}}, {4<<10, 16, {0, 0, 8, 1<<15, 1<<10}},
        {8<<10,  4, {0, 0, 8, 1<<16, 1<<12}}, {8<<10,  8, {0, 0, 8, 1<<16, 1<<11}}, {8<<10, 16, {0, 0, 8, 1<<16, 1<<10}},
        {2<<10, 16, {0, 0, 16, 1<<13, 1<<10}},
        {2<<10, 16, {0, 0, 32, 1<<12, 1<<10}},
        {2<<10, 16, {0, 0, 64, 1<<11, 1<<10}},
        {2<<10, 16, {0, 0, 128, 1<<10, 1<<10}},
        {2<<10, 16, {0, 0, 256, 1<<9, 1<<10}}
    }, org_entry;

    void set_channel_number(int channel);
    void set_rank_number(int rank);

    /* Speed */
    enum class Speed : int
    {
        DDR3_800D,  DDR3_800E,
        DDR3_1066E, DDR3_1066F, DDR3_1066G,
        DDR3_1333G, DDR3_1333H,
        DDR3_1600H, DDR3_1600J, DDR3_1600K,
        DDR3_1866K, DDR3_1866L,
        DDR3_2133L, DDR3_2133M, DDR3_2133_14,
        DDR3_2133L_unlimit_bandwidth,
        DDR3_2133_14_unlimit_bandwidth,
        MAX
    };

    int prefetch_size = 8; // 8n prefetch DDR
    int channel_width = 64;

    struct SpeedEntry {
        int rate;
        double freq, tCK;
        int nBL, nCCD, nRTRS;
        int nCL, nRCD, nRP, nCWL;
        int nRAS, nRC;
        int nRTP, nWTR, nWR;
        int nRRD, nFAW;
        int nRFC, nREFI;
        int nPD, nXP, nXPDLL;
        int nCKESR, nXS, nXSDLL;
    } speed_table[int(Speed::MAX)] = {
        {800,  (400.0/3)*3, (3/0.4)/3, 4, 4, 2,  5,  5,  5,  5, 15, 20, 4, 4,  6, 0, 0, 0, 3120, 3, 3, 10, 4, 0, 512},
        {800,  (400.0/3)*3, (3/0.4)/3, 4, 4, 2,  6,  6,  6,  5, 15, 21, 4, 4,  6, 0, 0, 0, 3120, 3, 3, 10, 4, 0, 512},
        {1066, (400.0/3)*4, (3/0.4)/4, 4, 4, 2,  6,  6,  6,  6, 20, 26, 4, 4,  8, 0, 0, 0, 4160, 3, 4, 13, 4, 0, 512},
        {1066, (400.0/3)*4, (3/0.4)/4, 4, 4, 2,  7,  7,  7,  6, 20, 27, 4, 4,  8, 0, 0, 0, 4160, 3, 4, 13, 4, 0, 512},
        {1066, (400.0/3)*4, (3/0.4)/4, 4, 4, 2,  8,  8,  8,  6, 20, 28, 4, 4,  8, 0, 0, 0, 4160, 3, 4, 13, 4, 0, 512},
        {1333, (400.0/3)*5, (3/0.4)/5, 4, 4, 2,  8,  8,  8,  7, 24, 32, 5, 5, 10, 0, 0, 0, 5200, 4, 4, 16, 5, 0, 512},
        {1333, (400.0/3)*5, (3/0.4)/5, 4, 4, 2,  9,  9,  9,  7, 24, 33, 5, 5, 10, 0, 0, 0, 5200, 4, 4, 16, 5, 0, 512},
        {1600, (400.0/3)*6, (3/0.4)/6, 4, 4, 2,  9,  9,  9,  8, 28, 37, 6, 6, 12, 0, 0, 0, 6240, 4, 5, 20, 5, 0, 512},
        {1600, (400.0/3)*6, (3/0.4)/6, 4, 4, 2, 10, 10, 10,  8, 28, 38, 6, 6, 12, 0, 0, 0, 6240, 4, 5, 20, 5, 0, 512},
        {1600, (400.0/3)*6, (3/0.4)/6, 4, 4, 2, 11, 11, 11,  8, 28, 39, 6, 6, 12, 0, 0, 0, 6240, 4, 5, 20, 5, 0, 512},
        {1866, (400.0/3)*7, (3/0.4)/7, 4, 4, 2, 11, 11, 11,  9, 32, 43, 7, 7, 14, 0, 0, 0, 7280, 5, 6, 23, 6, 0, 512},
        {1866, (400.0/3)*7, (3/0.4)/7, 4, 4, 2, 12, 12, 12,  9, 32, 44, 7, 7, 14, 0, 0, 0, 7280, 5, 6, 23, 6, 0, 512},
        {2133, (400.0/3)*8, (3/0.4)/8, 4, 4, 2, 12, 12, 12, 10, 36, 48, 8, 8, 16, 0, 0, 0, 8320, 6, 7, 26, 7, 0, 512},
        {2133, (400.0/3)*8, (3/0.4)/8, 4, 4, 2, 13, 13, 13, 10, 36, 49, 8, 8, 16, 0, 0, 0, 8320, 6, 7, 26, 7, 0, 512},
        {2133, (400.0/3)*8, (3/0.4)/8, 4, 4, 2, 14, 14, 14, 13, 36, 50, 8, 8, 16, 0, 0, 0, 8320, 6, 7, 26, 7, 0, 512},
        {2133, (400.0/3)*8, (3/0.4)/8, 0, 1, 2, 12, 12, 12, 10, 36, 48, 8, 8, 16, 0, 0, 0, 8320, 6, 7, 26, 7, 0, 512},
        {2133, (400.0/3)*8, (3/0.4)/8, 0, 1, 2, 14, 14, 14, 13, 36, 50, 8, 8, 16, 0, 0, 0, 8320, 6, 7, 26, 7, 0, 512},
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

#endif /*__DDR3_H*/
