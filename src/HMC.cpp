#include "HMC.h"
#include "DRAM.h"
#include <vector>
#include <functional>
#include <cassert>

using namespace std;
using namespace ramulator;

string HMC::standard_name = "HMC";
const int bytes_per_flit = 16;

map<string, enum HMC::Org> HMC::org_map = {
    {"HMC_4GB", HMC::Org::HMC_4GB}, {"HMC_8GB", HMC::Org::HMC_8GB},
    {"HMC_4GB_bank16", HMC::Org::HMC_4GB_bank16},
    {"HMC_4GB_bank32", HMC::Org::HMC_4GB_bank32},
    {"HMC_4GB_bank64", HMC::Org::HMC_4GB_bank64},
    {"HMC_4GB_bank128", HMC::Org::HMC_4GB_bank128},
    {"HMC_4GB_bank256", HMC::Org::HMC_4GB_bank256},
    {"HMC_4GB_va64", HMC::Org::HMC_4GB_va64},
    {"HMC_4GB_va128", HMC::Org::HMC_4GB_va128},
    {"HMC_4GB_va256", HMC::Org::HMC_4GB_va256},
    {"HMC_4GB_va512", HMC::Org::HMC_4GB_va512},
    {"HMC_4GB_va1024", HMC::Org::HMC_4GB_va1024}
};

map<string, enum HMC::Speed> HMC::speed_map = {
    {"HMC_2500_unlimit_bandwidth", HMC::Speed::HMC_2500_unlimit_bandwidth},
};

map<string, enum HMC::MaxBlock> HMC::maxblock_map = {
    {"HMC_32B", HMC::MaxBlock::HMC_32B},
    {"HMC_64B", HMC::MaxBlock::HMC_64B},
    {"HMC_128B", HMC::MaxBlock::HMC_128B},
    {"HMC_256B", HMC::MaxBlock::HMC_256B},
};

map<string, enum HMC::LinkWidth> HMC::linkwidth_map = {
    {"HMC_Full_Width", HMC::LinkWidth::HMC_Full_Width},
    {"HMC_Half_Width", HMC::LinkWidth::HMC_Half_Width},
    {"HMC_Quarter_Width", HMC::LinkWidth::HMC_Quarter_Width},
};

map<string, enum HMC::LaneSpeed> HMC::lanespeed_map = {
    {"HMC_12.5_Gbps", HMC::LaneSpeed::HMC_12_5_Gbps},
    {"HMC_15_Gbps", HMC::LaneSpeed::HMC_15_Gbps},
    {"HMC_25_Gbps", HMC::LaneSpeed::HMC_25_Gbps},
    {"HMC_28_Gbps", HMC::LaneSpeed::HMC_28_Gbps},
    {"HMC_30_Gbps", HMC::LaneSpeed::HMC_30_Gbps},
};

HMC::HMC(Org org, Speed speed, MaxBlock maxblock, LinkWidth linkwidth,
    LaneSpeed lanespeed, int source_links, int payload_flits) :
    org_entry(org_table[int(org)]),speed_entry(speed_table[int(speed)]),
    read_latency(speed_entry.nCL + speed_entry.nBL),
    maxblock_entry(maxblock_table[int(maxblock)]),
    link_width(link_width_table[int(linkwidth)]),
    lane_speed(lane_speed_table[int(lanespeed)]),
    source_links(source_links), payload_flits(payload_flits),
    burst_count(ceil(payload_flits / ((prefetch_size * channel_width / 8)/ bytes_per_flit)))
{
    init_speed();
    init_prereq();
    init_rowhit(); // SAUGATA: added row hit function
    init_rowopen();
    init_lambda();
    init_timing();
}

HMC::HMC(const string& org_str, const string& speed_str,
    const string& maxblock_str, const string& linkwidth_str,
    const string& lanespeed_str, int source_links, int payload_flits) :
    HMC(org_map[org_str], speed_map[speed_str], maxblock_map[maxblock_str],
        linkwidth_map[linkwidth_str], lanespeed_map[lanespeed_str], source_links, payload_flits)
{
}

void HMC::init_speed()
{
}


void HMC::init_prereq()
{
    // RD
    prereq[int(Level::Vault)][int(Command::RD)] = [] (DRAM<HMC>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::MAX;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SRX;
            default: assert(false);
        }};
    prereq[int(Level::Bank)][int(Command::RD)] = [] (DRAM<HMC>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::Closed): return Command::ACT;
            case int(State::Opened):
                if (node->row_state.find(id) != node->row_state.end())
                    return cmd;
                return Command::PRE;
            default: assert(false);
        }};

    // WR
    prereq[int(Level::Vault)][int(Command::WR)] = prereq[int(Level::Vault)][int(Command::RD)];
    prereq[int(Level::Bank)][int(Command::WR)] = prereq[int(Level::Bank)][int(Command::RD)];

    // REF
    prereq[int(Level::Vault)][int(Command::REF)] = [] (DRAM<HMC>* node, Command cmd, int id) {
        for (auto bg : node->children)
            for (auto bank: bg->children) {
                if (bank->state == State::Closed)
                    continue;
                return Command::PREA;
            }
        return Command::REF;};

    // PD
    prereq[int(Level::Vault)][int(Command::PDE)] = [] (DRAM<HMC>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::PDE;
            case int(State::ActPowerDown): return Command::PDE;
            case int(State::PrePowerDown): return Command::PDE;
            case int(State::SelfRefresh): return Command::SRX;
            default: assert(false);
        }};

    // SR
    prereq[int(Level::Vault)][int(Command::SRE)] = [] (DRAM<HMC>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::SRE;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SRE;
            default: assert(false);
        }};
}


