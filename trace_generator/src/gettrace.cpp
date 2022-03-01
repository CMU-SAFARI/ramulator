/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <fstream>
#include "pin.H"
#include "Cache.h"
#include "Cache.cpp"
#include "Config.h"
#include "Config.cpp"
#include "PPFileParser.cpp"

ofstream trace;

//Statistics
static UINT64 total = 0;
static UINT64 recorded_instr = 0;
static UINT64 recorded_instr_int = 0;
static UINT64 mem_req = 0;
static UINT64 filtered = 0;
static UINT64 num_ifetch = 0;
static UINT64 filt_ifetch = 0;
static int num_slices = 1;

static UINT64 c_total = 0;
static UINT64 read_count = 0;
static UINT64 write_count = 0;
static UINT64 read_hit = 0;
static UINT64 write_hit = 0;
static UINT64 icache_hit = 0;

bool full_sim = false;
bool mem_filtered = true;
bool mem2_filtered = true;

static int bbl_cnt = 0;

static bool control= false;
static bool record = false;
static std::list<PSlice*> * slices = new std::list<PSlice*>();
static std::map<Point *,long unsigned int> * execount = new std::map<Point * , long unsigned int>();
static std::map<std::string,IMG> * images = new std::map<std::string,IMG>();


static long unsigned int seq_number=0;
static std::map<string,long unsigned int> * dependency = new std::map<string,long unsigned int>();

static std::list<Request> * reqList = new std::list<Request>();

static std::list<long unsigned int> * depList = new std::list<long unsigned int>();
static std::list<std::list<string> *> * deletelist = new std::list<std::list<string> *>();

static std::list<Cache *> * caches = new std::list<Cache *>();

static std::list<REG> * compRRegs = new std::list<REG>();

//options
static KNOB<string> KnobTraceFile(KNOB_MODE_WRITEONCE , "pintool","t","trace.out","specify trace output file name.");
static KNOB<string> KnobStatsFile(KNOB_MODE_WRITEONCE , "pintool","s","stats.out","specify stats output file name.");
static KNOB<string> KnobConfigFile(KNOB_MODE_WRITEONCE , "pintool","c","Cache.cfg","specify config file name.");
static KNOB<string> KnobMode(KNOB_MODE_WRITEONCE, "pintool", "mode","cpu","specify the mode of the output trace.");

static KNOB<BOOL> KnobPhysicalAddress(KNOB_MODE_WRITEONCE,"pintool","paddr","0", "generate traces with physical addresses.");

static KNOB<string> KnobPinPoints(KNOB_MODE_WRITEONCE, "pintool", "ppoints", "", "set the pinpoints output file.");

//static KNOB<string> KnobSimpoints(KNOB_MODE_WRITEONCE, "pintool", "points", "", "set the simpoint file.");
static KNOB<string> KnobCoverage(KNOB_MODE_WRITEONCE, "pintool", "cvg", "", "set the coverage.");
static KNOB<BOOL> KnobFastOption(KNOB_MODE_WRITEONCE, "pintool", "fast", "0", "enable the fast simulation mode.");

//cache options

//L1
static KNOB<int> KnobL1Size(KNOB_MODE_WRITEONCE, "pintool", "l1_size","-1","specify the size of the first level cache as power of two.");
static KNOB<int> KnobL1Assoc(KNOB_MODE_WRITEONCE, "pintool", "l1_assoc","-1","specify the associativity of the first level cache as power of two.");
static KNOB<int> KnobL1BlockSize(KNOB_MODE_WRITEONCE, "pintool", "l1_block_size","-1","specify the block size of the first level cache as power of two.");
//L2
static KNOB<int> KnobL2Size(KNOB_MODE_WRITEONCE, "pintool", "l2_size","-1","specify the size of the second level cache as power of two.");
static KNOB<int> KnobL2Assoc(KNOB_MODE_WRITEONCE, "pintool", "l2_assoc","-1","specify the associativity of the second level cache as power of two.");
static KNOB<int> KnobL2BlockSize(KNOB_MODE_WRITEONCE, "pintool", "l2_block_size","-1","specify the block size of the second level cache as power of two.");
//L3
static KNOB<int> KnobL3Size(KNOB_MODE_WRITEONCE, "pintool", "l3_size","-1","specify the size of the third level cache as power of two.");
static KNOB<int> KnobL3Assoc(KNOB_MODE_WRITEONCE, "pintool", "l3_assoc","-1","specify the associativity of the third level cache as power of two.");
static KNOB<int> KnobL3BlockSize(KNOB_MODE_WRITEONCE, "pintool", "l3_block_size","-1","specify the block size of the third level cache as power of two.");

