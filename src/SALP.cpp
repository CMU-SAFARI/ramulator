#include "SALP.h"
#include "DRAM.h"
#include <vector>
#include <functional>
#include <cassert>

using namespace ramulator;

string SALP::level_str [int(Level::MAX)] = {"Ch", "Ra", "Ba", "Sa", "Ro", "Co"};

map<string, enum SALP::Org> SALP::org_map = {
    {"SALP_512Mb_x4", SALP::Org::SALP_512Mb_x4}, {"SALP_512Mb_x8", SALP::Org::SALP_512Mb_x8}, {"SALP_512Mb_x16", SALP::Org::SALP_512Mb_x16},
    {"SALP_1Gb_x4", SALP::Org::SALP_1Gb_x4}, {"SALP_1Gb_x8", SALP::Org::SALP_1Gb_x8}, {"SALP_1Gb_x16", SALP::Org::SALP_1Gb_x16},
    {"SALP_2Gb_x4", SALP::Org::SALP_2Gb_x4}, {"SALP_2Gb_x8", SALP::Org::SALP_2Gb_x8}, {"SALP_2Gb_x16", SALP::Org::SALP_2Gb_x16},
    {"SALP_4Gb_x4", SALP::Org::SALP_4Gb_x4}, {"SALP_4Gb_x8", SALP::Org::SALP_4Gb_x8}, {"SALP_4Gb_x16", SALP::Org::SALP_4Gb_x16},
    {"SALP_8Gb_x4", SALP::Org::SALP_8Gb_x4}, {"SALP_8Gb_x8", SALP::Org::SALP_8Gb_x8}, {"SALP_8Gb_x16", SALP::Org::SALP_8Gb_x16},
};

map<string, enum SALP::Speed> SALP::speed_map = {
    {"SALP_800D", SALP::Speed::SALP_800D}, {"SALP_800E", SALP::Speed::SALP_800E},
    {"SALP_1066E", SALP::Speed::SALP_1066E}, {"SALP_1066F", SALP::Speed::SALP_1066F}, {"SALP_1066G", SALP::Speed::SALP_1066G},
    {"SALP_1333G", SALP::Speed::SALP_1333G}, {"SALP_1333H", SALP::Speed::SALP_1333H},
    {"SALP_1600H", SALP::Speed::SALP_1600H}, {"SALP_1600J", SALP::Speed::SALP_1600J}, {"SALP_1600K", SALP::Speed::SALP_1600K},
    {"SALP_1866K", SALP::Speed::SALP_1866K}, {"SALP_1866L", SALP::Speed::SALP_1866L},
    {"SALP_2133L", SALP::Speed::SALP_2133L}, {"SALP_2133M", SALP::Speed::SALP_2133M},
};

map<string, enum SALP::Type> SALP::type_map = {
    {"SALP-1", SALP::Type::SALP_1},
    {"SALP-2", SALP::Type::SALP_2},
    {"SALP-MASA", SALP::Type::MASA},
};

SALP::SALP(Org org, Speed speed, Type type, int n_sa) :
    type(type),
    n_sa(n_sa),
    org_entry(org_table[int(org)]),
    speed_entry(speed_table[int(speed)]),
    read_latency(speed_entry.nCL + speed_entry.nBL)
{
    switch(int(type)){
        case int(Type::SALP_1): standard_name = "SALP-1"; break;
        case int(Type::SALP_2): standard_name = "SALP-2"; break;
        case int(Type::MASA):   standard_name = "SALP-MASA"; break;
    }
    if (type == Type::SALP_1) {
      scope[int(Command::PRE)] = Level::Bank;
    }
    assert(n_sa && n_sa <= 128 && (n_sa & (n_sa-1)) == 0); // is power of 2, within [1, 128]
    org_entry.count[int(Level::SubArray)] = n_sa;
    long tmp = long(org_entry.dq) * org_entry.count[int(Level::Bank)] * n_sa * org_entry.count[int(Level::Column)];
    org_entry.count[int(Level::Row)] = long(org_entry.size) * (1<<20) / tmp;
    init_speed();
    init_prereq();
    init_rowhit(); // SAUGATA: added row hit function
    init_rowopen();
    init_lambda();
    init_timing();
}

