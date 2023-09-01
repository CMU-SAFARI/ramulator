#ifndef __DRAM_H
#define __DRAM_H

#include "Statistics.h"
#include <iostream>
#include <vector>
#include <deque>
#include <map>
#include <functional>
#include <algorithm>
#include <cassert>
#include <type_traits>

using namespace std;

namespace ramulator
{

template <typename T>
class DRAM
{
public:
    ScalarStat active_cycles;
    ScalarStat refresh_cycles;
    ScalarStat busy_cycles;
    ScalarStat active_refresh_overlap_cycles;

    ScalarStat serving_requests;
    ScalarStat average_serving_requests;

    // Constructor
    DRAM(T* spec, typename T::Level level);
    ~DRAM();

    // Specification (e.g., DDR3)
    T* spec;

    // Tree Organization (e.g., Channel->Rank->Bank->Row->Column)
    typename T::Level level;
    int id;
    long size;
    DRAM* parent;
    vector<DRAM*> children;

    // State (e.g., Opened, Closed)
    typename T::State state;

    // State of Rows:
    // There are too many rows for them to be instantiated individually
    // Instead, their bank (or an equivalent entity) tracks their state for them
    map<int, typename T::State> row_state;

    // Insert a node as one of my child nodes
    void insert(DRAM<T>* child);

    // Decode a command into its "prerequisite" command (if any is needed)
    typename T::Command decode(typename T::Command cmd, const int* addr);

    // Check whether a command is ready to be scheduled
    bool check(typename T::Command cmd, const int* addr, long clk);

    // Check whether a command is a row hit
    bool check_row_hit(typename T::Command cmd, const int* addr);

    // Check whether a row is open
    bool check_row_open(typename T::Command cmd, const int* addr);

    // Return the earliest clock when a command is ready to be scheduled
    long get_next(typename T::Command cmd, const int* addr);

    // Update the timing/state of the tree, signifying that a command has been issued
    void update(typename T::Command cmd, const int* addr, long clk);
    // Update statistics:

    // Update the number of requests it serves currently
    void update_serving_requests(const int* addr, int delta, long clk);

    // TIANSHI: current serving requests count
    int cur_serving_requests = 0;
    long begin_of_serving = -1;
    long end_of_serving = -1;
    long begin_of_cur_reqcnt = -1;
    long begin_of_refreshing = -1;
    long end_of_refreshing = -1;
    std::vector<std::pair<long, long>> refresh_intervals;

    // register statistics
    void regStats(const std::string& identifier);

    void finish(long dram_cycles);

private:
    // Constructor
    DRAM(){}

    // Timing
    long cur_clk = 0;
    long next[int(T::Command::MAX)]; // the earliest time in the future when a command could be ready
    deque<long> prev[int(T::Command::MAX)]; // the most recent history of when commands were issued

    // Lookup table for which commands must be preceded by which other commands (i.e., "prerequisite")
    // E.g., a read command to a closed bank must be preceded by an activate command
    function<typename T::Command(DRAM<T>*, typename T::Command cmd, int)>* prereq;

    // SAUGATA: added table for row hits
    // Lookup table for whether a command is a row hit
    // E.g., a read command to a closed bank must be preceded by an activate command
    function<bool(DRAM<T>*, typename T::Command cmd, int)>* rowhit;
    function<bool(DRAM<T>*, typename T::Command cmd, int)>* rowopen;

    // Lookup table between commands and the state transitions they trigger
    // E.g., an activate command to a closed bank opens both the bank and the row
    function<void(DRAM<T>*, int)>* lambda;

    // Lookup table for timing parameters
    // E.g., activate->precharge: tRAS@bank, activate->activate: tRC@bank
    vector<typename T::TimingEntry>* timing;

