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


#include "KMeans.h"
#include "Utilities.h"
#include "Logger.h"
#include <cfloat>
#include <cmath>

void KMeans::initializeRandomly(int randSeed, const Dataset &data,
        Dataset *centers) {
    Random rand(randSeed);

    for (unsigned int i = 0; i < centers->numRows(); i++) {
        int ndx = rand.randInt() % data.numRows();
        (*centers)[i] = data[ndx];
    }
}


void KMeans::initializeFurthestFirst(int randSeed, const Dataset &data,
        Dataset *centers) {
    if ((! centers) || (centers->numRows() <= 0)) {
        return;
    }

    Random rand(randSeed);

    int ndx = rand.randInt() % data.numRows();
    (*centers)[0] = data[ndx];

    Datapoint distances(data.numRows());

    for (unsigned int i = 1; i < centers->numRows(); i++) {
        for (unsigned int j = 0; j < data.numRows(); j++) {
            double d = data[j].distSquared((*centers)[i - 1]);
            if ((i == 1) || (d < distances[j])) {
                distances[j] = d;
            }
        }

        int next = distances.maxNdx();
        (*centers)[i] = data[next];
    }
}


void KMeans::runKMeans(const Dataset &data, Dataset *centers,
                       int maxIterations) {
    Dataset tempCenters(*centers);
    Dataset *newCenters = &tempCenters, *oldCenters = centers;

    unsigned int numPoints = data.numRows();
    unsigned int k = centers->numRows();
    unsigned int dimension = centers->numCols();

    int iter;
    for (iter = 0; iter < maxIterations; iter++) {
        newCenters->fill(0.0);
        for (unsigned int ctr = 0; ctr < k; ctr++) {
            newCenters->setWeight(ctr, 0.0);
        }

        if (dimension < 3) {
            // if the dimension is low, just use this non-unrolled loop code,
            // and don't use partial distance search
            for (unsigned int point = 0; point < numPoints; point++) {
                const Datapoint &vector = data[point];
                unsigned int label = 0;
                double dist2 = DBL_MAX;
                for (unsigned int ctr = 0; ctr < k; ctr++) {
                    double d2 = 0.0;
                    const Datapoint &center = (*oldCenters)[ctr];
                    for (unsigned int d = 0; d < dimension; d++) {
                        d2 += (vector[d] - center[d]) * (vector[d] - center[d]);
                    }
                    if (d2 < dist2) { dist2 = d2; label = ctr; }
                }

                double weight = data.getWeight(point);
                for (unsigned int d = 0; d < dimension; d++) {
                    (*newCenters)[label][d] += vector[d] * weight;
                }
                newCenters->setWeight(label, newCenters->getWeight(label) + weight);
            }
        } else {
            // if the dimension is 3 or higher, use a partially-unrolled inner
            // loop and partial distance search
            for (unsigned int point = 0; point < numPoints; point++) {
                const Datapoint &vector = data[point];
                unsigned int label = 0;
                double dist2 = DBL_MAX;
                for (unsigned int ctr = 0; ctr < k; ctr++) {
                    double d2 = 0.0;
                    const Datapoint &center = (*oldCenters)[ctr];
                    // three loop iterations unrolled
                    d2 += (vector[0] - center[0]) * (vector[0] - center[0]);
                    d2 += (vector[1] - center[1]) * (vector[1] - center[1]);
                    d2 += (vector[2] - center[2]) * (vector[2] - center[2]);
                    // partial distance search ---------------> |**********|
                    for (unsigned int d = 3; (d < dimension) && (d2 < dist2); d++) {
                        d2 += (vector[d] - center[d]) * (vector[d] - center[d]);
                    }
                    if (d2 < dist2) { dist2 = d2; label = ctr; }
                }

                double weight = data.getWeight(point);
                for (unsigned int d = 0; d < dimension; d++) {
                    (*newCenters)[label][d] += vector[d] * weight;
                }
                newCenters->setWeight(label, newCenters->getWeight(label) + weight);
            }
        }

        for (unsigned int ctr = 0; ctr < k; ctr++) {
            double weight = newCenters->getWeight(ctr);
            if (weight > 0) { (*newCenters)[ctr] /= weight; }
        }

        if (tempCenters == *centers) { break; }

        Dataset *temp = newCenters;
        newCenters = oldCenters;
        oldCenters = temp;
    }

    if (newCenters != centers) {
        *centers = *newCenters;
    }

    Logger::log() << "  Number of k-means iterations performed: " << iter << endl;
}


void KMeans::findLabelsAndDists(const Dataset &data, const Dataset &centers,
                                vector<int> *labels, Datapoint *dists) {
    unsigned int n = data.numRows();
    unsigned int k = centers.numRows();
    for (unsigned int i = 0; i < n; i++) {
        (*labels)[i] = 0;
        double minDist = data[i].distSquared(centers[0]);

        for (unsigned int c = 1; c < k; c++) {
            double d = data[i].distSquared(centers[c]);
            if (d < minDist) {
                (*labels)[i] = c;
                minDist = d;
            }
        }
        if (dists) { (*dists)[i] = sqrt(minDist); }
    }
}

void KMeans::findWeights(const vector<int> &labels, vector<int> *weights) {
    unsigned int i;
    for (i = 0; i < weights->size(); i++) {
        (*weights)[i] = 0;
    }

    for (i = 0; i < labels.size(); i++) {
        (*weights)[labels[i]]++;
    }
}



double KMeans::distortion(const Dataset &data, const vector<int> &labels,
                          const Dataset &centers, Datapoint *distortionPerCluster) {
    double dist = 0.0;
    Datapoint origin(data.numCols()); // the zero vector

    if (distortionPerCluster) {
        distortionPerCluster->fill(0.0);
    }

    unsigned int i;
    double avgWeight = 0.0;
    for (i = 0; i < data.numRows(); i++) {
        double weight = data.getWeight(i);
        double pointDistortion = 
            (data[i] - centers[labels[i]]).distSquared(origin) * weight;
        dist += pointDistortion;
        avgWeight += weight;
        if (distortionPerCluster) {
            (*distortionPerCluster)[labels[i]] += pointDistortion;
        }
    }
    avgWeight = avgWeight / data.numRows();

    dist = dist / avgWeight;

    if (distortionPerCluster) {
        for (unsigned int k = 0; k < centers.numRows(); k++) {
            (*distortionPerCluster)[k] /= avgWeight;
        }
    }

    return dist;
}

double KMeans::bicScore(const Dataset &data, const Dataset &centers) {
    vector<int> labels(data.numRows());
    findLabelsAndDists(data, centers, &labels);

    double dist = distortion(data, labels, centers);

    double n = data.numRows();
    double dim = data.numCols();
    double totalWeight = 0.0;
    for (unsigned int i = 0; i < data.numRows(); i++) {
        totalWeight += data.getWeight(i);
    }
    double sigma2 = dist / (dim * n);

    const double PI = 3.14159265358979;

    double likelihood = - dim * (log(2.0 * PI * sigma2) + 1) / 2.0 - log(totalWeight);

    for (unsigned int i = 0; i < data.numRows(); i++) {
        double wt = centers.getWeight(labels[i]);
        if (wt > 0) {
            likelihood += log(wt) * data.getWeight(i) / totalWeight;
        }
    }
    likelihood = likelihood * n;

    double numParameters = (centers.numRows() - 1) + // cluster probabilities
                           centers.numRows() * data.numCols() + // cluster means
                           1; // variances

    double penalty = numParameters / 2.0 * log((double)data.numRows());

    double score = likelihood - penalty;

    return score;
}