SALP::SALP(const string& org_str, const string& speed_str, const string& type_str, int n_sa) :
    SALP(org_map[org_str], speed_map[speed_str], type_map[type_str], n_sa)
{
}

void SALP::set_channel_number(int channel) {
  org_entry.count[int(Level::Channel)] = channel;
}

void SALP::set_rank_number(int rank) {
  org_entry.count[int(Level::Rank)] = rank;
}

void SALP::init_speed()
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


void SALP::init_prereq()
{
    prereq[int(Level::Rank)][int(Command::RD)] = [] (DRAM<SALP>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::MAX;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SRX;
            default: assert(false);
        }};
    prereq[int(Level::Rank)][int(Command::WR)] = prereq[int(Level::Rank)][int(Command::RD)];

    switch(int(type)){
        case int(Type::SALP_1):
            prereq[int(Level::Bank)][int(Command::RD)] = [] (DRAM<SALP>* node, Command cmd, int id) {
                switch (int(node->state)){
                    case int(State::Closed): return Command::ACT;
                    case int(State::Opened):
                        return Command::MAX;
                    default: assert(false);}};
            prereq[int(Level::Bank)][int(Command::WR)] = prereq[int(Level::Bank)][int(Command::RD)];
            prereq[int(Level::SubArray)][int(Command::RD)] = [] (DRAM<SALP>* node, Command cmd, int id) {
              if (node->row_state.find(id) != node->row_state.end()) {
                return cmd;
              } else if (node->row_state.size()) {
                return Command::PRE;
              } else {
                return Command::PRE_OTHER;
              }
            };
            prereq[int(Level::SubArray)][int(Command::WR)] = prereq[int(Level::SubArray)][int(Command::RD)];
            prereq[int(Level::Rank)][int(Command::REF)] = [] (DRAM<SALP>* node, Command cmd, int id) {
                for (auto bank : node->children) {
                    if (bank->state == State::Closed)
                        continue;
                    return Command::PRER;
                }
                return Command::REF;};
            break;
        case int(Type::SALP_2):
            prereq[int(Level::SubArray)][int(Command::RD)] = [] (DRAM<SALP>* node, Command cmd, int id) {
                switch (int(node->state)){
                    case int(State::Closed): return Command::ACT;
                    case int(State::Opened):
                        if (node->row_state.find(id) != node->row_state.end()) {
                          for (auto sa : node->parent->children) {
                              if (sa != node && sa->state == State::Opened) {
                                  return Command::PRE_OTHER;
                              }
                          }
                          return cmd;
                        } else {
                          // if this subarray has another row open, close it
                          // first
                          return Command::PRE;
                        }
                    default: assert(false);}};
            prereq[int(Level::SubArray)][int(Command::WR)] = prereq[int(Level::SubArray)][int(Command::RD)];
            prereq[int(Level::Rank)][int(Command::REF)] = [] (DRAM<SALP>* node, Command cmd, int id) {
                for (auto bank : node->children)
                    for (auto sa : bank->children) {
                        if (sa->state == State::Closed)
                            continue;
                        return Command::PRER;
                    }
                return Command::REF;};
            break;
        case int(Type::MASA):
            prereq[int(Level::SubArray)][int(Command::RD)] = [] (DRAM<SALP>* node, Command cmd, int id) {
                switch (int(node->state)){
                    case int(State::Closed): return Command::ACT;
                    case int(State::Opened):
                        if (node->row_state.find(id) != node->row_state.end()) return Command::SASEL;
                        else return Command::PRE;
                    case int(State::Selected):
                        if (node->row_state.find(id) != node->row_state.end()) return cmd;
                        else return Command::PRE;
                    default: assert(false);
                }};
            prereq[int(Level::SubArray)][int(Command::WR)] = prereq[int(Level::SubArray)][int(Command::RD)];
            prereq[int(Level::Rank)][int(Command::REF)] = [] (DRAM<SALP>* node, Command cmd, int id) {
                for (auto bank : node->children)
                    for (auto sa : bank->children){
                        if (sa->state == State::Closed)
                            continue;
                        return Command::PRER;
                    }
                return Command::REF;};
            break;
        default: assert(false);
    }
    // PD
    prereq[int(Level::Rank)][int(Command::PDE)] = [] (DRAM<SALP>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::PDE;
            case int(State::ActPowerDown): return Command::PDE;
            case int(State::PrePowerDown): return Command::PDE;
            case int(State::SelfRefresh): return Command::SRX;
            default: assert(false);
        }};

    // SR
    prereq[int(Level::Rank)][int(Command::SRE)] = [] (DRAM<SALP>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::SRE;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SRE;
            default: assert(false);
        }};
}

