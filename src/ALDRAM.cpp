#include <iostream>

#include "ALDRAM.h"
#include "DRAM.h"
#include <vector>
#include <functional>
#include <cassert>

using namespace std;
using namespace ramulator;

string ALDRAM::standard_name = "ALDRAM";
string ALDRAM::level_str [int(Level::MAX)] = {"Ch", "Ra", "Ba", "Ro", "Co"};

map<string, enum ALDRAM::Org> ALDRAM::org_map = {
    {"ALDRAM_512Mb_x4", ALDRAM::Org::ALDRAM_512Mb_x4}, {"ALDRAM_512Mb_x8", ALDRAM::Org::ALDRAM_512Mb_x8}, {"ALDRAM_512Mb_x16", ALDRAM::Org::ALDRAM_512Mb_x16},
    {"ALDRAM_1Gb_x4", ALDRAM::Org::ALDRAM_1Gb_x4}, {"ALDRAM_1Gb_x8", ALDRAM::Org::ALDRAM_1Gb_x8}, {"ALDRAM_1Gb_x16", ALDRAM::Org::ALDRAM_1Gb_x16},
    {"ALDRAM_2Gb_x4", ALDRAM::Org::ALDRAM_2Gb_x4}, {"ALDRAM_2Gb_x8", ALDRAM::Org::ALDRAM_2Gb_x8}, {"ALDRAM_2Gb_x16", ALDRAM::Org::ALDRAM_2Gb_x16},
    {"ALDRAM_4Gb_x4", ALDRAM::Org::ALDRAM_4Gb_x4}, {"ALDRAM_4Gb_x8", ALDRAM::Org::ALDRAM_4Gb_x8}, {"ALDRAM_4Gb_x16", ALDRAM::Org::ALDRAM_4Gb_x16},
    {"ALDRAM_8Gb_x4", ALDRAM::Org::ALDRAM_8Gb_x4}, {"ALDRAM_8Gb_x8", ALDRAM::Org::ALDRAM_8Gb_x8}, {"ALDRAM_8Gb_x16", ALDRAM::Org::ALDRAM_8Gb_x16},
};

map<string, enum ALDRAM::Speed> ALDRAM::speed_map = {
    {"ALDRAM_800D", ALDRAM::Speed::ALDRAM_800D}, {"ALDRAM_800E", ALDRAM::Speed::ALDRAM_800E},
    {"ALDRAM_1066E", ALDRAM::Speed::ALDRAM_1066E}, {"ALDRAM_1066F", ALDRAM::Speed::ALDRAM_1066F}, {"ALDRAM_1066G", ALDRAM::Speed::ALDRAM_1066G},
    {"ALDRAM_1333G", ALDRAM::Speed::ALDRAM_1333G}, {"ALDRAM_1333H", ALDRAM::Speed::ALDRAM_1333H},
    {"ALDRAM_1600H", ALDRAM::Speed::ALDRAM_1600H}, {"ALDRAM_1600J", ALDRAM::Speed::ALDRAM_1600J}, {"ALDRAM_1600K", ALDRAM::Speed::ALDRAM_1600K},
    {"ALDRAM_1866K", ALDRAM::Speed::ALDRAM_1866K}, {"ALDRAM_1866L", ALDRAM::Speed::ALDRAM_1866L},
    {"ALDRAM_2133L", ALDRAM::Speed::ALDRAM_2133L}, {"ALDRAM_2133M", ALDRAM::Speed::ALDRAM_2133M},
};


ALDRAM::ALDRAM(Org org, Speed speed) :
    org_entry(org_table[int(org)]),
    speed_entry(speed_table[int(Temp::COLD)][int(speed)]),
    read_latency(speed_entry.nCL + speed_entry.nBL)
{
    current_speed = speed;
    init_speed();
    init_prereq();
    init_rowhit(); // SAUGATA: added row hit function
    init_lambda();
    init_timing(speed_table[int(Temp::HOT)][int(speed)]);
    temperature = Temp::COLD;
}

ALDRAM::ALDRAM(const string& org_str, const string& speed_str) :
    ALDRAM(org_map[org_str], speed_map[speed_str])
{
}

void ALDRAM::set_channel_number(int channel) {
  org_entry.count[int(Level::Channel)] = channel;
}

void ALDRAM::set_rank_number(int rank) {
  org_entry.count[int(Level::Rank)] = rank;
}

