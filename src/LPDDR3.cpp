#include "LPDDR3.h"
#include "DRAM.h"

#include <vector>
#include <functional>
#include <cassert>

using namespace std;
using namespace ramulator;

string LPDDR3::standard_name = "LPDDR3";

map<string, enum LPDDR3::Org> LPDDR3::org_map = {
    {"LPDDR3_4Gb_x16", LPDDR3::Org::LPDDR3_4Gb_x16}, {"LPDDR3_4Gb_x32", LPDDR3::Org::LPDDR3_4Gb_x32},
    {"LPDDR3_6Gb_x16", LPDDR3::Org::LPDDR3_6Gb_x16}, {"LPDDR3_6Gb_x32", LPDDR3::Org::LPDDR3_6Gb_x32},
    {"LPDDR3_8Gb_x16", LPDDR3::Org::LPDDR3_8Gb_x16}, {"LPDDR3_8Gb_x32", LPDDR3::Org::LPDDR3_8Gb_x32},
    {"LPDDR3_12Gb_x16", LPDDR3::Org::LPDDR3_12Gb_x16}, {"LPDDR3_12Gb_x32", LPDDR3::Org::LPDDR3_12Gb_x32},
    {"LPDDR3_16Gb_x16", LPDDR3::Org::LPDDR3_16Gb_x16}, {"LPDDR3_16Gb_x32", LPDDR3::Org::LPDDR3_16Gb_x32},
};

map<string, enum LPDDR3::Speed> LPDDR3::speed_map = {
    {"LPDDR3_1333", LPDDR3::Speed::LPDDR3_1333},
    {"LPDDR3_1600", LPDDR3::Speed::LPDDR3_1600},
    {"LPDDR3_1866", LPDDR3::Speed::LPDDR3_1866},
    {"LPDDR3_2133", LPDDR3::Speed::LPDDR3_2133},
};

LPDDR3::LPDDR3(Org org, Speed speed)
    : org_entry(org_table[int(org)]),
    speed_entry(speed_table[int(speed)]),
    read_latency(speed_entry.nCL + speed_entry.nDQSCK + speed_entry.nBL)
{
    init_speed();
    init_prereq();
    init_rowhit(); // SAUGATA: added row hit function
    init_rowopen();
    init_lambda();
    init_timing();
}

LPDDR3::LPDDR3(const string& org_str, const string& speed_str) :
    LPDDR3(org_map[org_str], speed_map[speed_str])
{
}

void LPDDR3::set_channel_number(int channel) {
  org_entry.count[int(Level::Channel)] = channel;
}

void LPDDR3::set_rank_number(int rank) {
  org_entry.count[int(Level::Rank)] = rank;
}

void LPDDR3::init_speed()
{
    // 12Gb/16Gb RFCab/RFCpb TBD

    const static int RFCPB_TABLE[3][4] = {
        {40, 48, 56, 64},
        {60, 72, 84, 96},
        {60, 72, 84, 96}
    };

    const static int RFCAB_TABLE[3][4] = {
        {87, 104, 122, 139},
        {140, 168, 196, 224},
        {140, 168, 196, 224}
    };

    const static int XSR_TABLE[3][4] = {
        {94, 112, 131, 150},
        {147, 176, 206, 235},
        {147, 176, 206, 235}
    };

    int speed = 0, density = 0;
    switch (speed_entry.rate) {
        case 1333: speed = 0; break;
        case 1600: speed = 1; break;
        case 1866: speed = 2; break;
        case 2133: speed = 3; break;
        default: assert(false);
    };
    switch (org_entry.size >> 10){
        case 4: density = 0; break;
        case 6: density = 1; break;
        case 8: density = 2; break;
        default: assert(false && "12Gb/16Gb is still TBD");
    }
    speed_entry.nRFCpb = RFCPB_TABLE[density][speed];
    speed_entry.nRFCab = RFCAB_TABLE[density][speed];
    speed_entry.nXSR = XSR_TABLE[density][speed];
}


void LPDDR3::init_prereq()
{
    // RD
    prereq[int(Level::Rank)][int(Command::RD)] = [] (DRAM<LPDDR3>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::MAX;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SREFX;
            default: assert(false);
        }};
    prereq[int(Level::Bank)][int(Command::RD)] = [] (DRAM<LPDDR3>* node, Command cmd, int id) {
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
    prereq[int(Level::Rank)][int(Command::REF)] = [] (DRAM<LPDDR3>* node, Command cmd, int id) {
        for (auto bank : node->children) {
            if (bank->state == State::Closed)
                continue;
            return Command::PRA;
        }
        return Command::REF;};

    // REFPB
    prereq[int(Level::Bank)][int(Command::REFPB)] = [] (DRAM<LPDDR3>* node, Command cmd, int id) {
      if (node->state == State::Closed) return Command::REFPB;
        else return Command::PRE;};

    // PD
    prereq[int(Level::Rank)][int(Command::PD)] = [] (DRAM<LPDDR3>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::PD;
            case int(State::ActPowerDown): return Command::PD;
            case int(State::PrePowerDown): return Command::PD;
            case int(State::SelfRefresh): return Command::SREFX;
            default: assert(false);
        }};

    // SR
    prereq[int(Level::Rank)][int(Command::SREF)] = [] (DRAM<LPDDR3>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::SREF;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SREF;
            default: assert(false);
        }};
}

