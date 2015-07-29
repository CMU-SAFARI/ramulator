/*
 * Refresh.h
 *
 * This is a refresh scheduler. A list of refresh policies implemented:
 *
 * 1. All-bank refresh
 * 2. Per-bank refresh (only DSARP memory module has been completed to work with REFpb).
 *     The other modules (LPDDRx) have not been updated to pass a knob to turn on/off REFpb.
 * 3. A re-implementation of DSARP from the refresh mechanisms proposed in Chang et al.,
 * "Improving DRAM Performance by Parallelizing Refreshes with Accesses", HPCA 2014.
 *
 *  Created on: Mar 17, 2015
 *      Author: kevincha
 */

#ifndef __REFRESH_H_
#define __REFRESH_H_

#include <stddef.h>
#include <cassert>
#include <iostream>
#include <vector>

#include "Request.h"
#include "DSARP.h"
#include "ALDRAM.h"

using namespace std;
using namespace ramulator;

namespace ramulator {

template <typename T>
class Controller;

template <typename T>
class Refresh {
public:
  Controller<T>* ctrl;
  long clk, refreshed;
  // Per-bank refresh counter to track the refresh progress for each rank
  vector<int> bank_ref_counters;
  int max_rank_count, max_bank_count;
  int level_chan, level_rank, level_bank, level_sa;

  // ctor
  Refresh(Controller<T>* ctrl) : ctrl(ctrl) {
    clk = refreshed = 0;
    max_rank_count = ctrl->channel->children.size();
    max_bank_count = ctrl->channel->spec->org_entry.count[(int)T::Level::Bank];

    // Init refresh counters
    for (int r = 0; r < max_rank_count; r++) {
      bank_ref_counters.push_back(0);
      bank_refresh_backlog.push_back(new vector<int>(max_bank_count, 0));
    }

    level_chan = (int)T::Level::Channel;
    level_rank = (int)T::Level::Rank;
    level_bank = (int)T::Level::Bank;
    level_sa   = -1; // Most DRAM doesn't have subarray level
  }

  // dtor
  virtual ~Refresh() {
    // Clean up backlog
    for (unsigned int i = 0; i < bank_refresh_backlog.size(); i++)
      delete bank_refresh_backlog[i];
  }

  // Basic refresh scheduling for all bank refresh that is applicable to all DRAM types
  void tick_ref() {
    clk++;

    int refresh_interval = ctrl->channel->spec->speed_entry.nREFI;

    // Time to schedule a refresh
    if ((clk - refreshed) >= refresh_interval) {
      inject_refresh(true);
      // ALDRAM: update timing parameters based on temperatures
      ALDRAM::Temp current_temperature = ALDRAM::Temp::COLD;
      ctrl->update_temp(current_temperature);
    }
  }

private:
  // Keeping track of refresh status of every bank: + means ahead of schedule, - means behind schedule
  vector<vector<int>*> bank_refresh_backlog;
  // Keeping track of which subarray to refresh next
  vector<vector<int>> subarray_ref_counters;
  int max_sa_count = 0;
  // As defined in the standards
  int backlog_max = 8;
  int backlog_min = -8;
  int backlog_early_pull_threshold = -6;
  bool ctrl_write_mode = false;

  // Refresh based on the specified address
  void refresh_target(Controller<T>* ctrl, int rank, int bank, int sa)
  {
    vector<int> addr_vec(int(T::Level::MAX), -1);
    addr_vec[0] = ctrl->channel->id;
    addr_vec[1] = rank;
    addr_vec[2] = bank;
    addr_vec[3] = sa;
    Request req(addr_vec, Request::Type::REFRESH, NULL);
    bool res = ctrl->enqueue(req);
    assert(res);
  }

  // Inject refresh at either rank or bank level
  void inject_refresh(bool b_ref_rank) {
    // Rank-level refresh
    if (b_ref_rank) {
      for (auto rank : ctrl->channel->children)
        refresh_target(ctrl, rank->id, -1, -1);
    }
    // Bank-level refresh. Simultaneously issue to all ranks (better performance than staggered refreshes).
    else {
      for (auto rank : ctrl->channel->children)
        refresh_target(ctrl, rank->id, bank_ref_counters[rank->id], -1);
    }
    refreshed = clk;
  }

  // DSARP
  void early_inject_refresh();
  void wrp();
};

// Declaration of specialized constructor and tick_ref, so the compiler knows
// where to look for these definitions when controller calls them!
template<> Refresh<DSARP>::Refresh(Controller<DSARP>* ctrl);
template<> void Refresh<DSARP>::tick_ref();

} /* namespace ramulator */

#endif /* SRC_REFRESH_H_ */
