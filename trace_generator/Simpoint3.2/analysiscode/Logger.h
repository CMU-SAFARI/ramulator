/***********************************************************************
 *  __________________________________________________________________
 *
 *              _____ _           ____        _       __
 *             / ___/(_)___ ___  / __ \____  (_)___  / /_
 *             \__ \/ / __ `__ \/ /_/ / __ \/ / __ \/ __/
 *            ___/ / / / / / / / ____/ /_/ / / / / / /_
 *           /____/_/_/ /_/ /_/_/    \____/_/_/ /_/\__/
 *
 *  __________________________________________________________________
 *
 * This file is part of the SimPoint Toolkit written by Greg Hamerly,
 * Erez Perelman, Jeremy Lau, Tim Sherwood, and Brad Calder as part of
 * Efficient Simulation Project at UCSD.  If you find this toolkit useful please
 * cite the following paper published at ASPLOS 2002.
 *
 *  Timothy Sherwood, Erez Perelman, Greg Hamerly and Brad Calder,
 *  Automatically Characterizing Large Scale Program Behavior , In the
 *  10th International Conference on Architectural Support for Programming
 *  Languages and Operating Systems, October 2002.
 *
 * Contact info:
 *        Brad Calder <calder@cs.ucsd.edu>, (858) 822 - 1619
 *        Greg Hamerly <Greg_Hamerly@baylor.edu>,
 *        Erez Perelman <eperelma@cs.ucsd.edu>,
 *        Jeremy Lau <jl@cs.ucsd.edu>,
 *        Tim Sherwood <sherwood@cs.ucsb.edu>
 *
 *        University of California, San Diego
 *        Department of Computer Science and Engineering
 *        9500 Gilman Drive, Dept 0114
 *        La Jolla CA 92093-0114 USA
 *
 *
 * Copyright 2001, 2002, 2003, 2004, 2005 The Regents of the University of
 * California All Rights Reserved
 *
 * Permission to use, copy, modify and distribute any part of this
 * SimPoint Toolkit for educational, non-profit, and industry research
 * purposes, without fee, and without a written agreement is hereby
 * granted, provided that the above copyright notice, this paragraph and
 * the following four paragraphs appear in all copies and every modified
 * file.   
 *
 * Permission is not granted to include SimPoint into a commercial product.
 * Those desiring to incorporate this SimPoint Toolkit into commercial
 * products should contact the Technology Transfer Office, University of
 * California, San Diego, 9500 Gilman Drive, La Jolla, CA 92093-0910, Ph:
 * (619) 534-5815, FAX: (619) 534-7345.
 *
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,
 * INCLUDING LOST PROFITS, ARISING OUT OF THE USE OF THE SimPoint
 * Toolkit, EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THE SimPoint Toolkit PROVIDED HEREIN IS ON AN "AS IS" BASIS, AND THE
 * UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO PROVIDE MAINTENANCE,
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS. THE UNIVERSITY OF
 * CALIFORNIA MAKES NO REPRESENTATIONS AND EXTENDS NO WARRANTIES OF ANY
 * KIND, EITHER IMPLIED OR EXPRESS, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR
 * PURPOSE, OR THAT THE USE OF THE SimPoint Toolkit WILL NOT INFRINGE ANY
 * PATENT, TRADEMARK OR OTHER RIGHTS.
 *
 * No non-profit user may place any restrictions on the use of this
 * software, including as modified by the user, by any other authorized
 * user.
 *
 ************************************************************************/


#ifndef LOGGER_H
#define LOGGER_H

/***********************************************************************
 * File: Logger.h
 * Author: Greg Hamerly
 * Date: 5/31/2005
 *
 * The Logger class is a singleton that is useful for fine-tuning the amount
 * of output (level of verbosity) from a program.
 ***********************************************************************/

#include <iostream>

using namespace std;

/* A NullStreamBuf is used in the NullStream class; it should produce
 * no output.
 */
class NullStreamBuf : public streambuf {};

/* A NullStream is simply a sink -- you can write anything to it and nothing
 * gets printed anywhere. This is to facilitate different levels of verbosity
 * when printing output.
 */
class NullStream : public ostream {
    public:
        NullStream() : ostream(new NullStreamBuf) {
            nsb = (NullStreamBuf *)rdbuf();
        }
        virtual ~NullStream() { if (nsb) delete nsb; nsb = NULL; }

    private:
        NullStreamBuf *nsb;
};

/* A Logger class is a singleton which keeps track of the level of verbosity
 * that should be presented by a program. The level of verbosity is represented
 * by an integer. The ostream can be accessed by:
 *      Logger::log() << "message goes here";
 * or
 *      Logger::log(level) << "message goes here";
 * Depending on the argument to log(), either a real output stream (i.e. cout),
 * or a sink (i.e. NullStream) will be returned.
 */
class Logger {
    public:
        // the central function for logging -- returns the appropriate stream
        // based on the given level and the current logging level
        static ostream &log(int level = 0) { return singleton.logInternal(level); }

        // sets the logging level (any logs that are below the given level will
        // be printed)
        static void setLoggingLevel(int level) { singleton.loggingLevel = level; }

    private:
        Logger() : loggingLevel(0), normalStream(&cout), nullStream(new NullStream) {}
        Logger(const Logger &);

        // object instance function
        ostream &logInternal(int level) {
            return (level <= loggingLevel) ? *normalStream : *nullStream;
        }

        // the Logger is a singleton pattern; only one Logger per program
        static Logger singleton;

        // the logging level defines how verbose a program will be
        int loggingLevel;

        // the two ostreams that can be returned depending on the logging level
        // (normalStream is real (cout), the other is just a NullStream)
        ostream *normalStream, *nullStream;
};

#endif

