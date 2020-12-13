#include "WideIO2.h"
#include "DRAM.h"
#include <vector>
#include <functional>
#include <cassert>

using namespace std;
using namespace ramulator;

string WideIO2::standard_name = "WideIO2";

map<string, enum WideIO2::Org> WideIO2::org_map = {
    {"WideIO2_8Gb", WideIO2::Org::WideIO2_8Gb},
};

map<string, enum WideIO2::Speed> WideIO2::speed_map = {
    {"WideIO2_800", WideIO2::Speed::WideIO2_800}, 
    {"WideIO2_1066", WideIO2::Speed::WideIO2_1066},
};

WideIO2::WideIO2(Org org, Speed speed, int channels) :  
    speed_entry(speed_table[int(speed)]),
    read_latency(speed_entry.nCL + speed_entry.nDQSCK + speed_entry.nBL)
{
    switch(int(org)){
        case int(Org::WideIO2_8Gb):
            org_entry.size = (8<<10) / channels;
            org_entry.dq = 64;
            if (channels == 4) {
                org_entry.size = 2<<10;
                org_entry.count[int(Level::Channel)] = channels;
                org_entry.count[int(Level::Rank)] = 0;
                org_entry.count[int(Level::Bank)] = 8;
                org_entry.count[int(Level::Row)] = 1<<13;
                org_entry.count[int(Level::Column)] = 1<<9;
            } else if (channels == 8) {
                org_entry.size = 1<<10;
                org_entry.count[int(Level::Channel)] = channels;
                org_entry.count[int(Level::Rank)] = 0;
                org_entry.count[int(Level::Bank)] = 4;
                org_entry.count[int(Level::Row)] = 1<<14;
                org_entry.count[int(Level::Column)] = 1<<8;
            } else assert(false && "The WideIO2 interface supports 4 or 8 physical channels.");
            break;
        default: assert(false);
    }
    speed_entry.nRPab = (channels == 4)? speed_entry.nRP8b: speed_entry.nRPpb;
    init_prereq();
    init_rowhit(); // SAUGATA: added row hit function
    init_rowopen();
    init_lambda();
    init_timing();
}

WideIO2::WideIO2(const string& org_str, const string& speed_str, int channels) :
    WideIO2(org_map[org_str], speed_map[speed_str], channels)
{
}

void WideIO2::set_channel_number(int channel) {
  assert((channel == org_entry.count[int(Level::Channel)]) && "channel number must be consistent with spec initializaiton configuration.");
  org_entry.count[int(Level::Channel)] = channel;
}

void WideIO2::set_rank_number(int rank) {
  assert(((rank == 1) || (rank == 2)) && "WideIO2 supports single and dual rank configurations.");
  org_entry.count[int(Level::Rank)] = rank;
}

void WideIO2::init_prereq()
{
    // RD
    prereq[int(Level::Rank)][int(Command::RD)] = [] (DRAM<WideIO2>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::MAX;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SREFX;
            default: assert(false);
        }};
    prereq[int(Level::Bank)][int(Command::RD)] = [] (DRAM<WideIO2>* node, Command cmd, int id) {
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
    prereq[int(Level::Rank)][int(Command::REF)] = [] (DRAM<WideIO2>* node, Command cmd, int id) {
        for (auto bank : node->children) {
            if (bank->state == State::Closed)
                continue;
            return Command::PRA;
        }
        return Command::REF;};
    // PD
    prereq[int(Level::Rank)][int(Command::PD)] = [] (DRAM<WideIO2>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::PD;
            case int(State::ActPowerDown): return Command::PD;
            case int(State::PrePowerDown): return Command::PD;
            case int(State::SelfRefresh): return Command::SREFX;
            default: assert(false);
        }};
    // SR
    prereq[int(Level::Rank)][int(Command::SREF)] = [] (DRAM<WideIO2>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::SREF;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SREF;
            default: assert(false);
        }};
}

// SAUGATA: added row hit check functions to see if the desired location is currently open
void WideIO2::init_rowhit()
{
    // RD
    rowhit[int(Level::Bank)][int(Command::RD)] = [] (DRAM<WideIO2>* node, Command cmd, int id) {
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

void WideIO2::init_rowopen()
{
    // RD
    rowopen[int(Level::Bank)][int(Command::RD)] = [] (DRAM<WideIO2>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::Closed): return false;
            case int(State::Opened): return true;
            default: assert(false);
        }};

    // WR
    rowopen[int(Level::Bank)][int(Command::WR)] = rowopen[int(Level::Bank)][int(Command::RD)];
}

