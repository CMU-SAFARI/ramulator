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


#include "Utilities.h"

string toString(int i) {
    char buf[128];
    sprintf(buf, "%d", i);
    return string(buf);
}

string toString(double d) {
    char buf[128];
    sprintf(buf, "%g", d);
    return string(buf);
}


void Utilities::sizeOfFVFile(FVParser &parser, int *numPoints,
                             int *numDims) {
    check(numPoints && numDims,
          "Utilities::sizeOfFVFile() return values are null");

    *numPoints = 0;
    *numDims = 0;

    list<FVParserToken> tokens;
    while (parser.nextLine(&tokens)) {
        for (list<FVParserToken>::iterator i = tokens.begin(); i != tokens.end(); i++) {
            if (i->dimension > *numDims) {
                *numDims = i->dimension;
            }
        }
    }

    *numPoints = parser.currentLineNumber();
}


void Utilities::randomProjectionMatrix(int randSeed, Dataset *projection) {
    check(NULL != projection, "Utilities::randomProjectionMatrix() projection is null");
    unsigned int rows = projection->size();
    check(rows > 0, "Utilities::randomProjectionMatrix() rows <= 0");
    unsigned int cols = (*projection)[0].size();
    check(cols > 0, "Utilities::randomProjectionMatrix() cols <= 0");

    Random rand(randSeed);

    for (unsigned int r = 0; r < rows; r++) {
        for (unsigned int c = 0; c < cols; c++) {
            (*projection)[r][c] = rand.randFloat() * 2.0 - 1.0;
        }
    }
}


void Utilities::loadFVFile(FVParser &parser,
        Dataset *result) {
    check(NULL != result, "Utilities::loadFVFile() result is null");

    list<FVParserToken> current_vector;
    double totalWeight = 0.0;
    vector<double> rowWeights(result->numRows());

    int largestDimensionSeen = 0;

    while (parser.nextLine(&current_vector)) {
        unsigned int point = parser.currentLineNumber() - 1;
        if (point >= result->numRows()) {
            check(false, "Utilities::loadFVFile() more vectors than expected "
                    "(loading vector " + toString((int)(point + 1)) + " when expecting "
                    + toString((int)(result->numRows())) + ")");
        }

        // normalize the vector
        double sumVector = 0.0;
        list<FVParserToken>::iterator i;
        for (i = current_vector.begin(); i != current_vector.end(); i++) {
            sumVector += i->value;
            if (i->dimension > largestDimensionSeen) { largestDimensionSeen = i->dimension; }
        }

        rowWeights[point] = sumVector;
        totalWeight += sumVector;

        for (i = current_vector.begin(); i != current_vector.end(); i++) {
            if (i->dimension > (int)result->numCols() || i->dimension < 1) {
                check(i->dimension <= (int)result->numCols(),
                    "Utilities::loadFVFile() expecting only " +
                    toString((int)result->numCols()) +
                    " dimensions, but found dimension " +
                    toString((int)i->dimension));
                check(i->dimension >= 1,
                    "Utilities::loadFVFile() dimension < 1");
            }
            // put the value in the destination dataset (switching to 0-offset
            // and normalizing by the total vector sum)
            (*result)[point][i->dimension - 1] = i->value / sumVector;
        }
    }
    check(parser.currentLineNumber() == (int)result->numRows(),
            "Utilities::loadFVFile() the number of vectors loaded "
            "disagrees with the number specified (expected " +
            toString((int)result->numRows()) + " but loaded " +
            toString(parser.currentLineNumber()) + ")");

    check(largestDimensionSeen == (int)result->numCols(),
            "Utilities::loadFVFile() the number of dimensions loaded "
            "disagrees with the number specified (expected " +
            toString((int)result->numCols()) + " but loaded " +
            toString((int)largestDimensionSeen) + ")");

    // normalize the weights and put them in the dataset
    for (unsigned int row = 0; row < result->numRows(); row++) {
        result->setWeight(row, rowWeights[row] / totalWeight);
    }
}