    // Helper Functions
    void update_state(typename T::Command cmd, const int* addr);
    void update_timing(typename T::Command cmd, const int* addr, long clk);
}; /* class DRAM */


// register statistics
template <typename T>
void DRAM<T>::regStats(const std::string& identifier) {
    active_cycles
        .name("active_cycles" + identifier + "_" + to_string(id))
        .desc("Total active cycles for level " + identifier + "_" + to_string(id))
        .precision(0)
        ;
    refresh_cycles
        .name("refresh_cycles" + identifier + "_" + to_string(id))
        .desc("(All-bank refresh only, only valid for rank level) The sum of cycles that is under refresh per memory cycle for level " + identifier + "_" + to_string(id))
        .precision(0)
        .flags(Stats::nozero)
        ;
    busy_cycles
        .name("busy_cycles" + identifier + "_" + to_string(id))
        .desc("(All-bank refresh only. busy cycles only include refresh time in rank level) The sum of cycles that the DRAM part is active or under refresh for level " + identifier + "_" + to_string(id))
        .precision(0)
        ;
    active_refresh_overlap_cycles
        .name("active_refresh_overlap_cycles" + identifier + "_" + to_string(id))
        .desc("(All-bank refresh only, only valid for rank level) The sum of cycles that are both active and under refresh per memory cycle for level " + identifier + "_" + to_string(id))
        .precision(0)
        .flags(Stats::nozero)
        ;
    serving_requests
        .name("serving_requests" + identifier + "_" + to_string(id))
        .desc("The sum of read and write requests that are served in this DRAM element per memory cycle for level " + identifier + "_" + to_string(id))
        .precision(0)
        ;
    average_serving_requests
        .name("average_serving_requests" + identifier + "_" + to_string(id))
        .desc("The average of read and write requests that are served in this DRAM element per memory cycle for level " + identifier + "_" + to_string(id))
        .precision(6)
        ;

    if (!children.size()) {
      return;
    }

    // recursively register children statistics
    for (auto child : children) {
      child->regStats(identifier + "_" + to_string(id));
    }
}

template <typename T>
void DRAM<T>::finish(long dram_cycles) {
  // finalize busy cycles
  busy_cycles = active_cycles.value() + refresh_cycles.value() - active_refresh_overlap_cycles.value();

  // finalize average serving requests
  average_serving_requests = serving_requests.value() / dram_cycles;

  if (!children.size()) {
    return;
  }

  for (auto child : children) {
    child->finish(dram_cycles);
  }
}

// Constructor
template <typename T>
DRAM<T>::DRAM(T* spec, typename T::Level level) :
    spec(spec), level(level), id(0), parent(NULL)
{

    state = spec->start[(int)level];
    prereq = spec->prereq[int(level)];
    rowhit = spec->rowhit[int(level)];
    rowopen = spec->rowopen[int(level)];
    lambda = spec->lambda[int(level)];
    timing = spec->timing[int(level)];

    fill_n(next, int(T::Command::MAX), -1); // initialize future
    for (int cmd = 0; cmd < int(T::Command::MAX); cmd++) {
        int dist = 0;
        for (auto& t : timing[cmd])
            dist = max(dist, t.dist);

        if (dist)
            prev[cmd].resize(dist, -1); // initialize history
    }

    // try to recursively construct my children
    int child_level = int(level) + 1;
    if (child_level == int(T::Level::Row))
        return; // stop recursion: rows are not instantiated as nodes

    int child_max = spec->org_entry.count[child_level];
    if (!child_max)
        return; // stop recursion: the number of children is unspecified

    // recursively construct my children
    for (int i = 0; i < child_max; i++) {
        DRAM<T>* child = new DRAM<T>(spec, typename T::Level(child_level));
        child->parent = this;
        child->id = i;
        children.push_back(child);
    }

}

template <typename T>
DRAM<T>::~DRAM()
{
    for (auto child: children)
        delete child;
}

// Insert
template <typename T>
void DRAM<T>::insert(DRAM<T>* child)
{
    child->parent = this;
    child->id = children.size();
    children.push_back(child);
}

// Decode
template <typename T>
typename T::Command DRAM<T>::decode(typename T::Command cmd, const int* addr)
{
    int child_id = addr[int(level)+1];
    if (prereq[int(cmd)]) {
        typename T::Command prereq_cmd = prereq[int(cmd)](this, cmd, child_id);
        if (prereq_cmd != T::Command::MAX)
            return prereq_cmd; // stop recursion: there is a prerequisite at this level
    }

    if (child_id < 0 || !children.size())
        return cmd; // stop recursion: there were no prequisites at any level

    // recursively decode at my child
    return children[child_id]->decode(cmd, addr);
}


// Check
template <typename T>
bool DRAM<T>::check(typename T::Command cmd, const int* addr, long clk)
{
    if (next[int(cmd)] != -1 && clk < next[int(cmd)])
        return false; // stop recursion: the check failed at this level

    int child_id = addr[int(level)+1];
    if (child_id < 0 || level == spec->scope[int(cmd)] || !children.size())
        return true; // stop recursion: the check passed at all levels

    // recursively check my child
    return children[child_id]->check(cmd, addr, clk);
}

// SAUGATA: added function to check whether a command is a row hit
// Check row hits
template <typename T>
bool DRAM<T>::check_row_hit(typename T::Command cmd, const int* addr)
{
    int child_id = addr[int(level)+1];
    if (rowhit[int(cmd)]) {
        return rowhit[int(cmd)](this, cmd, child_id);  // stop recursion: there is a row hit at this level
    }

    if (child_id < 0 || !children.size())
        return false; // stop recursion: there were no row hits at any level

    // recursively check for row hits at my child
    return children[child_id]->check_row_hit(cmd, addr);
}

template <typename T>
bool DRAM<T>::check_row_open(typename T::Command cmd, const int* addr)
{
    int child_id = addr[int(level)+1];
    if (rowopen[int(cmd)]) {
        return rowopen[int(cmd)](this, cmd, child_id);  // stop recursion: there is a row hit at this level
    }

    if (child_id < 0 || !children.size())
        return false; // stop recursion: there were no row hits at any level

    // recursively check for row hits at my child
    return children[child_id]->check_row_open(cmd, addr);
}

template <typename T>
long DRAM<T>::get_next(typename T::Command cmd, const int* addr)
{
    long next_clk = max(cur_clk, next[int(cmd)]);
    auto node = this;
    for (int l = int(level); l < int(spec->scope[int(cmd)]) && node->children.size() && addr[l + 1] >= 0; l++){
        node = node->children[addr[l + 1]];
        next_clk = max(next_clk, node->next[int(cmd)]);
    }
    return next_clk;
}

// Update
template <typename T>
void DRAM<T>::update(typename T::Command cmd, const int* addr, long clk)
{
    cur_clk = clk;
    update_state(cmd, addr);
    update_timing(cmd, addr, clk);
}


// Update (State)
template <typename T>
void DRAM<T>::update_state(typename T::Command cmd, const int* addr)
{
    int child_id = addr[int(level)+1];
    if (lambda[int(cmd)])
        lambda[int(cmd)](this, child_id); // update this level

    if (level == spec->scope[int(cmd)] || !children.size())
        return; // stop recursion: updated all levels

    // recursively update my child
    children[child_id]->update_state(cmd, addr);
}


// Update (Timing)
template <typename T>
void DRAM<T>::update_timing(typename T::Command cmd, const int* addr, long clk)
{
    // I am not a target node: I am merely one of its siblings
    if (id != addr[int(level)]) {
        for (auto& t : timing[int(cmd)]) {
            if (!t.sibling)
                continue; // not an applicable timing parameter

            assert (t.dist == 1);

            long future = clk + t.val;
            next[int(t.cmd)] = max(next[int(t.cmd)], future); // update future
        }

        return; // stop recursion: only target nodes should be recursed
    }

    // I am a target node
    if (prev[int(cmd)].size()) {
        prev[int(cmd)].pop_back();  // FIXME TIANSHI why pop back?
        prev[int(cmd)].push_front(clk); // update history
    }

    for (auto& t : timing[int(cmd)]) {
        if (t.sibling)
            continue; // not an applicable timing parameter

        long past = prev[int(cmd)][t.dist-1];
        if (past < 0)
            continue; // not enough history

        long future = past + t.val;
        next[int(t.cmd)] = max(next[int(t.cmd)], future); // update future
        // TIANSHI: for refresh statistics
        if (spec->is_refreshing(cmd) && spec->is_opening(t.cmd)) {
          assert(past == clk);
          begin_of_refreshing = clk;
          end_of_refreshing = max(end_of_refreshing, next[int(t.cmd)]);
          refresh_cycles += end_of_refreshing - clk;
          if (cur_serving_requests > 0) {
            refresh_intervals.push_back(make_pair(begin_of_refreshing, end_of_refreshing));
          }
        }
    }

    // Some commands have timings that are higher that their scope levels, thus
    // we do not stop at the cmd's scope level
    if (!children.size())
        return; // stop recursion: updated all levels

    // recursively update *all* of my children
    for (auto child : children)
        child->update_timing(cmd, addr, clk);

}

template <typename T>
void DRAM<T>::update_serving_requests(const int* addr, int delta, long clk) {
  assert(id == addr[int(level)]);
  assert(delta == 1 || delta == -1);
  // update total serving requests
  if (begin_of_cur_reqcnt != -1 && cur_serving_requests > 0) {
    serving_requests += (clk - begin_of_cur_reqcnt) * cur_serving_requests;
    active_cycles += clk - begin_of_cur_reqcnt;
  }
  // update begin of current request number
  begin_of_cur_reqcnt = clk;
  cur_serving_requests += delta;
  assert(cur_serving_requests >= 0);

  if (delta == 1 && cur_serving_requests == 1) {
    // transform from inactive to active
    begin_of_serving = clk;
    if (end_of_refreshing > begin_of_serving) {
      active_refresh_overlap_cycles += end_of_refreshing - begin_of_serving;
    }
  } else if (cur_serving_requests == 0) {
    // transform from active to inactive
    assert(begin_of_serving != -1);
    assert(delta == -1);
    active_cycles += clk - begin_of_cur_reqcnt;
    end_of_serving = clk;

    for (const auto& ref: refresh_intervals) {
      active_refresh_overlap_cycles += min(end_of_serving, ref.second) - ref.first;
    }
    refresh_intervals.clear();
  }

  int child_id = addr[int(level) + 1];
  // We only count the level bank or the level higher than bank
  if (child_id < 0 || !children.size() || (int(level) > int(T::Level::Bank)) ) {
    return;
  }
  children[child_id]->update_serving_requests(addr, delta, clk);
}

} /* namespace ramulator */

#endif /* __DRAM_H */