// SAUGATA: added row hit check functions to see if the desired location is currently open
void LPDDR3::init_rowhit()
{
    // RD
    rowhit[int(Level::Bank)][int(Command::RD)] = [] (DRAM<LPDDR3>* node, Command cmd, int id) {
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

void LPDDR3::init_rowopen()
{
    // RD
    rowopen[int(Level::Bank)][int(Command::RD)] = [] (DRAM<LPDDR3>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::Closed): return false;
            case int(State::Opened): return true;
            default: assert(false);
        }};

    // WR
    rowopen[int(Level::Bank)][int(Command::WR)] = rowopen[int(Level::Bank)][int(Command::RD)];
}

void LPDDR3::init_lambda()
{
    lambda[int(Level::Bank)][int(Command::ACT)] = [] (DRAM<LPDDR3>* node, int id) {
        node->state = State::Opened;
        node->row_state[id] = State::Opened;};
    lambda[int(Level::Bank)][int(Command::PRE)] = [] (DRAM<LPDDR3>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Rank)][int(Command::PRA)] = [] (DRAM<LPDDR3>* node, int id) {
        for (auto bank : node->children) {
            bank->state = State::Closed;
            bank->row_state.clear();}};
    lambda[int(Level::Rank)][int(Command::REF)] = [] (DRAM<LPDDR3>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::RD)] = [] (DRAM<LPDDR3>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::WR)] = [] (DRAM<LPDDR3>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::RDA)] = [] (DRAM<LPDDR3>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Bank)][int(Command::WRA)] = [] (DRAM<LPDDR3>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Rank)][int(Command::PD)] = [] (DRAM<LPDDR3>* node, int id) {
        for (auto bank : node->children) {
            if (bank->state == State::Closed)
                continue;
            node->state = State::ActPowerDown;
            return;
        }
        node->state = State::PrePowerDown;};
    lambda[int(Level::Rank)][int(Command::PDX)] = [] (DRAM<LPDDR3>* node, int id) {
        node->state = State::PowerUp;};
    lambda[int(Level::Rank)][int(Command::SREF)] = [] (DRAM<LPDDR3>* node, int id) {
        node->state = State::SelfRefresh;};
    lambda[int(Level::Rank)][int(Command::SREFX)] = [] (DRAM<LPDDR3>* node, int id) {
        node->state = State::PowerUp;};
}


