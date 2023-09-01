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


#include <iostream>
#include <fstream>
#include <set>
#include <algorithm>
#include <ctime>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include "Utilities.h"
#include "Dataset.h"
#include "KMeans.h"
#include "SimpointOptions.h"
#include "Simpoint.h"
#include "CmdLineParser.h"
#include "Logger.h"


bool Simpoint::isRegularFile(const string &filename) {
    struct stat info;
    if (stat(filename.c_str(), &info) != 0) {
        return false;
    }
    return S_ISREG(info.st_mode);
}

FILE *Simpoint::openInputVectorFile(const string &filename, bool isGzipped) {
    Utilities::check(isRegularFile(filename),
            "openInputVectorFile() not regular file: " + filename);
    FILE *input = NULL;
    if (isGzipped) {
        string command = "gzip -dc " + filename;
        input = popen(command.c_str(), "r");
    } else {
        input = fopen(filename.c_str(), "r");
    }
    Utilities::check(input != NULL, 
        "openInputVectorFile() could not open file " + filename);
    return input;
}

void Simpoint::closeInputVectorFile(FILE *fp, bool isGzipped) {
    if (isGzipped) { pclose(fp); } else { fclose(fp); }
}

void Simpoint::loadData() {
    Utilities::check(wholeDataset == NULL, "Simpoint::loadData() wholeDataset is not NULL");

    // load the data, project it, etc.
    if (options.loadVectorsTxtFmtName != "") {
        FILE *input = openInputVectorFile(options.loadVectorsTxtFmtName,
                options.inputVectorsAreGzipped);
        wholeDataset = new Dataset;
        wholeDataset->read(input);
        Logger::log() << "Loaded data from text format data file '"
            << options.loadVectorsTxtFmtName << "' (size: " 
            << wholeDataset->numRows() << "x" << wholeDataset->numCols() << ")\n";
        closeInputVectorFile(input, options.inputVectorsAreGzipped);
    } else if (options.loadVectorsBinFmtName != "") {
        FILE *input = openInputVectorFile(options.loadVectorsBinFmtName,
                options.inputVectorsAreGzipped);
        wholeDataset = new Dataset;
        wholeDataset->readBinary(input);
        Logger::log() << "Loaded data from binary format data file '" 
            << options.loadVectorsBinFmtName << "' (size: " 
            << wholeDataset->numRows() << "x" << wholeDataset->numCols() << ")\n";
        closeInputVectorFile(input, options.inputVectorsAreGzipped);
    } else if (options.frequencyVectorFileName != "") {
        FILE *input = openInputVectorFile(options.frequencyVectorFileName,
                options.inputVectorsAreGzipped);
        FVParser *parser = new FVParser(input);

        int numPts, numDims;
        if ((options.numFreqVectors != options.DEFAULT_NUM_FREQ_VECTORS) && 
            (options.numFVDims != options.DEFAULT_NUM_FREQ_DIMS)) {
            numPts = options.numFreqVectors;
            numDims = options.numFVDims;
        } else {
            Utilities::sizeOfFVFile(*parser, &numPts, &numDims);
            delete parser;
            closeInputVectorFile(input, options.inputVectorsAreGzipped);
            input = openInputVectorFile(options.frequencyVectorFileName,
                                        options.inputVectorsAreGzipped);
            parser = new FVParser(input);
        }

        Logger::log() << "  Loading data from frequency vector file '"
             << options.frequencyVectorFileName 
             << "' (size: " << numPts << "x" << numDims << ")\n";

        if (options.useNoProjection) {
            wholeDataset = new Dataset(numPts, numDims);
            Utilities::loadFVFile(*parser, wholeDataset);
            Logger::log() << "  Loaded frequency vectors without projecting them.\n";
            delete parser;
        } else {
            // Get the projection matrix
            Dataset *projection = NULL;
            if (options.loadProjMatrixTxtFmtName != "") {
                Utilities::check(isRegularFile(options.loadProjMatrixTxtFmtName),
                        "loadData() not regular file: " +
                        options.loadProjMatrixTxtFmtName);
                projection = new Dataset;
                ifstream input(options.loadProjMatrixTxtFmtName.c_str());
                Utilities::check(input, "Simpoint::loadData(): could not open file " +
                                        options.loadProjMatrixTxtFmtName);
                projection->read(input);
                input.close();
                Logger::log() << "  Loaded projection matrix from file '" 
                     << options.loadProjMatrixTxtFmtName
                     << "' (size: " << projection->numRows() << "x" 
                     << projection->numCols() << ")\n";
                Utilities::check(projection->numRows() == (unsigned int)numDims,
                    "loadData(): projection matrix rows != original dimensions");
            } else if (options.loadProjMatrixBinFmtName != "") {
                Utilities::check(isRegularFile(options.loadProjMatrixBinFmtName),
                        "loadData() not regular file: " +
                        options.loadProjMatrixBinFmtName);
                projection = new Dataset;
                ifstream input(options.loadProjMatrixBinFmtName.c_str());
                Utilities::check(input, "Simpoint::loadData(): could not open file " +
                                        options.loadProjMatrixBinFmtName);
                projection->readBinary(input);
                input.close();
                Logger::log() << "  Loaded projection matrix from binary file '" 
                    << options.loadProjMatrixBinFmtName
                    << "' (size: " << projection->numRows() << "x" 
                    << projection->numCols() << ")\n";
                Utilities::check(projection->numRows() == (unsigned int)numDims,
                    "loadData(): projection matrix rows != original dimensions");
            } else {
                projection = new Dataset(numDims, options.projectionDimension);
                Utilities::randomProjectionMatrix(options.randSeedProjection, projection);
                Logger::log() << "  Created random projection matrix (size: " << numDims
                     << "x" << options.projectionDimension << ")\n";
            }

            // save the projection matrix
            if (options.saveProjMatrixTxtFmtName != "") {
                Logger::log() << "  Saving the projection matrix to file '" 
                    << options.saveProjMatrixTxtFmtName << "'\n";
                ofstream output(options.saveProjMatrixTxtFmtName.c_str());
                Utilities::check(output, "Simpoint::loadData(): could not open file " +
                                        options.saveProjMatrixTxtFmtName);
                projection->write(output);
                output.close();
            }

            if (options.saveProjMatrixBinFmtName != "") {
                Logger::log() << "  Saving the projection matrix to binary file '" 
                    << options.saveProjMatrixBinFmtName << "'\n";
                ofstream output(options.saveProjMatrixBinFmtName.c_str());
                Utilities::check(output, "Simpoint::loadData(): could not open file " +
                                        options.saveProjMatrixBinFmtName);
                projection->writeBinary(output);
                output.close();
            }

            wholeDataset = new Dataset(numPts, options.projectionDimension);
            Utilities::loadAndProjectFVFile(*parser, *projection, wholeDataset);
            delete parser;
            if (options.inputVectorsAreGzipped) { pclose(input); } else { fclose(input); }
            Logger::log() << "  Loaded and projected frequency vector file\n";

            delete projection;
        }
    } else {
        // shouldn't get here
        Utilities::check(false, "loadData(): no data to load");
    }
}


