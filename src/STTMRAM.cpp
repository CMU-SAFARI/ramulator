/*
*
* The timing parameters used in this file are provided by the following study:
* Kazi Asifuzzaman, Rommel Sanchez Verdejo, and Petar Radojkovic.2017.
* Enabling a reliable STT-MRAM main memory simulation.
* In Proceedings of the International Symposium on Memory Systems (MEMSYS '17).
* Washington DC, USA, 283-292. DOI: https://doi.org/10.1145/3132402.3132416
*
*/
#include "STTMRAM.h"
#include "DRAM.h"
#include <vector>
#include <functional>
#include <cassert>
#include <math.h>

using namespace std;
using namespace ramulator;

string STTMRAM::standard_name = "STTMRAM";
string STTMRAM::level_str [int(Level::MAX)] = {"Ch", "Ra", "Ba", "Ro", "Co"};

map<string, enum STTMRAM::Org> STTMRAM::org_map = {
    {"STTMRAM_512Mb_x4", STTMRAM::Org::STTMRAM_512Mb_x4}, {"STTMRAM_512Mb_x8", STTMRAM::Org::STTMRAM_512Mb_x8}, {"STTMRAM_512Mb_x16", STTMRAM::Org::STTMRAM_512Mb_x16},
    {"STTMRAM_1Gb_x4", STTMRAM::Org::STTMRAM_1Gb_x4}, {"STTMRAM_1Gb_x8", STTMRAM::Org::STTMRAM_1Gb_x8}, {"STTMRAM_1Gb_x16", STTMRAM::Org::STTMRAM_1Gb_x16},
    {"STTMRAM_2Gb_x4", STTMRAM::Org::STTMRAM_2Gb_x4}, {"STTMRAM_2Gb_x8", STTMRAM::Org::STTMRAM_2Gb_x8}, {"STTMRAM_2Gb_x16", STTMRAM::Org::STTMRAM_2Gb_x16},
    {"STTMRAM_4Gb_x4", STTMRAM::Org::STTMRAM_4Gb_x4}, {"STTMRAM_4Gb_x8", STTMRAM::Org::STTMRAM_4Gb_x8}, {"STTMRAM_4Gb_x16", STTMRAM::Org::STTMRAM_4Gb_x16},
    {"STTMRAM_8Gb_x4", STTMRAM::Org::STTMRAM_8Gb_x4}, {"STTMRAM_8Gb_x8", STTMRAM::Org::STTMRAM_8Gb_x8}, {"STTMRAM_8Gb_x16", STTMRAM::Org::STTMRAM_8Gb_x16},
};

map<string, enum STTMRAM::Speed> STTMRAM::speed_map = {
    {"STT_1600_1_2", STTMRAM::Speed::STT_1600_1_2}, {"STT_1600_1_5", STTMRAM::Speed::STT_1600_1_5}, {"STT_1600_2_0", STTMRAM::Speed::STT_1600_2_0},
};


STTMRAM::STTMRAM(Org org, Speed speed) :
    org_entry(org_table[int(org)]),
    speed_entry(speed_table[int(speed)]),
    read_latency(speed_entry.nCL + speed_entry.nBL)
{
    init_speed();
    init_prereq();
    init_rowhit(); // SAUGATA: added row hit function
    init_rowopen();
    init_lambda();
    init_timing();
}

STTMRAM::STTMRAM(const string& org_str, const string& speed_str) :
    STTMRAM(org_map[org_str], speed_map[speed_str])
{
}

void STTMRAM::set_channel_number(int channel) {
  org_entry.count[int(Level::Channel)] = channel;
}

void STTMRAM::set_rank_number(int rank) {
  org_entry.count[int(Level::Rank)] = rank;
}

