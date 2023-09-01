#include "Cache.h"

Cache::Cache(int size, int assoc, int block_size,
    Level level):
    level(level), higher_cache(0),
    lower_cache(nullptr), size(size), assoc(assoc),
    block_size(block_size) {


  if (level == Level::L1) {
    level_string = "L1";
  } else if (level == Level::L2) {
    level_string = "L2";
  } else if (level == Level::L3) {
    level_string = "L3";
  } else if (level == Level::ICache) {
    level_string =  "ICache";
  }
  //is_first_level = (level == cachesys->first_level);
  //is_last_level = (level == cachesys->last_level);

  // Check size, block size and assoc are 2^N
  assert((size & (size - 1)) == 0);
  assert((block_size & (block_size - 1)) == 0);
  assert((assoc & (assoc - 1)) == 0);
  assert(size >= block_size);

  // Initialize cache configuration
  block_num = size / (block_size * assoc);
  index_mask = block_num - 1;
  index_offset = calc_log2(block_size);
  tag_offset = calc_log2(block_num) + index_offset;

}

bool Cache::send(Request req, std::list<Request> * reqList) {
  // If there isn't a set, create it.
  auto& lines = get_lines(req.addr);
  std::list<Line>::iterator line;

  if (is_hit(lines, req.addr, &line)) {
  //  printf("HIT lvl: %d, address: %lu, index: %d, tag: %lu\n",level,req.addr,get_index(req.addr),get_tag(req.addr));
    lines.push_back(Line(req.addr, get_tag(req.addr), false,
        line->dirty || (req.type == Request::Type::WRITE)));
    lines.erase(line);

    // No need to keep track of the hits
    //cachesys->hit_list.push_back(
      //  make_pair(cachesys->clk + latency[int(level)], req));

    return true;

  } else {
    //printf("MISS lvl: %d, address: %lu, index: %d, tag: %lu\n",level,req.addr,get_index(req.addr),get_tag(req.addr));

    // The dirty bit will be set if this is a write request and @L1
    bool dirty = (req.type == Request::Type::WRITE);

    // Modify the type of the request to lower level
    if (req.type == Request::Type::WRITE) {
      req.type = Request::Type::READ;
    }

    assert(req.type == Request::Type::READ);

    auto newline = allocate_line(lines, req.addr, reqList);
    if (newline == lines.end()) {
      if(is_last_level)
        reqList->push_back(req);
      return false;
    }
  //  printf("added lvl: %d, address: %lu, index: %d, tag: %lu\n",level,req.addr,get_index(req.addr),get_tag(req.addr));
    newline->dirty = dirty;

    // Send the request to next level;
    if (!is_last_level) {
      lower_cache->send(req, reqList);
      //don't need to record it because it will be recorded at the LLC
    } else {
      //cachesys->wait_list.push_back(
        //  make_pair(cachesys->clk + latency[int(level)], req));
        reqList->push_back(req);
    }
    return false;
  }
}

void Cache::evictline(long addr, bool dirty) {
  auto it = cache_lines.find(get_index(addr));
  assert(it != cache_lines.end()); // check inclusive cache
  auto& lines = it->second;
  auto line = find_if(lines.begin(), lines.end(),
      [addr, this](Line l){return (l.tag == get_tag(addr));});

  assert(line != lines.end());
  // Update LRU queue. The dirty bit will be set if the dirty
  // bit inherited from higher level(s) is set.
  lines.push_back(Line(addr, get_tag(addr), false,
      dirty || line->dirty));
  lines.erase(line);
}

