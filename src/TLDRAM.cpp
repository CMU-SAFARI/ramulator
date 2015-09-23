#include "TLDRAM.h"
#include "DRAM.h"
#include <vector>
#include <functional>
#include <cassert>

#include <iostream>

using namespace std;
using namespace ramulator;

string TLDRAM::standard_name = "TLDRAM";

map<string, enum TLDRAM::Org> TLDRAM::org_map = {
    {"TLDRAM_512Mb_x4", TLDRAM::Org::TLDRAM_512Mb_x4},
    {"TLDRAM_512Mb_x8", TLDRAM::Org::TLDRAM_512Mb_x8},
    {"TLDRAM_512Mb_x16", TLDRAM::Org::TLDRAM_512Mb_x16},
    {"TLDRAM_1Gb_x4", TLDRAM::Org::TLDRAM_1Gb_x4},
    {"TLDRAM_1Gb_x8", TLDRAM::Org::TLDRAM_1Gb_x8},
    {"TLDRAM_1Gb_x16", TLDRAM::Org::TLDRAM_1Gb_x16},
    {"TLDRAM_2Gb_x4", TLDRAM::Org::TLDRAM_2Gb_x4},
    {"TLDRAM_2Gb_x8", TLDRAM::Org::TLDRAM_2Gb_x8},
    {"TLDRAM_2Gb_x16", TLDRAM::Org::TLDRAM_2Gb_x16},
    {"TLDRAM_4Gb_x4", TLDRAM::Org::TLDRAM_4Gb_x4},
    {"TLDRAM_4Gb_x8", TLDRAM::Org::TLDRAM_4Gb_x8},
    {"TLDRAM_4Gb_x16", TLDRAM::Org::TLDRAM_4Gb_x16},
    {"TLDRAM_8Gb_x4", TLDRAM::Org::TLDRAM_8Gb_x4},
    {"TLDRAM_8Gb_x8", TLDRAM::Org::TLDRAM_8Gb_x8},
    {"TLDRAM_8Gb_x16", TLDRAM::Org::TLDRAM_8Gb_x16},
};

map<string, enum TLDRAM::Speed> TLDRAM::speed_map = {
    {"TLDRAM_800D", TLDRAM::Speed::TLDRAM_800D},
    {"TLDRAM_800E", TLDRAM::Speed::TLDRAM_800E},
    {"TLDRAM_1066E", TLDRAM::Speed::TLDRAM_1066E},
    {"TLDRAM_1066F", TLDRAM::Speed::TLDRAM_1066F},
    {"TLDRAM_1066G", TLDRAM::Speed::TLDRAM_1066G},
    {"TLDRAM_1333G", TLDRAM::Speed::TLDRAM_1333G},
    {"TLDRAM_1333H", TLDRAM::Speed::TLDRAM_1333H},
    {"TLDRAM_1600H", TLDRAM::Speed::TLDRAM_1600H},
    {"TLDRAM_1600J", TLDRAM::Speed::TLDRAM_1600J},
    {"TLDRAM_1600K", TLDRAM::Speed::TLDRAM_1600K},
    {"TLDRAM_1866K", TLDRAM::Speed::TLDRAM_1866K},
    {"TLDRAM_1866L", TLDRAM::Speed::TLDRAM_1866L},
    {"TLDRAM_2133L", TLDRAM::Speed::TLDRAM_2133L},
    {"TLDRAM_2133M", TLDRAM::Speed::TLDRAM_2133M},
};


TLDRAM::TLDRAM(Org org, Speed speed, int segment_ratio) :
    segment_ratio(segment_ratio),
    org_entry(org_table[int(org)]),
    speed_entry(speed_table[int(speed)]),
    read_latency(speed_entry.nCL + speed_entry.nBL)
{
    this->segment_ratio = segment_ratio;
    init_speed();
    init_prereq();
    init_rowhit(); // SAUGATA: added row hit function
    init_rowopen();
    init_lambda();
    init_timing();
}

TLDRAM::TLDRAM(const string& org_str, const string& speed_str, int segment_ratio) :
    TLDRAM(org_map[org_str], speed_map[speed_str], segment_ratio)
{
    this->segment_ratio = segment_ratio;
}