void STTMRAM::init_speed()
{
    // nRRD, nFAW
    int page = (org_entry.dq * org_entry.count[int(Level::Column)]) >> 13;
    switch (speed_entry.rate) {
          case 1600: speed_entry.nRRD = ceil(speed_entry.nRRD  * ((page==1) ? 5 : 6) /10.0); speed_entry.nFAW = ceil(speed_entry.nFAW *((page==1) ? 24 : 32)/10.0); break;
        default: assert(false);
    }

    // nRFC, nXS
    int chip = org_entry.size;
    switch (speed_entry.rate) {
        case 1600: speed_entry.nRFC = 1; break;
        default: assert(false);
    }
    switch (speed_entry.rate) {
          case 1600: speed_entry.nXS  = ceil(speed_entry.nXS * ((chip==512) ? 80  : (chip==1<<10) ? 96  : (chip==1<<11) ? 136 : (chip==1<<12) ? 216 : 288) /10.0); break;
        default: assert(false);
    }
}


void STTMRAM::init_prereq()
{
    // RD
    prereq[int(Level::Rank)][int(Command::RD)] = [] (DRAM<STTMRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::MAX;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SRX;
            default: assert(false);
        }};
    prereq[int(Level::Bank)][int(Command::RD)] = [] (DRAM<STTMRAM>* node, Command cmd, int id) {
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
    prereq[int(Level::Rank)][int(Command::REF)] = [] (DRAM<STTMRAM>* node, Command cmd, int id) {
        for (auto bank : node->children) {
            if (bank->state == State::Closed)
                continue;
            return Command::PREA;
        }
        return Command::REF;};

    // PD
    prereq[int(Level::Rank)][int(Command::PDE)] = [] (DRAM<STTMRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::PDE;
            case int(State::ActPowerDown): return Command::PDE;
            case int(State::PrePowerDown): return Command::PDE;
            case int(State::SelfRefresh): return Command::SRX;
            default: assert(false);
        }};

    // SR
    prereq[int(Level::Rank)][int(Command::SRE)] = [] (DRAM<STTMRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::SRE;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SRE;
            default: assert(false);
        }};
}


// SAUGATA: added row hit check functions to see if the desired location is currently open
void STTMRAM::init_rowhit()
{
    // RD
    rowhit[int(Level::Bank)][int(Command::RD)] = [] (DRAM<STTMRAM>* node, Command cmd, int id) {
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

void STTMRAM::init_rowopen()
{
    // RD
    rowopen[int(Level::Bank)][int(Command::RD)] = [] (DRAM<STTMRAM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::Closed): return false;
            case int(State::Opened): return true;
            default: assert(false);
        }};

    // WR
    rowopen[int(Level::Bank)][int(Command::WR)] = rowopen[int(Level::Bank)][int(Command::RD)];
}

void STTMRAM::init_lambda()
{
    lambda[int(Level::Bank)][int(Command::ACT)] = [] (DRAM<STTMRAM>* node, int id) {
        node->state = State::Opened;
        node->row_state[id] = State::Opened;};
    lambda[int(Level::Bank)][int(Command::PRE)] = [] (DRAM<STTMRAM>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Rank)][int(Command::PREA)] = [] (DRAM<STTMRAM>* node, int id) {
        for (auto bank : node->children) {
            bank->state = State::Closed;
            bank->row_state.clear();}};
    lambda[int(Level::Rank)][int(Command::REF)] = [] (DRAM<STTMRAM>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::RD)] = [] (DRAM<STTMRAM>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::WR)] = [] (DRAM<STTMRAM>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::RDA)] = [] (DRAM<STTMRAM>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Bank)][int(Command::WRA)] = [] (DRAM<STTMRAM>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Rank)][int(Command::PDE)] = [] (DRAM<STTMRAM>* node, int id) {
        for (auto bank : node->children) {
            if (bank->state == State::Closed)
                continue;
            node->state = State::ActPowerDown;
            return;
        }
        node->state = State::PrePowerDown;};
    lambda[int(Level::Rank)][int(Command::PDX)] = [] (DRAM<STTMRAM>* node, int id) {
        node->state = State::PowerUp;};
    lambda[int(Level::Rank)][int(Command::SRE)] = [] (DRAM<STTMRAM>* node, int id) {
        node->state = State::SelfRefresh;};
    lambda[int(Level::Rank)][int(Command::SRX)] = [] (DRAM<STTMRAM>* node, int id) {
        node->state = State::PowerUp;};
}


void STTMRAM::init_timing()
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
