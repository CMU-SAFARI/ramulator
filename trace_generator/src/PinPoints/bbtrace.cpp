/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2012 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/* ===================================================================== */
/*
  @ORIGINAL_AUTHOR: Robert Muth
*/

/* ===================================================================== */
/*! @file
 *  This file contains an ISA-portable PIN tool for counting dynamic instructions
 */

#include <map>
#include <iostream>
#include <fstream>
#include <string.h>

#include "pin.H"
#include "instlib.H"

#define MAX_THREADS 8
#define MAX_IMAGES 250

using namespace INSTLIB;

class IMG_INFO
{
    public:
        IMG_INFO(IMG img);
        INT32 Id() { return _imgId;}
        CHAR * Name() { return _name;}
        ADDRINT  LowAddress() { return _low_address;}
        static INT32 _currentImgId;
    private:
        CHAR * _name; 
        ADDRINT _low_address; 
        // static members
        INT32 _imgId;
};

IMG_INFO * img_info[MAX_IMAGES]; 

IMG_INFO::IMG_INFO(IMG img)
    : _imgId(_currentImgId)
{
    _name = (CHAR *) calloc(strlen(IMG_Name(img).c_str())+1, 1);
    strcpy(_name,IMG_Name(img).c_str());
    _low_address = IMG_LowAddress(img);
}

UINT32 FindImgInfoId(IMG img)
{
    if (!IMG_Valid(img))
        return 0;
    
    ADDRINT low_address = IMG_LowAddress(img);
    
    for (UINT32 i = IMG_INFO::_currentImgId - 1; i >=1; i--)
    {
        if(img_info[i]->LowAddress() == low_address)
            return i;
    }
    // cerr << "FindImgInfoId(0x" << hex << low_address << ")" <<   endl;
    return 0;
}

class BLOCK_KEY
{
    friend bool operator<(const BLOCK_KEY & p1, const BLOCK_KEY & p2);
        
  public:
    BLOCK_KEY(ADDRINT s, ADDRINT e, USIZE z) : _start(s),_end(e),_size(z) {};
    BOOL IsPoint() const { return (_start - _end) == 1;  }
    ADDRINT Start() const { return _start; }
    ADDRINT End() const { return _end; }
    USIZE Size() const { return _size; }
    BOOL Contains(ADDRINT addr) const;
    
  private:
    const ADDRINT _start;
    const ADDRINT _end;
    const USIZE _size;
};

class BLOCK
{
  public:
    BLOCK(const BLOCK_KEY & key, INT32 instructionCount, IMG i);
    INT32 StaticInstructionCount() const { return _staticInstructionCount; }
    VOID Execute(THREADID tid) { _globalBlockCount[tid]++; }
    VOID EmitProgramEnd(const BLOCK_KEY & key, THREADID tid) const;
    INT64 GlobalBlockCount(THREADID tid) const { return _globalBlockCount[tid] + _sliceBlockCount[tid]; }
    UINT32 ImgId() const { return _imgId; }
    const BLOCK_KEY & Key() const { return _key; }
    INT32 Id() const { return _id; }
    
  private:

    // static members
    static INT32 _currentId;

    const INT32 _staticInstructionCount;
    const INT32 _id;
    const BLOCK_KEY _key;

    INT32 _sliceBlockCount[MAX_THREADS];
    INT64 _globalBlockCount[MAX_THREADS];
    UINT32 _imgId;
};


LOCALTYPE typedef pair<BLOCK_KEY, BLOCK*> BLOCK_PAIR;
LOCALTYPE typedef multimap<BLOCK_KEY, BLOCK*> BLOCK_MAP;
LOCALVAR UINT32 Pid;

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

LOCALVAR BLOCK_MAP block_map;
LOCALVAR string commandLine;

class PROFILE
{
    public: 
    PROFILE()
    {
        first = true;
        GlobalInstructionCount = 0;
    }
    ofstream BbFile;
    INT64 GlobalInstructionCount;
    // The first time, we want a marker, but no T vector
    BOOL first;
};

// LOCALVAR INT32 maxThread = 0;
LOCALVAR PROFILE ** profiles;
// LOCALVAR INT32 firstPid = 0;


INT32 BLOCK::_currentId = 1;
INT32 IMG_INFO::_currentImgId = 1;


/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "out", "specify bb file name");
KNOB<BOOL>  KnobNoSymbolic(KNOB_MODE_WRITEONCE,  "pintool",
    "nosymbolic", "0", "Do not emit symbolic information for markers");
KNOB<BOOL>  KnobPid (KNOB_MODE_WRITEONCE,  "pintool",
    "pid", "0", "Use PID for naming files.");


/* ===================================================================== */

INT32 Usage()
{
    cerr <<
        "This tool collects profiles for SimPoint.\n"
        "\n";


    cerr << KNOB_BASE::StringKnobSummary();

    cerr << endl;

    return -1;
}



bool operator<(const BLOCK_KEY & p1, const BLOCK_KEY & p2)
{
    if (p1.IsPoint())
        return p1._start < p2._start;

    if (p2.IsPoint())
        return p1._end <= p2._start;
    
    if (p1._start == p2._start)
        return p1._end < p2._end;
    
    return p1._start < p2._start;
}

BOOL BLOCK_KEY::Contains(ADDRINT address) const
{
    if (address >= _start && address <= _end)
        return true;
    else
        return false;
}


