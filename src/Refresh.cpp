/*
 * Refresh.cpp
 *
 * Mainly DSARP specialization at the moment.
 *
 *  Created on: Mar 17, 2015
 *      Author: kevincha
 */

#include <stdlib.h>

#include "Refresh.h"
#include "Controller.h"
#include "DRAM.h"
#include "DSARP.h"

using namespace std;
using namespace ramulator;

namespace ramulator {

/**** DSARP specialization ****/
template<>
Refresh<DSARP>::Refresh(Controller<DSARP>* ctrl) : ctrl(ctrl) {
  clk = refreshed = 0;
  max_rank_count = ctrl->channel->children.size();
  max_bank_count = ctrl->channel->spec->org_entry.count[(int)DSARP::Level::Bank];
  max_sa_count = ctrl->channel->spec->org_entry.count[(int)DSARP::Level::SubArray];

  // Init refresh counters
  for (int r = 0; r < max_rank_count; r++) {
    bank_ref_counters.push_back(0);
    bank_refresh_backlog.push_back(new vector<int>(max_bank_count, 0));
    vector<int> sa_counters(ctrl->channel->spec->org_entry.count[(int)DSARP::Level::SubArray], 0);
    subarray_ref_counters.push_back(sa_counters);
  }

  level_chan = (int)DSARP::Level::Channel;
  level_rank = (int)DSARP::Level::Rank;
  level_bank = (int)DSARP::Level::Bank;
  level_sa   = (int)DSARP::Level::SubArray;
}

template<>
void Refresh<DSARP>::early_inject_refresh() {
  // Only enabled during reads
  if (ctrl->write_mode)
    return;

  // OoO bank-level refresh
  vector<bool> is_bank_occupied(max_rank_count * max_bank_count, false);
  Controller<DSARP>::Queue& rdq = ctrl->readq;

  // Figure out which banks are idle in order to refresh one of them
  for (auto req: rdq.q)
  {
    assert(req.addr_vec[level_chan] == ctrl->channel->id);
    int ridx = req.addr_vec[level_rank] * max_bank_count;
    int bidx = req.addr_vec[level_bank];
    is_bank_occupied[ridx+bidx] = true;
  }

  // Try to pick an idle bank to refresh per rank
  for (int r = 0; r < max_rank_count; r++) {
    // Randomly pick a bank to examine
    int bidx_start = rand() % max_bank_count;

    for (int b = 0; b < max_bank_count; b++)
    {
      int bidx = (bidx_start + b) % max_bank_count;
      // Idle cycle only
      if (is_bank_occupied[(r * max_bank_count) + bidx])
        continue;

      // Pending refresh
      bool pending_ref = false;
      for (Request req : ctrl->otherq.q)
        if (req.type == Request::Type::REFRESH
            && req.addr_vec[level_chan] == ctrl->channel->id
            && req.addr_vec[level_rank] == r && req.addr_vec[level_bank] == bidx)
          pending_ref = true;
      if (pending_ref)
        continue;

      // Only pull in refreshes when we are almost running out of credits
      if ((*(bank_refresh_backlog[r]))[bidx] >= backlog_early_pull_threshold ||
          ctrl->otherq.q.size() >= ctrl->otherq.max)
        continue;

      // Refresh now
      refresh_target(ctrl, r, bidx, subarray_ref_counters[r][bidx]);
      // One credit for delaying a future ref
      (*(bank_refresh_backlog[r]))[bidx]++;
      subarray_ref_counters[r][bidx] = (subarray_ref_counters[r][bidx]+1) % max_sa_count;
      break;
    }
  }
}

template<>
void Refresh<DSARP>::inject_refresh(bool b_ref_rank) {
  // Rank-level refresh
  if (b_ref_rank)
    for (auto rank : ctrl->channel->children)
      refresh_target(ctrl, rank->id, -1, -1);
  // Bank-level refresh. Simultaneously issue to all ranks (better performance than staggered refreshes).
  else {
    for (auto rank : ctrl->channel->children) {
      int rid = rank->id;
      int bid = bank_ref_counters[rid];

      // Behind refresh schedule by 1 ref
      (*(bank_refresh_backlog[rid]))[bid]--;

      // Next time, refresh the next bank in the same bank
      bank_ref_counters[rid] = (bank_ref_counters[rid] + 1) % max_bank_count;

      // Check to see if we can skip a refresh
      if (ctrl->channel->spec->type == DSARP::Type::DARP ||
        ctrl->channel->spec->type == DSARP::Type::DSARP) {

        bool ref_now = false;
        // 1. Any pending refrehes?
        bool pending_ref = false;
        for (Request req : ctrl->otherq.q) {
          if (req.type == Request::Type::REFRESH) {
            pending_ref = true;
            break;
          }
        }

        // 2. Track readq
        if (!pending_ref && ctrl->readq.size() == 0)
          ref_now = true;

        // 3. Track log status. If we are too behind the schedule, then we need to refresh now.
        if ((*(bank_refresh_backlog[rid]))[bid] <= backlog_min)
          ref_now = true;

        // Otherwise skip refresh
        if (!ref_now)
          continue;
      }

      refresh_target(ctrl, rid, bid, subarray_ref_counters[rid][bid]);
      // Get 1 ref credit
      (*(bank_refresh_backlog[rid]))[bid]++;
      // Next time, refresh the next sa in the same bank
      subarray_ref_counters[rid][bid] = (subarray_ref_counters[rid][bid]+1) % max_sa_count;
    }
  }
  refreshed = clk;
}

// first = wrq.count; second = bank idx
typedef pair<int, int> wrq_idx;
bool wrq_comp (wrq_idx l, wrq_idx r)
{
  return l.first < r.first;
}

// WRP
template<>
void Refresh<DSARP>::wrp() {
  for (int ref_rid = 0; ref_rid < max_rank_count; ref_rid++)
  {
    // Pending refresh in the rank?
    bool pending_ref = false;
    for (Request req : ctrl->otherq.q) {
      if (req.type == Request::Type::REFRESH && req.addr_vec[level_rank] == ref_rid) {
        pending_ref = true;
        break;
      }
    }
    if (pending_ref)
      continue;

    // Find the bank with the lowest number of writes+reads
    vector<wrq_idx> sorted_bank_demand;
    for (int b = 0; b < max_bank_count; b++)
      sorted_bank_demand.push_back(wrq_idx(0,b));
    // Filter out all the writes to this rank
    int total_wr = 0;
    for (auto req : ctrl->writeq.q) {
      if (req.addr_vec[level_rank] == ref_rid) {
        sorted_bank_demand[req.addr_vec[level_bank]].first++;
        total_wr++;
      }
    }
    // If there's no write, just skip.
    if (total_wr == 0)
      continue;

    // Add read
    for (auto req : ctrl->readq.q)
      if (req.addr_vec[level_rank] == ref_rid)
        sorted_bank_demand[req.addr_vec[level_bank]].first++;

    // Sort based on the entries
    std::sort(sorted_bank_demand.begin(), sorted_bank_demand.end(), wrq_comp);

    // Randomly select an idle bank to refresh
    int top_idle_idx = 0;
    for (int i = 0; i < max_bank_count; i++) {
      if (sorted_bank_demand[i].second != 0) {
        top_idle_idx = i;
        break;
      }
    }

    // Select a bank to ref
    int ref_bid_idx = (top_idle_idx == 0) ? 0 : rand() % top_idle_idx;
    int ref_bid = sorted_bank_demand[ref_bid_idx].second;

    // Make sure we don't exceed the credit
    if ((*(bank_refresh_backlog[ref_rid]))[ref_bid] < backlog_max
        && ctrl->otherq.q.size() < ctrl->otherq.max) {
      refresh_target(ctrl, ref_rid, ref_bid, subarray_ref_counters[ref_rid][ref_bid]);
      // Get 1 ref credit
      (*(bank_refresh_backlog[ref_rid]))[ref_bid]++;
      subarray_ref_counters[ref_rid][ref_bid] = (subarray_ref_counters[ref_rid][ref_bid]+1) % max_sa_count;
    }
  }
}

// OoO refresh of DSARP
template<>
void Refresh<DSARP>::tick_ref() {
  clk++;

  bool b_ref_rank = ctrl->channel->spec->b_ref_rank;
  int refresh_interval =
      (b_ref_rank) ?
          ctrl->channel->spec->speed_entry.nREFI :
          ctrl->channel->spec->speed_entry.nREFIpb;

  // DARP
  if (ctrl->channel->spec->type == DSARP::Type::DARP ||
    ctrl->channel->spec->type == DSARP::Type::DSARP) {
    // Write-Refresh Parallelization. Issue refreshes when the controller enters writeback mode
    if (!ctrl_write_mode && ctrl->write_mode)
      wrp();
    // Record write mode
    ctrl_write_mode = ctrl->write_mode;
    // Inject early to pull in some refreshes during read mode
    early_inject_refresh();
  }

  // Time to schedule a refresh and also try to skip some refreshes
  if ((clk - refreshed) >= refresh_interval)
    inject_refresh(b_ref_rank);
}
/**** End DSARP specialization ****/

} /* namespace ramulator */