// SAUGATA: added row hit check functions to see if the desired location is currently open
void SALP::init_rowhit()
{
    switch(int(type)) {
        case int(Type::SALP_1):
            // RD
            rowhit[int(Level::SubArray)][int(Command::RD)] = [] (DRAM<SALP>* node, Command cmd, int id) {
              switch (int(node->state)) {
                case int(State::Closed): return false;
                case int(State::Opened):
                  if (node->row_state.find(id) != node->row_state.end())  return true;
                  else return false;
                default: assert(false);
              }
            };
            // WR
            rowhit[int(Level::SubArray)][int(Command::WR)] = rowhit[int(Level::SubArray)][int(Command::RD)];
            break;
        case int(Type::SALP_2):
            // RD
            rowhit[int(Level::SubArray)][int(Command::RD)] = [] (DRAM<SALP>* node, Command cmd, int id) {
                switch (int(node->state)){
                    case int(State::Closed): return false;
                    case int(State::Opened):
                        if (node->row_state.find(id) != node->row_state.end()) return true;
                        else return false;
                    default: assert(false);
                }};
            // WR
            rowhit[int(Level::SubArray)][int(Command::WR)] = rowhit[int(Level::SubArray)][int(Command::RD)];
            break;
        case int(Type::MASA):
            // RD
            rowhit[int(Level::SubArray)][int(Command::RD)] = [] (DRAM<SALP>* node, Command cmd, int id) {
                switch (int(node->state)){
                    case int(State::Closed): return false;
                    case int(State::Opened):
                        // opened but not selected still counts as a row hit
                        if (node->row_state.find(id) != node->row_state.end()) return true;
                        else return false;
                    case int(State::Selected):
                        if (node->row_state.find(id) != node->row_state.end()) return true;
                        else return false;
                    default: assert(false);
                }};
            // WR
            rowhit[int(Level::SubArray)][int(Command::WR)] = rowhit[int(Level::SubArray)][int(Command::RD)];
            break;
        default: assert(false);
    }
}

void SALP::init_rowopen()
{
    switch(int(type)) {
        case int(Type::SALP_1):
            // RD
            rowopen[int(Level::Bank)][int(Command::RD)] = [] (DRAM<SALP>* node, Command cmd, int id) {
                switch (int(node->state)){
                    case int(State::Closed): return false;
                    case int(State::Opened): return true;
                    default: assert(false);
                }};
            // WR
            rowopen[int(Level::Bank)][int(Command::WR)] = rowopen[int(Level::Bank)][int(Command::RD)];
            break;
        case int(Type::SALP_2):
            // RD
            rowopen[int(Level::SubArray)][int(Command::RD)] = [] (DRAM<SALP>* node, Command cmd, int id) {
                switch (int(node->state)){
                    case int(State::Closed): return false;
                    case int(State::Opened): return true;
                    default: assert(false);
                }};
            // WR
            rowopen[int(Level::SubArray)][int(Command::WR)] = rowopen[int(Level::SubArray)][int(Command::RD)];
            break;
        case int(Type::MASA):
            // RD
            rowopen[int(Level::SubArray)][int(Command::RD)] = [] (DRAM<SALP>* node, Command cmd, int id) {
                switch (int(node->state)){
                    case int(State::Closed): return false;
                    case int(State::Opened): return true;
                    case int(State::Selected): return true;
                    default: assert(false);
                }};
            // WR
            rowopen[int(Level::SubArray)][int(Command::WR)] = rowopen[int(Level::SubArray)][int(Command::RD)];
            break;
        default: assert(false);
    }
}

