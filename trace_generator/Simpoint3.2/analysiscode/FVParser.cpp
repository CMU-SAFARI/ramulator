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


/***********************************************************************
 * File: FVParser.cpp
 * Author: Greg Hamerly
 * Date: 8/20/2002
 ***********************************************************************/

#include "FVParser.h"
#include "Utilities.h"
#include <string>

// take care of a difference between G++ 2.96 and 3.x
#if (__GNUC__ >= 3)
    #include <sstream>
#else
    #include <strstream>
#endif


FVParser::FVParser(FILE *input_file) {
    Utilities::check(input_file != NULL, 
                     "FVParser::FVParser() input_file is NULL");
    input = input_file;
    lineNumber = 0;
}


bool FVParser::nextLine(list<FVParserToken> *result) {
    string line;
    const int BUF_SIZE = 1024 * 1024;
    char buffer[BUF_SIZE];
    buffer[0] = '\0';
    do {
        fgets(buffer, BUF_SIZE, input);
    } while ((! eof()) && ((buffer[0] == '\0') || ('T' != buffer[0]))); // change by shen
    //std::cout << "buffer size " << sizeof(*buffer) << std::endl;
    Utilities::check(sizeof(*buffer) != BUF_SIZE,
            "FVParser::nextLine() lines are too long for buffer");

    if (eof()) { return false; }

#if (__GNUC__ >= 3)
    istringstream parser(buffer);
#else
    istrstream parser(buffer, strlen(buffer));
#endif

    char t;
    parser >> t;

    result->clear();

    char colon;
    int dimension;
    double value;
    while (parser >> colon >> dimension >> colon >> value) {
        result->push_back(FVParserToken(dimension, value));
    }

    if (result->size() > 0) {
        lineNumber++;
        return true;
    } else {
        return false;
    }
}

