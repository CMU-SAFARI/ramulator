/*
 * DSARP.h
 *
 * This a re-implementation of the refresh mechanisms proposed in Chang et al.,
 * "Improving DRAM Performance by Parallelizing Refreshes with Accesses", HPCA
 * 2014.
 *
 * Note: the re-implementation of DSARP has not been widely tested across
 * different benchmarks and parameters. However, timing violations of
 * SARP/DSARP have been checked.
 *
 * Usage: The "type" determines the refresh mechanisms.
 * Examples:
 * DSARP::Org test_org = DSARP::Org::DSARP_8Gb_x8;
 *
 * DSARP* dsddr3_ab = new DSARP(test_org,
 * DSARP::Speed::DSARP_1333, DSARP::Type::REFAB, 8);
 *
 * DSARP* dsddr3_pb = new DSARP(test_org,
 * DSARP::Speed::DSARP_1333, DSARP::Type::REFPB, 8);
 *
 * DSARP* dsddr3_darp = new DSARP(test_org,
 * DSARP::Speed::DSARP_1333, DSARP::Type::DARP, 8);
 *
 * DSARP* dsddr3_sarp = new DSARP(test_org,
 * DSARP::Speed::DSARP_1333, DSARP::Type::SARP, 8);
 *
 * DSARP* dsddr3_dsarp = new DSARP(test_org,
 * DSARP::Speed::DSARP_1333, DSARP::Type::DSARP, 8);
 *
 *  Created on: Mar 16, 2015
 *      Author: kevincha
 */

#ifndef DSARP_H_
#define DSARP_H_

#include <vector>
#include <functional>
#include "DRAM.h"
#include "Request.h"

using namespace std;

namespace ramulator
{

class DSARP
{
public:
    static string standard_name;
    enum class Org;
    enum class Speed;
    enum class Type;
    DSARP(Org org, Speed speed, Type type, int n_sa);
    DSARP(const string& org_str, const string& speed_str, Type type, int n_sa);

    static map<string, enum Org> org_map;
    static map<string, enum Speed> speed_map;

    enum class Type : int
    {
        REFAB, REFPB, DARP, SARP, DSARP, MAX
    } type;

    /* Level */
    // NOTE: Although there's subarray, there's no SALP at all. This is used
    // for parallelizing REF and demand accesses.
    enum class Level : int
    {
      Channel, Rank, Bank, SubArray, Row, Column, MAX
    };
    
    static std::string level_str [int(Level::MAX)];

    /* Command */
    enum class Command : int
    {
        ACT, PRE, PREA,
        RD,  WR,  RDA,  WRA,
        REF, REFPB, PDE, PDX, SRE, SRX,
        MAX
    };

    string command_name[int(Command::MAX)] = {
        "ACT", "PRE", "PREA",
        "RD",  "WR",  "RDA",  "WRA",
        "REF", "REFPB",
        "PDE", "PDX", "SRE", "SRX"
    };

    // SubArray scope for REFPB to propagate the timings
    Level scope[int(Command::MAX)] = {
        Level::Row,    Level::Bank,   Level::Rank,
        Level::Column, Level::Column, Level::Column, Level::Column,
        Level::Rank,   Level::Bank,
        Level::Rank,   Level::Rank,   Level::Rank,   Level::Rank
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
        Command::REF, Command::PDE, Command::SRE
    };

    /* Prerequisite */
    function<Command(DRAM<DSARP>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    // SAUGATA: added function object container for row hit status
    /* Row hit */
    function<bool(DRAM<DSARP>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];
    function<bool(DRAM<DSARP>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

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
    function<void(DRAM<DSARP>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    /* Organization */
    enum class Org : int
    {
        // These are the configurations used in the original paper, essentially DDR3
        DSARP_8Gb_x8,
        DSARP_16Gb_x8,
        DSARP_32Gb_x8,
        MAX
    };

    struct OrgEntry {
        int size;
        int dq;
        int count[int(Level::MAX)];
    } org_table[int(Org::MAX)] = {
        // IMPORTANT: Do not change the count for channel/rank, where is set to
        // 0 now. 0 means that this a flexible configuration that is not part
        // of the spec, but rather something to change at a higher level
        // (main.cpp).
        {8<<10, 8,  {0, 0, 8, 0, 1<<16, 1<<11}},
        {16<<10, 8, {0, 0, 8, 0, 1<<17, 1<<11}},
        {32<<10, 8, {0, 0, 8, 0, 1<<18, 1<<11}},
    }, org_entry;

    void set_channel_number(int channel);
    void set_rank_number(int rank);

    /* Speed */
    enum class Speed : int
    {
        DSARP_1333,
        MAX
    };

    enum class RefreshMode : int
    {
        Refresh_1X,
        MAX
    } refresh_mode = RefreshMode::Refresh_1X;

    int prefetch_size = 16; // 16n prefetch DDR
    int channel_width = 32;

    struct SpeedEntry {
        int rate;
        double freq, tCK;
        int nBL, nCCD, nRTRS;
        int nCL, nRCD, nRPpb, nRPab, nCWL;
        int nRAS, nRC;
        int nRTP, nWTR, nWR;
        int nRRD, nFAW;
        int nRFCab, nRFCpb, nREFI, nREFIpb;
        int nPD, nXP, nXPDLL;
        int nCKESR, nXS, nXSDLL;
        //int nCKE, nXP; // CKE value n/a
        //int nSR, nXSR; // tXSR = tRFCab + 7.5ns
    } speed_table[int(Speed::MAX)] = {
      {1333,
      (400.0/3)*5, (3/0.4)/5,
       4, 4, 2,
       9, 9, 8, 9, 7,
       24, 33,
       5, 5, 10,
       5, 30,
       0, 0, 0, 0, // set in DSARP.cpp
       4, 4, 16,
       5, 114, 512},
    }, speed_entry;

    int read_latency;

    // Number of subarrays -- mainly for SARP. Doesn't affect others.
    int n_sa;

    // Refresh rank?
    bool b_ref_rank;

    // Increase RRD b/w REF and ACT when they go to the same bank (SARP)
    double nRRD_factor = 1.138;

private:
    void init_speed();
    void init_lambda();
    void init_prereq();
    void init_rowhit();  // SAUGATA: added function to check for row hits
    void init_rowopen();
    void init_timing();
};

} /*namespace ramulator*/

#endif /* DSARP_H_ */
