#ifndef __WIDEIO_H
#define __WIDEIO_H

#include "DRAM.h"
#include "Request.h"
#include <vector>
#include <functional>

using namespace std;

namespace ramulator
{

class WideIO
{
public:
    static string standard_name;
    enum class Org;
    enum class Speed;
    WideIO(Org org, Speed speed);
    WideIO(const string& org_str, const string& speed_str);
    
    static map<string, enum Org> org_map;
    static map<string, enum Speed> speed_map;

    /*** Level ***/
    enum class Level : int
    { 
        Channel, Rank, Bank, Row, Column, MAX
    };
    
    static std::string level_str [int(Level::MAX)];

    /*** Command ***/
    enum class Command : int
    { 
        ACT, PRE, PRA, 
        RD,  WR,  RDA,  WRA, 
        REF, PD,  PDX,  SREF, SREFX, 
        MAX
    };

    string command_name[int(Command::MAX)] = {
        "ACT", "PRE", "PRA", 
        "RD",  "WR",  "RDA",  "WRA", 
        "REF", "PD", "PDX",  "SREF", "SREFX"
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
    function<Command(DRAM<WideIO>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    // SAUGATA: added function object container for row hit status
    /* Row hit */
    function<bool(DRAM<WideIO>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];
    function<bool(DRAM<WideIO>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];


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
    function<void(DRAM<WideIO>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    /* Organization */
    enum class Org : int
    {
        WideIO_1Gb,
        WideIO_2Gb,
        WideIO_4Gb,
        WideIO_8Gb,
        MAX
    };

    struct OrgEntry {
        int size;
        int dq;
        int count[int(Level::MAX)];
    } org_table[int(Org::MAX)] = {
        // fixed to have 1 rank
        { 256, 128, {0, 1, 4, 1<<12, 1<<7}},
        { 512, 128, {0, 1, 4, 1<<13, 1<<7}},
        {1024, 128, {0, 1, 4, 1<<14, 1<<7}},
        {2048, 128, {0, 1, 4, 1<<15, 1<<7}}
    }, org_entry;

    void set_channel_number(int channel);
    void set_rank_number(int rank);

    /* Speed */
    enum class Speed : int
    {
        WideIO_200,
        WideIO_266,
        MAX
    };
    
    int prefetch_size = 4; // 4n prefetch SDR
    int channel_width = 128;

    struct SpeedEntry {
        int rate;
        double freq, tCK;
        int nBL, nCCD, nDQSCK;
        int nCL, nRCD, nRP, nCWL;
        int nRAS, nRC;
        int nRTP, nRTW, nWTR, nWR;
        int nRRD, nTAW;
        int nRFC, nREFI;
        int nCKE, nXP;
        int nCKESR, nXSR; // tXSR = tRFC+10
    } speed_table[int(Speed::MAX)] = {
        {200, 200.0/3*3, 5.0*3/3, 4, 4, 1, 3, 4, 4, 1,  9, 12, 4, 8, 3, 3, 2, 10, 0, 0, 3, 2, 3, 0},
        {266, 200.0/3*4, 5.0*3/4, 4, 4, 1, 3, 5, 5, 1, 12, 16, 4, 8, 4, 4, 3, 14, 0, 0, 3, 3, 4, 0}
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

#endif /*__WIDEIO_H*/
