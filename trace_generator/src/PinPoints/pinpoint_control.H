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
#ifndef PINPOINT_CONTROL_H
#define PINPOINT_CONTROL_H
#include "pinpoint_alarm.H"
#include "portability.H"

class CONTROL_PINPOINT;

class CONTROL_PINPOINT_T
{
    friend class CONTROL_PINPOINT;

    private: // only the friend above needs to see this
    CONTROL_PINPOINT_T(CONTROL_PINPOINT * master, THREADID watchThread, ofstream *outstreamp)
        : 
        _master(master),
    _watchThread(watchThread),
    _outstreamp(outstreamp)
    {
        _currentPp = 0;
        _currentPhase = 0;
        _lastPp = 0;
        _icount.Activate();
        _version = 2; // by default, the pp-file is from version 2 of
                      // SimPoint
    }


    class MARKREF
    {
        public:
            bool _start;
            INT32 _markNo;
            UINT64 _count;
            UINT64 _skew;
            INT32 _ppNo;
            INT32 _phaseNo;
            CONTROL_PINPOINT_T *_pp;
    };

    class PPREGION
    {
        public:
            INT32 _ppNo;
            INT32 _sliceNo;
            INT32 _warmup_factor;
            INT32 _phaseNo;
            UINT64 _DistanceTimesMillion;
            vector<MARKREF> _starts, _stops;
            double _weight;
            UINT32 _weightTimesThousand;
            UINT32 _numStarts;
            UINT32 _numStops;
            UINT64 _length;
            UINT64 _icountFromStart;
    };



    VOID ReadPPFileT(istream &pp, UINT32 & lineNum)
    {
        // regions are 1 based so put a dummy at 0
        _regions.push_back(PPREGION());

        PPREGION * region = 0;
        while(true)
        {
            istringstream line( ReadLine(pp, lineNum) );

            if( pp.eof() )
            {
                break;
            }

            string command;
            line >> command;

            if ("markedInstrs" == command)
            {
                line >> _numMarks;

                // Marks are 1 based, so put a dummy at 0
                _marks = new PINPOINT_ADDRESS_COUNT_ALARM[_numMarks+1];
#if defined(TARGET_IA32) || defined(TARGET_IA32E)
                for(unsigned int i=0;i<_numMarks+1;i++)
                {
                    _marks[i].setPassContext(true);
                }
#endif
            }
            else if ("pinpoints" == command)
            {
            }
            else if ("mark" == command)
            {
                UINT32 index;
                line >> index;

                ASSERTX(index <= _numMarks);

                UINT64 address;
                line >> hex >> address >> dec;
                _marks[index].Activate(address, _watchThread);
            }
            else if ("mark-symbolic" == command)
            {
                UINT32 index;
                line >> index;

                ASSERTX(index <= _numMarks);

                UINT64 address;
                line >> hex >> address >> dec;
                _marks[index].Activate(address, _watchThread);
            }
            else if ("region" == command)
            {
                _regions.push_back(PPREGION());

                region = & _regions.back();

                line >> region->_ppNo;
                line >> region->_weight;
                line >> region->_numStarts;
                line >> region->_numStops;
                line >> region->_length;
                line >> region->_icountFromStart;
                region->_numStarts=1;
                region->_numStops=1;
                region->_warmup_factor=0;
                region->_weightTimesThousand = (UINT32)(region->_weight*1000.0);

                ASSERTX(_regions.size() == static_cast<UINT32>(region->_ppNo + 1));
            }
            else if ("slice" == command)
            {
                region = & _regions.back();
                double distance;

                line >> region->_sliceNo;
                line >> region->_phaseNo;
                line >>  distance; 
                region->_DistanceTimesMillion= (UINT64)(distance*1000000.0);
            }
            else if ("warmup_factor" == command)
            {
                region = & _regions.back();
                line >> region->_warmup_factor;
            }
            else if ("start" == command)
            {
                MARKREF m;

                line >> m._markNo;
                line >> m._count;
                line >> m._skew;
                m._start = true;
                m._pp = this;
                m._ppNo = region->_ppNo;
                m._phaseNo = region->_phaseNo;

                if(region->_starts.empty())
                {
                    region->_starts.push_back(m);
                }
            }
            else if ("endp" == command)
            {
                return;
            }
            else if ("totalIcount" == command)
            {
                UINT64 num;
                line >> num;
            }
            else if ("version" == command)
            {
                line >> _version;
            }
            else if ("end" == command)
            {
                MARKREF m;

                line >> m._markNo;
                line >> m._count;
                line >> m._skew;
                m._start = false;
                m._pp = this;
                m._ppNo = region->_ppNo;

                if(region->_stops.empty())
                {
                    region->_stops.push_back(m);
                }
            }
            else
            {
                PpFileError(lineNum);
                cerr << "at token \"" << command << "\"" << endl;
                exit(1);
            }
        }

        PpFileError(lineNum);
        cerr << "No endp" << endl;
        exit(1);
    }