//icache and dcache
static KNOB<BOOL> KnobIFEnable(KNOB_MODE_WRITEONCE, "pintool", "ifetch", "1", "enable the instruction cache.");
static KNOB<BOOL> KnobDCEnable(KNOB_MODE_WRITEONCE, "pintool", "dcache", "1", "enable the instruction cache.");

static KNOB<BOOL> KnobDebugPrints(KNOB_MODE_WRITEONCE, "pintool", "debug", "0", "enable debug prints.");
static KNOB<long unsigned int> KnobISize(KNOB_MODE_WRITEONCE, "pintool", "intervalsize", "100000000", "enable debug prints.");

//Instruction Cache and Instruction Fetch
static KNOB<BOOL> KnobICEnable(KNOB_MODE_WRITEONCE, "pintool", "icache", "1", "enable the instruction cache.");
static KNOB<int> KnobICSize(KNOB_MODE_WRITEONCE, "pintool", "ic_size","-1","specify the size of the instruction cache as power of two.");
static KNOB<int> KnobICAssoc(KNOB_MODE_WRITEONCE, "pintool", "ic_assoc","-1","specify the associativity of the instruction cache as power of two.");
static KNOB<int> KnobICBlockSize(KNOB_MODE_WRITEONCE, "pintool", "ic_block_size","-1","specify the block size of the instruction cache as power of two.");
//options end.

Cache * first;
Cache * icache;
#define PAGEMAP_LENGTH 8 // pagemap contains 8 bytes of info for each virtual page
#define PAGE_SHIFT 12 // change if the page size is different than 4KB.
FILE * addr_file;
FILE *pagemap;

void finish(){
  //Close the files and delete the data structures
  trace.close();
  if(KnobPhysicalAddress.Value()){
    fclose(pagemap);
  }
  for(std::map<Point *, long unsigned int>::iterator it=execount->begin();it!=execount->end();++it) {
    delete it->first;
  }
  delete execount;
  //This should always print "Slices empty: 1"
  if(KnobDebugPrints.Value())
    printf("Slices empty : %d\n",slices->empty());
  delete slices;
  delete images;
  delete dependency;
  delete reqList;
  delete depList;
  for(std::list<std::list<string> *>::iterator it = deletelist->begin();it!=deletelist->end();++it){
    delete *it;
  }
  delete deletelist;
  for(std::list<Cache *>::iterator it=caches->begin();it!=caches->end();++it) {
    delete *it;
  }
  delete caches;
  // Start printing the statistics
  printf("Total Number of Instructions\t\t\t\t:%lu\t\t(until the end of selected slices for coverage traces.)\n",total);
  printf("Total Number of Recorded Instructions\t\t\t:%lu\n", recorded_instr);
  printf("Total Number of Recorded Memory Requests\t\t:%lu\n", mem_req);
  printf("Total Number of Filtered Memory Requests\t\t:%lu\n", filtered);
  printf("Total Number of Recorded IFetch Requests\t\t:%lu\n", num_ifetch);
  printf("Total Number of Filtered IFetch Requests\t\t:%lu\n", filt_ifetch);
  printf("Number of Slices\t\t\t\t\t:%d\n", num_slices);
}

