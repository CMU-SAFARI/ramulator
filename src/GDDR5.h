#ifndef __GDDR5_H
#define __GDDR5_H

#include "DRAM.h"
#include "Request.h"
#include <vector>
#include <functional>

using namespace std;

namespace ramulator
{

class GDDR5
{
public:
    static string standard_name;
    enum class Org;
    enum class Speed;
    GDDR5(Org org, Speed speed);
    GDDR5(const string& org_str, const string& speed_str);
    
    static map<string, enum Org> org_map;
    static map<string, enum Speed> speed_map;

    /*** Level ***/
    enum class Level : int
    { 
        Channel, Rank, BankGroup, Bank, Row, Column, MAX
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
        State::MAX, State::PowerUp, State::MAX, State::Closed, State::Closed, State::MAX
    };

    /* Translate */
    Command translate[int(Request::Type::MAX)] = {
        Command::RD,  Command::WR,
        Command::REF, Command::PDE, Command::SRE
    };

    /* Prerequisite */
    function<Command(DRAM<GDDR5>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    // SAUGATA: added function object container for row hit status
    /* Row hit */
    function<bool(DRAM<GDDR5>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];
    function<bool(DRAM<GDDR5>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

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
    function<void(DRAM<GDDR5>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    /* Organization */
    enum class Org : int
    {
        GDDR5_512Mb_x16, GDDR5_512Mb_x32,
        GDDR5_1Gb_x16,   GDDR5_1Gb_x32,
        GDDR5_2Gb_x16,   GDDR5_2Gb_x32,
        GDDR5_4Gb_x16,   GDDR5_4Gb_x32,
        GDDR5_8Gb_x16,   GDDR5_8Gb_x32,
        GDDR5_2Gb_x16_bank32,
        GDDR5_2Gb_x16_bank64,
        GDDR5_2Gb_x16_bank128,
        GDDR5_2Gb_x16_bank256,
        GDDR5_2Gb_x16_bank512,
        MAX
    };

    struct OrgEntry {
        int size;
        int dq;
        int count[int(Level::MAX)];
    } org_table[int(Org::MAX)] = {
        // fixed to have 1 rank
        // in GDDR5 the column address is unique for a burst. e.g. 64 column addresses correspond with
        // 256 column addresses actually. So we multiply 8 to the original address bit number in JEDEC standard
        {  512, 16, {0, 1, 4, 2, 1<<12, 1<<(7+3)}}, {  512, 32, {0, 1, 4, 2, 1<<12, 1<<(6+3)}},
        {1<<10, 16, {0, 1, 4, 4, 1<<12, 1<<(7+3)}}, {1<<10, 32, {0, 1, 4, 4, 1<<12, 1<<(6+3)}},
        {2<<10, 16, {0, 1, 4, 4, 1<<13, 1<<(7+3)}}, {2<<10, 32, {0, 1, 4, 4, 1<<13, 1<<(6+3)}},
        {4<<10, 16, {0, 1, 4, 4, 1<<14, 1<<(7+3)}}, {2<<10, 32, {0, 1, 4, 4, 1<<14, 1<<(6+3)}},
        {8<<10, 16, {0, 1, 4, 4, 1<<14, 1<<(8+3)}}, {8<<10, 32, {0, 1, 4, 4, 1<<14, 1<<(7+3)}},
        {2<<10, 16, {0, 1, 4, 8, 1<<12, 1<<(7+3)}},
        {2<<10, 16, {0, 1, 4, 16, 1<<11, 1<<(7+3)}},
        {2<<10, 16, {0, 1, 4, 32, 1<<10, 1<<(7+3)}},
        {2<<10, 16, {0, 1, 4, 64, 1<<9, 1<<(7+3)}},
        {2<<10, 16, {0, 1, 4, 128, 1<<8, 1<<(7+3)}}
    }, org_entry;

    void set_channel_number(int channel);
    void set_rank_number(int rank);

    /* Speed */
    enum class Speed : int
    {
        GDDR5_4000, GDDR5_4500,
        GDDR5_5000, GDDR5_5500,
        GDDR5_6000, GDDR5_6500,
        GDDR5_7000,
        GDDR5_7000_disable_bg,
        GDDR5_7000_larger_REFI,
        GDDR5_7000_disable_bg_larger_REFI,
        GDDR5_7000_unlimit_bandwidth,
        MAX
    };

    int prefetch_size = 8; // 8n prefetch QDR
    int channel_width = 64;

    struct SpeedEntry {
        int rate;
        double freq, tCK;
        int nBL, nCCDS, nCCDL;
        int nCL, nRCDR, nRCDW, nRP, nCWL;
        int nRAS, nRC;
        int nPPD, nRTP, nWTR, nWR;
        int nRRD, nFAW, n32AW;
        int nRFC, nREFI;
        int nPD, nXPN, nLK;
        int nCKESR, nXS, nXSDLL;
    } speed_table[int(Speed::MAX)] = {
        {4000,  8*500/4,  8.0/8, 2, 2, 3, 12, 12, 10, 12, 3, 28, 40, 1, 2, 5, 12,  6, 23, 184, 0, 0, 10, 10, 0, 0, 0, 0},
        {4500,  9*500/4,  8.0/9, 2, 2, 3, 14, 14, 12, 14, 4, 32, 46, 2, 2, 6, 14,  7, 26, 207, 0, 0, 10, 10, 0, 0, 0, 0},
        {5000, 10*500/4, 8.0/10, 2, 2, 3, 15, 15, 13, 15, 4, 35, 50, 2, 2, 7, 15,  7, 29, 230, 0, 0, 10, 10, 0, 0, 0, 0},
        {5500, 11*500/4, 8.0/11, 2, 2, 3, 17, 17, 14, 17, 5, 39, 56, 2, 2, 7, 17,  8, 32, 253, 0, 0, 10, 10, 0, 0, 0, 0},
        {6000, 12*500/4, 8.0/12, 2, 2, 3, 18, 18, 15, 18, 5, 42, 60, 2, 2, 8, 18,  9, 35, 276, 0, 0, 10, 10, 0, 0, 0, 0},
        {6500, 13*500/4, 8.0/13, 2, 2, 3, 20, 20, 17, 20, 5, 46, 66, 2, 2, 9, 20,  9, 38, 299, 0, 0, 10, 10, 0, 0, 0, 0},
        {7000, 14*500/4, 8.0/14, 2, 2, 3, 21, 21, 18, 21, 6, 49, 70, 2, 2, 9, 21, 10, 41, 322, 0, 0, 10, 10, 0, 0, 0, 0},
        {7000, 14*500/4, 8.0/14, 2, 2, 2, 21, 21, 18, 21, 6, 49, 70, 2, 2, 9, 21, 10, 41, 322, 0, 0, 10, 10, 0, 0, 0, 0},
        {7000, 14*500/4, 8.0/14, 2, 2, 3, 21, 21, 18, 21, 6, 49, 70, 2, 2, 9, 21, 10, 41, 322, 0, 13650, 10, 10, 0, 0, 0, 0},
        {7000, 14*500/4, 8.0/14, 2, 2, 2, 21, 21, 18, 21, 6, 49, 70, 2, 2, 9, 21, 10, 41, 322, 0, 13650, 10, 10, 0, 0, 0, 0},
        {7000, 14*500/4, 8.0/14, 0, 1, 1, 21, 21, 18, 21, 6, 49, 70, 2, 2, 9, 21, 10, 41, 322, 0, 0, 10, 10, 0, 0, 0, 0},
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

#endif /*__GDDR5_H*/