    static string ReadLine(istream& inputFile, UINT32 &lineNum)
    {
        CHAR buf[4096];
        do
        {

            inputFile.getline(buf, sizeof(buf), '\n');
            lineNum++;
            if(!inputFile.eof())ASSERTX(!inputFile.fail()); // Check for buf overflow
        }
        while( !inputFile.eof() && (buf[0] == '#' || buf[0] == 0 ));

        // workaround for some library problems
        return string(buf);
    }

    static VOID PpFileError(INT32 line)
    {
        cerr << "Error in pinpoint file at line " << line << ": ";
    }

    VOID SetAlarms(UINT64 threadid)
    {
        for (UINT32 r = 1; r < _regions.size(); r++)
        {
            PPREGION & region = _regions[r];

            for (UINT32 start = 0; start < region._numStarts; start++)
            {
                MARKREF & ref = region._starts[start];
                PINPOINT_ADDRESS_COUNT_ALARM & al = _marks[ref._markNo];

                // In the future and earlier than current alarm
                // if (ref._count > al.Count() || ref._count < al.Count())
                if (ref._count > al.Count(threadid)
                        && (al.Alarm(threadid) == PINPOINT_ADDRESS_COUNT_ALARM::NEVER
                            || ref._count < al.Alarm(threadid))
                   )
                {
                    al.SetAlarm(ref._count, ProcessAddressCountAlarm, &ref, threadid);
                }
            }

            for (UINT32 end = 0; end < region._numStops; end++)
            {
                MARKREF & ref = region._stops[end];
                PINPOINT_ADDRESS_COUNT_ALARM & al = _marks[ref._markNo];

                // In the future and earlier than current alarm
                if (ref._count > al.Count(threadid)
                        && (al.Alarm(threadid) == PINPOINT_ADDRESS_COUNT_ALARM::NEVER
                            || ref._count < al.Alarm(threadid))
                   )
                {
                    al.SetAlarm(ref._count, ProcessAddressCountAlarm, &ref, threadid);
                }
            }
        }
    }

    VOID OutOfOrder(MARKREF * ref)
    {
        cerr << "pid: " << getpid_portable() << " ERROR: Pinpoint markers are out of order, current pp " << _currentPp
            << " last pp " << _lastPp << " mark ref " << *ref << endl;
        cerr << *this;
        exit(-1);
    }
    static VOID ProcessAddressCountAlarm(VOID *v, CONTEXT * ctxt, VOID * ip, THREADID tid)
    {
        MARKREF *ref = static_cast<MARKREF*>(v);
        CONTROL_PINPOINT_T *pp = ref->_pp;


        ASSERTX(tid == pp->_watchThread);

        ref->_pp->CheckAlarm(ref, ctxt, ip);
        ref->_pp->SetAlarms(tid);
    }

    inline VOID CheckAlarm(MARKREF * ref, CONTEXT * ctxt, VOID * ip);

    const CONTROL_PINPOINT * _master;
    const THREADID _watchThread;
    ofstream *_outstreamp;
    UINT32 _numMarks;
    vector<PPREGION> _regions;
    PINPOINT_ADDRESS_COUNT_ALARM *_marks;
    INT32 _currentPp;
    INT32 _currentPhase;
    INT32 _lastPp;
    // Track the number of instructions executed
    ICOUNT _icount;
    UINT64 _currentPpStartIcount;
    UINT32 _version;