Dataset *Simpoint::loadInitialCentersFromLabels(const string &file, const Dataset &data) {
    Utilities::check(isRegularFile(file), 
        "Simpoint::loadInitialCentersFromLabels() not regular file: " + file);
    int n = data.numRows(), d = data.numCols();
    ifstream input(file.c_str());
    Utilities::check(input, 
        "Simpoint::loadInitialCentersFromLabels() could not open file " + file);

    // load all the labels, find the largest one in the process (this is
    // the number of clusters)
    map<string, int> labelMap; // map from user-given (external) label to actual (internal) label
    vector<int> labels(n);
    for (int i = 0; i < n; i++) {
        string externalLabel;
        input >> externalLabel;
        map<string, int>::iterator itr = labelMap.find(externalLabel);
        if (itr == labelMap.end()) {
            int lmsize = labelMap.size();
            labelMap[externalLabel] = lmsize;
            itr = labelMap.find(externalLabel);
        }
        labels[i] = itr->second;
    }

    int k = labelMap.size();

    // create the initial centers
    Dataset *centers = new Dataset(k, d);
    for (int i = 0; i < k; i++) { centers->setWeight(i, 0.0); }

    // assign each datapoint to each center
    for (int i = 0; i < n; i++) {
        double wt = data.getWeight(i);
        (*centers)[labels[i]].multAndAdd(data[i], wt);
        centers->setWeight(labels[i], centers->getWeight(labels[i]) + wt);
    }

    // normalize each center
    for (int i = 0; i < k; i++) { (*centers)[i] /= centers->getWeight(i); }

    return centers;
}

