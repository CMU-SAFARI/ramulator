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
#include <iostream>

#include "pin.H"
#include "instlib.H"
#include "bimodal.H"

#define MAXPP 10
LOCALVAR BIMODAL bimodal;

using namespace INSTLIB; 





class PPSTAT{
    public:
        // FIXME: Avoiding floating point arithmatic in analysis routines
        // double ppWeight;
        UINT32 ppWeightTimesThousand;
        UINT64 ppMispredicts;
        UINT64 ppInstructions;
};

class PPINFO
{
    public:
        UINT64 startIcount, startMispredicts;
        UINT32 currentpp;
        PPSTAT ppstats[MAXPP+1];
};

LOCALVAR PPINFO ppinfo;
LOCALVAR ofstream *outfile;


using namespace INSTLIB;

// Track the number of instructions executed
ICOUNT icount;

// Contains knobs and instrumentation to recognize start/stop points
CONTROL control;
/* ===================================================================== */

VOID Handler(CONTROL_EVENT ev, VOID * v, CONTEXT * ctxt, VOID * ip, THREADID tid)
{
    std::cout << "ip: " << ip << " Instructions: "  << icount.Count() << " ";

    switch(ev)
    {
      case CONTROL_START:
        std::cout << "Start" << endl;
        ppinfo.startMispredicts = bimodal.Mispredicts();
        ppinfo.startIcount = icount.Count();
        if(control.PinPointsActive())
        {
            std::cout << "PinPoint: " << control.CurrentPp() << endl;
            UINT32 pp = control.CurrentPp();
            ASSERTX( pp <= MAXPP);
            ppinfo.ppstats[pp].ppWeightTimesThousand = control.CurrentPpWeightTimesThousand();
            ppinfo.currentpp = pp; 
        }
        break;

      case CONTROL_STOP:
        std::cout << "Stop" << endl;
        if(control.PinPointsActive())
        {
            std::cout << "PinPoint: " << control.CurrentPp() << endl;
            UINT64 mispredicts = bimodal.Mispredicts() - ppinfo.startMispredicts;
            UINT64 instructions = icount.Count() - ppinfo.startIcount;
    
            UINT32 pp = ppinfo.currentpp;
            ppinfo.ppstats[pp].ppMispredicts = mispredicts;
            ppinfo.ppstats[pp].ppInstructions = instructions;
        }
        break;

      default:
        ASSERTX(false);
        break;
    }
}
    
INT32 Usage()
{
    cerr <<
        "This pin tool is a simple branch predictor \n"
        "\n";

    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

LOCALFUN VOID Fini(int n, void *v)
{
    *outfile << endl;
    double whole_MPKI = 1000.0 * (double)bimodal.Mispredicts()/icount.Count();
    *outfile << "Whole-program MPKI = " << whole_MPKI << dec << endl;
    if (control.PinPointsActive())
    {
        UINT32 NumPp = control.NumPp();
        double predicted_MPKI = 0.0;
        *outfile << "PP #," << " %Weight," << " MPKI" << endl;
        for (UINT32 p = 1; p <= NumPp ; p++)
        {
            double  weight = (double) ppinfo.ppstats[p].ppWeightTimesThousand/1000.0;
            double  mpki = (double)ppinfo.ppstats[p].ppMispredicts*1000/ppinfo.ppstats[p].ppInstructions;
            *outfile << dec << p << ", "  << weight << ", " << mpki << endl;
            predicted_MPKI +=  (double) weight*mpki/100.0;
        }
        *outfile << "Predicted MPKI = " << predicted_MPKI << dec << endl;
    }
}

int main(int argc, char *argv[])
{
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    icount.Activate();
    bimodal.Activate();


    // Activate alarm, must be done before PIN_StartProgram
    control.CheckKnobs(Handler, 0);


    outfile = new ofstream("bimodal.out");

    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
}
