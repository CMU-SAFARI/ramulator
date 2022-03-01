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


#include "Datapoint.h"
#include "Utilities.h"
#include <cstring>
#include <string>
#include <algorithm>

// take care of a difference between G++ 2.96 and 3.x
#if (__GNUC__ >= 3)
    #include <sstream>
#else
    #include <strstream>
#endif

Datapoint::Datapoint() : vector<double>() {
    fill(0.0);
}

Datapoint::Datapoint(unsigned int length) : vector<double>(length) {
    fill(0.0);
}

Datapoint::Datapoint(const Datapoint &dp) : vector<double>(dp) {
    // does nothing... parent constructor handles everything
}

double Datapoint::distSquared(const Datapoint &dp) const {
    double dist = 0.0;
    unsigned int s = size();
    for (unsigned int i = 0; i < s; i++) {
        dist += ((*this)[i] - dp[i]) * ((*this)[i] - dp[i]);
    }

    return dist;
}


void Datapoint::fill(double value) {
    unsigned int s = size();
    for (unsigned int i = 0; i < s; i++) {
        (*this)[i] = value;
    }
}


int Datapoint::maxNdx(int start, int end, double *value) const {
    start = start < 0 ? 0 : start;
    if (end == -1) { end = size(); }
    else { end = (end >= (int)size()) ? size() : end; }

    if (size() <= 0 || start >= end || start >= (int)size()) {
        return -1;
    }

    int ndx = start;
    double max = (*this)[start];

    for (unsigned int i = start + 1; i < (unsigned int)end; i++) {
        if ((*this)[i] > max) {
            max = (*this)[i];
            ndx = i;
        }
    }

    if (value) { *value = max; }

    return ndx;
}


Datapoint &Datapoint::operator+=(const Datapoint &dp) {
    unsigned int s = size();
    for (unsigned int i = 0; i < s; i++) {
        (*this)[i] += dp[i];
    }

    return *this;
}

Datapoint &Datapoint::operator/=(double d) {
    unsigned int s = size();
    for (unsigned int i = 0; i < s; i++) {
        (*this)[i] /= d;
    }

    return *this;
}

void Datapoint::multAndAdd(const Datapoint &dp, double d) {
    unsigned int s = size();
    for (unsigned int i = 0; i < s; i++) {
        (*this)[i] += (dp[i] * d);
    }
}



Datapoint Datapoint::operator-(const Datapoint &dp) const {
    Datapoint toReturn(*this);
    unsigned int s = size();

    for (unsigned int i = 0; i < s; i++) {
        toReturn[i] -= dp[i];
    }

    return toReturn;
}


void Datapoint::write(ostream &os) const {
    unsigned int s = size();
    os << s << ": ";

    for (unsigned int i = 0; i < s; i++) {
        os << (*this)[i] << " ";
    }
}

void Datapoint::write(FILE *out) const {
    unsigned int s = size();
    fprintf(out, "%u: ", s);

    for (unsigned int i = 0; i < s; i++) {
        fprintf(out, "%.20f ", (*this)[i]);
    }
}

void Datapoint::read(istream &is) {
    unsigned int length;
    is >> length;

    char c = is.get();
    Utilities::check(':' == c, "Datapoint::read() missing : separator");

    resize(length);

    for (unsigned int i = 0; i < length; i++) {
        is >> (*this)[i];
    }
}

void Datapoint::read(FILE *in) {
    unsigned int length;
    fscanf(in, "%u", &length);

    char c = fgetc(in);
    Utilities::check(':' == c, "Datapoint::read() missing : separator");

    resize(length);

    for (unsigned int i = 0; i < length; i++) {
        fscanf(in, "%lf", &(*this)[i]);
    }
}


void Datapoint::writeBinary(ostream &os) const {
    unsigned int s = size();
    os.write((const char *)&s, sizeof(unsigned int));
    Datapoint::const_iterator b = this->begin();
    os.write((const char *)&(*b), size() * sizeof(double));
}

void Datapoint::writeBinary(FILE *out) const {
    unsigned int s = size();
    fwrite((void *)&s, sizeof(unsigned int), 1, out);
    Datapoint::const_iterator b = this->begin();
    fwrite((void *)&(*b), sizeof(double), size(), out);
}

void Datapoint::readBinary(istream &is) {
    unsigned int length;
    is.read((char *)&length, sizeof(unsigned int));
    resize(length);
    Datapoint::iterator b = this->begin();
    is.read((char *)&(*b), length * sizeof(double));
}

void Datapoint::readBinary(FILE *in) {
    unsigned int length;
    fread((void *)&length, sizeof(unsigned int), 1, in);
    resize(length);
    Datapoint::iterator b = this->begin();
    fread((void *)&(*b), sizeof(double), length, in);
}


ostream &operator<<(ostream &os, const Datapoint &dp) {
    unsigned int s = dp.size();
    for (unsigned int i = 0; i < s; i++) {
        os.precision(20);
        os << dp[i] << "\t";
    }
    return os;
}