void TLDRAM::set_channel_number(int channel) {
  org_entry.count[int(Level::Channel)] = channel;
}

void TLDRAM::set_rank_number(int rank) {
  org_entry.count[int(Level::Rank)] = rank;
}

void TLDRAM::init_speed()
{
    // nRRD, nFAW
    int page = (org_entry.dq * org_entry.count[int(Level::Column)]) >> 13;
    switch (speed_entry.rate) {
        case 800:  speed_entry.nRRD = (page==1) ? 4 : 4;
          speed_entry.nFAW = (page==1) ? 16 : 20;
          break;
        case 1066: speed_entry.nRRD = (page==1) ? 4 : 6;
          speed_entry.nFAW = (page==1) ? 20 : 27;
          break;
        case 1333:
          speed_entry.nRRD = (page==1) ? 4 : 5;
          speed_entry.nFAW = (page==1) ? 20 : 30;
          break;
        case 1600:
          speed_entry.nRRD = (page==1) ? 5 : 6;
          speed_entry.nFAW = (page==1) ? 24 : 32;
          break;
        case 1866:
          speed_entry.nRRD = (page==1) ? 5 : 6;
          speed_entry.nFAW = (page==1) ? 26 : 33;
          break;
        case 2133:
          speed_entry.nRRD = (page==1) ? 5 : 6;
          speed_entry.nFAW = (page==1) ? 27 : 34;
          break;
        default: assert(false);
    }

    // nRFC, nXS
    int chip = org_entry.size;
    switch (speed_entry.rate) {
        case 800:  speed_entry.nRFC = (chip==512) ? 36  : (chip==1<<10) ? 44
          : (chip==1<<11) ? 64  : (chip==1<<12) ? 104 : 140; break;
        case 1066: speed_entry.nRFC = (chip==512) ? 48  : (chip==1<<10) ? 59
          : (chip==1<<11) ? 86  : (chip==1<<12) ? 139 : 187; break;
        case 1333: speed_entry.nRFC = (chip==512) ? 60  : (chip==1<<10) ? 74
          : (chip==1<<11) ? 107 : (chip==1<<12) ? 174 : 234; break;
        case 1600: speed_entry.nRFC = (chip==512) ? 72  : (chip==1<<10) ? 88
          : (chip==1<<11) ? 128 : (chip==1<<12) ? 208 : 280; break;
        case 1866: speed_entry.nRFC = (chip==512) ? 84  : (chip==1<<10) ? 103
          : (chip==1<<11) ? 150 : (chip==1<<12) ? 243 : 327; break;
        case 2133: speed_entry.nRFC = (chip==512) ? 96  : (chip==1<<10) ? 118
          : (chip==1<<11) ? 171 : (chip==1<<12) ? 278 : 374; break;
        default: assert(false);
    }
    switch (speed_entry.rate) {
        case 800:  speed_entry.nXS  = (chip==512) ? 40  : (chip==1<<10) ? 48
          : (chip==1<<11) ? 68  : (chip==1<<12) ? 108 : 144; break;
        case 1066: speed_entry.nXS  = (chip==512) ? 54  : (chip==1<<10) ? 64
          : (chip==1<<11) ? 91  : (chip==1<<12) ? 144 : 192; break;
        case 1333: speed_entry.nXS  = (chip==512) ? 67  : (chip==1<<10) ? 80
          : (chip==1<<11) ? 114 : (chip==1<<12) ? 180 : 240; break;
        case 1600: speed_entry.nXS  = (chip==512) ? 80  : (chip==1<<10) ? 96
          : (chip==1<<11) ? 136 : (chip==1<<12) ? 216 : 288; break;
        case 1866: speed_entry.nXS  = (chip==512) ? 94  : (chip==1<<10) ? 112
          : (chip==1<<11) ? 159 : (chip==1<<12) ? 252 : 336; break;
        case 2133: speed_entry.nXS  = (chip==512) ? 107 : (chip==1<<10) ? 128
          : (chip==1<<11) ? 182 : (chip==1<<12) ? 288 : 384; break;
        default: assert(false);
    }
}