std::pair<long, bool> Cache::invalidate(long addr) {
//  printf("invalidate: lvl: %d, address: %lu, index: %d, tag: %lu\n",level,addr,get_index(addr),get_tag(addr));
  long delay = latency_each[int(level)];
  bool dirty = false;

  auto& lines = get_lines(addr);
  if (lines.size() == 0) {
    // The line of this address doesn't exist.
    return make_pair(0, false);
  }
  auto line = find_if(lines.begin(), lines.end(),
      [addr, this](Line l){return (l.tag == get_tag(addr));});

  // If the line is in this level cache, then erase it from
  // the buffer.
  if (line != lines.end()) {
    assert(!line->lock);
  //  printf("DELETE from lvl: %d, address: %lu, index: %d, tag: %lu\n",level,addr,get_index(addr),get_tag(addr));
    lines.erase(line);
  } else {
    // If it's not in current level, then no need to go up.
    return make_pair(delay, false);
  }

  if (higher_cache.size()) {
    long max_delay = delay;
    for (auto hc : higher_cache) {
      auto result = hc->invalidate(addr);
      if (result.second) {
        max_delay = max(max_delay, delay + result.first * 2);
      } else {
        max_delay = max(max_delay, delay + result.first);
      }
      dirty = dirty || line->dirty || result.second;
    }
    delay = max_delay;
  } else {
    dirty = line->dirty;
  }
  return make_pair(delay, dirty);
}


void Cache::evict(std::list<Line>* lines,
    std::list<Line>::iterator victim, std::list<Request> * reqList) {
  long addr = victim->addr;
  long invalidate_time = 0;
  bool dirty = victim->dirty;

  // First invalidate the victim line in higher level.
  if (higher_cache.size()) {
    for (auto hc : higher_cache) {
      auto result = hc->invalidate(addr);
      invalidate_time = max(invalidate_time,
          result.first + (result.second ? latency_each[int(level)] : 0));
      dirty = dirty || result.second || victim->dirty;
    }
  }

  if (!is_last_level) {
    // not LLC eviction
    assert(lower_cache != nullptr);
    lower_cache->evictline(addr, dirty);
  } else {
    // LLC eviction
    if (dirty) {
      Request write_req(addr, Request::Type::WRITE);
          reqList->push_back(write_req);
    }
  }
  lines->erase(victim);
}

std::list<Cache::Line>::iterator Cache::allocate_line(
    std::list<Line>& lines, long addr, std::list<Request> * reqList) {
  // See if an eviction is needed
  if (need_eviction(lines, addr)) {
    // Get victim.
    // The first one might still be locked due to reorder in MC
    auto victim = find_if(lines.begin(), lines.end(),
        [this](Line line) {
          bool check = !line.lock;
          if (!is_first_level) {
            for (auto hc : higher_cache) {
              if(!check) {
                return check;
              }
              check = check && hc->check_unlock(line.addr);
            }
          }
          return check;
        });
    if (victim == lines.end()) {
      return victim;  // doesn't exist a line that's already unlocked in each level
    }
    assert(victim != lines.end());
    evict(&lines, victim,reqList);
  }

  // Allocate newline, with lock bit on and dirty bit off
  lines.push_back(Line(addr, get_tag(addr)));
  auto last_element = lines.end();
  --last_element;
  return last_element;
}

bool Cache::is_hit(std::list<Line>& lines, long addr,
    std::list<Line>::iterator* pos_ptr) {
  auto pos = find_if(lines.begin(), lines.end(),
      [addr, this](Line l){return (l.tag == get_tag(addr));});
  *pos_ptr = pos;
  if (pos == lines.end()) {
    return false;
  }
  return !pos->lock;
}

void Cache::concatlower(Cache* lower) {
  lower_cache = lower;
  assert(lower != nullptr);
  lower->higher_cache.push_back(this);
};

bool Cache::need_eviction(const std::list<Line>& lines, long addr) {
    if (lines.size() < assoc) {
      return false;
    } else {
      return true;
    }
}
string Cache::dump_content(){
  std::ostringstream content;
  for(std::map<int, std::list<Line>>::iterator it=cache_lines.begin();it!=cache_lines.end();++it) {
    content<< it->first << "\t";
    for(std::list<Line>::iterator it2=it->second.begin();it2!=it->second.end();++it2){
      content<<it2->addr<<"\t";
    }
    content<<"\n";
  }
  return content.str();
}