void Utilities::loadAndProjectFVFile(FVParser &parser,
                                     const Dataset &projection,
                                     Dataset *result) {
    check(NULL != result, "Utilities::loadAndProjectFVFile() result is null");
    check(projection.numCols() == (*result).numCols(),
            "Utilities::loadAndProjectFVFile() dimensions are wrong");

    result->fill(0.0);

    list<FVParserToken> current_vector;
    double totalWeight = 0.0;
    vector<double> rowWeights(result->numRows());
    int numProjectedColumns = projection.numCols();
    int numProjectedRows = projection.numRows();
    int largestDimensionSeen = 0;

    while (parser.nextLine(&current_vector)) {
        int point = parser.currentLineNumber() - 1;
        if (point >= (int)result->numRows()) {
            check(false, "Utilities::loadAndProjectFVFile() more vectors than expected "
                    "(loading vector " + toString(point + 1) + " when expecting "
                    + toString((int)result->numRows()) + ")");
        }

        // normalize the vector
        double sumVector = 0.0;
        list<FVParserToken>::iterator i;
        for (i = current_vector.begin(); i != current_vector.end(); i++) {
            sumVector += i->value;
        }

        rowWeights[point] = sumVector;
        totalWeight += sumVector;

        // multiply the row we just pulled from the parser by *each column*
        // of the projection matrix to obtain the row in the result.

        // fill the resulting vector with zeros
        (*result)[point].fill(0.0);

        // over all original dimensions (columns of the original vector, rows of
        // the projection matrix)
        for (i = current_vector.begin(); i != current_vector.end(); i++) {
            int dim = i->dimension;
            double val = i->value / sumVector;
            if (dim > largestDimensionSeen) { largestDimensionSeen = dim; }
            if (dim < 1 || dim > numProjectedRows) {
                check(dim >= 1, "Utilities::loadAndProjectFVFile() dimension < 1");
                check(dim <= numProjectedRows,
                    "Utilities::loadAndProjectFVFile() expecting only " +
                    toString((int)numProjectedRows) +
                    " dimensions, but found dimension " +
                    toString((int)dim));
            }

            // project the row
            for (int projCol = 0; projCol < numProjectedColumns; projCol++) {
                // note that dimension is changed to be offset 0
                (*result)[point][projCol] += val * projection[dim - 1][projCol];
            }
        }
    }
    check(parser.currentLineNumber() == (int)result->numRows(),
            "Utilities::loadAndProjectFVFile() the number of vectors loaded "
            "disagrees with the number specified (expected " +
            toString((int)result->numRows()) + " but loaded " +
            toString((int)parser.currentLineNumber()) + ")");

    check(largestDimensionSeen == (int)numProjectedRows,
            "Utilities::loadAndProjectFVFile() the number of dimensions loaded "
            "disagrees with the number specified (expected " +
            toString((int)numProjectedRows) + " but loaded " +
            toString((int)largestDimensionSeen) + ")");

    // normalize the weights and put them in the dataset
    for (unsigned int row = 0; row < result->numRows(); row++) {
        result->setWeight(row, rowWeights[row] / totalWeight);
    }
}

// This code is adapted from ran2 on page 282 from "Numerical Recipes in C"
// http://www.library.cornell.edu/nr/bookcpdf/c7-1.pdf

#define IM1 2147483563
#define IM2 2147483399
#define AM (1.0/IM1)
#define IMM1 (IM1-1)
#define IA1 40014
#define IA2 40692
#define IQ1 53668
#define IQ2 52774
#define IR1 12211
#define IR2 3791
#define NTAB 32
#define NDIV (1+IMM1/NTAB)
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

float Random::randFloat() {
    int j;
    long k;
    float temp;

    if (state <= 0) {
        if (-(state) < 1) state=1;
        else state = -(state);
        idum2=(state);
        for (j=NTAB+7;j>=0;j--) {
            k=(state)/IQ1;
            state=IA1*(state-k*IQ1)-k*IR1;
            if (state < 0) state += IM1;
            if (j < NTAB) iv[j] = state;
        }
        iy=iv[0];
    }
    k=(state)/IQ1;
    state=IA1*(state-k*IQ1)-k*IR1;
    if (state < 0) state += IM1;
    k=idum2/IQ2;
    idum2=IA2*(idum2-k*IQ2)-k*IR2;
    if (idum2 < 0) idum2 += IM2;
    j=iy/NDIV;
    iy=iv[j]-idum2;
    iv[j] = state;
    if (iy < 1) iy += IMM1;
    if ((temp=AM*iy) > RNMX) return RNMX;
    else return temp;
}