void TLDRAM::init_prereq()
{
    // RD
    prereq[int(Level::Rank)][int(Command::RD)] = [] (DRAM<TLDRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::MAX;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SRX;
            default: assert(false);
        }
    };
    prereq[int(Level::Rank)][int(Command::MIG)] = prereq[int(Level::Rank)][int(Command::RD)];

    prereq[int(Level::Bank)][int(Command::RD)] = [] (DRAM<TLDRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::Closed):
                if (id % node->spec->segment_ratio)
                    return Command::ACT;
                else
                    return Command::ACTF;
            case int(State::Opened):
                if (node->row_state.find(id) != node->row_state.end())
                    return cmd;
                if (id % node->spec->segment_ratio)
                    return Command::PRE;
                else
                    return Command::PREF;
            default: assert(false);
        }
    };
    prereq[int(Level::Bank)][int(Command::MIG)] = [] (DRAM<TLDRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::Closed):
                return Command::ACTM;
            case int(State::Opened):
                if (node->row_state.find(id) != node->row_state.end())
                    return cmd;
                return Command::PREM;
            default: assert(false);
        }
    };

    // WR
    prereq[int(Level::Rank)][int(Command::WR)] = prereq[int(Level::Rank)][int(Command::RD)];
    prereq[int(Level::Bank)][int(Command::WR)] = prereq[int(Level::Bank)][int(Command::RD)];

    // REF
    prereq[int(Level::Rank)][int(Command::REF)] = [] (DRAM<TLDRAM>* node, Command cmd, int id) {
        for (auto bank : node->children) {
            if (bank->state == State::Closed)
                continue;
            if (id % node->spec->segment_ratio)
                return Command::PREA;
            else
                return Command::PREAF;
        }
        return Command::REF;
    };

    // PD
    prereq[int(Level::Rank)][int(Command::PDE)] = [] (DRAM<TLDRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::PDE;
            case int(State::ActPowerDown): return Command::PDE;
            case int(State::PrePowerDown): return Command::PDE;
            case int(State::SelfRefresh): return Command::SRX;
            default: assert(false);
        }
    };

    // SR
    prereq[int(Level::Rank)][int(Command::SRE)] = [] (DRAM<TLDRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::SRE;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SRE;
            default: assert(false);
        }
    };
}

// SAUGATA: added row hit check functions to see if the desired location is currently open
void TLDRAM::init_rowhit()
{
    // RD
    rowhit[int(Level::Bank)][int(Command::RD)] = [] (DRAM<TLDRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::Closed): return false;
            case int(State::Opened):
                if (node->row_state.find(id) != node->row_state.end())
                    return true;
                return false;
            default: assert(false);
        }};

    // WR
    rowhit[int(Level::Bank)][int(Command::WR)] = rowhit[int(Level::Bank)][int(Command::RD)];
}

void TLDRAM::init_rowopen()
{
    // RD
    rowopen[int(Level::Bank)][int(Command::RD)] = [] (DRAM<TLDRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::Closed): return false;
            case int(State::Opened): return true;
            default: assert(false);
        }};

    // WR
    rowopen[int(Level::Bank)][int(Command::WR)] = rowopen[int(Level::Bank)][int(Command::RD)];
}