UINT64 getPhysicalAddr(long unsigned int addr){
  //long unsigned limit = 1UL << 33;
  //unsigned long get_page_frame_number_of_address(void *addr) {
   // Open the pagemap file for the current process

   // Seek to the page that the buffer is on
   unsigned long offset = addr / getpagesize() * PAGEMAP_LENGTH;
   assert(fseek(pagemap, (unsigned long)offset, SEEK_SET) == 0 &&  "Failed to seek pagemap to proper location");

   // The page frame number is in bits 0-54 so read the first 7 bytes and clear the 55th bit
   unsigned long page_frame_number = 0;
   fread(&page_frame_number, 1, PAGEMAP_LENGTH-1, pagemap);

   page_frame_number &= 0x7FFFFFFFFFFFFF;

   unsigned long  distance_from_page_boundary_of_buffer = addr % getpagesize();
   unsigned long physical_addr = (page_frame_number << PAGE_SHIFT) + distance_from_page_boundary_of_buffer;
   //fprintf(addr_file,"%lu %lu %lu %lu\n",(long)addr, (long)physical_addr,page_frame_number,offset);
   //assert(limit>=physical_addr && "oops");
   return physical_addr;
}
void initializeCounters(){
  for(std::list<PSlice*>::iterator it=slices->begin();it!=slices->end();++it) {
    execount->insert(std::make_pair((*it)->start,0));
    execount->insert(std::make_pair((*it)->end,0));
  }
}
//returns the counter value for the address if not in the counters return -1.
std::pair<bool, long unsigned int> getCounter(ADDRINT address) {
  for(std::map<Point *,long unsigned int>::iterator it = execount->begin();it!=execount->end();++it) {
    Point * pt = (*it).first;
    if(pt->symbolic){
      std::map<string, IMG>::iterator img = images->find(pt->lib_name);
      if (img == images->end()) continue;
      long unsigned int offset =  address - IMG_LowAddress(img->second);
      if(offset == pt->offset)
        return make_pair(true,(*it).second);
    }
    else {
      if(pt->address == address)
        return make_pair(true,(*it).second);
    }
  }
  return make_pair(false, 0);
}
void incrementCounter(ADDRINT address) {
  for(std::map<Point *,long unsigned int>::iterator it = execount->begin();it!=execount->end();++it) {
    Point * pt = (*it).first;
    if(pt->symbolic){
      std::map<string, IMG>::iterator img = images->find(pt->lib_name);
      if (img == images->end()) continue;
      long unsigned int offset =  address - IMG_LowAddress(img->second);
      if(offset == pt->offset){
        (*it).second++;
      }
    }
    else {
      if(pt->address == address)
        (*it).second++;
    }
  }
}

void removeOld(){
  for(std::map<string,long unsigned int>::iterator it = dependency->begin();it!=dependency->end();++it) {
    if(it->second==seq_number)
      dependency->erase(it);
  }
}

//update sequence number used for data dependency trace
void updateSeqNumber(){
  seq_number++;
  if(seq_number==128) seq_number = 0;
}

//reset sequence number used for data dependency traces
void resetSeqNumber(){
  seq_number=0;
}

//update data dependency window - returns the old entry with given address (-1 if not exists)
int insertDependency(string regname){
  std::map<string,long unsigned int>::iterator it = dependency->find(regname);
  int old = -1;
  if(it!=dependency->end()) {
    old = it->second;
    dependency->erase(it);
  }
  dependency->insert(make_pair(regname, seq_number-1));
  return old;
}


//between traces we should reset the window.
void resetDependencyWindow(){
  dependency->clear();
  resetSeqNumber();
}
//
int getDep(REG regobj){
  string reg = REG_StringShort(regobj);
  std::map<string, long unsigned int>::iterator it = dependency->find(reg);
  if(it!=dependency->end())
    return it->second;
  return -1;
}
//updates the execution count list for the starting and ending points in slices.
void updateCounters(ADDRINT address){
  incrementCounter(address);
}


string getCompDependency(REG read_base, REG read2_base, REG read_index, REG read2_index){
  for(std::list<REG>::iterator it = compRRegs->begin();it!=compRRegs->end();++it) {
    if(!mem_filtered && (strcmp(REG_StringShort(*it).c_str(), REG_StringShort(read_base).c_str())==0
                                || strcmp(REG_StringShort(*it).c_str(), REG_StringShort(read_index).c_str())==0)) {
      continue;
    }
    if(!mem2_filtered && (strcmp(REG_StringShort(*it).c_str(), REG_StringShort(read2_base).c_str())==0
                                || strcmp(REG_StringShort(*it).c_str(), REG_StringShort(read2_index).c_str())==0)) {
      continue;
    }
    int dep = getDep((*it));
    if(dep!=-1) {
      depList->push_back(dep);
    }
  }
  depList->sort();
  depList->unique();
  stringstream compDep;
  for(std::list<long unsigned int>::iterator it = depList->begin();it!=depList->end();++it) {
    compDep << " " << *it;
  }
  depList->clear();
  return compDep.str();
}