// SAUGATA: added row hit check functions to see if the desired location is currently open
void HMC::init_rowhit()
{
    // RD
    rowhit[int(Level::Bank)][int(Command::RD)] = [] (DRAM<HMC>* node, Command cmd, int id) {
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

void HMC::init_rowopen()
{
    // RD
    rowopen[int(Level::Bank)][int(Command::RD)] = [] (DRAM<HMC>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::Closed): return false;
            case int(State::Opened): return true;
            default: assert(false);
        }};

    // WR
    rowopen[int(Level::Bank)][int(Command::WR)] = rowopen[int(Level::Bank)][int(Command::RD)];
}

void HMC::init_lambda()
{
    lambda[int(Level::Bank)][int(Command::ACT)] = [] (DRAM<HMC>* node, int id) {
        node->state = State::Opened;
        node->row_state[id] = State::Opened;};
    lambda[int(Level::Bank)][int(Command::PRE)] = [] (DRAM<HMC>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Vault)][int(Command::PREA)] = [] (DRAM<HMC>* node, int id) {
        for (auto bg : node->children)
            for (auto bank : bg->children) {
                bank->state = State::Closed;
                bank->row_state.clear();
            }};
    lambda[int(Level::Vault)][int(Command::REF)] = [] (DRAM<HMC>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::RD)] = [] (DRAM<HMC>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::WR)] = [] (DRAM<HMC>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::RDA)] = [] (DRAM<HMC>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Bank)][int(Command::WRA)] = [] (DRAM<HMC>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Vault)][int(Command::PDE)] = [] (DRAM<HMC>* node, int id) {
        for (auto bg : node->children)
            for (auto bank : bg->children) {
                if (bank->state == State::Closed)
                    continue;
                node->state = State::ActPowerDown;
                return;
            }
        node->state = State::PrePowerDown;};
    lambda[int(Level::Vault)][int(Command::PDX)] = [] (DRAM<HMC>* node, int id) {
        node->state = State::PowerUp;};
    lambda[int(Level::Vault)][int(Command::SRE)] = [] (DRAM<HMC>* node, int id) {
        node->state = State::SelfRefresh;};
    lambda[int(Level::Vault)][int(Command::SRX)] = [] (DRAM<HMC>* node, int id) {
        node->state = State::PowerUp;};
}


void HMC::init_timing()
{
    SpeedEntry& s = speed_entry;
    vector<TimingEntry> *t;

    /*** Channel ***/
    t = timing[int(Level::Vault)];

    // CAS <-> CAS
    t[int(Command::RD)].push_back({Command::RD, 1, s.nBL});
    t[int(Command::RD)].push_back({Command::RDA, 1, s.nBL});
    t[int(Command::RDA)].push_back({Command::RD, 1, s.nBL});
    t[int(Command::RDA)].push_back({Command::RDA, 1, s.nBL});
    t[int(Command::WR)].push_back({Command::WR, 1, s.nBL});
    t[int(Command::WR)].push_back({Command::WRA, 1, s.nBL});
    t[int(Command::WRA)].push_back({Command::WR, 1, s.nBL});
    t[int(Command::WRA)].push_back({Command::WRA, 1, s.nBL});

    // CAS <-> CAS
    t[int(Command::RD)].push_back({Command::RD, 1, s.nCCDS});
    t[int(Command::RD)].push_back({Command::RDA, 1, s.nCCDS});
    t[int(Command::RDA)].push_back({Command::RD, 1, s.nCCDS});
    t[int(Command::RDA)].push_back({Command::RDA, 1, s.nCCDS});
    t[int(Command::WR)].push_back({Command::WR, 1, s.nCCDS});
    t[int(Command::WR)].push_back({Command::WRA, 1, s.nCCDS});
    t[int(Command::WRA)].push_back({Command::WR, 1, s.nCCDS});
    t[int(Command::WRA)].push_back({Command::WRA, 1, s.nCCDS});
    t[int(Command::RD)].push_back({Command::WR, 1, s.nCL + s.nCCDS + 2 - s.nCWL});
    t[int(Command::RD)].push_back({Command::WRA, 1, s.nCL + s.nCCDS + 2 - s.nCWL});
    t[int(Command::RDA)].push_back({Command::WR, 1, s.nCL + s.nCCDS + 2 - s.nCWL});
    t[int(Command::RDA)].push_back({Command::WRA, 1, s.nCL + s.nCCDS + 2 - s.nCWL});
    t[int(Command::WR)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTRS});
    t[int(Command::WR)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nWTRS});
    t[int(Command::WRA)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTRS});
    t[int(Command::WRA)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nWTRS});

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
    t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRRDS});
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

    /*** Bank Group ***/
    t = timing[int(Level::BankGroup)];
    // CAS <-> CAS
    t[int(Command::RD)].push_back({Command::RD, 1, s.nCCDL});
    t[int(Command::RD)].push_back({Command::RDA, 1, s.nCCDL});
    t[int(Command::RDA)].push_back({Command::RD, 1, s.nCCDL});
    t[int(Command::RDA)].push_back({Command::RDA, 1, s.nCCDL});
    t[int(Command::WR)].push_back({Command::WR, 1, s.nCCDL});
    t[int(Command::WR)].push_back({Command::WRA, 1, s.nCCDL});
    t[int(Command::WRA)].push_back({Command::WR, 1, s.nCCDL});
    t[int(Command::WRA)].push_back({Command::WRA, 1, s.nCCDL});
    t[int(Command::WR)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTRL});
    t[int(Command::WR)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nWTRL});
    t[int(Command::WRA)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTRL});
    t[int(Command::WRA)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nWTRL});

    // RAS <-> RAS
    t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRRDL});

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
