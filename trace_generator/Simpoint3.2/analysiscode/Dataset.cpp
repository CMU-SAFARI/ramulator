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


#include "Dataset.h"
#include "Utilities.h"

Dataset::Dataset() : vector<Datapoint>(1, Datapoint(1)), weights(1, 1) {
}

Dataset::Dataset(unsigned int numPoints, unsigned int numDimensions) : 
    vector<Datapoint>(numPoints, Datapoint(numDimensions)), 
    weights(numPoints, 1.0 / numPoints) {
}

Dataset::Dataset(const Dataset &ds) : vector<Datapoint>(ds), 
    weights(ds.weights) {
}


bool Dataset::operator==(const Dataset &other) const {
    unsigned int nrows = numRows();
    unsigned int ncols = numCols();

    if (! (nrows == other.numRows() && ncols == other.numCols())) {
        return false;
    }

    for (unsigned int i = 0; i < nrows; i++) {
        for (unsigned int j = 0; j < ncols; j++) {
            if ((*this)[i][j] != other[i][j]) {
                return false;
            }
        }
    }

    return true;
}


void Dataset::fill(double value) {
    unsigned int s = size();
    for (unsigned int i = 0; i < s; i++) {
        (*this)[i].fill(value);
    }
}


void Dataset::write(ostream &os) const {
    bool hasWeights = weights.size() > 0;
    os.precision(20);

    unsigned int nr = numRows();
    os << nr << ":" << (hasWeights ? "w\n" : "\n");

    for (unsigned int row = 0; row < nr; row++) {
        if (hasWeights) { os << weights[row] << " "; }
        (*this)[row].write(os);
        os << endl;
    }
}

void Dataset::write(FILE *out) const {
    bool hasWeights = weights.size() > 0;
    unsigned int nr = numRows();

    fprintf(out, "%u:", nr);
    if (hasWeights) { fprintf(out, "w"); }
    fprintf(out, "\n");

    for (unsigned int row = 0; row < nr; row++) {
        if (hasWeights) { fprintf(out, "%.20f ", weights[row]); }
        (*this)[row].write(out);
        fprintf(out, "\n");
    }
}

void Dataset::read(istream &is) {
    unsigned int length;
    is >> length;

    char c = is.get();
    Utilities::check(':' == c, "Dataset::read() missing : separator");

    resize(length);

    // check for weights (backwards compatibility)
    c = is.get();
    bool hasWeights = (c == 'w');
    double weight;
    if (! hasWeights) { is.putback(c); }

    for (unsigned int i = 0; i < length; i++) {
        if (hasWeights) {
            is >> weight;
            setWeight(i, weight);
        } else {
            setWeight(i, 1.0 / length);
        }
        (*this)[i].read(is);
    }
}

void Dataset::read(FILE *in) {
    unsigned int length;
    fscanf(in, "%u", &length);

    char c = fgetc(in);
    Utilities::check(':' == c, "Dataset::read() missing : separator");

    resize(length);

    // check for weights (backwards compatibility)
    c = fgetc(in);
    bool hasWeights = (c == 'w');
    double weight;
    if (! hasWeights) { ungetc(c, in); }

    for (unsigned int i = 0; i < length; i++) {
        if (hasWeights) {
            fscanf(in, "%lf", &weight);
            setWeight(i, weight);
        } else {
            setWeight(i, 1.0 / length);
        }
        (*this)[i].read(in);
    }
}

void Dataset::writeBinary(ostream &os) const {
    // write the header information... size and whether weights are included
    unsigned int length = numRows();
    os.write((char *)&length, sizeof(unsigned int));

    unsigned int hasWeights = weights.size() > 0;
    os.write((char *)&hasWeights, sizeof(unsigned int));

    // write out all the data & weights (if appropriate)
    for (unsigned int row = 0; row < length; row++) {
        if (hasWeights) { os.write((char *)&(weights[row]), sizeof(double)); }
        (*this)[row].writeBinary(os);
    }
}

void Dataset::writeBinary(FILE *out) const {
    // write the header information... size and whether weights are included
    unsigned int length = numRows();
    fwrite((void *)&length, sizeof(unsigned int), 1, out);

    unsigned int hasWeights = weights.size() > 0;
    fwrite((void *)&hasWeights, sizeof(unsigned int), 1, out);

    // write out all the data & weights (if appropriate)
    for (unsigned int row = 0; row < length; row++) {
        if (hasWeights) { fwrite((void *)&(weights[row]), sizeof(double), 1, out); }
        (*this)[row].writeBinary(out);
    }
}

void Dataset::readBinary(istream &is) {
    unsigned int length;
    is.read((char *)&length, sizeof(unsigned int));

    unsigned int hasWeights;
    is.read((char *)&hasWeights, sizeof(unsigned int));

    resize(length);

    double weight;
    for (unsigned int i = 0; i < length; i++) {
        if (hasWeights) {
            is.read((char *)&weight, sizeof(double));
            setWeight(i, weight);
        } else {
            setWeight(i, 1.0 / length);
        }
        (*this)[i].readBinary(is);
    }
}

void Dataset::readBinary(FILE *in) {
    unsigned int length;
    fread((void *)&length, sizeof(unsigned int), 1, in);

    unsigned int hasWeights;
    fread((void *)&hasWeights, sizeof(unsigned int), 1, in);

    resize(length);

    double weight;
    for (unsigned int i = 0; i < length; i++) {
        if (hasWeights) {
            fread((void *)&weight, sizeof(double), 1, in);
            setWeight(i, weight);
        } else {
            setWeight(i, 1.0 / length);
        }
        (*this)[i].readBinary(in);
    }
}

// this resize() method overrides the one in vector<Datatype> because we have
// the weights vector which must also be resized.
void Dataset::resize(unsigned int new_size, Datapoint prototype) {
    vector<Datapoint>::resize(new_size, prototype);
    weights.resize(new_size);
}

ostream &operator<<(ostream &os, const Dataset &ds) {
    unsigned int s = ds.size();
    for (unsigned int i = 0; i < s; i++) {
        os << ds[i] << endl;
    }

    return os;
}