BOOL checkLimits(ADDRINT address) {
  if(full_sim) return true;
  bool isInLimits=false;
  if(control) { //we are in the slice limits but this could be the end point.
/*
    Point * pt = slices->front()->end;
    if(pt->symbolic){ //shared lib
      std::map<string, IMG>::iterator img = images->find(pt->lib_name);
      if (img == images->end()) return false;
      long unsigned int offset = address - IMG_LowAddress(img->second);
      if(offset == pt->offset && getCounter(address).first && pt->execount == getCounter(address).second) {
        control = false;
        trace.close();
        PSlice * slc = slices->front();
        delete slc;
        slices->pop_front();
        if(KnobDebugPrints.Value()) {
          printf("[DEBUG] Remaining Slices:\n");
          for(std::list<PSlice *>::iterator it=slices->begin();it!=slices->end();it++) {
            printf("%s\n",(*it)->dump_content().c_str());
          }
        }
        if(slices->empty()) {
          if(KnobDebugPrints.Value())
            printf("No slices left to execute.\n");
          finish();
          exit(0);
        }
      }
    }
    else { //not shared lib
      if(pt->address == address && getCounter(address).first && pt->execount == getCounter(address).second)
      {
        control = false;
        trace.close();
        PSlice * slc = slices->front();
        delete slc;
        slices->pop_front();
        if(KnobDebugPrints.Value()) {
          printf("[DEBUG] Remaining Slices:\n");
          for(std::list<PSlice *>::iterator it=slices->begin();it!=slices->end();it++) {
            printf("%s\n",(*it)->dump_content().c_str());
          }
        }
        if(slices->empty()){
          if(KnobDebugPrints.Value())
            printf("No slices left to execute.\n");
          finish();
          exit(0);
        }
      }
    }
*/
  /* 1. Check the recorded instruction count for the current interval
   *   Remove the current interval,
   *   close the file,
   *   reset the recorded instruction count
   */
    if (recorded_instr_int == KnobISize.Value()) {
      control = false;
      trace.close();
      PSlice * slc = slices->front();
      delete slc;
      slices->pop_front();
      if(KnobDebugPrints.Value()) {
        printf("[DEBUG] Remaining Slices:\n");
        for(std::list<PSlice *>::iterator it=slices->begin();it!=slices->end();it++) {
          printf("%s\n",(*it)->dump_content().c_str());
        }
      }
      if(slices->empty()){
        if(KnobDebugPrints.Value())
          printf("[DEBUG] No slices left to execute.\n");
        finish();
        exit(0);
      }
      recorded_instr_int=0;
    }
    isInLimits = true;
  }
  if(!control) //we are out of the slice limit. But the instruction may be the starting point.
  {
    if(slices->empty()) return false; //if we dont have any slices left then we won't record any more traces.
    Point * pt = slices->front()->start;
    if(pt->symbolic) { //shared lib
      std::map<string, IMG>::iterator img = images->find(pt->lib_name);
      if (img == images->end()) return false;
      long unsigned int offset =  address - IMG_LowAddress(img->second);
      if(offset == pt->offset && getCounter(address).first && pt->execount == getCounter(address).second) {
        resetDependencyWindow();
        control=true;
        if(KnobDebugPrints.Value())
          printf("[DEBUG] Started Slice: %d\n",slices->front()->slice);
        std::ostringstream tracefilename;
        tracefilename << KnobTraceFile.Value() << "." << slices->front()->slice;
        trace.open(tracefilename.str().c_str()); //open the new one for the new slice
        return true;
      }
    }
    else {
      if(pt->address == address && getCounter(address).first && pt->execount == getCounter(address).second)
      {
        control = true;
        if(KnobDebugPrints.Value())
          printf("[DEBUG] Started Slice: %d\n",slices->front()->slice);
        std::ostringstream tracefilename;
        tracefilename << KnobTraceFile.Value() << "." << slices->front()->slice;
        trace.open(tracefilename.str().c_str()); //open the new one for the new slice
        return true;
      }
    }
  }
  if(isInLimits) return true; //if this is the ending point then it should be recorded.
  bbl_cnt=0;
  return false;
}

