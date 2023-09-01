#include "WideIO.h"
#include "DRAM.h"
#include <vector>
#include <functional>
#include <cassert>

using namespace std;
using namespace ramulator;

string WideIO::standard_name = "WideIO";
string WideIO::level_str [int(Level::MAX)] = {"Ch", "Ra", "Ba", "Ro", "Co"};

map<string, enum WideIO::Org> WideIO::org_map = {
    {"WideIO_1Gb", WideIO::Org::WideIO_1Gb},
    {"WideIO_2Gb", WideIO::Org::WideIO_2Gb},
    {"WideIO_4Gb", WideIO::Org::WideIO_4Gb},
    {"WideIO_8Gb", WideIO::Org::WideIO_8Gb},
};

map<string, enum WideIO::Speed> WideIO::speed_map = {
    {"WideIO_200", WideIO::Speed::WideIO_200}, 
    {"WideIO_266", WideIO::Speed::WideIO_266},
};

WideIO::WideIO(Org org, Speed speed) : 
    org_entry(org_table[int(org)]), 
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

WideIO::WideIO(const string& org_str, const string& speed_str) :
    WideIO(org_map[org_str], speed_map[speed_str]) 
{
}

void WideIO::set_channel_number(int channel) {
  assert((channel == 4) && "The Wide I/O interface supports 4 physical and 4 logical channels.");
  org_entry.count[int(Level::Channel)] = channel;
}

void WideIO::set_rank_number(int rank) {
  assert((rank == 1) && "WideIO rank number is fixed to 1.");
  org_entry.count[int(Level::Rank)] = rank;
}

void WideIO::init_speed()
{
    const static int RFC_TABLE[int(Speed::MAX)][int(Org::MAX)] = {
        {18, 26, 26, 42},
        {24, 35, 35, 56}
    };
    const static int REFI_TABLE[int(Speed::MAX)][int(Org::MAX)] = {
        {3120, 1560, 780, 780},
        {4160, 2080, 1040, 520}
    };
    int speed = 0, density = 0;
    switch(speed_entry.rate){
        case 200: speed = 0; break;
        case 266: speed = 1; break;
        default: assert(false);
    }
    switch(org_entry.size >> 8){
        case 1: density = 0; break;
        case 2: density = 1; break;
        case 4: density = 2; break;
        case 8: density = 3; break;
        default: assert(false);
    }
    speed_entry.nRFC = RFC_TABLE[speed][density];
    speed_entry.nREFI = REFI_TABLE[speed][density];
}


void WideIO::init_prereq()
{
    // RD
    prereq[int(Level::Rank)][int(Command::RD)] = [] (DRAM<WideIO>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::MAX;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SREFX;
            default: assert(false);
        }};
    prereq[int(Level::Bank)][int(Command::RD)] = [] (DRAM<WideIO>* node, Command cmd, int id) {
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
    prereq[int(Level::Rank)][int(Command::REF)] = [] (DRAM<WideIO>* node, Command cmd, int id) {
        for (auto bank : node->children) {
            if (bank->state == State::Closed)
                continue;
            return Command::PRA;
        }
        return Command::REF;};

    // PD
    prereq[int(Level::Rank)][int(Command::PD)] = [] (DRAM<WideIO>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::PD;
            case int(State::ActPowerDown): return Command::PD;
            case int(State::PrePowerDown): return Command::PD;
            case int(State::SelfRefresh): return Command::SREFX;
            default: assert(false);
        }};

    // SR
    prereq[int(Level::Rank)][int(Command::SREF)] = [] (DRAM<WideIO>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::SREF;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SREF;
            default: assert(false);
        }};
}

// SAUGATA: added row hit check functions to see if the desired location is currently open
void WideIO::init_rowhit()
{
    // RD
    rowhit[int(Level::Bank)][int(Command::RD)] = [] (DRAM<WideIO>* node, Command cmd, int id) {
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

void WideIO::init_rowopen()
{
    // RD
    rowopen[int(Level::Bank)][int(Command::RD)] = [] (DRAM<WideIO>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::Closed): return false;
            case int(State::Opened): return true;
            default: assert(false);
        }};

    // WR
    rowopen[int(Level::Bank)][int(Command::WR)] = rowopen[int(Level::Bank)][int(Command::RD)];
}