Dataset *Simpoint::loadInitialCenters(int runNumber, int seedNumber) const {
    Dataset *centers = NULL;
    // user has provided initial labels?
    if (options.loadInitialLabelsName != "") {
        centers = loadInitialCentersFromLabels(options.loadInitialLabelsName,
                                               *wholeDataset);
    // user has provided initial centers?
    } else if (options.loadInitialCentersName != "") {
        Utilities::check(isRegularFile(options.loadInitialCentersName),
                "loadInitialCenters() not regular file: " +
                options.loadInitialCentersName);
        centers = new Dataset;
        ifstream input(options.loadInitialCentersName.c_str());
        Utilities::check(input, "Simpoint::loadInitialCenters(): could not open file " +
                                options.loadInitialCentersName);
        centers->read(input);
        input.close();
        Logger::log() << "  Loaded initial k-means centers from file '"
             << options.loadInitialCentersName << "' (k = " 
             << centers->numRows() << ")\n";
        Utilities::check(centers->numCols() == wholeDataset->numCols(),
            "loadInitialCenters(): initial centers dimension != "
            "projected data dimension");

        // user has not provided initial labels or centers; create the initial centers
    } else {
        centers = new Dataset(options.kValues[runNumber],
                                     wholeDataset->numCols());
        if (options.kMeansInitType == "samp") {
            KMeans::initializeRandomly(options.randSeedKMeansInit + seedNumber,
                    *wholeDataset, centers);
            Logger::log() << "  Initialized k-means centers using random sampling: " 
                << options.kValues[runNumber] << " centers\n";
        } else if (options.kMeansInitType == "ff") {
            KMeans::initializeFurthestFirst(options.randSeedKMeansInit + seedNumber,
                    *wholeDataset, centers);
            Logger::log() << "  Initialized k-means centers using furthest-first: " 
                << options.kValues[runNumber] << " centers\n";
        } else {
            Utilities::check(false, 
                    "loadInitialCenters(): unknown k-means initialization type");
        }
    }

    // Set the (VLI) weights for the centers properly. If vectors should not be
    // VLI-weighted, the appropriate weights should (will) be applied outside
    // this scope, after this function completes.
    for (unsigned int i = 0; i < centers->numRows(); i++) {
        centers->setWeight(i, 0.0);
    }
    vector<int> labels(wholeDataset->numRows());
    KMeans::findLabelsAndDists(*wholeDataset, *centers, &labels);
    for (unsigned int i = 0; i < wholeDataset->numRows(); i++) {
        centers->setWeight(labels[i],
                wholeDataset->getWeight(i) + centers->getWeight(labels[i]));
    }

    // make sure the center weights add to 1.0
    double totalWeight = 0.0;
    for (unsigned int i = 0; i < centers->numRows(); i++) {
        totalWeight += centers->getWeight(i);
    }
    for (unsigned int i = 0; i < centers->numRows(); i++) {
        centers->setWeight(i, centers->getWeight(i) / totalWeight);
    }

    return centers;
}