void TLDRAM::init_lambda()
{
    lambda[int(Level::Bank)][int(Command::ACT)] = [] (DRAM<TLDRAM>* node, int id) {
        node->state = State::Opened;
        node->row_state[id] = State::Opened;
    };
    lambda[int(Level::Bank)][int(Command::ACTF)] = [] (DRAM<TLDRAM>* node, int id) {
        node->state = State::Opened;
        node->row_state[id] = State::Opened;
    };
    lambda[int(Level::Bank)][int(Command::ACTM)] = [] (DRAM<TLDRAM>* node, int id) {
        node->state = State::Opened;
        node->row_state[id] = State::Opened;
    };
    lambda[int(Level::Bank)][int(Command::PRE)] = [] (DRAM<TLDRAM>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();
    };
    lambda[int(Level::Bank)][int(Command::PREF)] = [] (DRAM<TLDRAM>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();
    };
    lambda[int(Level::Bank)][int(Command::PREM)] = [] (DRAM<TLDRAM>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();
    };
    lambda[int(Level::Rank)][int(Command::PREA)] = [] (DRAM<TLDRAM>* node, int id) {
        for (auto bank : node->children) {
            bank->state = State::Closed;
            bank->row_state.clear();
        }
    };
    lambda[int(Level::Rank)][int(Command::PREAF)] = [] (DRAM<TLDRAM>* node, int id) {
        for (auto bank : node->children) {
            bank->state = State::Closed;
            bank->row_state.clear();
        }
    };
    lambda[int(Level::Rank)][int(Command::PREAM)] = [] (DRAM<TLDRAM>* node, int id) {
        for (auto bank : node->children) {
            bank->state = State::Closed;
            bank->row_state.clear();
        }
    };
    lambda[int(Level::Rank)][int(Command::REF)] = [] (DRAM<TLDRAM>* node, int id) {};
    lambda[int(Level::Rank)][int(Command::REF)] = [] (DRAM<TLDRAM>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::RD)] = [] (DRAM<TLDRAM>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::WR)] = [] (DRAM<TLDRAM>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::MIG)] = [] (DRAM<TLDRAM>* node, int id) {};
    lambda[int(Level::Rank)][int(Command::PDE)] = [] (DRAM<TLDRAM>* node, int id) {
        for (auto bank : node->children) {
            if (bank->state == State::Closed)
                continue;
            node->state = State::ActPowerDown;
            return;
        }
        node->state = State::PrePowerDown;
    };
    lambda[int(Level::Rank)][int(Command::PDX)] = [] (DRAM<TLDRAM>* node, int id) {
        node->state = State::PowerUp;
    };
    lambda[int(Level::Rank)][int(Command::SRE)] = [] (DRAM<TLDRAM>* node, int id) {
        node->state = State::SelfRefresh;
    };
    lambda[int(Level::Rank)][int(Command::SRX)] = [] (DRAM<TLDRAM>* node, int id) {
        node->state = State::PowerUp;
    };
}