void ALDRAM::aldram_timing(Temp current_temperature)
{
    for (int i = 0; i < int(Level::MAX); i++) {
        for (int j = 0; j < int(Command::MAX); j++) {
            timing[i][j].clear();
        }
    }
    temperature = current_temperature;
    read_latency = speed_entry.nCL + speed_entry.nBL;
    init_timing(speed_table[int(temperature)][int(current_speed)]);

    //std::cout << "vector size: " << timing[int(Temp::HOT)][int(current_speed)].size() << endl;
    //std::cout << "after cold nRCD : ";
    //std::cout << int(timing[int(Level::Bank)][int(Command::ACT)][0].val) << endl << endl;
}

void ALDRAM::init_speed()
{
    // nRRD, nFAW
    int page = (org_entry.dq * org_entry.count[int(Level::Column)]) >> 13;
    switch (speed_entry.rate) {
        case 800:  speed_entry.nRRD = (page==1) ? 4 : 4; speed_entry.nFAW = (page==1) ? 16 : 20; break;
        case 1066: speed_entry.nRRD = (page==1) ? 4 : 6; speed_entry.nFAW = (page==1) ? 20 : 27; break;
        case 1333: speed_entry.nRRD = (page==1) ? 4 : 5; speed_entry.nFAW = (page==1) ? 20 : 30; break;
        case 1600: speed_entry.nRRD = (page==1) ? 5 : 6; speed_entry.nFAW = (page==1) ? 24 : 32; break;
        case 1866: speed_entry.nRRD = (page==1) ? 5 : 6; speed_entry.nFAW = (page==1) ? 26 : 33; break;
        case 2133: speed_entry.nRRD = (page==1) ? 5 : 6; speed_entry.nFAW = (page==1) ? 27 : 34; break;
        default: assert(false);
    }

    // nRFC, nXS
    int chip = org_entry.size;
    switch (speed_entry.rate) {
        case 800:  speed_entry.nRFC = (chip==512) ? 36  : (chip==1<<10) ? 44  : (chip==1<<11) ? 64  : (chip==1<<12) ? 104 : 140; break;
        case 1066: speed_entry.nRFC = (chip==512) ? 48  : (chip==1<<10) ? 59  : (chip==1<<11) ? 86  : (chip==1<<12) ? 139 : 187; break;
        case 1333: speed_entry.nRFC = (chip==512) ? 60  : (chip==1<<10) ? 74  : (chip==1<<11) ? 107 : (chip==1<<12) ? 174 : 234; break;
        case 1600: speed_entry.nRFC = (chip==512) ? 72  : (chip==1<<10) ? 88  : (chip==1<<11) ? 128 : (chip==1<<12) ? 208 : 280; break;
        case 1866: speed_entry.nRFC = (chip==512) ? 84  : (chip==1<<10) ? 103 : (chip==1<<11) ? 150 : (chip==1<<12) ? 243 : 327; break;
        case 2133: speed_entry.nRFC = (chip==512) ? 96  : (chip==1<<10) ? 118 : (chip==1<<11) ? 171 : (chip==1<<12) ? 278 : 374; break;
        default: assert(false);
    }
    switch (speed_entry.rate) {
        case 800:  speed_entry.nXS  = (chip==512) ? 40  : (chip==1<<10) ? 48  : (chip==1<<11) ? 68  : (chip==1<<12) ? 108 : 144; break;
        case 1066: speed_entry.nXS  = (chip==512) ? 54  : (chip==1<<10) ? 64  : (chip==1<<11) ? 91  : (chip==1<<12) ? 144 : 192; break;
        case 1333: speed_entry.nXS  = (chip==512) ? 67  : (chip==1<<10) ? 80  : (chip==1<<11) ? 114 : (chip==1<<12) ? 180 : 240; break;
        case 1600: speed_entry.nXS  = (chip==512) ? 80  : (chip==1<<10) ? 96  : (chip==1<<11) ? 136 : (chip==1<<12) ? 216 : 288; break;
        case 1866: speed_entry.nXS  = (chip==512) ? 94  : (chip==1<<10) ? 112 : (chip==1<<11) ? 159 : (chip==1<<12) ? 252 : 336; break;
        case 2133: speed_entry.nXS  = (chip==512) ? 107 : (chip==1<<10) ? 128 : (chip==1<<11) ? 182 : (chip==1<<12) ? 288 : 384; break;
        default: assert(false);
    }
}