void Simpoint::sampleDataset() {
    Utilities::check(sampledDataset == NULL, 
            "Simpoint::sampleDataset() sampledDataset is not NULL");

    if ((options.sampleSize > 0) && 
            ((unsigned int)options.sampleSize < wholeDataset->numRows())) {
        // choose enough samples to satisfy the number of desired samples
        Random rand(options.randSeedSample);

        vector<pair<double, int> > sortedWeights(wholeDataset->numRows());
        for (unsigned int i = 0; i < wholeDataset->numRows(); i++) {
            sortedWeights[i] = pair<double, int>(wholeDataset->getWeight(i), i);
        }
        sort(sortedWeights.begin(), sortedWeights.end(), greater<pair<double, int> >());

        for (unsigned int i = 1; i < wholeDataset->numRows(); i++) {
            sortedWeights[i].first += sortedWeights[i-1].first;
        }
        sortedWeights[wholeDataset->numRows() - 1].first = 1.0; // just to be sure

        set<int> samples;
        double sampledPct = 0.0;
        while (samples.size() < (unsigned int)options.sampleSize) {
            double r = (double)rand.randFloat();

            // binary search for the sample corresponding to the generated
            // random number
            unsigned int lower = 0, upper = sortedWeights.size();
            unsigned int sample = (upper + lower) / 2;
            while (true) {
                bool below = (sample > 0) ? (sortedWeights[sample - 1].first <= r) : true;
                bool above = (sample < sortedWeights.size()) ? 
                             (sortedWeights[sample].first >= r) : true;
                if (above && below) { break; }
                if (below) { lower = sample; }
                if (above) { upper = sample; }
                sample = (upper + lower) / 2;
            }

            if (samples.find(sortedWeights[sample].second) == samples.end()) {
                sampledPct += wholeDataset->getWeight(sortedWeights[sample].second);
                samples.insert(sortedWeights[sample].second);
            }
        }

        unsigned int sampleSize = samples.size();
        Logger::log() << "  Creating a random sample of size " << sampleSize 
            << " vectors for clustering\n";
        Logger::log() << "    which represents " << (sampledPct * 100) 
            << "% of the weights\n";
        sampledDataset = new Dataset(sampleSize, wholeDataset->numCols());

        // copy the sampled data into the working data structure and reweight it 
        int j = 0;
        for (set<int>::iterator i = samples.begin(); i != samples.end(); i++) {
            for (unsigned int col = 0; col < wholeDataset->numCols(); col++) {
                (*sampledDataset)[j][col] = (*wholeDataset)[*i][col];
                sampledDataset->setWeight(j, wholeDataset->getWeight(*i) / sampledPct);
            }
            j++;
        }
    } else {
        sampledDataset = wholeDataset;
    }
}

void Simpoint::applyWeights() {
    if (options.fixedLength == "on") {
        Logger::log() << "  Applying fixed-length vector weights (uniform weights)\n";
        double weight = 1.0 / (double)wholeDataset->numRows();
        for (unsigned int i = 0; i < wholeDataset->numRows(); i++) {
            wholeDataset->setWeight(i, weight);
        }
    } else if (options.loadVectorWeightsName != "") {
        Logger::log() << "  Applying vector weights from file " << options.loadVectorWeightsName << endl;
        Utilities::check(isRegularFile(options.loadVectorWeightsName),
                "applyWeights() not regular file " + options.loadVectorWeightsName);
        ifstream input(options.loadVectorWeightsName.c_str());
        Utilities::check(input, "Simpoint::applyWeights(): could not open file " +
                                options.loadVectorWeightsName);
        vector<double> weights(wholeDataset->numRows());
        double totalWeight = 0.0;
        for (unsigned int i = 0; i < wholeDataset->numRows(); i++) {
            input >> weights[i];
            totalWeight += weights[i];
        }

        for (unsigned int i = 0; i < wholeDataset->numRows(); i++) {
            wholeDataset->setWeight(i, weights[i] / totalWeight);
        }

        input.close();
    }
}

string Simpoint::createFileNameFromRun(const string &baseName, int runNumber, int kValue) {
    char newname[1024];
    sprintf(newname, "%s.run_%d_k_%d", baseName.c_str(), runNumber, kValue);
    return string(newname);
}

void Simpoint::savePreClusteringData() {
    if (options.saveVectorsTxtFmtName != "") {
        Logger::log() << "  Saving Simpoint-format vector data to text file '" 
            << options.saveVectorsTxtFmtName << "'\n";
        ofstream output(options.saveVectorsTxtFmtName.c_str());
        Utilities::check(output, "Simpoint::savePreClusteringData(): could not open file " +
                                options.saveVectorsTxtFmtName);
        wholeDataset->write(output);
        output.close();
    }

    if (options.saveVectorsBinFmtName != "") {
        Logger::log() << "  Saving Simpoint-format vector data to binary file '" 
            << options.saveVectorsBinFmtName << "'\n";
        ofstream output(options.saveVectorsBinFmtName.c_str());
        Utilities::check(output, "Simpoint::savePreClusteringData(): could not open file " +
                                options.saveVectorsBinFmtName);
        wholeDataset->writeBinary(output);
        output.close();
    }

    if (options.saveVectorWeightsName != "") {
        Logger::log() << "  Saving weights of each input vector to file '" 
            << options.saveVectorWeightsName << "'\n";

        ofstream output(options.saveVectorWeightsName.c_str());
        Utilities::check(output, "Simpoint::saveVectorWeights(): could not open file " +
                                options.saveVectorWeightsName);
        output.precision(20);
        for (unsigned int i = 0; i < wholeDataset->numRows(); i++) {
            output << wholeDataset->getWeight(i) << endl;
        }
        output.close();
    }
}