void LPDDR3::init_timing()
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
    // section 4.7.3 table 11
    t[int(Command::RD)].push_back({Command::WR, 1, s.nCL + s.nBL + s.nDQSCK + 1 - s.nCWL});
    t[int(Command::RD)].push_back({Command::WRA, 1, s.nCL + s.nBL + s.nDQSCK + 1 - s.nCWL});
    t[int(Command::RDA)].push_back({Command::WR, 1, s.nCL + s.nBL + s.nDQSCK + 1 - s.nCWL});
    t[int(Command::RDA)].push_back({Command::WRA, 1, s.nCL + s.nBL + s.nDQSCK + 1 - s.nCWL});

    t[int(Command::WR)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTR + 1});
    t[int(Command::WR)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nWTR + 1});
    t[int(Command::WRA)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTR + 1});
    t[int(Command::WRA)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nWTR + 1});

    // CAS <-> CAS (between sibling ranks)
    t[int(Command::RD)].push_back({Command::RD, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RD)].push_back({Command::RDA, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RDA)].push_back({Command::RD, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RDA)].push_back({Command::RDA, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RD)].push_back({Command::WR, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RD)].push_back({Command::WRA, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RDA)].push_back({Command::WR, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RDA)].push_back({Command::WRA, 1, s.nBL + s.nRTRS, true});
    t[int(Command::RD)].push_back({Command::WR, 1, s.nCL + s.nBL + s.nDQSCK + 1 + s.nRTRS - s.nCWL, true});
    t[int(Command::RD)].push_back({Command::WRA, 1, s.nCL + s.nBL + s.nDQSCK + 1 + s.nRTRS - s.nCWL, true});
    t[int(Command::RDA)].push_back({Command::WR, 1, s.nCL + s.nBL + s.nDQSCK + 1 + s.nRTRS - s.nCWL, true});
    t[int(Command::RDA)].push_back({Command::WRA, 1, s.nCL + s.nBL + s.nDQSCK + 1 + s.nRTRS - s.nCWL, true});
    t[int(Command::WR)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nRTRS - s.nCL, true});
    t[int(Command::WR)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nRTRS - s.nCL, true});
    t[int(Command::WRA)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nRTRS - s.nCL, true});
    t[int(Command::WRA)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nRTRS - s.nCL, true});

    // CAS <-> PRA
    t[int(Command::RD)].push_back({Command::PRA, 1, s.nRTP});
    t[int(Command::WR)].push_back({Command::PRA, 1, s.nCWL + s.nBL + s.nWR});

    // CAS <-> PD
    t[int(Command::RD)].push_back({Command::PD, 1, s.nCL + s.nBL + 1});
    t[int(Command::RDA)].push_back({Command::PD, 1, s.nCL + s.nBL + 1});
    t[int(Command::WR)].push_back({Command::PD, 1, s.nCWL + s.nBL + s.nWR});
    t[int(Command::WRA)].push_back({Command::PD, 1, s.nCWL + s.nBL + s.nWR + 1}); // +1 for pre
    t[int(Command::PDX)].push_back({Command::RD, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::RDA, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::WR, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::WRA, 1, s.nXP});

    // RAS <-> RAS
    t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRRD});
    t[int(Command::ACT)].push_back({Command::ACT, 4, s.nFAW});
    t[int(Command::ACT)].push_back({Command::PRA, 1, s.nRAS});
    t[int(Command::PRA)].push_back({Command::ACT, 1, s.nRPab});

    // RAS <-> REF
    t[int(Command::PRE)].push_back({Command::REF, 1, s.nRPpb});
    t[int(Command::PRA)].push_back({Command::REF, 1, s.nRPab});
    t[int(Command::REF)].push_back({Command::ACT, 1, s.nRFCab});

    // RAS <-> PD
    t[int(Command::ACT)].push_back({Command::PD, 1, 1});
    t[int(Command::PDX)].push_back({Command::ACT, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::PRE, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::PRA, 1, s.nXP});

    // RAS <-> SR
    t[int(Command::PRE)].push_back({Command::SREF, 1, s.nRPpb});
    t[int(Command::PRA)].push_back({Command::SREF, 1, s.nRPab});
    t[int(Command::SREFX)].push_back({Command::ACT, 1, s.nXSR});

    // REF <-> REF
    t[int(Command::REF)].push_back({Command::REF, 1, s.nRFCab});
    t[int(Command::REF)].push_back({Command::REFPB, 1, s.nRFCab});
    t[int(Command::REFPB)].push_back({Command::REF, 1, s.nRFCpb});

    // REF <-> PD
    t[int(Command::REF)].push_back({Command::PD, 1, 1});
    t[int(Command::REFPB)].push_back({Command::PD, 1, 1});
    t[int(Command::PDX)].push_back({Command::REF, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::REFPB, 1, s.nXP});

    // REF <-> SR
    t[int(Command::SREFX)].push_back({Command::REF, 1, s.nXSR});
    t[int(Command::SREFX)].push_back({Command::REFPB, 1, s.nXSR});

    // PD <-> PD
    t[int(Command::PD)].push_back({Command::PDX, 1, s.nCKE});
    t[int(Command::PDX)].push_back({Command::PD, 1, s.nXP});

    // PD <-> SR
    t[int(Command::PDX)].push_back({Command::SREF, 1, s.nXP});
    t[int(Command::SREFX)].push_back({Command::PD, 1, s.nXSR});

    // SR <-> SR
    t[int(Command::SREF)].push_back({Command::SREFX, 1, s.nCKESR});
    t[int(Command::SREFX)].push_back({Command::SREF, 1, s.nXSR});

    /*** Bank ***/
    t = timing[int(Level::Bank)];

    // CAS <-> RAS
    t[int(Command::ACT)].push_back({Command::RD, 1, s.nRCD});
    t[int(Command::ACT)].push_back({Command::RDA, 1, s.nRCD});
    t[int(Command::ACT)].push_back({Command::WR, 1, s.nRCD});
    t[int(Command::ACT)].push_back({Command::WRA, 1, s.nRCD});

    t[int(Command::RD)].push_back({Command::PRE, 1, s.nRTP});
    t[int(Command::WR)].push_back({Command::PRE, 1, s.nCWL + s.nBL + s.nWR});

    t[int(Command::RDA)].push_back({Command::ACT, 1, s.nRTP + s.nRPpb});
    t[int(Command::WRA)].push_back({Command::ACT, 1, s.nCWL + s.nBL + s.nWR + s.nRPpb});

    // RAS <-> RAS
    t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRC});
    t[int(Command::ACT)].push_back({Command::PRE, 1, s.nRAS});
    t[int(Command::PRE)].push_back({Command::ACT, 1, s.nRPpb});
    t[int(Command::PRE)].push_back({Command::REFPB, 1, s.nRPpb});

    // between different banks
    t[int(Command::ACT)].push_back({Command::REFPB, 1, s.nRRD, true});
    t[int(Command::REFPB)].push_back({Command::ACT, 1, s.nRRD, true});

    // REFSB
    t[int(Command::REFPB)].push_back({Command::REFPB, 1, s.nRFCpb});
    t[int(Command::REFPB)].push_back({Command::ACT, 1, s.nRFCpb});
}