void SALP::init_lambda()
{
    switch(int(type)){
        case int(Type::SALP_1):
            lambda[int(Level::Bank)][int(Command::ACT)] = [] (DRAM<SALP>* node, int id) {
                node->state = State::Opened;
            };
            lambda[int(Level::SubArray)][int(Command::ACT)] = [] (DRAM<SALP>* node, int id) {
              node->state = State::Opened;
              node->row_state[id] = State::Opened;
            };
            lambda[int(Level::Bank)][int(Command::PRE)] = [] (DRAM<SALP>* node, int id) {
                node->state = State::Closed;
                // For SALP_1, we stick to original design that allows
                // only one row in a bank open, so here close subarray id
                // is equivalent to close the whole bank
                node->children[id]->state = State::Closed;
                node->children[id]->row_state.clear();
                };
            lambda[int(Level::Bank)][int(Command::PRE_OTHER)] = lambda[int(Level::Bank)][int(Command::PRE)];
            lambda[int(Level::Rank)][int(Command::PRER)] = [] (DRAM<SALP>* node, int id) {
                for (auto bank : node->children) {
                    bank->state = State::Closed;
                    for (auto sa : bank->children){
                        sa->state = State::Closed;
                        sa->row_state.clear();}}};
            lambda[int(Level::Bank)][int(Command::RDA)] = [] (DRAM<SALP>* node, int id) {
                node->state = State::Closed;
                node->children[id]->state = State::Closed;
                node->children[id]->row_state.clear();};
            lambda[int(Level::Bank)][int(Command::WRA)] = lambda[int(Level::Bank)][int(Command::RDA)];
            lambda[int(Level::Rank)][int(Command::PDE)] = [] (DRAM<SALP>* node, int id) {
                for (auto bank : node->children) {
                    if (bank->state == State::Closed)
                        continue;
                    node->state = State::ActPowerDown;
                    return;
                }
                node->state = State::PrePowerDown;};
            break;
        case int(Type::SALP_2):
            lambda[int(Level::SubArray)][int(Command::ACT)] = [] (DRAM<SALP>* node, int id) {
                node->state = State::Opened;
                node->row_state[id] = State::Opened;};
            lambda[int(Level::SubArray)][int(Command::PRE)] = [] (DRAM<SALP>* node, int id) {
                node->state = State::Closed;
                node->row_state.clear();};
            lambda[int(Level::SubArray)][int(Command::PRE_OTHER)] = lambda[int(Level::SubArray)][int(Command::PRE)];
            lambda[int(Level::Rank)][int(Command::PRER)] = [] (DRAM<SALP>* node, int id) {
                for (auto bank : node->children)
                    for (auto sa : bank->children) {
                        sa->state = State::Closed;
                        sa->row_state.clear();}};
            lambda[int(Level::SubArray)][int(Command::RDA)] = [] (DRAM<SALP>* node, int id) {
                node->state = State::Closed;
                node->row_state.clear();};
            lambda[int(Level::SubArray)][int(Command::WRA)] = [] (DRAM<SALP>* node, int id) {
                node->state = State::Closed;
                node->row_state.clear();};
            lambda[int(Level::Rank)][int(Command::PDE)] = [] (DRAM<SALP>* node, int id) {
                for (auto bank : node->children)
                    for (auto sa : bank->children) {
                        if (sa->state == State::Closed)
                            continue;
                        node->state = State::ActPowerDown;
                        return;
                    }
                node->state = State::PrePowerDown;};
            break;
        case int(Type::MASA):
            lambda[int(Level::SubArray)][int(Command::ACT)] = [] (DRAM<SALP>* node, int id) {
                node->state = State::Selected;
                node->row_state[id] = State::Opened;
                for (auto sa : node->parent->children)
                    if (sa != node && sa->state == State::Selected) {
                        sa->state = State::Opened;
                        break;}};
            lambda[int(Level::SubArray)][int(Command::SASEL)] = [] (DRAM<SALP>* node, int id) {
                node->state = State::Selected;
                for (auto sa : node->parent->children)
                    if (sa != node && sa->state == State::Selected) {
                        sa->state = State::Opened;
                        break;}};
            lambda[int(Level::SubArray)][int(Command::PRE)] = [] (DRAM<SALP>* node, int id) {
                node->state = State::Closed;
                node->row_state.clear();};

            lambda[int(Level::Rank)][int(Command::PRER)] = [] (DRAM<SALP>* node, int id) {
                for (auto bank : node->children)
                    for (auto sa : bank->children) {
                        sa->state = State::Closed;
                        sa->row_state.clear();}};
            lambda[int(Level::SubArray)][int(Command::RDA)] = [] (DRAM<SALP>* node, int id) {
                node->state = State::Closed;
                node->row_state.clear();};
            lambda[int(Level::SubArray)][int(Command::WRA)] = [] (DRAM<SALP>* node, int id) {
                node->state = State::Closed;
                node->row_state.clear();};
            lambda[int(Level::Rank)][int(Command::PDE)] = [] (DRAM<SALP>* node, int id) {
                for (auto bank : node->children)
                    for (auto sa : bank->children) {
                        if (sa->state == State::Closed)
                            continue;
                        node->state = State::ActPowerDown;
                        return;
                    }
                node->state = State::PrePowerDown;};
            break;
        default: assert(false);
    }
    lambda[int(Level::Rank)][int(Command::PDX)] = [] (DRAM<SALP>* node, int id) {
        node->state = State::PowerUp;};
    lambda[int(Level::Rank)][int(Command::SRE)] = [] (DRAM<SALP>* node, int id) {
        node->state = State::SelfRefresh;};
    lambda[int(Level::Rank)][int(Command::SRX)] = [] (DRAM<SALP>* node, int id) {
        node->state = State::PowerUp;};
}