VOID RecordInstructionFetch(ADDRINT iaddr){
  if(KnobIFEnable.Value()){
    ADDRINT phy_iaddr=iaddr;
    if (KnobPhysicalAddress.Value())
      phy_iaddr = getPhysicalAddr(iaddr);
    if(icache!=NULL){
      bool hit = icache->send(Request(phy_iaddr, Request::Type::READ),reqList);
      if(hit) icache_hit++;
    }
    else {
      reqList->push_back(Request(phy_iaddr, Request::Type::READ));
    }
    if(record) {
      num_ifetch++;
      if(reqList->empty()) filt_ifetch++;
      for (std::list<Request>::iterator it = reqList->begin(); it != reqList->end(); ++it) {
        assert((it->type == Request::Type::READ) && "Instruction fetch should not return a write.");
        if(strcmp(KnobMode.Value().c_str(),"cpu")==0) {
          if(!KnobICEnable.Value() && !KnobDCEnable.Value())
            trace << bbl_cnt << " " << it->addr << " R" << std::endl;
          else
            trace << bbl_cnt << " " << it->addr << std::endl;
          bbl_cnt=0;
        } else if (strcmp(KnobMode.Value().c_str(),"datadep")==0) {
          removeOld();
          trace << seq_number << " READ " << it->addr << std::endl;
          updateSeqNumber();
        } else
          trace << "0x"<< std::hex << it->addr << " R" << std::endl;
      }
    }
    reqList->clear();
  }
}

VOID CountTotalInst(ADDRINT address)
{
  total++;
  bbl_cnt++;
  updateCounters(address);
  record = checkLimits(address);
  if(record) {
    recorded_instr++;
    recorded_instr_int++;
  }
  mem_filtered=true;
  mem2_filtered=true;
}
//recording read/write requests to the memory
VOID RecordGeneral(VOID * ip, VOID * addr, BOOL isWrite, REG base, REG index, BOOL isfirst){
  if(record) mem_req++;
  if(isWrite) write_count++;
  else read_count++;
  UINT64 addr_req = (long)addr;
  if(KnobPhysicalAddress.Value()){
    UINT64 paddr = getPhysicalAddr((long)addr);
    addr_req = paddr;
  }
  if(first!=NULL) {
    bool hit;
    if(!isWrite)
      hit = first->send(Request(addr_req, Request::Type::READ),reqList);
    else
      hit = first->send(Request(addr_req, Request::Type::WRITE),reqList);
    if(hit && !isWrite)
      read_hit++;
    else if (hit&&isWrite)
      write_hit++;
    if(hit && reqList->empty() && record) filtered++;
  }
  else {
    if(isWrite) reqList->push_back(Request(addr_req,Request::Type::WRITE));
    else reqList->push_back(Request(addr_req,Request::Type::READ));
  }
  UINT64 r_addr=0;
  UINT64 w_addr=0;
  bool w_check = false;
  bool r_check = false;
  //checking requestlist elements added in send()
  //iterating through the list
  for (std::list<Request>::iterator it = reqList->begin(); it != reqList->end(); ++it) {
    if(it->type == Request::Type::READ) {
      r_addr = it->addr;
      //assert(r_addr);
      r_check = true;
    }
    else if( it->type == Request::Type::WRITE){
      w_addr = it->addr;
      w_check = true;
    }
  }
  //update filtered memory op check
  if (record && !reqList->empty() && strcmp(KnobMode.Value().c_str(),"datadep")==0) {
    if (isfirst)
      mem_filtered = false;
    else
      mem2_filtered = false;
  }
  //recording the requests
  if(record && (r_check || w_check)) {
    if(strcmp(KnobMode.Value().c_str(),"cpu")==0)   { //Collect CPU traces
      if(!KnobDCEnable.Value() && (!KnobICEnable.Value()|| !KnobIFEnable.Value())) { //unfiltered trace
        if(r_check) {
          trace << bbl_cnt << " " << r_addr << " R" << std::endl;
          bbl_cnt=0;
        }
        if(w_check) {
          trace << bbl_cnt << " " << w_addr << " W" << std::endl;
          bbl_cnt=0;
        }
      }
      else {
        if(!w_check && r_check) {
          trace << bbl_cnt << " " << r_addr << std::endl;
        }
        else if( w_check && r_check )
          trace << bbl_cnt << " " << r_addr << " " << w_addr << std::endl; //includes writeback address
        assert(!(w_check && !r_check) && "A write should be always with a read request.");
        bbl_cnt=0;
      }
    } else if(strcmp(KnobMode.Value().c_str(),"datadep")==0) { //Collect data dependency included CPU traces
        //fprintf(trace,"dependent to reg: base: %s, index: %s\n",REG_StringShort(base).c_str(), REG_StringShort(index).c_str());
        stringstream deplist;
        int base_seq = getDep(base);
        int index_seq = getDep(index);
        if(base_seq != -1 || index_seq != -1 ) {
          deplist << " :";
          if(base_seq != index_seq) {
            if(base_seq!=-1)  deplist << " " << base_seq;
            if(index_seq!=-1) deplist << " " << index_seq;
          }
          else {
            if( base_seq!=-1)
              deplist << " " << base_seq;
          }
        }
      //  printf("deplist:%s\n",deplist.str().c_str() );
        if(r_check) {
          removeOld();
          trace << seq_number << " READ " << r_addr;
          if(deplist.str().length()>1)
            trace << deplist.str().c_str();
          trace << std::endl;
          updateSeqNumber();
      }
      if(w_check){
        removeOld();
        trace << seq_number << " WRITE " << w_addr << " : " << (seq_number -1) << std::endl;
        updateSeqNumber();
      }

    } else { //Collect memory traces
      if(r_check)
        trace << "0x" << std::hex << r_addr << " R" << std::endl;
      if(w_check)
        trace << "0x" << std::hex << w_addr << " W" << std::endl;
    }
  }
  reqList->clear();
}