void ALDRAM::init_prereq()
{
    // RD
    prereq[int(Level::Rank)][int(Command::RD)] = [] (DRAM<ALDRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::MAX;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SRX;
            default: assert(false);
        }};
    prereq[int(Level::Bank)][int(Command::RD)] = [] (DRAM<ALDRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::Closed): return Command::ACT;
            case int(State::Opened):
                if (node->row_state.find(id) != node->row_state.end())
                    return cmd;
                return Command::PRE;
            default: assert(false);
        }};

    // WR
    prereq[int(Level::Rank)][int(Command::WR)] = prereq[int(Level::Rank)][int(Command::RD)];
    prereq[int(Level::Bank)][int(Command::WR)] = prereq[int(Level::Bank)][int(Command::RD)];

    // REF
    prereq[int(Level::Rank)][int(Command::REF)] = [] (DRAM<ALDRAM>* node, Command cmd, int id) {
        for (auto bank : node->children) {
            if (bank->state == State::Closed)
                continue;
            return Command::PREA;
        }
        return Command::REF;};

    // PD
    prereq[int(Level::Rank)][int(Command::PDE)] = [] (DRAM<ALDRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::PDE;
            case int(State::ActPowerDown): return Command::PDE;
            case int(State::PrePowerDown): return Command::PDE;
            case int(State::SelfRefresh): return Command::SRX;
            default: assert(false);
        }};

    // SR
    prereq[int(Level::Rank)][int(Command::SRE)] = [] (DRAM<ALDRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::SRE;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SRE;
            default: assert(false);
        }};
}

// SAUGATA: added row hit check functions to see if the desired location is currently open
void ALDRAM::init_rowhit()
{
    // RD
    rowhit[int(Level::Bank)][int(Command::RD)] = [] (DRAM<ALDRAM>* node, Command cmd, int id) {
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


void ALDRAM::init_lambda()
{
    lambda[int(Level::Bank)][int(Command::ACT)] = [] (DRAM<ALDRAM>* node, int id) {
        node->state = State::Opened;
        node->row_state[id] = State::Opened;};
    lambda[int(Level::Bank)][int(Command::PRE)] = [] (DRAM<ALDRAM>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Rank)][int(Command::PREA)] = [] (DRAM<ALDRAM>* node, int id) {
        for (auto bank : node->children) {
            bank->state = State::Closed;
            bank->row_state.clear();}};
    lambda[int(Level::Rank)][int(Command::REF)] = [] (DRAM<ALDRAM>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::RD)] = [] (DRAM<ALDRAM>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::WR)] = [] (DRAM<ALDRAM>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::RDA)] = [] (DRAM<ALDRAM>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Bank)][int(Command::WRA)] = [] (DRAM<ALDRAM>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Rank)][int(Command::PDE)] = [] (DRAM<ALDRAM>* node, int id) {
        for (auto bank : node->children) {
            if (bank->state == State::Closed)
                continue;
            node->state = State::ActPowerDown;
            return;
        }
        node->state = State::PrePowerDown;};
    lambda[int(Level::Rank)][int(Command::PDX)] = [] (DRAM<ALDRAM>* node, int id) {
        node->state = State::PowerUp;};
    lambda[int(Level::Rank)][int(Command::SRE)] = [] (DRAM<ALDRAM>* node, int id) {
        node->state = State::SelfRefresh;};
    lambda[int(Level::Rank)][int(Command::SRX)] = [] (DRAM<ALDRAM>* node, int id) {
        node->state = State::PowerUp;};
}