int Simpoint::findBestRun() {
    int bestRun = 0;
    if (options.kValues.size() > 1) {
        double min_bic = bicScores[0], max_bic = bicScores[0];
        for (unsigned int i = 1; i < options.kValues.size(); i++) {
            if (bicScores[i] > max_bic) { max_bic = bicScores[i]; }
            if (bicScores[i] < min_bic) { min_bic = bicScores[i]; }
        }

        double threshold = (max_bic - min_bic) * options.bicThreshold + min_bic;
        bestRun = -1;
        for (unsigned int i = 0; i < options.kValues.size(); i++) {
            if ((bicScores[i] >= threshold) &&
                ((bestRun == -1) || (options.kValues[i] < options.kValues[bestRun]))) {
                bestRun = i;
            }
        }
    }

    return bestRun;
}

vector<bool> Simpoint::getLargestClusters(double coveragePct, const Dataset &finalCenters) {
    Utilities::check(coveragePct >= 0 && coveragePct <= 1,
            "getLargestClusters(): coveragePct is out of bounds");

    // sort the clusters by size
    // the pair represents percentage (double) and cluster (int)
    vector<pair<double, int> > sortedClusters(finalCenters.numRows());
    for (unsigned int i = 0; i < sortedClusters.size(); i++) {
        sortedClusters[i] = make_pair(finalCenters.getWeight(i), i);
    }
    sort(sortedClusters.begin(), sortedClusters.end());
    reverse(sortedClusters.begin(), sortedClusters.end());

    // now that they're sorted, select the largest that fulfill the
    // desired percentage and mark them as largestClusters
    double percentExecution = 0.0;
    vector<bool> largestClusters(sortedClusters.size(), false); // order of orig. clusters
    Logger::log(1) << "    Largest non-empty clusters with total weight >= "
        << coveragePct << " (listed largest to smallest): ";
    for (unsigned int i = 0; (i < sortedClusters.size()) &&
            (percentExecution < coveragePct) &&
            (sortedClusters[i].first > 0.0); i++) {
        percentExecution += sortedClusters[i].first;
        largestClusters[sortedClusters[i].second] = true;
        Logger::log(1) << sortedClusters[i].second << " ";
    }
    Logger::log(1) << endl;

    return largestClusters;
}

void Simpoint::saveSimpoints(const string &filename, const vector<bool> &largestClusters,
        const Datapoint &distsToCenters, const vector<int> &labels, unsigned int k) {

    vector<double> minDists(k, -1.0);
    vector<int> simpoints(k, -1);
    for (unsigned int i = 0; i < distsToCenters.size(); i++) {
        int label = labels[i];
        if ((simpoints[label] == -1) || (distsToCenters[i] < minDists[label])) {
            simpoints[label] = i;
            minDists[label] = distsToCenters[i];
        }
    }

    ofstream output(filename.c_str());
    Utilities::check(output, "Simpoint::saveSimpoints(): could not open file " +
                            filename);
    for (unsigned int i = 0; i < k; i++) {
        if (largestClusters[i]) {
            output << simpoints[i] << " " << i << endl;
        }
    }
    output.close();
}

void Simpoint::saveSimpointWeights(const string &filename, 
        const vector<bool> &largestClusters, const Dataset &centers) {
    ofstream output(filename.c_str());
    Utilities::check(output, "Simpoint::saveSimpointWeights(): could not open file " +
                            filename);
    double sumWeights = 0.0;
    for (unsigned int r = 0; r < centers.numRows(); r++) {
        if (largestClusters[r]) {
            sumWeights += centers.getWeight(r);
        }
    }

    for (unsigned int r = 0; r < centers.numRows(); r++) {
        if (largestClusters[r]) {
            output << (centers.getWeight(r) / sumWeights) << " " << r << endl;
        }
    }
    output.close();
}