void TLDRAM::init_timing()
{
    SpeedEntry& s = speed_entry;
    vector<TimingEntry> *t;

    /*** Channel ***/
    t = timing[int(Level::Channel)];

    // CAS <-> CAS
    t[int(Command::RD)].push_back({Command::RD, 1, s.nBL});
    t[int(Command::RD)].push_back({Command::MIG, 1, s.nBL});
    t[int(Command::MIG)].push_back({Command::RD, 1, s.nBL});
    t[int(Command::MIG)].push_back({Command::MIG, 1, s.nBL});
    t[int(Command::WR)].push_back({Command::WR, 1, s.nBL});


    /*** Rank ***/
    t = timing[int(Level::Rank)];

    // CAS <-> CAS
    t[int(Command::RD)].push_back({Command::RD, 1, s.nCCD});
    t[int(Command::RD)].push_back({Command::MIG, 1, s.nCCD});
    t[int(Command::MIG)].push_back({Command::RD, 1, s.nCCD});
    t[int(Command::MIG)].push_back({Command::MIG, 1, s.nCCD});

    t[int(Command::RD)].push_back({Command::WR, 1, s.nCL + s.nCCD + 2 - s.nCWL});
    t[int(Command::MIG)].push_back({Command::WR, 1, s.nCL + s.nCCD + 2 - s.nCWL});
    t[int(Command::WR)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTR});
    t[int(Command::WR)].push_back({Command::MIG, 1, s.nCWL + s.nBL + s.nWTR});

    t[int(Command::WR)].push_back({Command::WR, 1, s.nCCD});

    // CAS <-> CAS (between sibling ranks)
    t[int(Command::RD)].push_back({Command::RD, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RD)].push_back({Command::MIG, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RD)].push_back({Command::WR, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RD)].push_back({Command::WR, 1, s.nCL + s.nBL + s.nRTRS - s.nCWL, true});
    t[int(Command::RD)].push_back({Command::PREA, 1, s.nRTP});
    t[int(Command::RD)].push_back({Command::PREAF, 1, s.nRTP});
    t[int(Command::RD)].push_back({Command::PREAM, 1, s.nRTP});

    t[int(Command::MIG)].push_back({Command::RD, 1, s.nBL + s.nRTRS, true});
    t[int(Command::MIG)].push_back({Command::MIG, 1, s.nBL + s.nRTRS, true});
    t[int(Command::MIG)].push_back({Command::WR, 1, s.nBL + s.nRTRS, true});
    t[int(Command::MIG)].push_back({Command::WR, 1, s.nCL + s.nBL + s.nRTRS - s.nCWL, true});
    t[int(Command::MIG)].push_back({Command::PREA, 1, s.nRTP});
    t[int(Command::MIG)].push_back({Command::PREAF, 1, s.nRTP});
    t[int(Command::MIG)].push_back({Command::PREAM, 1, s.nRTP});

    t[int(Command::WR)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nRTRS - s.nCL, true});
    t[int(Command::WR)].push_back({Command::MIG, 1, s.nCWL + s.nBL + s.nRTRS - s.nCL, true});
    t[int(Command::WR)].push_back({Command::PREA, 1, s.nCWL + s.nBL + s.nWR});
    t[int(Command::WR)].push_back({Command::PREAF, 1, s.nCWL + s.nBL + s.nWR});
    t[int(Command::WR)].push_back({Command::PREAM, 1, s.nCWL + s.nBL + s.nWR});

    // CAS <-> PD
    t[int(Command::RD)].push_back({Command::PDE, 1, s.nCL + s.nBL + 1});
    t[int(Command::MIG)].push_back({Command::PDE, 1, s.nCL + s.nBL + 1});
    t[int(Command::WR)].push_back({Command::PDE, 1, s.nCWL + s.nBL + s.nWR});
    t[int(Command::PDX)].push_back({Command::RD, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::MIG, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::WR, 1, s.nXP});

    // CAS <-> SR: none (all banks have to be precharged)

    // RAS <-> RAS
    t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRRD});
    t[int(Command::ACT)].push_back({Command::ACT, 4, s.nFAW});
    t[int(Command::ACT)].push_back({Command::ACTF, 1, s.nRRD});
    t[int(Command::ACT)].push_back({Command::ACTF, 4, s.nFAW});
    t[int(Command::ACT)].push_back({Command::ACTM, 1, s.nRRD});
    t[int(Command::ACT)].push_back({Command::ACTM, 4, s.nFAW});
    t[int(Command::ACT)].push_back({Command::PRE, 1, s.nRAS});
    t[int(Command::ACT)].push_back({Command::PREA, 1, s.nRAS});
    t[int(Command::ACT)].push_back({Command::PREF, 1, s.nRAS});
    t[int(Command::ACT)].push_back({Command::PREAF, 1, s.nRAS});
    t[int(Command::ACT)].push_back({Command::PREM, 1, s.nRAS});
    t[int(Command::ACT)].push_back({Command::PREAM, 1, s.nRAS});

    t[int(Command::ACTF)].push_back({Command::ACT, 1, s.nRRD});
    t[int(Command::ACTF)].push_back({Command::ACT, 4, s.nFAW});
    t[int(Command::ACTF)].push_back({Command::ACTF, 1, s.nRRD});
    t[int(Command::ACTF)].push_back({Command::ACTF, 4, s.nFAW});
    t[int(Command::ACTF)].push_back({Command::ACTM, 1, s.nRRD});
    t[int(Command::ACTF)].push_back({Command::ACTM, 4, s.nFAW});
    t[int(Command::ACTF)].push_back({Command::PRE, 1, s.nRASF});
    t[int(Command::ACTF)].push_back({Command::PREA, 1, s.nRASF});
    t[int(Command::ACTF)].push_back({Command::PREF, 1, s.nRASF});
    t[int(Command::ACTF)].push_back({Command::PREAF, 1, s.nRASF});
    t[int(Command::ACTF)].push_back({Command::PREM, 1, s.nRASF});
    t[int(Command::ACTF)].push_back({Command::PREAM, 1, s.nRASF});

    t[int(Command::ACTM)].push_back({Command::ACT, 1, s.nRRD});
    t[int(Command::ACTM)].push_back({Command::ACT, 4, s.nFAW});
    t[int(Command::ACTM)].push_back({Command::ACTF, 1, s.nRRD});
    t[int(Command::ACTM)].push_back({Command::ACTF, 4, s.nFAW});
    t[int(Command::ACTM)].push_back({Command::ACTM, 1, s.nRRD});
    t[int(Command::ACTM)].push_back({Command::ACTM, 4, s.nFAW});
    t[int(Command::ACTM)].push_back({Command::PRE, 1, s.nRASM});
    t[int(Command::ACTM)].push_back({Command::PREA, 1, s.nRASM});
    t[int(Command::ACTM)].push_back({Command::PREF, 1, s.nRASM});
    t[int(Command::ACTM)].push_back({Command::PREAF, 1, s.nRASM});
    t[int(Command::ACTM)].push_back({Command::PREM, 1, s.nRASM});
    t[int(Command::ACTM)].push_back({Command::PREAM, 1, s.nRASM});

    t[int(Command::PRE)].push_back({Command::ACT, 1, s.nRP});
    t[int(Command::PRE)].push_back({Command::ACTF, 1, s.nRP});
    t[int(Command::PRE)].push_back({Command::ACTM, 1, s.nRP});

    t[int(Command::PREF)].push_back({Command::ACT, 1, s.nRPF});
    t[int(Command::PREF)].push_back({Command::ACTF, 1, s.nRPF});
    t[int(Command::PREF)].push_back({Command::ACTM, 1, s.nRPF});

    t[int(Command::PREM)].push_back({Command::ACT, 1, s.nRPM});
    t[int(Command::PREM)].push_back({Command::ACTF, 1, s.nRPM});
    t[int(Command::PREM)].push_back({Command::ACTM, 1, s.nRPM});

    // RAS <-> REF
    t[int(Command::PRE)].push_back({Command::REF, 1, s.nRP});
    t[int(Command::PREF)].push_back({Command::REF, 1, s.nRPF});
    t[int(Command::PREM)].push_back({Command::REF, 1, s.nRPM});

    t[int(Command::PREA)].push_back({Command::REF, 1, s.nRP});
    t[int(Command::PREAF)].push_back({Command::REF, 1, s.nRPF});
    t[int(Command::PREAM)].push_back({Command::REF, 1, s.nRPM});

    t[int(Command::REF)].push_back({Command::ACT, 1, s.nRFC});
    t[int(Command::REF)].push_back({Command::ACTF, 1, s.nRFC});
    t[int(Command::REF)].push_back({Command::ACTM, 1, s.nRFC});

    // RAS <-> PD
    t[int(Command::ACT)].push_back({Command::PDE, 1, 1});
    t[int(Command::ACTF)].push_back({Command::PDE, 1, 1});
    t[int(Command::ACTM)].push_back({Command::PDE, 1, 1});

    t[int(Command::PDX)].push_back({Command::ACT, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::ACTF, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::ACTM, 1, s.nXP});

    t[int(Command::PDX)].push_back({Command::PRE, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::PREF, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::PREM, 1, s.nXP});

    // RAS <-> SR
    t[int(Command::PRE)].push_back({Command::SRE, 1, s.nRP});
    t[int(Command::PREF)].push_back({Command::SRE, 1, s.nRPF});
    t[int(Command::PREM)].push_back({Command::SRE, 1, s.nRPM});
    t[int(Command::PREA)].push_back({Command::SRE, 1, s.nRP});
    t[int(Command::PREAF)].push_back({Command::SRE, 1, s.nRPF});
    t[int(Command::PREAM)].push_back({Command::SRE, 1, s.nRPM});

    t[int(Command::SRX)].push_back({Command::ACT, 1, s.nXS});
    t[int(Command::SRX)].push_back({Command::ACTF, 1, s.nXS});
    t[int(Command::SRX)].push_back({Command::ACTM, 1, s.nXS});

    // REF <-> REF
    t[int(Command::REF)].push_back({Command::REF, 1, s.nRFC});

    // REF <-> PD
    t[int(Command::REF)].push_back({Command::PDE, 1, 1});
    t[int(Command::PDX)].push_back({Command::REF, 1, s.nXP});

    // REF <-> SR
    t[int(Command::SRX)].push_back({Command::REF, 1, s.nXS});

    // PD <-> PD
    t[int(Command::PDE)].push_back({Command::PDX, 1, s.nPD});
    t[int(Command::PDX)].push_back({Command::PDE, 1, s.nXP});

    // PD <-> SR
    t[int(Command::PDX)].push_back({Command::SRE, 1, s.nXP});
    t[int(Command::SRX)].push_back({Command::PDE, 1, s.nXS});

    // SR <-> SR
    t[int(Command::SRE)].push_back({Command::SRX, 1, s.nCKESR});
    t[int(Command::SRX)].push_back({Command::SRE, 1, s.nXS});


    /*** Bank ***/
    t = timing[int(Level::Bank)];

    // CAS <-> RAS
    t[int(Command::ACT)].push_back({Command::RD, 1, s.nRCD});
    t[int(Command::ACTF)].push_back({Command::RD, 1, s.nRCDF});    // Fast Segment
    t[int(Command::ACTM)].push_back({Command::RD, 1, s.nRCDM});    // Fast Segment

    t[int(Command::ACT)].push_back({Command::MIG, 1, s.nRCD});
    t[int(Command::ACTF)].push_back({Command::MIG, 1, s.nRCDF});  // Fast Segment
    t[int(Command::ACTM)].push_back({Command::MIG, 1, s.nRCDM});  // Fast Segment

    t[int(Command::ACT)].push_back({Command::WR, 1, s.nRCD});
    t[int(Command::ACTF)].push_back({Command::WR, 1, s.nRCDF});    // Fast Segment
    t[int(Command::ACTM)].push_back({Command::WR, 1, s.nRCDM});    // Fast Segment

    t[int(Command::RD)].push_back({Command::PRE, 1, s.nRTP});
    t[int(Command::RD)].push_back({Command::PREF, 1, s.nRTP});
    t[int(Command::RD)].push_back({Command::PREM, 1, s.nRTP});

    t[int(Command::MIG)].push_back({Command::PRE, 1, s.nRTP});
    t[int(Command::MIG)].push_back({Command::PREF, 1, s.nRTP});
    t[int(Command::MIG)].push_back({Command::PREM, 1, s.nRTP});

    t[int(Command::WR)].push_back({Command::PRE, 1, s.nCWL + s.nBL + s.nWR});
    t[int(Command::WR)].push_back({Command::PREF, 1, s.nCWL + s.nBL + s.nWR});


    // RAS <-> RAS
    t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRC});
    t[int(Command::ACT)].push_back({Command::ACTF, 1, s.nRC});
    t[int(Command::ACT)].push_back({Command::ACTM, 1, s.nRC});
    t[int(Command::ACTF)].push_back({Command::ACT, 1, s.nRCF});
    t[int(Command::ACTF)].push_back({Command::ACTF, 1, s.nRCF});
    t[int(Command::ACTF)].push_back({Command::ACTM, 1, s.nRCF});
    t[int(Command::ACTM)].push_back({Command::ACT, 1, s.nRCM});
    t[int(Command::ACTM)].push_back({Command::ACTF, 1, s.nRCM});
    t[int(Command::ACTM)].push_back({Command::ACTM, 1, s.nRCM});

    t[int(Command::ACT)].push_back({Command::PRE, 1, s.nRAS});
    t[int(Command::ACT)].push_back({Command::PREF, 1, s.nRAS});
    t[int(Command::ACT)].push_back({Command::PREM, 1, s.nRAS});
    t[int(Command::ACTF)].push_back({Command::PRE, 1, s.nRASF});
    t[int(Command::ACTF)].push_back({Command::PREF, 1, s.nRASF});
    t[int(Command::ACTF)].push_back({Command::PREM, 1, s.nRASF});
    t[int(Command::ACTM)].push_back({Command::PRE, 1, s.nRASM});
    t[int(Command::ACTM)].push_back({Command::PREF, 1, s.nRASM});
    t[int(Command::ACTM)].push_back({Command::PREM, 1, s.nRASM});

    t[int(Command::PRE)].push_back({Command::ACT, 1, s.nRP});
    t[int(Command::PRE)].push_back({Command::ACTF, 1, s.nRP});
    t[int(Command::PRE)].push_back({Command::ACTM, 1, s.nRP});
    t[int(Command::PREF)].push_back({Command::ACT, 1, s.nRPF});
    t[int(Command::PREF)].push_back({Command::ACTF, 1, s.nRPF});
    t[int(Command::PREF)].push_back({Command::ACTM, 1, s.nRPF});
    t[int(Command::PREM)].push_back({Command::ACT, 1, s.nRPM});
    t[int(Command::PREM)].push_back({Command::ACTF, 1, s.nRPM});
    t[int(Command::PREM)].push_back({Command::ACTM, 1, s.nRPM});

}