void WideIO::init_lambda()
{
    lambda[int(Level::Bank)][int(Command::ACT)] = [] (DRAM<WideIO>* node, int id) {
        node->state = State::Opened;
        node->row_state[id] = State::Opened;};
    lambda[int(Level::Bank)][int(Command::PRE)] = [] (DRAM<WideIO>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Rank)][int(Command::PRA)] = [] (DRAM<WideIO>* node, int id) {
        for (auto bank : node->children) {
            bank->state = State::Closed;
            bank->row_state.clear();}};
    lambda[int(Level::Rank)][int(Command::REF)] = [] (DRAM<WideIO>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::RD)] = [] (DRAM<WideIO>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::WR)] = [] (DRAM<WideIO>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::RDA)] = [] (DRAM<WideIO>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Bank)][int(Command::WRA)] = [] (DRAM<WideIO>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Rank)][int(Command::PD)] = [] (DRAM<WideIO>* node, int id) {
        for (auto bank : node->children) {
            if (bank->state == State::Closed)
                continue;
            node->state = State::ActPowerDown;
            return;
        }
        node->state = State::PrePowerDown;};
    lambda[int(Level::Rank)][int(Command::PDX)] = [] (DRAM<WideIO>* node, int id) {
        node->state = State::PowerUp;};
    lambda[int(Level::Rank)][int(Command::SREF)] = [] (DRAM<WideIO>* node, int id) {
        node->state = State::SelfRefresh;};
    lambda[int(Level::Rank)][int(Command::SREFX)] = [] (DRAM<WideIO>* node, int id) {
        node->state = State::PowerUp;};
}


void WideIO::init_timing()
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
    t[int(Command::RD)].push_back({Command::WR, 1, s.nRTW});
    t[int(Command::RD)].push_back({Command::WRA, 1, s.nRTW});
    t[int(Command::RDA)].push_back({Command::WR, 1, s.nRTW});
    t[int(Command::RDA)].push_back({Command::WRA, 1, s.nRTW});
    t[int(Command::WR)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTR});
    t[int(Command::WR)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nWTR});
    t[int(Command::WRA)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTR});
    t[int(Command::WRA)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nWTR});

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
    
    // CAS <-> SR: none (all banks have to be precharged)

    // RAS <-> RAS
    t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRRD});
    t[int(Command::ACT)].push_back({Command::ACT, 2, s.nTAW});
    t[int(Command::ACT)].push_back({Command::PRA, 1, s.nRAS});
    t[int(Command::PRA)].push_back({Command::ACT, 1, s.nRP});

    // RAS <-> REF
    t[int(Command::PRE)].push_back({Command::REF, 1, s.nRP});
    t[int(Command::PRA)].push_back({Command::REF, 1, s.nRP});
    t[int(Command::REF)].push_back({Command::ACT, 1, s.nRFC});

    // RAS <-> PD
    t[int(Command::ACT)].push_back({Command::PD, 1, 1});
    t[int(Command::PDX)].push_back({Command::ACT, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::PRE, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::PRA, 1, s.nXP});

    // RAS <-> SR
    t[int(Command::PRE)].push_back({Command::SREF, 1, s.nRP});
    t[int(Command::PRA)].push_back({Command::SREF, 1, s.nRP});
    t[int(Command::SREFX)].push_back({Command::ACT, 1, s.nXSR});

    // REF <-> REF
    t[int(Command::REF)].push_back({Command::REF, 1, s.nRFC});

    // REF <-> PD
    t[int(Command::REF)].push_back({Command::PD, 1, 1});
    t[int(Command::PDX)].push_back({Command::REF, 1, s.nXP});

    // REF <-> SR
    t[int(Command::SREFX)].push_back({Command::REF, 1, s.nXSR});
    
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

    t[int(Command::RDA)].push_back({Command::ACT, 1, s.nRTP + s.nRP});
    t[int(Command::WRA)].push_back({Command::ACT, 1, s.nCWL + s.nBL + s.nWR + s.nRP});

    // RAS <-> RAS
    t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRC});
    t[int(Command::ACT)].push_back({Command::PRE, 1, s.nRAS});
    t[int(Command::PRE)].push_back({Command::ACT, 1, s.nRP});
}
