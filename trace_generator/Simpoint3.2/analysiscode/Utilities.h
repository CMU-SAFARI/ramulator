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


#ifndef UTILITIES_H
#define UTILITIES_H

/***********************************************************************
 * File: Utilities.h
 * Author: Greg Hamerly
 * Date: 8/20/2002
 *
 * This class contains generally useful functions for dealing with frequency
 * vector files and checking argument validity.
 ***********************************************************************/

#include "FVParser.h"
#include "Dataset.h"
#include "Logger.h"
#include <string>
// add by shen
#include <climits>
#include <fstream>
#include <cstdlib>

string toString(int i);
string toString(double d);

class Utilities {
    public:
        /* This function parses the provided file counts the number of
         * points and the maximum dimension listed. Their values are
         * returned in the out parameters numPoints and numDims.
         */
        static void sizeOfFVFile(FVParser &parser, int *numPoints,
                                 int *numDims);

        /* This function fills a Dataset (which is the random projection
         * matrix) with uniformly random values between -1.0 and 1.0.
         * The parameter should already have the correct dimensions
         * (d1xd2, where d1 is the dimensionality of the domain space,
         * and d2 is the dimensionality of the range space).
         */
        static void randomProjectionMatrix(int randSeed, Dataset *projection);

        /* This function reads a FV file (provided through the parser)
         * and parses it into its values, multiplies the results by the
         * projection matrix (on the fly), and stores the results in the
         * out parameter result. All Datasets must be pre-allocated.
         */
        static void loadAndProjectFVFile(FVParser &parser,
                const Dataset &projection, Dataset *result);

        /* This function reads a FV file (provided through the parser)
         * and parses it into its values and stores the values in the
         * out parameter result. The resulting Dataset must be pre-allocated.
         */
        static void loadFVFile(FVParser &parser, Dataset *result);


        /* A run-time assertion checker that will print the message and
         * quit the program if the value of checkVal is not true.
         */
        static inline void check(bool checkval, const string &msg) {
            if (! checkval) {
                Logger::log() << "\nError: " << msg << endl;
                exit(1);
            }
        }
        // add by shen
        static inline void check(std::ofstream & checkval, const string &msg) {
            if (!checkval.good()) {
                Logger::log() << "\nError: " << msg << endl;
                exit(1);
            }
        }

        static inline void check(std::ifstream & checkval, const string &msg) {
            if (!checkval.good()) {
                Logger::log() << "\nError: " << msg << endl;
                exit(1);
            }
        }
};


/* To achieve the same behavior from the random number generator, we implement our own
 * random number generator rather than rely on the system's random number generator.
 * The randFloat method implementation is taken from ran2 in "Numerical Recipes
 * in C" -- see http://www.library.cornell.edu/nr/bookcpdf/c7-1.pdf
 */

#define NTAB 32 // used below to define the random number state table

class Random {
    public:
        /* Initialize the random number generator with a random seed. This
         * particular random number generator requires the first call to use a
         * negative number (or zero), which signals that the function should
         * initialize its tables, so we force any positive seed values to be
         * negative.
         */
        Random(long seed = 0) {
            state = seed > 1 ? -seed : seed;
            // these are the initial values used by Numerical Recipes
            idum2 = 123456789;
            iy = 0;
        }

        /* This function returns a value between 0.0 and 1.0. It is the same as
         * the function ran2 from Numerical Recipes */
        float randFloat();

        /* This function returns a value between 0 and INT_MAX */
        inline int randInt() { return (int)(randFloat() * INT_MAX); }

    private:
        // state is the random number state
        long state;

        // These three variables were static variables in the ran2 function,
        // but we make them data members instead.
        long idum2, iy;
        long iv[NTAB];
};

#endif
