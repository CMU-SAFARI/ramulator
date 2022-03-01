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
#ifndef PINPOINT_ALARM_H
#define PINPOINT_ALARM_H

//#define USE_COND_INST


typedef VOID (*ALARM_HANDLER)(VOID *v, CONTEXT * ctxt, VOID *, THREADID tid);

class PINPOINT_ADDRESS_COUNT_ALARM
{
  inline friend ostream& operator<<(ostream& o, PINPOINT_ADDRESS_COUNT_ALARM & a)
    {
        o.setf(ios::showbase);
        o << hex << "Address: " << a._address << endl;
        o << (IMG_Valid(a._img)?IMG_Name(a._img):"InvalidImage") << " LowAddress: " << hex  << (IMG_Valid(a._img)?IMG_LowAddress(a._img):1) << " LoadOffset: " << hex << (IMG_Valid(a._img)?IMG_LoadOffset(a._img):0) << endl;
        for (INT64 i = 0; i < a._maxThreads; i++)
        {
            o << dec << "  threadid: " << i << " count: " << a.Count(i) << " alarm: ";
            if (a.Alarm(i) == PINPOINT_ADDRESS_COUNT_ALARM::NEVER)
                o << "off" << endl;
            else
                o << a.Alarm(i) << endl;
        }
     
        return o;
    }
  public:
    enum { NEVER = ~0ULL };
    
    PINPOINT_ADDRESS_COUNT_ALARM(BOOL passContext=false)
        : _passContext(passContext)
    {
        memset(this, 0, sizeof(*this));
    }
    VOID setPassContext(BOOL passContext)
    {
        _passContext = passContext;
    }

    VOID Activate(UINT64 address, THREADID watchThread)
    {
        _address = address;
        _active = true;

        _watchThread = watchThread;
        _maxThreads = ISIMPOINT_MAX_THREADS; 
        ASSERTX((INT32)_watchThread < _maxThreads);


        _counts = new UINT64[_maxThreads];
        memset(_counts, 0, sizeof(_counts[0]) * _maxThreads);
    
        _alarms = new UINT64[_maxThreads];
        for(INT32 i = 0; i < _maxThreads; i++)
        {
            _alarms[i] = NEVER;
        }

        _handlers = new ALARM_HANDLER[_maxThreads];
        memset(_handlers, 0, sizeof(_handlers[0]) * _maxThreads);

        _vs = new VOID*[_maxThreads];
        memset(_vs, 0, sizeof(_vs[0]) * _maxThreads);

        TRACE_AddInstrumentFunction(Trace, this);
    }
    bool IsActive() const { return _active; };
    
    UINT64 Address() const { return _address; };

    UINT64 Count(UINT64 threadid = 0) const { return _counts[threadid]; };
    VOID SetCount(UINT64 count, UINT64 threadid = 0) { _counts[threadid] = count; };


    VOID SetAlarm(UINT64 alarm, ALARM_HANDLER handler, VOID *v = 0, UINT64 threadid = 0)
    {
        ASSERTX(_counts[threadid] < alarm);
        _alarms[threadid] = alarm;
        _handlers[threadid] = handler;
        _vs[threadid] = v;
    }
    VOID Clear(UINT64 threadid = 0) { _alarms[threadid] = NEVER; };

    UINT64 Alarm(UINT64 threadid = 0) const { return _alarms[threadid]; };
    
  private:
    BOOL _passContext;
    INT32 _maxThreads;
    THREADID _watchThread;
    UINT64 _address;
    UINT64* _counts;
    ALARM_HANDLER* _handlers;
    UINT64* _alarms;
    VOID **_vs;
    bool _active;
    IMG _img;