void ALDRAM::init_timing(SpeedEntry speed_entry)
{
    SpeedEntry& s = speed_entry;
    vector<TimingEntry> *t;

    /*** Channel ***/
    t = timing[int(Level::Channel)];

    // CAS <-> CAS
    t[int(Command::RD)].push_back({Command::RD, 1, s.nBL});
    t[int(Command::RD)].push_back({Command::RDA, 1, s.nBL});
    t[int(Command::RDA)].push_back({Command::RD, 1, s.nBL});
    t[int(Command::RDA)].push_back({Command::RDA, 1, s.nBL});
    t[int(Command::WR)].push_back({Command::WR, 1, s.nBL});
    t[int(Command::WR)].push_back({Command::WRA, 1, s.nBL});
    t[int(Command::WRA)].push_back({Command::WR, 1, s.nBL});
    t[int(Command::WRA)].push_back({Command::WRA, 1, s.nBL});


    /*** Rank ***/
    t = timing[int(Level::Rank)];

    // CAS <-> CAS
    t[int(Command::RD)].push_back({Command::RD, 1, s.nCCD});
    t[int(Command::RD)].push_back({Command::RDA, 1, s.nCCD});
    t[int(Command::RDA)].push_back({Command::RD, 1, s.nCCD});
    t[int(Command::RDA)].push_back({Command::RDA, 1, s.nCCD});
    t[int(Command::WR)].push_back({Command::WR, 1, s.nCCD});
    t[int(Command::WR)].push_back({Command::WRA, 1, s.nCCD});
    t[int(Command::WRA)].push_back({Command::WR, 1, s.nCCD});
    t[int(Command::WRA)].push_back({Command::WRA, 1, s.nCCD});
    t[int(Command::RD)].push_back({Command::WR, 1, s.nCL + s.nCCD + 2 - s.nCWL});
    t[int(Command::RD)].push_back({Command::WRA, 1, s.nCL + s.nCCD + 2 - s.nCWL});
    t[int(Command::RDA)].push_back({Command::WR, 1, s.nCL + s.nCCD + 2 - s.nCWL});
    t[int(Command::RDA)].push_back({Command::WRA, 1, s.nCL + s.nCCD + 2 - s.nCWL});
    t[int(Command::WR)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTR});
    t[int(Command::WR)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nWTR});
    t[int(Command::WRA)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTR});
    t[int(Command::WRA)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nWTR});

    // CAS <-> CAS (between sibling ranks)
    t[int(Command::RD)].push_back({Command::RD, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RD)].push_back({Command::RDA, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RDA)].push_back({Command::RD, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RDA)].push_back({Command::RDA, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RD)].push_back({Command::WR, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RD)].push_back({Command::WRA, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RDA)].push_back({Command::WR, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RDA)].push_back({Command::WRA, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RD)].push_back({Command::WR, 1, s.nCL + s.nBL + s.nRTRS - s.nCWL, true});
    t[int(Command::RD)].push_back({Command::WRA, 1, s.nCL + s.nBL + s.nRTRS - s.nCWL, true});
    t[int(Command::RDA)].push_back({Command::WR, 1, s.nCL + s.nBL + s.nRTRS - s.nCWL, true});
    t[int(Command::RDA)].push_back({Command::WRA, 1, s.nCL + s.nBL + s.nRTRS - s.nCWL, true});
    t[int(Command::WR)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nRTRS - s.nCL, true});
    t[int(Command::WR)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nRTRS - s.nCL, true});
    t[int(Command::WRA)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nRTRS - s.nCL, true});
    t[int(Command::WRA)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nRTRS - s.nCL, true});

    t[int(Command::RD)].push_back({Command::PREA, 1, s.nRTP});
    t[int(Command::WR)].push_back({Command::PREA, 1, s.nCWL + s.nBL + s.nWR});

    // CAS <-> PD
    t[int(Command::RD)].push_back({Command::PDE, 1, s.nCL + s.nBL + 1});
    t[int(Command::RDA)].push_back({Command::PDE, 1, s.nCL + s.nBL + 1});
    t[int(Command::WR)].push_back({Command::PDE, 1, s.nCWL + s.nBL + s.nWR});
    t[int(Command::WRA)].push_back({Command::PDE, 1, s.nCWL + s.nBL + s.nWR + 1}); // +1 for pre
    t[int(Command::PDX)].push_back({Command::RD, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::RDA, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::WR, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::WRA, 1, s.nXP});

    // CAS <-> SR: none (all banks have to be precharged)

    // RAS <-> RAS
    t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRRD});
    t[int(Command::ACT)].push_back({Command::ACT, 4, s.nFAW});
    t[int(Command::ACT)].push_back({Command::PREA, 1, s.nRAS});
    t[int(Command::PREA)].push_back({Command::ACT, 1, s.nRP});

    // RAS <-> REF
    t[int(Command::PRE)].push_back({Command::REF, 1, s.nRP});
    t[int(Command::PREA)].push_back({Command::REF, 1, s.nRP});
    t[int(Command::REF)].push_back({Command::ACT, 1, s.nRFC});

    // RAS <-> PD
    t[int(Command::ACT)].push_back({Command::PDE, 1, 1});
    t[int(Command::PDX)].push_back({Command::ACT, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::PRE, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::PREA, 1, s.nXP});

    // RAS <-> SR
    t[int(Command::PRE)].push_back({Command::SRE, 1, s.nRP});
    t[int(Command::PREA)].push_back({Command::SRE, 1, s.nRP});
    t[int(Command::SRX)].push_back({Command::ACT, 1, s.nXS});

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
    t[int(Command::ACT)].push_back({Command::RDA, 1, s.nRCD});
    t[int(Command::ACT)].push_back({Command::WR, 1, s.nRCD});
    t[int(Command::ACT)].push_back({Command::WRA, 1, s.nRCD});

    t[int(Command::RD)].push_back({Command::PRE, 1, s.nRTP});
    t[int(Command::WR)].push_back({Command::PRE, 1, s.nCWL + s.nBL + s.nWR});

    t[int(Command::RDA)].push_back({Command::ACT, 1, s.nRTP + s.nRP});
    t[int(Command::WRA)].push_back({Command::ACT, 1, s.nCWL + s.nBL + s.nWR + s.nRP});

    // RAS <-> RAS
    t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRC});
    t[int(Command::ACT)].push_back({Command::PRE, 1, s.nRAS});
    t[int(Command::PRE)].push_back({Command::ACT, 1, s.nRP});
}