//recording COMP
VOID RecordComp(VOID * ip, REG read_base, REG read2_base, REG read_index, REG read2_index){
    if(record) {
      removeOld();
      stringstream deplist;
      if(!(mem_filtered && mem2_filtered) || compRRegs->size()>0)
      deplist << ":";
      if(!mem_filtered && mem2_filtered)
        deplist << " " << seq_number-1;
      else if(!mem2_filtered && mem_filtered)
        deplist << " " << seq_number-1;
      else if(!mem_filtered && !mem2_filtered)
        deplist << " " << seq_number-1 << " " << seq_number-2;
      deplist << getCompDependency( read_base, read2_base, read_index, read2_index);
      compRRegs->clear();
      if(deplist.str().length()>1)
        trace << seq_number << " COMP " << deplist.str().c_str() << std::endl;
      else
        trace << seq_number << " COMP" << std::endl;
      updateSeqNumber();
  }

}

VOID UpdateRegisterMap(VOID * ip, REG reg) {
  insertDependency(REG_StringShort(reg));
}
VOID UpdateCompRegs(VOID * ip, REG reg) {
  if(REG_valid(reg)) {
    compRRegs->push_back(reg);
  }
}
// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    c_total++;
    ADDRINT adr = INS_Address(ins);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountTotalInst,IARG_UINT64, adr,
      IARG_END);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordInstructionFetch, IARG_UINT64, adr, IARG_END);
    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP
    // prefixed instructions appear as predicated instructions in Pin.
    UINT32 memOperands = INS_MemoryOperandCount(ins);
    // determine which registers are used for address generation for memory accesses
    REG read_base = REG_INVALID();
    REG read_index = REG_INVALID();
    REG read2_base = REG_INVALID();
    REG read2_index = REG_INVALID();
    int read_count=0;

    if(strcmp(KnobMode.Value().c_str(),"datadep")==0) {
      UINT32 opCount = INS_OperandCount(ins);
      for(UINT32 op = 0; op < opCount; op++) {
        REG base = INS_OperandMemoryBaseReg(ins, op);
        REG index = INS_OperandMemoryIndexReg(ins, op);
        if(REG_valid(base) || REG_valid(index)) {
          assert(read_count<2 && "Memory Operands should be less than 3. right??");
          if(read_count==0) {
            read_base = base;
            read_index = index;
            read_count=1;
          }
          else if( read_count==1) {
            read2_base = base;
            read2_index = index;
            read_count++;
          }
        }
      }
      read_count=0;
    }
    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        //determine which register is used for address generation
        REG base;
        REG index;
        if(read_count==0) {
          base = read_base;
          index= read_index;
          read_count++;
        }
        else {
          index= read2_index;
          base = read2_base;
        }
        bool isfirst = (memOp==0);
        //issue recording calls for memory operations
        if (INS_MemoryOperandIsRead(ins, memOp))
        {
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordGeneral,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_BOOL, false,
                IARG_UINT32, base,
                IARG_UINT32, index,
                IARG_BOOL, isfirst,
                IARG_END);
        }
        // Note that in some architectures a single memory operand can be
        // both read and written (for instance incl (%eax) on IA-32)
        // In that case we instrument it once for read and once for write.
        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordGeneral,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_BOOL, true,
                IARG_UINT32, base,
                IARG_UINT32, index,
                IARG_BOOL, isfirst,
                IARG_END);
        }
    }
    //If instruction is not a read or write and collecting data dependency traces
    //determine which registers are a dependency (except the ones used in memory accesses)
    if(strcmp(KnobMode.Value().c_str(),"datadep")==0) {
      for(unsigned int i=0;i<INS_MaxNumRRegs(ins);i++) {
        REG reg = INS_RegR(ins,i);
        INS_InsertCall(ins,IPOINT_BEFORE, (AFUNPTR)UpdateCompRegs,
                      IARG_INST_PTR,
                      IARG_UINT32, reg,
                      IARG_END);
      }
      for(unsigned int i=0;i<INS_MaxNumWRegs(ins);i++) {
        REG reg = INS_RegW(ins,i);
        INS_InsertCall(ins,IPOINT_BEFORE, (AFUNPTR)UpdateCompRegs,
                      IARG_INST_PTR,
                      IARG_UINT32, reg,
                      IARG_END);
      }
      INS_InsertCall(ins,IPOINT_BEFORE, (AFUNPTR)RecordComp,
                    IARG_INST_PTR,
                    IARG_UINT32, read_base,
                    IARG_UINT32, read2_base,
                    IARG_UINT32, read_index,
                    IARG_UINT32, read2_index,
                    IARG_END);
      for(unsigned int i =0;i<INS_MaxNumWRegs(ins);i++) {
        REG reg = INS_RegW(ins,i);
        INS_InsertPredicatedCall(
          ins, IPOINT_BEFORE, (AFUNPTR)UpdateRegisterMap,
          IARG_INST_PTR,
          IARG_UINT32, reg,
          IARG_END);
        }
      }
}