void Simpoint::savePostClusteringData() {

    Logger::log() << endl
          << "------------------------------------------------------------------\n"
          << "------------------------------------------------------------------\n"
          << "Post-processing runs\n"
          << "------------------------------------------------------------------\n"
          << "------------------------------------------------------------------\n";

    int bestRun = findBestRun();
    Logger::log() << "  For the BIC threshold, the best clustering was run " 
        << (bestRun+1) << " (k = " << options.kValues[bestRun] << ")\n";

    vector<int> labels(wholeDataset->numRows(), 0);
    Datapoint distsToCenters(wholeDataset->numRows());

    // save everything or just the best
    for (unsigned int runNumber = 0; runNumber < options.kValues.size(); runNumber++) {
        if ((runNumber != (unsigned int)bestRun) && (! options.saveAll)) {
            continue;
        }

        Logger::log() << "  Post-processing run " << (runNumber+1) << " (k = " 
             << options.kValues[runNumber] << ")\n";
        // save the initial centers
        if (options.saveInitialCentersName != "") {
            string name = options.saveInitialCentersName;
            if (options.saveAll) {
                name = createFileNameFromRun(options.saveInitialCentersName, runNumber+1, options.kValues[runNumber]);
            }
            Logger::log() << "    Saving initial centers to file '" << name << "'\n";
            ofstream output(name.c_str());
            Utilities::check(output, "Simpoint::savePostClusteringData(): could not open file " +
                                    name);
            initialCenters[runNumber]->write(output);
            output.close();
        }

        // save the final centers
        if (options.saveFinalCentersName != "") {
            string name = options.saveFinalCentersName;
            if (options.saveAll) {
                name = createFileNameFromRun(options.saveFinalCentersName, runNumber+1, options.kValues[runNumber]);
            }
            Logger::log() << "    Saving final centers to file '" << name << "'\n";
            ofstream output(name.c_str());
            Utilities::check(output, "Simpoint::savePostClusteringData(): could not open file " +
                                    name);
            finalCenters[runNumber]->write(output);
            output.close();
        }


        // pre-compute the labels and distances for saveSimpoints or saveLabels
        KMeans::findLabelsAndDists(*wholeDataset, *finalCenters[runNumber], 
                                       &labels, &distsToCenters);

        // save labels
        if (options.saveLabelsName != "") {
            string name = options.saveLabelsName;
            if (options.saveAll) {
                name = createFileNameFromRun(options.saveLabelsName, runNumber+1, options.kValues[runNumber]);
            }
            Logger::log() << "    Saving labels and distance from center of "
                << "each input vector to file '" << name << "'\n";

            ofstream output(name.c_str());
            Utilities::check(output, "Simpoint::savePostClusteringData(): could not open file " +
                                    name);
            for (unsigned int r = 0; r < labels.size(); r++) {
                /* output the label and the distance from the center of this point */
                output << labels[r] << " " << distsToCenters[r] << endl;
            }
            output.close();
        }

        // prepare to save only the largest simpoints and weights
        vector<bool> nonEmptyClusters = getLargestClusters(1.0, *finalCenters[runNumber]);
        vector<bool> largestClusters = nonEmptyClusters;
        if (options.coveragePct < 1.0) {
            largestClusters = getLargestClusters(options.coveragePct, 
                                                 *finalCenters[runNumber]);
        }

        // save simpoints
        if (options.saveSimpointsName != "") {
            string name = options.saveSimpointsName;
            if (options.saveAll) { name = createFileNameFromRun(name, runNumber+1, options.kValues[runNumber]); }

            Logger::log() << "    Saving simpoints of all non-empty clusters to file '" 
                << name << "'\n";
            saveSimpoints(name, nonEmptyClusters, distsToCenters, labels,
                          finalCenters[runNumber]->numRows());

            if (options.coveragePct < 1.0) {
                name = options.saveSimpointsName + ".lpt" + 
                        toString(options.coveragePct);
                if (options.saveAll) { name = createFileNameFromRun(name, runNumber+1, options.kValues[runNumber]); }

                Logger::log() << "    Saving simpoints of largest clusters "
                    << "making up proportion " << options.coveragePct 
                    << " of all weights to file '" << name << "'\n";
                saveSimpoints(name, largestClusters, distsToCenters, labels,
                              finalCenters[runNumber]->numRows());
            }
        }


        // save weights
        if (options.saveSimpointWeightsName != "") {
            string name = options.saveSimpointWeightsName;
            if (options.saveAll) { name = createFileNameFromRun(name, runNumber+1, options.kValues[runNumber]); }
            Logger::log() << "    Saving weights of all non-empty clusters to file '" 
                << name << "'\n";

            saveSimpointWeights(name, nonEmptyClusters, *finalCenters[runNumber]);

            if (options.coveragePct < 1.0) {
                name = options.saveSimpointWeightsName + ".lpt" + 
                    toString(options.coveragePct);
                if (options.saveAll) { name = createFileNameFromRun(name, runNumber+1, options.kValues[runNumber]); }

                Logger::log() << "    Saving weights of largest clusters "
                    << "making up proportion " << options.coveragePct 
                    << " of all weights to file '" << name << "'\n";
                saveSimpointWeights(name, largestClusters, *finalCenters[runNumber]);
            }
        }
    }
}