void WideIO2::init_lambda()
{
    lambda[int(Level::Bank)][int(Command::ACT)] = [] (DRAM<WideIO2>* node, int id) {
        node->state = State::Opened;
        node->row_state[id] = State::Opened;};
    lambda[int(Level::Bank)][int(Command::PRE)] = [] (DRAM<WideIO2>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Rank)][int(Command::PRA)] = [] (DRAM<WideIO2>* node, int id) {
        for (auto bank : node->children) {
            bank->state = State::Closed;
            bank->row_state.clear();}};
    lambda[int(Level::Bank)][int(Command::RDA)] = [] (DRAM<WideIO2>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Bank)][int(Command::WRA)] = [] (DRAM<WideIO2>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Rank)][int(Command::PD)] = [] (DRAM<WideIO2>* node, int id) {
        for (auto bank : node->children) {
            if (bank->state == State::Closed)
                continue;
            node->state = State::ActPowerDown;
            return;
        }
        node->state = State::PrePowerDown;};
    lambda[int(Level::Rank)][int(Command::PDX)] = [] (DRAM<WideIO2>* node, int id) {
        node->state = State::PowerUp;};
    lambda[int(Level::Rank)][int(Command::SREF)] = [] (DRAM<WideIO2>* node, int id) {
        node->state = State::SelfRefresh;};
    lambda[int(Level::Rank)][int(Command::SREFX)] = [] (DRAM<WideIO2>* node, int id) {
        node->state = State::PowerUp;};
}


void WideIO2::init_timing()
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
    t[int(Command::RD)].push_back({Command::WR, 1, s.nCL + s.nDQSCK + s.nBL + 1 - s.nCWL});
    t[int(Command::RD)].push_back({Command::WRA, 1, s.nCL + s.nDQSCK + s.nBL + 1 - s.nCWL});
    t[int(Command::RDA)].push_back({Command::WR, 1, s.nCL + s.nDQSCK + s.nBL + 1 - s.nCWL});
    t[int(Command::RDA)].push_back({Command::WRA, 1, s.nCL + s.nDQSCK + s.nBL + 1 - s.nCWL});
    t[int(Command::WR)].push_back({Command::RD, 1, s.nCWL + 1 + s.nBL + s.nWTR});
    t[int(Command::WR)].push_back({Command::RDA, 1, s.nCWL + 1 + s.nBL + s.nWTR});
    t[int(Command::WRA)].push_back({Command::RD, 1, s.nCWL + 1 + s.nBL + s.nWTR});
    t[int(Command::WRA)].push_back({Command::RDA, 1, s.nCWL + 1 + s.nBL + s.nWTR});

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
    t[int(Command::WR)].push_back({Command::PRA, 1, s.nCWL + 1 + s.nBL + s.nWR});

    // CAS <-> PD
    t[int(Command::RD)].push_back({Command::PD, 1, s.nCL + s.nDQSCK + s.nBL + 1});
    t[int(Command::RDA)].push_back({Command::PD, 1, s.nCL + s.nDQSCK + s.nBL + 1});
    t[int(Command::WR)].push_back({Command::PD, 1, s.nCWL + 1 + s.nBL + s.nWR});
    t[int(Command::WRA)].push_back({Command::PD, 1, s.nCWL + 1 + s.nBL + s.nWR + 1}); // +1 for pre
    t[int(Command::PDX)].push_back({Command::RD, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::RDA, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::WR, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::WRA, 1, s.nXP});
    
    // CAS <-> SR: none (all banks have to be precharged)

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
    t[int(Command::WR)].push_back({Command::PRE, 1, s.nCWL + 1 + s.nBL + s.nWR});

    t[int(Command::RDA)].push_back({Command::ACT, 1, s.nRTP + s.nRPpb});
    t[int(Command::WRA)].push_back({Command::ACT, 1, s.nCWL + 1 + s.nBL + s.nWR + s.nRPpb});

    // RAS <-> RAS
    t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRC});
    t[int(Command::ACT)].push_back({Command::PRE, 1, s.nRAS});
    t[int(Command::PRE)].push_back({Command::ACT, 1, s.nRPpb});
    t[int(Command::PRE)].push_back({Command::REFPB, 1, s.nRPpb});

    // between different banks
    t[int(Command::ACT)].push_back({Command::REFPB, 1, s.nRRD, true});
    t[int(Command::REFPB)].push_back({Command::ACT, 1, s.nRRD, true});

    // REFPB
    t[int(Command::REFPB)].push_back({Command::REFPB, 1, s.nRFCpb});
    t[int(Command::REFPB)].push_back({Command::ACT, 1, s.nRFCpb});
}