VOID Fini(INT32 code, VOID *v)
{
    finish();
}


/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory accesses\n"
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

//returns the first slice id.
int setSliceLimits(){
  if(!KnobFastOption.Value() && strcmp(KnobCoverage.Value().c_str(),"")==0) return 0;
  Parser * parser = new Parser();
  parser->parse(KnobPinPoints.Value());
  PSlice * first = NULL;
  if(KnobFastOption.Value()) {
    first = parser->getFastPoint();
    if(first==NULL) {
      printf("Cannot find any slices, try changing the interval size.\n");
      exit(0);
    }
    slices->push_back(first);
  }
  else if (strcmp(KnobCoverage.Value().c_str(),"")!=0) {
    slices = parser->slices;
    num_slices = slices->size();
    if(num_slices==0) {
      printf("Cannot find any slices, try changing the interval size.\n");
      exit(0);
    }
    if(KnobDebugPrints.Value())
      printf("[DEBUG] Content:\n%s\n",parser->dump_content().c_str());
    first = slices->front();
  }
  initializeCounters();
  delete parser;
  return first ? first->slice : 0;
}

VOID Image(IMG img, VOID * v)
{
  images->insert(make_pair(IMG_Name(img),img));
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    if(PIN_Init(argc, argv)) return Usage();
    IMG_AddInstrumentFunction(Image, 0);
    Config cfg(KnobConfigFile.Value().c_str());
    control=false; //this is used to check if the instrumented instruction is in the slice for recording the traces.
    printf("isize: %ld\n",KnobISize.Value());
    if(KnobPhysicalAddress.Value()) {
      pagemap = fopen("/proc/self/pagemap", "rb");
    }
    if(!KnobFastOption.Value() && strcmp(KnobCoverage.Value().c_str(),"")==0) {
      full_sim=true;
    }
    int slice_id=0;
    if(!full_sim && strcmp(KnobPinPoints.Value().c_str(),"")!=0) {
      slice_id = setSliceLimits();
    }

    std::ostringstream tracefilename;
    if(!slices->empty()){
      tracefilename<<KnobTraceFile.Value() << "." << slice_id ;
    }
    else{ //if we are running the full program
      tracefilename <<KnobTraceFile.Value();
      trace.open(tracefilename.str().c_str());
    }
    std::list<CacheParams *> * c_list = cfg.get_caches();
    if(c_list->empty()) {
      first=NULL;
      icache=NULL;
    }
    else {

    Cache * cur = NULL;
    for(std::list<CacheParams *>::iterator it = c_list->begin();it!=c_list->end();it++) {
      bool isItIC = false;
      int csize = (*it)->get_size();
      int cassoc = (*it)->get_assoc();
      int cblock = (*it)->get_block_size();
      Cache::Level lvl;
      //command line arguments override the config file values.
      switch ((*it)->get_level()) {
        case 1:
        lvl = Cache::Level::L1;
        if(KnobL1Size.Value()>0) csize = 2<<KnobL1Size.Value();
        if(KnobL1Assoc.Value()>0) cassoc = 2<<KnobL1Assoc.Value();
        if(KnobL1BlockSize.Value()>0) cblock = (2<<KnobL1BlockSize.Value());
        break;
        case 2:
        lvl = Cache::Level::L2;
        if(KnobL2Size.Value()>0) csize = (2<<KnobL2Size.Value());
        if(KnobL2Assoc.Value()>0) cassoc = (2<<KnobL2Assoc.Value());
        if(KnobL2BlockSize.Value()>0) cblock = (2<<KnobL2BlockSize.Value());
        break;
        case 3:
        lvl = Cache::Level::L3;
        if(KnobL3Size.Value()>0) csize = (2<<KnobL3Size.Value());
        if(KnobL3Assoc.Value()>0) cassoc = (2<<KnobL3Assoc.Value());
        if(KnobL3BlockSize.Value()>0) cblock =(2<<KnobL3BlockSize.Value());
        break;
        case -1:
        lvl = Cache::Level::ICache;
        isItIC = true;
        if(KnobICSize.Value()>0) csize = (2<<KnobICSize.Value());
        if(KnobICAssoc.Value()>0) cassoc = (2<<KnobICAssoc.Value());
        if(KnobICBlockSize.Value()>0) cblock = (2<<KnobICBlockSize.Value());
        break;
        default:
        printf("A caches level can't be out of the range [1,3]. (-1 for ICache)\n");
        exit(0);
      }
      if(isItIC) {
        icache = new Cache(csize,cassoc,cblock,lvl);
        //icache->set_next_cache(NULL);
        //icache->concatlower(NULL);
        icache->set_last_level();
      }
      else {
        Cache * c= new Cache(csize,cassoc,cblock,lvl);
        if(cur!=NULL)  {
	         //cur->set_next_cache(c);
           cur->concatlower(c);
	      }
	      else  {
          c->set_first_level();
          first = c;
        }
        caches->push_back(c);
        cur = c;
      }
    }
    cur->set_last_level();
    }
    if(!KnobICEnable.Value()) {
      icache = NULL;
    }
    if(!KnobDCEnable.Value()) {
      first = NULL;
    }
    if(KnobDebugPrints.Value()) {
      for (auto debug : *caches) {
        printf("[DEBUG] Level: %d, next_cache: %lu, is_last_level: %d is_first_level: %d\n",debug->level, debug->higher_cache.size(),
        debug->is_llc(),debug->is_first());
      }
    }
    INS_AddInstrumentFunction(Instruction, 0);

    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}