    static VOID Trace(TRACE trace, VOID * val)
    {
        PINPOINT_ADDRESS_COUNT_ALARM * al = static_cast<PINPOINT_ADDRESS_COUNT_ALARM*>(val);

        for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
        {
            for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
            {

                if (INS_Address(ins) == al->_address)
                {
                    RTN rtn = INS_Rtn(ins);
                    SEC sec = SEC_Invalid();
                    IMG img = IMG_Invalid();
                    if(RTN_Valid(rtn)) 
                        sec = RTN_Sec(rtn);
                    if(SEC_Valid(sec))
                        img = SEC_Img(sec);

                    al->_img = img;

#ifdef USE_COND_INST
                    // Use conditional instrumentation to trigger partial inlining
                    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)doIf, IARG_THREAD_ID,
                             IARG_ADDRINT, al,
                             IARG_ADDRINT, &al->_counts[al->_watchThread],
                             IARG_ADDRINT, &al->_alarms[al->_watchThread],
                             IARG_END);
                    if (al->_passContext)
                    {
                        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)doThen,
                                   IARG_ADDRINT, al,
                                   IARG_CONTEXT,
                                   IARG_INST_PTR,
                                   IARG_ADDRINT, al->_watchThread,
                                   IARG_END);
                    }
                    else 
                    {
                        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)doThen,
                                   IARG_ADDRINT, al,
                                   IARG_ADDRINT, static_cast<ADDRINT>(0), // PASS A NULL
                                   IARG_INST_PTR,
                                   IARG_ADDRINT, al->_watchThread,
                                   IARG_END);
                    }
#else
                    if (al->_passContext) 
                    {
                        INS_InsertCall(ins, IPOINT_BEFORE, 
                               (AFUNPTR)docountTat,
                               IARG_THREAD_ID,
                               IARG_ADDRINT, al,
                               IARG_CONTEXT,
                               IARG_INST_PTR,
                               IARG_ADDRINT, al->_watchThread,
                               IARG_END);
                    }
                    else
                    {
                        INS_InsertCall(ins, IPOINT_BEFORE, 
                               (AFUNPTR)docountTat,
                               IARG_THREAD_ID,
                               IARG_ADDRINT, al,
                               IARG_ADDRINT, static_cast<ADDRINT>(0), // PASS A NULL
                               IARG_INST_PTR,
                               IARG_ADDRINT, al->_watchThread,
                               IARG_END);
                    }
#endif            
                }
            }
        }
    }

#ifdef USE_COND_INST
    static ADDRINT doIf(THREADID threadid, PINPOINT_ADDRESS_COUNT_ALARM *a, UINT64* watchedCount, UINT64* watchedAlarm)
    {
        a->_counts[threadid]++;

        // Originally is "return a->_counts[watchThread] == a->_alarms[watchThread]"
        // Here watchedCount is set to &a->_counts[watchThread] and watchedThread to &a->_alarms[watchThread]
        // at instrumentation time.
        
        return (*watchedCount == *watchedAlarm);
        
    }
    static VOID doThen(PINPOINT_ADDRESS_COUNT_ALARM *a, CONTEXT * ctxt, VOID *ip, INT32 watchThread)
    {
        //UINT64 alarm = a->_alarms[threadid];
        a->Clear(watchThread);
        a->_handlers[watchThread](a->_vs[watchThread], ctxt, ip, a->_watchThread);
    }
#else    
    // docountTat == Thread at a time
    // We have an alarm per (address,thread) combination.
    // However, each alarm has _maxThreads elements in _counts.
    // This function gets called for each thread and we increment
    // _counts for that thread. However, we only trigger hanlder
    // for the watchThread for the alarm passed in.
    // All this to avoid the comparison (threadid == watchThread)
    // for easier partial inlining.
    static VOID docountTat(THREADID threadid, PINPOINT_ADDRESS_COUNT_ALARM *a, CONTEXT * ctxt, VOID *ip, INT32 watchThread)
    {
        // We have a uniq alarm per (address, thread) combination
        // hence passing threadid 0 to always access the _counts[0]
        // docountT(threadid, watchThread, a, ctxt, ip);
        a->_counts[threadid]++;
        if (a->_counts[watchThread] == a->_alarms[watchThread])
        {
            //UINT64 alarm = a->_alarms[threadid];
            a->Clear(watchThread);
            a->_handlers[watchThread](a->_vs[watchThread], ctxt, ip, a->_watchThread);
        }
    }
#endif
};
#endif
