/*
*
* The timing parameters used in this file are provided by the following study:
* Benjamin C. Lee, Engin Ipek, Onur Mutlu, and Doug Burger. 2009.
* Architecting phase change memory as a scalable dram alternative.
* In Proceedings of the 36th annual international symposium on Computer architecture (ISCA '09).
* ACM, New York, NY, USA, 2-13.
* DOI: https://doi.org/10.1145/1555754.1555758
*
*/
#ifndef __PCM_H
#define __PCM_H

#include "DRAM.h"
#include "Request.h"
#include <vector>
#include <map>
#include <string>
#include <functional>

using namespace std;

namespace ramulator
{

class PCM
{
public:
    static string standard_name;
    enum class Org;
    enum class Speed;
    PCM(Org org, Speed speed);
    PCM(const string& org_str, const string& speed_str);

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
    function<Command(DRAM<PCM>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    // SAUGATA: added function object container for row hit status
    /* Row hit */
    function<bool(DRAM<PCM>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];
    function<bool(DRAM<PCM>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

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
    function<void(DRAM<PCM>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    /* Organization */
    enum class Org : int
    {
        PCM_512Mb_x4, PCM_512Mb_x8, PCM_512Mb_x16,
        PCM_1Gb_x4,   PCM_1Gb_x8,   PCM_1Gb_x16,
        PCM_2Gb_x4,   PCM_2Gb_x8,   PCM_2Gb_x16,
        PCM_4Gb_x4,   PCM_4Gb_x8,   PCM_4Gb_x16,
        PCM_8Gb_x4,   PCM_8Gb_x8,   PCM_8Gb_x16,
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
        {8<<10,  4, {0, 0, 8, 1<<16, 1<<12}}, {8<<10,  8, {0, 0, 8, 1<<16, 1<<11}}, {8<<10, 16, {0, 0, 8, 1<<16, 1<<10}}
    }, org_entry;

    void set_channel_number(int channel);
    void set_rank_number(int rank);

    /* Speed */
    enum class Speed : int
    {
        PCM_800D,
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
        int nRRDact, nRRDpre, nFAW;
        int nRFC, nREFI;
        int nPD, nXP, nXPDLL;
        int nCKESR, nXS, nXSDLL;
    } speed_table[int(Speed::MAX)] = {
        {800,  (400.0/3)*3, 2.5, 4, 4, 2,  5,  22,  60,  5, 22, 60, 3, 3,  6, 2, 11, 0, 0, 3900, 0, 3, 10, 4, 0, 512},
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

#endif /*__PCM_H*/