VOID CountBlock(BLOCK * block, THREADID tid)
{
    block->Execute(tid);
    profiles[tid]->BbFile << dec << block->Id() <<  dec << endl;
}

BLOCK::BLOCK(const BLOCK_KEY & key, INT32 instructionCount, IMG img)
    :
    _staticInstructionCount(instructionCount),
    _id(_currentId++),
    _key(key)
{
    _imgId = FindImgInfoId(img);
    for (THREADID tid = 0; tid < MAX_THREADS; tid++)
    {
        _sliceBlockCount[tid] = 0;
        _globalBlockCount[tid] = 0;
    }
}

LOCALFUN BLOCK * LookupBlock(BBL bbl)
{
    BLOCK_KEY key(INS_Address(BBL_InsHead(bbl)), INS_Address(BBL_InsTail(bbl)), BBL_Size(bbl));
    BLOCK_MAP::const_iterator bi = block_map.find(key);
    
    if (bi == block_map.end())
    {
        // Block not there, add it
        RTN rtn = INS_Rtn(BBL_InsHead(bbl));
        SEC sec = SEC_Invalid();
        IMG img = IMG_Invalid();
        if(RTN_Valid(rtn))
            sec = RTN_Sec(rtn);
        if(SEC_Valid(sec))
            img = SEC_Img(sec);
        BLOCK * block = new BLOCK(key, BBL_NumIns(bbl), img);
        block_map.insert(BLOCK_PAIR(key, block));

        return block;
    }
    else
    {
        return bi->second;
    }
}

/* ===================================================================== */

VOID Trace(TRACE trace, VOID *v)
{
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        BLOCK * block = LookupBlock(bbl);
        INS_InsertCall(BBL_InsTail(bbl), IPOINT_BEFORE, (AFUNPTR)CountBlock, IARG_PTR, block, IARG_THREAD_ID, IARG_END);
    }
}

/* ===================================================================== */
LOCALFUN VOID OpenFile(THREADID tid)
{
    char num[100];
    if (KnobPid)
        sprintf(num, ".T.%u.%d.bbtrace", (unsigned)Pid, (int)tid);
    else
        sprintf(num, ".T.%d.bbtrace", (int)tid);
    string tname = num;
    profiles[tid]->BbFile.open((KnobOutputFile.Value()+tname).c_str());
    profiles[tid]->BbFile.setf(ios::showbase);
}
/* ===================================================================== */

VOID Image(IMG img, VOID * v)
{
    if(!profiles[0]->BbFile.is_open())
    {
        OpenFile(0);
    }
    ASSERTX(IMG_INFO::_currentImgId < (MAX_IMAGES - 1));
    img_info[IMG_INFO::_currentImgId] = new IMG_INFO(img);
    IMG_INFO::_currentImgId++;
    profiles[0]->BbFile << "G: " << IMG_Name(img) << " LowAddress: " << hex  << IMG_LowAddress(img) << " LoadOffset: " << hex << IMG_LoadOffset(img) << endl;
}


VOID BLOCK::EmitProgramEnd(const BLOCK_KEY & key, THREADID tid) const
{
    if (_globalBlockCount[tid] == 0)
        return;
    
    profiles[tid]->GlobalInstructionCount += (_globalBlockCount[tid] * _staticInstructionCount);
    profiles[tid]->BbFile << "Block id: " << dec << _id << " " << hex << key.Start() << ":" << key.End() << dec
           << " static instructions: " << _staticInstructionCount
           << " block count: " << _globalBlockCount[tid]
           << " block size: " << key.Size()
           << endl;
}

LOCALFUN VOID EmitProgramEnd(THREADID tid)
{
    for (BLOCK_MAP::const_iterator bi = block_map.begin(); bi != block_map.end(); bi++)
    {
        bi->second->EmitProgramEnd(bi->first, tid);
    }
    profiles[tid]->BbFile << "Dynamic instruction count " << dec << profiles[tid]->GlobalInstructionCount << endl;
}

/* ===================================================================== */

VOID Fini(INT32 code, VOID *v)
{
}

VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    ASSERTX(tid < MAX_THREADS);
    if(!profiles[tid]->BbFile.is_open())
    {
        OpenFile(tid);
    }
}

VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
        ASSERTX(profiles[threadid]->BbFile.is_open());
        EmitProgramEnd(threadid);
        profiles[threadid]->BbFile << "End of bb" << endl;
        profiles[threadid]->BbFile.close();
}


VOID GetCommand(int argc, char *argv[])
{
    for (INT32 i = 0; i < argc; i++)
    {
            commandLine += " ";
            commandLine += argv[i];
    }
}
/* ===================================================================== */

int main(int argc, char *argv[])
{
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    GetCommand(argc, argv);

    //maxThread = MaxThreadsKnob.ValueInt64();
    profiles = new PROFILE*[MAX_THREADS];
    memset(profiles, 0, MAX_THREADS * sizeof(profiles[0]));

    Pid = getpid_portable();

    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);


    for (THREADID tid = 0; tid < MAX_THREADS; tid++)
    {
        profiles[tid] = new PROFILE();
    }


#if defined(TARGET_MAC)
    // On Mac, ImageLoad() works only after we call PIN_InitSymbols().
    PIN_InitSymbols();
#endif

    TRACE_AddInstrumentFunction(Trace, 0);
    IMG_AddInstrumentFunction(Image, 0);

    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