    friend ostream& operator<<(ostream& o, PPREGION &region)
    {
        // o << "weight " << region._weight
        o << "weightTimesThousand " << region._weightTimesThousand
            << " starts " << dec << region._numStarts
            << " stops " << dec << region._numStops
            << " length " << dec << region._length
            << " count from start " << dec << region._icountFromStart
            << endl;

        o << "Starts" << endl;
        for (UINT32 i = 0; i < region._numStarts; i++)
        {
            o << dec << region._starts[i] << endl;
        }

        o << "Stops" << endl;
        for (UINT32 i = 0; i < region._numStops; i++)
        {
            o << dec << region._stops[i] << endl;
        }

        return o;
    }

    friend ostream& operator<<(ostream& o, MARKREF &mr)
    {
        o << "Mark " << dec << mr._markNo << " " << mr._pp->_marks[mr._markNo]
            << " skew " << dec << mr._skew
            << " alarm count " << dec << mr._count;

        return o;
    }


    friend ostream& operator<<(ostream& o, CONTROL_PINPOINT_T &pp)
    {
        o << "WatchThread " << pp._watchThread << endl;

        for (UINT32 r = 1; r < pp._regions.size(); r++)
        {
            o << "Region " << r << " " << pp._regions[r];
        }

        o << "Mark State" << endl;
        for (UINT32 i = 1; i <= pp._numMarks; i++)
        {
            o << i << " " << pp._marks[i] << endl;
        }

        return o;
    }
};

/*! @defgroup CONTROLLER_PINPOINT
  @ingroup CONTROLLER
  Controller for  pinpoints
  Use -ppfile filename
*/

/*! @ingroup CONTROLLER_PINPOINT
*/
class CONTROL_PINPOINT
{
    friend class CONTROL_PINPOINT_T;

    public:
    CONTROL_PINPOINT(BOOL passContext=false, const string & prefix = "",
                     const string& knob_family = "pintool:control")
        : _passContext(passContext),
          _ppFileKnob(KNOB_MODE_WRITEONCE, knob_family, "ppfile", "", "PinPoint file", prefix),
          _ppSelectKnob(KNOB_MODE_WRITEONCE, knob_family, "ppnum", "-1", "Pinpoint number to trace", prefix),
          _ppDeSelectKnob(KNOB_MODE_WRITEONCE, knob_family, "skip_ppnum", "-1", "Pinpoint number to skip", prefix),
          _ppSelectThread(KNOB_MODE_WRITEONCE, knob_family, "pptid", "-1", "Pinpoint thread to focus", prefix)
    {
        _valid = true;
        _maxThreads = ISIMPOINT_MAX_THREADS;
    }
    /*! @ingroup CONTROLLER_PINPOINT
      Activate the controller if the -ppfile knob is provided
      @return 1 if controller can start an interval, otherwise 0
    */
    INT32 CheckKnobs(CONTROL_HANDLER ch, VOID * val, ofstream * outstreamp)
    {
        if (strcmp(_ppFileKnob.Value().c_str(),"") == 0)
        {
            return 0;
        }
        _active = true;
        _outstreamp = outstreamp;

        ReadPPFile();

        _controlHandler = ch;
        _controlVal = val;
        return 1;
    }
    bool IsActive() const { return _active; };
    size_t NumPp(THREADID threadid = 0) const { ASSERTX(_active && (threadid < _maxThreads) && ThreadActive(threadid)); return _threads[threadid]->_regions.size() - 1; };

    UINT32 PP_Phase(UINT32 pp, THREADID threadid = 0) const { ASSERTX(_active && (threadid < _maxThreads) && ThreadActive(threadid)); return _threads[threadid]->_regions[pp]._phaseNo; };
    UINT32 PP_Slice(UINT32 pp, THREADID threadid = 0) const { ASSERTX( _active && (threadid < _maxThreads) && ThreadActive(threadid)); return _threads[threadid]->_regions[pp]._sliceNo; };
    UINT32 PP_WarmupFactor(UINT32 pp, THREADID threadid = 0) const { ASSERTX( _active && (threadid < _maxThreads) && ThreadActive(threadid)); return _threads[threadid]->_regions[pp]._warmup_factor; };
    // FIXME: Avoiding floating point arithmatic in analysis routine.
    // double CurrentPpWeight(UINT64 threadid = 0) const { ASSERTX(threadid < _maxThreads && ThreadActive(threadid)); return _threads[threadid]->_regions[CurrentPp(threadid)]._weight; };
    UINT32 PP_WeightTimesThousand(UINT32 pp, THREADID threadid = 0) const { ASSERTX(_active && (threadid < _maxThreads) && ThreadActive(threadid)); return _threads[threadid]->_regions[pp]._weightTimesThousand; };
    UINT64 PP_Length(UINT32 pp, THREADID threadid = 0) const { ASSERTX(_active && (threadid < _maxThreads) && ThreadActive(threadid)); return _threads[threadid]->_regions[pp]._length; };

