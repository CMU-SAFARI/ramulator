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


#ifndef SIMPOINT_H
#define SIMPOINT_H

/***********************************************************************
 * File: Simpoint.h
 * Author: Greg Hamerly
 * Date: 8/20/2002
 ***********************************************************************/

#include <iostream>
#include <string>
#include <vector>
#include "SimpointOptions.h"

// The Simpoint class glues the SimPoint analysis tool together. It holds the
// datasets that will be clustered and the k-means centers and associated BIC
// scores
class Simpoint {
    public:
        // construct an empty Simpoint analyzer
        Simpoint() { wholeDataset = sampledDataset = NULL; }

        // clean up all the memory used
        ~Simpoint();

        // parse the command line options and print out options used
        bool parseCmdLineOptions(int argc, char **argv);

        // perform all the clustering (called after initialization)
        void doClustering();

    private:
        // save the data that should be saved prior to clustering
        void savePreClusteringData();

        // save the data that should be saved after clustering
        void savePostClusteringData();

        // load the dataset and project as necessary
        void loadData();

        // sample the dataset that will be used for clustering
        void sampleDataset();

        // apply weights to the dataset, according to the option settings
        void applyWeights();

        // find the best run based on the BIC scores and threshold
        int findBestRun();

        // load the centers associated with the given run number (corresponding
        // to a k value) and a seed number
        Dataset *loadInitialCenters(int runNumber, int seedNumber) const;

        // create centers from a file filled with labels, and the associated dataset
        static Dataset *loadInitialCentersFromLabels(const string &file, const Dataset &data);

        // save the simpoints and labels to the given filename
        static void saveSimpoints(const string &filename, const vector<bool> &largestClusters, 
                const Datapoint &distsToCenters, const vector<int> &labels, unsigned int k);

        // save the simpoint weights and labels to the given filename
        static void saveSimpointWeights(const string &filename, 
                const vector<bool> &largestClusters, const Dataset &centers);

        // create a filename that is useful for saving multiple runs
        static string createFileNameFromRun(const string &baseName, int runNumber, int kValue);

        // get the largest clusters whose weights add up to <= coveragePct
        static vector<bool> getLargestClusters(double coveragePct, const Dataset &finalCenters);

        // opens an input vector file based on the given filename, using
        // fopen() if isGzipped == false, and popen("gzip -c <file>") if
        // isGzipped == true
        static FILE *openInputVectorFile(const string &filename, bool isGzipped = false);

        // closes an input file using fclose() or pclose() depending on isGzipped
        static void closeInputVectorFile(FILE *fp, bool isGzipped = false);

        // checks to see if the given filename is a "regular file" according to
        // stat() (useful for checking if the file exists)
        static bool isRegularFile(const string &filename);

    private:
        // wholeDataset is the entire dataset of intervals to be clustered
        Dataset *wholeDataset;

        // sampledDataset is the sub-sampled dataset of intervals that we
        // actually run k-means on
        Dataset *sampledDataset;

        // the initial and final centers for k-means clustering
        vector<Dataset *> initialCenters, finalCenters; 

        // the bic scores associated with each clustering
        vector<double> bicScores;

        // the options that control the program
        SimpointOptions options;
};

#endif