void SALP::init_timing()
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

    // CAS <-> PRER
    t[int(Command::RD)].push_back({Command::PRER, 1, s.nRTP});
    t[int(Command::WR)].push_back({Command::PRER, 1, s.nCWL + s.nBL + s.nWR});

    // CAS <-> REF: none (all banks have to be precharged)

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

    t[int(Command::ACT)].push_back({Command::PRER, 1, s.nRAS});
    t[int(Command::PRER)].push_back({Command::ACT, 1, s.nRP});

    // RAS <-> REF
    t[int(Command::PRE)].push_back({Command::REF, 1, s.nRP});
    t[int(Command::PRER)].push_back({Command::REF, 1, s.nRP});
    t[int(Command::PRE_OTHER)].push_back({Command::REF, 1, s.nRP});
    t[int(Command::REF)].push_back({Command::ACT, 1, s.nRFC});

    // RAS <-> PD
    t[int(Command::ACT)].push_back({Command::PDE, 1, 1});
    t[int(Command::PDX)].push_back({Command::ACT, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::PRE, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::PRER, 1, s.nXP});

    // RAS <-> SR
    t[int(Command::PRE)].push_back({Command::SRE, 1, s.nRP});
    t[int(Command::PRER)].push_back({Command::SRE, 1, s.nRP});
    t[int(Command::PRE_OTHER)].push_back({Command::SRE, 1, s.nRP});
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

    switch(int(type)) {
        case int(Type::SALP_1):
          // memory controller doesn't specify a row to precharge,
          // all subarrays are precharged together, so we should check
          // whether other activation/column access are still ongoing.
          t[int(Command::ACT)].push_back({Command::PRE, 1, s.nRAS});
          t[int(Command::RD)].push_back({Command::PRE, 1, s.nRTP});
          t[int(Command::WR)].push_back({Command::PRE, 1, s.nCWL + s.nBL + s.nWR,});
          t[int(Command::ACT)].push_back({Command::PRE_OTHER, 1, s.nRAS});
          t[int(Command::RD)].push_back({Command::PRE_OTHER, 1, s.nRTP});
          t[int(Command::WR)].push_back({Command::PRE_OTHER, 1, s.nCWL + s.nBL + s.nWR,});
        case int(Type::SALP_2):
        case int(Type::MASA):
        break;
        default: assert(false);
    }

    /*** SubArray ***/
    t = timing[int(Level::SubArray)];

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

    switch(int(type)) {
        case int(Type::SALP_1):
        case int(Type::SALP_2):
        break;
        case int(Type::MASA):
          t[int(Command::SASEL)].push_back({Command::RD, 1, s.nSCD});
          t[int(Command::SASEL)].push_back({Command::RDA, 1, s.nSCD});
          t[int(Command::SASEL)].push_back({Command::WR, 1, s.nSCD});
          t[int(Command::SASEL)].push_back({Command::WRA, 1, s.nSCD});
        break;
        default: assert(false);
    }

    // sibling subarray constraints
    switch(int(type)) {
        case int(Type::SALP_1):
          t[int(Command::PRE)].push_back({Command::ACT, 1, s.nPA, true});
          t[int(Command::PRE_OTHER)].push_back({Command::ACT, 1, s.nPA, true});
          // for auto precharge command
          t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRC - s.nRP + s.nPA, true});
          t[int(Command::RDA)].push_back({Command::ACT, 1, s.nRTP + s.nPA, true});
          t[int(Command::WRA)].push_back({Command::ACT, 1, s.nCWL + s.nBL + s.nWR + s.nPA, true});
        break;

        case int(Type::SALP_2):
          t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRCD + s.nRA, true});
          t[int(Command::RD)].push_back({Command::ACT, 1, s.nRA, true});
          t[int(Command::RDA)].push_back({Command::ACT, 1, s.nRA, true});
          t[int(Command::WR)].push_back({Command::ACT, 1, s.nWA, true});
          t[int(Command::WRA)].push_back({Command::ACT, 1, s.nWA, true});
        break;
        case int(Type::MASA):
          t[int(Command::RD)].push_back({Command::ACT, 1, s.nRA, true});
          t[int(Command::RDA)].push_back({Command::ACT, 1, s.nRA, true});
          t[int(Command::WR)].push_back({Command::ACT, 1, s.nWA, true});
          t[int(Command::WRA)].push_back({Command::ACT, 1, s.nWA, true});

          t[int(Command::RD)].push_back({Command::SASEL, 1, s.nRA, true});
          t[int(Command::RDA)].push_back({Command::SASEL, 1, s.nRA, true});
          t[int(Command::WR)].push_back({Command::SASEL, 1, s.nWA, true});
          t[int(Command::WRA)].push_back({Command::SASEL, 1, s.nWA, true});

          t[int(Command::RD)].push_back({Command::RD, 1, s.nRA, true});
          t[int(Command::RDA)].push_back({Command::RDA, 1, s.nRA, true});
          t[int(Command::WR)].push_back({Command::WR, 1, s.nWA, true});
          t[int(Command::WRA)].push_back({Command::WRA, 1, s.nWA, true});
        break;
        default: assert(false);
    }
    // between sibling subarrays
}