    UINT32 CurrentPp(THREADID threadid = 0) const { ASSERTX(_active && (threadid < _maxThreads) && ThreadActive(threadid)); return _threads[threadid]->_currentPp; };
    UINT32 PP_Version(THREADID threadid = 0) const { ASSERTX( _active && (threadid < _maxThreads) && ThreadActive(threadid)); return _threads[threadid]->_version; };
    UINT32 CurrentPhase(THREADID threadid = 0) const { ASSERTX( _active && (threadid < _maxThreads) && ThreadActive(threadid)); return _threads[threadid]->_currentPhase; };
    // FIXME: Avoiding floating point arithmatic in analysis routine.
    // double CurrentPpWeight(UINT64 threadid = 0) const { ASSERTX(threadid < _maxThreads && ThreadActive(threadid)); return _threads[threadid]->_regions[CurrentPp(threadid)]._weight; };
    UINT32 CurrentPpWeightTimesThousand(THREADID threadid = 0) const { ASSERTX( _active && (threadid < _maxThreads) && ThreadActive(threadid)); return _threads[threadid]->_regions[CurrentPp(threadid)]._weightTimesThousand; };
    UINT64 CurrentPpLength(THREADID threadid = 0) const { ASSERTX( _active && (threadid < _maxThreads) && ThreadActive(threadid)); return _threads[threadid]->_regions[CurrentPp(threadid)]._length; };
    UINT64 CurrentPpStartIcount(THREADID threadid = 0) const { ASSERTX(_active && (threadid < _maxThreads) && ThreadActive(threadid)); return _threads[threadid]->_regions[CurrentPp(threadid)]._icountFromStart; };

    private:
    BOOL _passContext;
    ofstream *_outstreamp;
    bool _valid;
    bool ThreadActive(THREADID threadid = 0) const { return _threads[threadid] != NULL; };
    VOID ReadPPFile()
    {
        string filename = _ppFileKnob.Value().c_str();

        ifstream pp(filename.c_str());
        if (!pp.is_open())
        {
            cerr << "Could not open pinpoint file " << _ppFileKnob.Value().c_str() << endl;
            exit(-1);
        }

        _threads = new CONTROL_PINPOINT_T*[_maxThreads];
        memset(_threads, 0, _maxThreads * sizeof(_threads[0]));

        UINT32 lineNum = 0;
        while(true)
        {
            istringstream line( CONTROL_PINPOINT_T::ReadLine(pp, lineNum) );

            if( pp.eof() )
            {
                break;
            }

            string command;
            line >> command;

            if (command == "thread")
            {
                THREADID threadid;
                line >> threadid;

                if (threadid >= _maxThreads)
                {
                    CONTROL_PINPOINT_T::PpFileError(lineNum);
                    cerr << "threadid is " << threadid << " but maximum thread id allowed is " << _maxThreads-1 << endl;
                    exit(1);
                }

                _threads[threadid] = new CONTROL_PINPOINT_T(this, threadid, _outstreamp);
                _threads[threadid]->ReadPPFileT(pp, lineNum);
                _threads[threadid]->SetAlarms(threadid);
            }
            else
            {
                CONTROL_PINPOINT_T::PpFileError(lineNum);
                cerr << "at token \"" << command << "\"" << endl;
                exit(1);
            }
        }
    }

    KNOB<string> _ppFileKnob;
    KNOB<INT32> _ppSelectKnob;
    KNOB<INT32> _ppDeSelectKnob;
    KNOB<INT32> _ppSelectThread ;
    CONTROL_HANDLER _controlHandler;
    VOID * _controlVal;
    bool _active;
    THREADID _maxThreads;
    CONTROL_PINPOINT_T ** _threads;



