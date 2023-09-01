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


#ifndef FV_PARSER_H
#define FV_PARSER_H

/***********************************************************************
 * File: FVParser.h
 * Author: Greg Hamerly
 * Date: 8/20/2002
 *
 * FVParser is used to parse frequency vector files. Frequency vector files
 * have a format of one vector per line, with each valid line beginning with
 * the letter 'T', and each entry having the form
 *   :x:y
 * where x is the dimension, and y is the value.
 *
 * The key method here is nextLine(), which retrieves a list of
 * FVParserTokens from the next line in the file. Each FVParserToken has
 * a dimension and a value (x and y from the above example).
 *
 * The FVParser may be initialized from an existing input stream (which
 * the caller is responsible for deallocating) or from a file name (for
 * which the stream's memory is handled internally).
 ***********************************************************************/

#include <list>
#include <cstdio>

using namespace std;

/*
 * FVParserToken represents a dimension/value pair.
 */
class FVParserToken {
    public:
        int dimension;
        double value;

        FVParserToken() : dimension(0), value(0) {}
        FVParserToken(int dim, double val) : dimension(dim), value(val) {}
};

/*
 * FVParser is used to parse frequency vector files.
 */
class FVParser {
    private:
        FILE *input; // a pointer to the input stream to read from
        int lineNumber; // the current line number in the stream

    public:
        // Construct a FVParser from a FILE *; caller should close the FILE *
        FVParser(FILE *input_file);

        // Destructor
        ~FVParser() {}

        // Parses the next line out of the file, returns a vector of
        // dimension/value pairs for the entire line through the "result"
        // parameter. Returns true if the file has not been exhausted,
        // false otherwise. If this method returns false, the caller
        // should not use "result". 
        bool nextLine(list<FVParserToken> *result);

        // Gets the current line number for the stream.
        int currentLineNumber() const { return lineNumber; }

        // Checks for the end-of-file status.
        bool eof() const { return feof(input); }
};

#endif