void Simpoint::doClustering() {
    loadData();
    applyWeights();
    sampleDataset();
    savePreClusteringData();

    // create vectors to save all the data that will be produced
    vector<int> labels(wholeDataset->numRows(), 0);
    Datapoint distsToCenters(wholeDataset->numRows());

    // binary search variables
    int search_k_max = options.max_k, search_k_min = 1,
        min_bic_ndx = 0, max_bic_ndx = 0;
    if (options.useBinarySearch) {
        Logger::log() << "  Searching for best clustering for k <= " 
            << search_k_max << endl;
        options.kValues.clear();
        options.kValues.push_back(search_k_min);
        options.kValues.push_back(search_k_max);
        options.kValues.push_back((search_k_max + search_k_min) / 2);
    } else {
        Logger::log() << "  Clustering for user-defined k-values\n"; 

        if (options.learnKFromFile) {
            // find out the k value from the clusters the user provided
            Dataset *tempCtrs = loadInitialCenters(0, 0);
            options.kValues.push_back(tempCtrs->numRows());
            delete tempCtrs;
        }
    }

    vector<Dataset *> initSeedInitialCenters(options.numInitSeeds),
                      initSeedFinalCenters(options.numInitSeeds);
    vector<double> initSeedBicScores(options.numInitSeeds);

    for (unsigned int runNumber = 0; runNumber < options.kValues.size(); runNumber++) {
        Logger::log() << endl
            << "--------------------------------------------------------------\n"
            << "Run number " << (runNumber+1) << " of ";
        if (options.useBinarySearch) {
            int maxRuns = (int)(log((double)options.max_k) / log(2.0) + 3);
            Logger::log() << "at most " << maxRuns;
        } else {
            Logger::log() << options.kValues.size();
        }
        Logger::log() << ", k = " << options.kValues[runNumber] << endl
            << "--------------------------------------------------------------\n";

        int bestInitSeedRun = 0;
        for (int initSeedRun = 0; initSeedRun < options.numInitSeeds; initSeedRun++) {
            Logger::log() 
                << "  --------------------------------------------------------------\n"
                << "  Initialization seed trial #" << (initSeedRun+1) << " of "
                << options.numInitSeeds << "; initialization seed = " 
                << options.randSeedKMeansInit << endl
                << "  --------------------------------------------------------------\n";

            initSeedInitialCenters[initSeedRun] = loadInitialCenters(runNumber, initSeedRun);
            initSeedFinalCenters[initSeedRun] =
                new Dataset(*initSeedInitialCenters[initSeedRun]);

            int iteration_limit = options.numKMeansIterations;
            if (options.useNoIterationLimit) { iteration_limit = INT_MAX; }
            KMeans::runKMeans(*sampledDataset, initSeedFinalCenters[initSeedRun], 
                    iteration_limit);

            // calculate and save the BIC score
            initSeedBicScores[initSeedRun] = KMeans::bicScore(*wholeDataset,
                    *initSeedFinalCenters[initSeedRun]);
            Logger::log() << "  BIC score: " << initSeedBicScores[initSeedRun] << endl;

            if (initSeedBicScores[initSeedRun] > initSeedBicScores[bestInitSeedRun]) {
                bestInitSeedRun = initSeedRun;
            }

            // report the distortions and variances
            KMeans::findLabelsAndDists(*wholeDataset,
                    *initSeedFinalCenters[initSeedRun], 
                    &labels, &distsToCenters);
            Datapoint clusterDistortions(initSeedFinalCenters[initSeedRun]->numRows());
            double dist = KMeans::distortion(*wholeDataset, labels,
                    *initSeedFinalCenters[initSeedRun], &clusterDistortions);
            Logger::log() << "  Distortion: " << dist << endl
                          << "  Distortions/cluster: ";
            for (unsigned int i = 0; i < clusterDistortions.size(); i++) {
                Logger::log() << clusterDistortions[i] << " ";
            }
            Logger::log() << endl;

            double degreesOfFreedom = wholeDataset->numRows() -
                                initSeedFinalCenters[initSeedRun]->numRows();
            Logger::log() << "  Variance: " << (dist/degreesOfFreedom) << endl 
                << "  Variances/cluster: ";
            for (unsigned int i = 0; i < clusterDistortions.size(); i++) {
                double weight = initSeedFinalCenters[initSeedRun]->getWeight(i)
                                * wholeDataset->numRows();
                if (weight > 1.0) {
                    Logger::log() << (clusterDistortions[i] / (weight - 1.0)) << " ";
                } else {
                    Logger::log() << 0 << " ";
                }
            }
            Logger::log() << endl;
            options.randSeedKMeansInit++;
        }

        Logger::log() << "  The best initialization seed trial was #" 
            << (bestInitSeedRun+1) << endl;

        initialCenters.push_back(initSeedInitialCenters[bestInitSeedRun]);
        finalCenters.push_back(initSeedFinalCenters[bestInitSeedRun]);
        bicScores.push_back(initSeedBicScores[bestInitSeedRun]);

        // free up the memory and reset the pointers for the next go-round
        for (int initSeedRun = 0; initSeedRun < options.numInitSeeds; initSeedRun++) {
            if (initSeedRun != bestInitSeedRun) {
                delete initSeedInitialCenters[initSeedRun];
                delete initSeedFinalCenters[initSeedRun];
            }
            initSeedInitialCenters[initSeedRun] = NULL;
            initSeedFinalCenters[initSeedRun] = NULL;
        }

        if (options.useBinarySearch) {
            if (bicScores[runNumber] > bicScores[max_bic_ndx]) max_bic_ndx = runNumber;
            if (bicScores[runNumber] < bicScores[min_bic_ndx]) min_bic_ndx = runNumber;

            double bic_range = (bicScores[max_bic_ndx] - bicScores[min_bic_ndx]);
            double bic_threshold = bicScores[min_bic_ndx] +
                bic_range * options.bicThreshold;
            int last_k = options.kValues[runNumber];

            if (runNumber >= 2) { // determine where we should search next
                int next_k = -1;
                bool searchUpper = (max_bic_ndx > min_bic_ndx) ?
                    (bicScores.back() < bic_threshold) : false;

                if (searchUpper) { // search in upper window
                    next_k = (last_k + search_k_max) / 2;
                    search_k_min = last_k;
                } else { // search in the lower window
                    next_k = (last_k + search_k_min) / 2;
                    search_k_max = last_k;
                }

                // the ending condition for binary search
                if ((search_k_max - search_k_min) > 1) {
                    options.kValues.push_back(next_k);
                }
            }
        }
    }

    savePostClusteringData();
}