    friend ostream& operator<<(ostream& o, CONTROL_PINPOINT &pp)
    {
        for (UINT32 t = 0; t < pp._maxThreads; t++)
        {
            if (pp._threads[t])
                o << *pp._threads[t];
        }

        return o;
    }
};


VOID CONTROL_PINPOINT_T::CheckAlarm(MARKREF * ref, CONTEXT * ctxt, VOID * ip)
{
    if (!ref->_pp->_master->_valid) return;
    if (ref->_start)
    {
        if (_currentPp == ref->_ppNo)
        {
            // Pinpoint is already started
            return;
        }
        else if (_currentPp == 0)
        {
            if (_lastPp + 1 != ref->_ppNo)
            {
                cerr << "START:(_currentPp==0)&&(_lastPp + 1 != ref->_ppNo): _lastPp:" << _lastPp << " ref->_ppNo:" << ref->_ppNo << endl;
                OutOfOrder(ref);
            }

            // Starting a pinpoint
            _currentPp = ref->_ppNo;
            _currentPhase = ref->_phaseNo;
            _currentPpStartIcount  = _icount.Count(_watchThread);
            if(_master->_ppDeSelectKnob.Value() != _currentPp)
            {
                if ((_master->_ppSelectKnob.Value() == -1
                     || _master->_ppSelectKnob.Value() == _currentPp) &&
                    (_master->_ppSelectThread.Value() == -1
                     || static_cast<THREADID>(_master->_ppSelectThread.Value()) == _watchThread ) )
                {
                    if (_outstreamp)
                        *_outstreamp << endl << "Starting (main loop) PinPoint " << _currentPp << " Actual_start " << dec << _currentPpStartIcount  << "  " << _regions[_currentPp] << endl;
             
                    _master->_controlHandler(CONTROL_START, _master->_controlVal, ctxt, ip, _watchThread);
                }
            }

            // This could happen with warmup
            // Now see if any new pinpoint is starting here
            for (UINT32 r = 1; r < _regions.size(); r++)
            {
                PPREGION & region = _regions[r];

                for (UINT32 start = 0; start < region._numStarts; start++)
                {
                    MARKREF & localref = region._starts[start];
                    // Same time as the current alarm
                    if ((ref->_count == localref._count ) && (localref._ppNo > _currentPp) && (ref->_markNo == localref._markNo ))
                    {
                        if(_master->_ppDeSelectKnob.Value() != localref._ppNo)
                        {
                            if ((_master->_ppSelectKnob.Value() == -1
                                || _master->_ppSelectKnob.Value() == localref._ppNo) && 
                                (_master->_ppSelectThread.Value() == -1
                                 || static_cast<THREADID>(_master->_ppSelectThread.Value()) == _watchThread ))
                            {
                                INT32 tmpPp = _currentPp;
                                INT32 tmpPhase = _currentPhase;
                                UINT64 tmpIcount = _currentPpStartIcount;
                                // Set these  momentorily so that the handler sees the right info.
                                _currentPp = localref._ppNo;
                                _currentPhase = localref._phaseNo;
                                _currentPpStartIcount  = _icount.Count(_watchThread);
                                if (_outstreamp)
                                    *_outstreamp << endl << "Starting (aux loop) PinPoint " << _currentPp << " Actual_start " << dec << _currentPpStartIcount << " " << _regions[_currentPp] << endl;
                                _master->_controlHandler(CONTROL_START, _master->_controlVal, ctxt, ip, _watchThread);
                                _currentPp = tmpPp;
                                _currentPhase = tmpPhase;
                                _currentPpStartIcount  = tmpIcount;
                            }
                        }
                    }
                }
            }
            return;
        }
        else
        {
            cerr << "START:(_currentPp==" << _currentPp << ")" << "(_lastPp==" << _lastPp << " ref->_ppNo:" << ref->_ppNo << ")" <<  endl;
            OutOfOrder(ref);
        }
    }
    else
    {
        if (_currentPp == ref->_ppNo)
        {
            // Stopping a pinpoint
            if(_master->_ppDeSelectKnob.Value() != _currentPp)
            {
                // Last pinpoint
                if ((_master->_ppSelectKnob.Value() == -1
                    || _master->_ppSelectKnob.Value() == ref->_ppNo) &&
                    (_master->_ppSelectThread.Value() == -1
                     || static_cast<THREADID>(_master->_ppSelectThread.Value()) == _watchThread ))
                {
                    if (ref->_ppNo == _regions.back()._ppNo)
                    {
                        if (_outstreamp)
                            *_outstreamp << endl << "Stopping PinPoint " << ref->_ppNo << " Actual_length " << dec <<  (_icount.Count(_watchThread) - _currentPpStartIcount) << endl;
                        _master->_controlHandler(CONTROL_STOP, _master->_controlVal, ctxt, ip, _watchThread);
                    }
                    else
                    {
                        if (_outstreamp)
                            *_outstreamp << endl << "Stopping PinPoint " << ref->_ppNo << " Actual_length " << dec <<  (_icount.Count(_watchThread) - _currentPpStartIcount) << endl;
                        _master->_controlHandler(CONTROL_STOP, _master->_controlVal, ctxt, ip, _watchThread);
                    }
                }
            }
            _lastPp = 0;
            // This could happen with warmup
            // Now see if any other pinpoint is stopping here
            for (UINT32 r = 1; r < _regions.size(); r++)
            {
                PPREGION & region = _regions[r];

                for (UINT32 stop = 0; stop < region._numStops; stop++)
                {
                    MARKREF & localref = region._stops[stop];
                    // Same time as the current alarm
                    if ((ref->_count == localref._count ) && (localref._ppNo > _currentPp) && (ref->_markNo == localref._markNo ))
                    {
                        // Stopping a pinpoint
                        if(_master->_ppDeSelectKnob.Value() != localref._ppNo)
                        {
                            if ((_master->_ppSelectKnob.Value() == -1
                                 || _master->_ppSelectKnob.Value() == localref._ppNo)  &&
                                (_master->_ppSelectThread.Value() == -1
                                 || static_cast<THREADID>(_master->_ppSelectThread.Value()) == _watchThread ))
                            {
                                // Set these  momentorily.
                                _currentPp = localref._ppNo;
                                _currentPhase = localref._phaseNo;
                                if (_outstreamp)
                                    *_outstreamp << endl << "Stopping PinPoint with another" << localref._ppNo << endl;
                                _master->_controlHandler(CONTROL_STOP, _master->_controlVal, ctxt, ip, _watchThread);
                            }
                        }
                        // Set _lastPp even if the handler call was skipped
                        _lastPp = localref._ppNo;
                    }
                }
            }
            _currentPpStartIcount  = 0;
            if(_lastPp==0)_lastPp = ref->_ppNo;
            _currentPp = 0;

            // Now see if any new pinpoint is starting here
            for (UINT32 r = 1; r < _regions.size(); r++)
            {
                PPREGION & region = _regions[r];

                for (UINT32 start = 0; start < region._numStarts; start++)
                {
                    MARKREF & localref = region._starts[start];
                    // Same time as the current alarm
                    if ((ref->_count == localref._count ) && (ref->_markNo == localref._markNo ))
                    {
                        // Starting a pinpoint
                        _currentPp = localref._ppNo;
                        _currentPhase = localref._phaseNo;
                        _currentPpStartIcount  = _icount.Count(_watchThread);
                        if(_master->_ppDeSelectKnob.Value() != _currentPp)
                        {
                            if ((_master->_ppSelectKnob.Value() == -1
                                || _master->_ppSelectKnob.Value() == _currentPp) &&
                                (_master->_ppSelectThread.Value() == -1
                                 || static_cast<THREADID>(_master->_ppSelectThread.Value()) == _watchThread ))
                            {
                                if (_outstreamp)
                                    *_outstreamp << endl << "Starting (on stop) PinPoint " << _currentPp << " Actual_start " << dec << _currentPpStartIcount << " " << _regions[_currentPp] << endl;
                                _master->_controlHandler(CONTROL_START, _master->_controlVal, ctxt, ip, _watchThread);

                            }
                        }
                    }
                }
            }
            return;
        }
        else if (_currentPp == 0 && _lastPp == ref->_ppNo)
        {
            // slice is over, this is an additional marker
            return;
        }
        else 
        {
            cerr << "STOP: (_currentPp==" << _currentPp << ")" << "(_lastPp==" << _lastPp << " ref->_ppNo:" << ref->_ppNo <<  ")" << endl;
            OutOfOrder(ref);
        }
    }
}
#endif