Simpoint::~Simpoint() {
    if (wholeDataset && (wholeDataset == sampledDataset)) {
        delete wholeDataset;
    } else {
        if (wholeDataset) { delete wholeDataset; }
        if (sampledDataset) { delete sampledDataset; }
    }
    wholeDataset = sampledDataset = NULL;

    for (unsigned int i = 0; i < initialCenters.size(); i++) {
        if (initialCenters[i]) { delete initialCenters[i]; initialCenters[i] = NULL; }
    }

    for (unsigned int i = 0; i < finalCenters.size(); i++) {
        if (finalCenters[i]) { delete finalCenters[i]; finalCenters[i] = NULL; }
    }
}


bool Simpoint::parseCmdLineOptions(int argc, char **argv) {
    Logger::log() << "Command-line: \"";
    for (int i = 0; i < argc; i++) {
        if (i > 0) { Logger::log() << ' '; }
        Logger::log() << argv[i];
    }
    Logger::log() << "\"\n";

    if (argc == 1) {
        options.usage(argv[0]);
        return false;
    }

    if (! options.parseOptions(argc, argv)) {
        return false;
    }

    Logger::log() << "Using these options (*** indicates user-specified option):\n";
    options.printOptionSettings(Logger::log());
    Logger::log() << "-------------------------------------------------------------\n";

    return true;
}


// MAIN
int main(int argc, char **argv) {
    Simpoint simpointAnalyzer;

    if (simpointAnalyzer.parseCmdLineOptions(argc, argv)) {
        // do everything else
        simpointAnalyzer.doClustering();
    }

    return 0;
}

