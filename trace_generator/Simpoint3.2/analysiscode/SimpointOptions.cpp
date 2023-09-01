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


#include "SimpointOptions.h"
#include "Utilities.h"
#include "Logger.h"

// Initialize the class constants for the SimpointOptions class
const int SimpointOptions::DEFAULT_KMEANS_ITERATIONS = 100;
const int SimpointOptions::DEFAULT_DIMENSIONS = 15;
const int SimpointOptions::DEFAULT_RANDSEED_KMEANS_INIT = 493575226;
const int SimpointOptions::DEFAULT_RANDSEED_PROJECTION = 2042712918;
const int SimpointOptions::DEFAULT_RANDSEED_SAMPLE = 385089224;
const int SimpointOptions::DEFAULT_NUM_FREQ_VECTORS = -1;
const int SimpointOptions::DEFAULT_NUM_FREQ_DIMS = -1;
const int SimpointOptions::DEFAULT_SAMPLE_SIZE = -1;
const int SimpointOptions::DEFAULT_MAX_K = -1;
const int SimpointOptions::DEFAULT_NUM_INIT_SEEDS = 5;
const string SimpointOptions::DEFAULT_KMEANS_INIT_TYPE = "samp";
const string SimpointOptions::DEFAULT_FIXED_LENGTH = "on";
const double SimpointOptions::DEFAULT_COVERAGE_PERCENTAGE = 1.0;
const double SimpointOptions::DEFAULT_BIC_THRESHOLD = 0.9;

// Initialize the data members of a SimpointOptions object to default values
SimpointOptions::SimpointOptions() {
    cmdLineParser.addOption(new StringCmdLineOption("loadFVFile", "file",
            "An un-projected sparse format frequency vector file to load, "
            "possibly project, and analyze.", &frequencyVectorFileName));

    cmdLineParser.addOption(new NumClustersCmdLineOption("k", "regex",
            "regex := \"search\" | R(,R)* and R := k|start:end|start:step:end "
            "where k represents a single value, start:end represents a range "
            "from start to end (inclusive), and start:step:end represents the "
            "values start, start+step, start+2*step, ... until reaching or "
            "passing end. Reverse ranges are also allowed. "
            "Default is \"search\".", &useBinarySearch,
            &kValues));

    cmdLineParser.addOption(new NumItersCmdLineOption("iters", "n | \"off\"",
            "Maximum number of iterations that k-means should perform. "
            "The option \"off\" means no limit is imposed. "
            "Default is " + toString(DEFAULT_KMEANS_ITERATIONS) + ".",
            &useNoIterationLimit, &numKMeansIterations,
            DEFAULT_KMEANS_ITERATIONS));

    cmdLineParser.addOption(new DimensionCmdLineOption("dim", "d | \"noProject\"",
            "Number of dimensions to which un-projected frequency vectors will "
            "be projected. Used with -loadFVFile, not compatible with "
            "-loadVectorsTxtFmtName or -loadVectorsBinFmtName. It is not "
            "advised to use \"noProject\" unless you have a very "
            "low-dimensional frequency vector file. Default is " +
            toString(DEFAULT_DIMENSIONS) + ".", &useNoProjection,
            &projectionDimension, DEFAULT_DIMENSIONS));

    cmdLineParser.addOption(new IntCmdLineOption("maxK", "k",
            "Maximum number of clusters to use when using \"-k search\". "
            "There is no default value; this option must be specified when "
            "using search.", &max_k, DEFAULT_MAX_K, 1));

    cmdLineParser.addOption(new IntCmdLineOption("numInitSeeds", "n",
            "Run k-means this many times for each value of k, with a different random "
            "initialization for each run, taking only the best clustering. "
            "Default is " + toString(DEFAULT_NUM_INIT_SEEDS) + ".",
            &numInitSeeds, DEFAULT_NUM_INIT_SEEDS, 1));

    cmdLineParser.addOption(new DoubleCmdLineOption("coveragePct", "p",
            "Options -saveSimpoints and -saveSimpointWeights save all non-empty "
            "clusters. This option specifies that an addition file should be "
            "saved for each of these two options with partial filename '.lpt<p>' "
            "(where <p> is the user-set value). Each of these files will have "
            "the simpoints/simpoint weights for only the largest clusters making "
            "up proportion p of the total weights, where 0 <= p <= 1. "
            "Default is " + toString(DEFAULT_COVERAGE_PERCENTAGE) + ".",
            &coveragePct, DEFAULT_COVERAGE_PERCENTAGE, 0.0, 1.0));

    cmdLineParser.addOption(new DoubleCmdLineOption("bicThreshold", "t",
            "The threshold for choosing the best clustering based on the BIC, with t "
            "between 0.0 and 1.0. The best clustering is defined as "
            "t*(max_bic-min_bic)+min_bic. Default is " +
            toString(DEFAULT_BIC_THRESHOLD) + ".",
            &bicThreshold, DEFAULT_BIC_THRESHOLD, 0.0, 1.0));

    cmdLineParser.addOption(new FlagCmdLineOption("saveAll",
            "When specified, save all outputs pertaining to each value of k "
            "that is run. Without this option, only the outputs for the best "
            "clustering will be saved. This option affects all saved data that "
            "is specific to a particular value of k (-saveSimpoints, "
            "-saveSimpointWeights, -saveLabels, -saveInitCtrs, "
            "-saveFinalCtrs).", &saveAll));

    string initkmOptions[] = { "samp", "ff" };
    set<string> options(initkmOptions, initkmOptions + 2);
    cmdLineParser.addOption(new StringCmdLineOption("initkm", "\"samp\" | \"ff\"",
            "The type of initialization that will be used for k-means. "
            "\"samp\" means sample k vectors at random (without replacement) "
            "as the initial centers. \"ff\" means furthest-first, which "
            "chooses a random vector as the first center, and then repeatedly "
            "chooses as the next center the furthest vector from any chosen "
            "center. Default is " + DEFAULT_KMEANS_INIT_TYPE + ".",
            &kMeansInitType, DEFAULT_KMEANS_INIT_TYPE, options));

    cmdLineParser.addOption(new StringCmdLineOption("saveLabels", "file",
            "Saves to the given file the label and distance to nearest centroid "
            "for each clustered vector.", &saveLabelsName));

    cmdLineParser.addOption(new StringCmdLineOption("saveSimpoints", "file",
            "Saves to the given file the simulation point (index into the "
            "clustered vectors, starting at 0) and cluster label for the "
            "largest non-empty clusters that together make up the proportion "
            "of weights specified by -coveragePct.", &saveSimpointsName));

    cmdLineParser.addOption(new StringCmdLineOption("saveSimpointWeights", "file",
            "Saves to the given file the weight and cluster label of each of "
            "the clusters associated with the simulation points that have "
            "been chosen. Saved in the same format as -saveSimpoints, but "
            "with weights.", &saveSimpointWeightsName));

    cmdLineParser.addOption(new StringCmdLineOption("saveVectorWeights", "file",
            "Saves to the given file the weights associated with each vector "
            "that was analyzed. These weights are also stored in files saved "
            "with -saveVectorsTxtFmt and -saveVectorsBinFmt, so this option is "
            "not necessary just for saveing and loading vector weights.",
            &saveVectorWeightsName));

    cmdLineParser.addOption(new StringCmdLineOption("saveInitCtrs", "file",
            "Saves to the given file the initial centers (prior to k-means "
            "clustering).", &saveInitialCentersName));

    cmdLineParser.addOption(new StringCmdLineOption("saveFinalCtrs", "file",
            "Saves to the given file the final centers (after k-means "
            "clustering).", &saveFinalCentersName));

    cmdLineParser.addOption(new StringCmdLineOption("saveVectorsTxtFmt", "file",
            "Saves to the given file a text version of the projected version "
            "of the frequency vectors, which can save load time in future "
            "runs.", &saveVectorsTxtFmtName));

    cmdLineParser.addOption(new StringCmdLineOption("saveVectorsBinFmt", "file",
            "Saves to the given file a binary version of the projected version "
            "of the frequency vectors, which can save load time in future "
            "runs.", &saveVectorsBinFmtName));

    cmdLineParser.addOption(new StringCmdLineOption("saveProjMatrixTxtFmt", "file",
            "Saves to the given file a text version of the projection matrix "
            "used to project the frequency vector file given with -loadFVFile.",
            &saveProjMatrixTxtFmtName));

    cmdLineParser.addOption(new StringCmdLineOption("saveProjMatrixBinFmt", "file",
            "Saves to the given file a binary version of the projection matrix "
            "used to project the frequency vector file given with -loadFVFile.",
            &saveProjMatrixBinFmtName));

    cmdLineParser.addOption(new StringCmdLineOption("loadVectorsTxtFmt", "file",
            "Loads the given text file of pre-projected vectors to analyze.",
            &loadVectorsTxtFmtName));

    cmdLineParser.addOption(new StringCmdLineOption("loadVectorsBinFmt", "file",
            "Loads the given binary file of pre-projected vectors to analyze.",
            &loadVectorsBinFmtName));

    cmdLineParser.addOption(new StringCmdLineOption("loadProjMatrixTxtFmt", "file",
            "Loads the given text file of a matrix for projecting a frequency "
            "vector file. Each projection matrix is tied to both the original "
            "dimension of the frequency vectors, and the projected dimension.",
            &loadProjMatrixTxtFmtName));

    cmdLineParser.addOption(new StringCmdLineOption("loadProjMatrixBinFmt", "file",
            "Binary file version of -loadProjMatrixTxtFmt.",
            &loadProjMatrixBinFmtName));

    cmdLineParser.addOption(new StringCmdLineOption("loadInitCtrs", "file",
            "Loads the initial centers from the given file. These are used "
            "instead of generating them randomly.", &loadInitialCentersName));

    cmdLineParser.addOption(new StringCmdLineOption("loadInitLabels", "file",
            "Loads the initial labels of the vectors from the given file."
            "These are used to form the initial k-means clusters instead of "
            "generating them randomly.", &loadInitialLabelsName));

    cmdLineParser.addOption(new StringCmdLineOption("loadVectorWeights", "file",
            "Loads the weights for each vector from the given file.",
            &loadVectorWeightsName));

    cmdLineParser.addOption(new FlagCmdLineOption("inputVectorsGzipped", 
            "Specifies that the file holding the input vectors (from -loadFVFile, "
            "-loadVectorsTxtFmt, or -loadVectorsBinFmt) has been compressed with "
            "gzip and should be decompressed. Requires gzip to be in the path.",
            &inputVectorsAreGzipped));

    options.clear(); options.insert("on"); options.insert("off");
    cmdLineParser.addOption(new StringCmdLineOption("fixedLength", "\"on\" | \"off\"",
            "When on, this setting allows vector weights to be non-uniform "
            "(but always positive), and will calculate them based on the "
            "frequency counts in the frequency vector file given to -loadFVFile. "
            "When off, all vectors will be forced to have the same weight. "
            "Default is " + DEFAULT_FIXED_LENGTH + ".",
            &fixedLength, DEFAULT_FIXED_LENGTH, options));

    cmdLineParser.addOption(new IntCmdLineOption("numFVs", "n",
            "Number of frequency vectors in the un-projected frequency vector "
            "file. This option must be specified with -FVDim.",
            &numFreqVectors, DEFAULT_NUM_FREQ_VECTORS, 1));

    cmdLineParser.addOption(new IntCmdLineOption("FVDim", "n",
            "Number of dimensions in the un-projected frequency vector file. "
            "This option must be specified with -numFVs.",
            &numFVDims, DEFAULT_NUM_FREQ_DIMS, 1));

    cmdLineParser.addOption(new IntCmdLineOption("sampleSize", "n",
            "Number of vectors to choose for a sample to save time during "
            "k-means clustering. Default is to use all vectors.",
            &sampleSize, DEFAULT_SAMPLE_SIZE, -1));

    cmdLineParser.addOption(new IntCmdLineOption("seedkm", "seed",
            "Random seed for choosing initial k-means centers. This can be "
            "any integer. Default is " + toString(DEFAULT_RANDSEED_KMEANS_INIT)
            + ".", &randSeedKMeansInit, DEFAULT_RANDSEED_KMEANS_INIT));

    cmdLineParser.addOption(new IntCmdLineOption("seedproj", "seed",
            "Random seed for random linear projection. This can be "
            "any integer. Default is " + toString(DEFAULT_RANDSEED_PROJECTION) +
            ".", &randSeedProjection, DEFAULT_RANDSEED_PROJECTION));

    cmdLineParser.addOption(new IntCmdLineOption("seedsample", "seed",
            "Random seed for sub-sampling frequency vectors prior to "
            "clustering. This can be set to any integer. Default is " +
            toString(DEFAULT_RANDSEED_SAMPLE) + ".", &randSeedSample,
            DEFAULT_RANDSEED_SAMPLE));

    cmdLineParser.addOption(new IntCmdLineOption("verbose", "level",
            "Level of verbosity in output (>= 0). Default is 0.",
            &verboseLevel, 0, 0));

    learnKFromFile = false;
}


bool SimpointOptions::parseOptions(int argc, char **argv) {
    if (! cmdLineParser.parseCmdLine(argc, argv)) {
        Logger::log() << "Error in parsing command line options:\n"
             << cmdLineParser.getErrorMsg() << endl;
        return false;
    }

    Logger::setLoggingLevel(verboseLevel);

    // check that the user has specified options in a way that is sensible
    string errMessage;
    if (! validateOptions(&errMessage)) {
        Logger::log() << "Error in combination of command-line options:\n";
        Logger::log() << errMessage << endl;
        return false;
    }

    return true;
}


bool SimpointOptions::validateOptions(string *errMsg) {
    // exactly one of loadVectorsTxtFmt or loadVectorsBinFmt or loadFVFile must be specified
    int numLoadOptions = (loadVectorsTxtFmtName != "") +
        (loadVectorsBinFmtName != "") + (frequencyVectorFileName != "");
    if ((numLoadOptions == 0) || (numLoadOptions > 1)) {
        *errMsg = "Exactly one of -loadVectorsTxtFmt, -loadVectorsBinFmt, or "
            "-loadFVFile must be specified";
        return false;
    }

    // check that the user has not specified an initial set of centers or labels
    // and also specified -k
    if ((loadInitialLabelsName != "") || (loadInitialCentersName != "")) {
        if (cmdLineParser.findOption("k")->isSpecified()) {
            *errMsg = "Cannot specify -k when loading initial centers or labels; "
                      "the k value comes from the loaded file";
            return false;
        }

        if (cmdLineParser.findOption("numInitSeeds")->isSpecified()) {
            *errMsg = "Cannot specify -numInitSeeds when loading initial "
                "centers or labels; only the given initialization is used";
            return false;
        }

        if (cmdLineParser.findOption("seedkm")->isSpecified()) {
            *errMsg = "Cannot specify -seedkm when loading initial "
                "centers or labels; only the given initialization is used";
            return false;
        }

        if (cmdLineParser.findOption("initkm")->isSpecified()) {
            *errMsg = "Cannot specify -initkm when loading initial "
                "centers or labels; only the given initialization is used";
            return false;
        }

        if (cmdLineParser.findOption("maxK")->isSpecified()) {
            *errMsg = "Cannot specify -maxK when loading initial "
                "centers or labels; only the given initialization is used";
            return false;
        }

        if (cmdLineParser.findOption("bicThreshold")->isSpecified()) {
            *errMsg = "Should not specify -bicThreshold when loading initial "
                "centers or labels, as only the given centers are used (no "
                "search is performed)";
            return false;
        }

        // if the user did specify an initialization, find the k value from the
        // user-provided file
        learnKFromFile = true;
        useBinarySearch = false;

        numInitSeeds = 1;
    }


    // if using binary search, maxK must be specified
    if (useBinarySearch && (! cmdLineParser.findOption("maxK")->isSpecified())) {
        *errMsg = "When -k \"search\" is used, -maxK must also be specified";
        return false;
    }

    // if not using binary search, maxK has no effect
    if ((! useBinarySearch) && cmdLineParser.findOption("maxK")->isSpecified()) {
        *errMsg = "When specific values are given for -k, -maxK has no effect "
            "and should not be specified";
        return false;
    }

    // if loading from projected data and specifying -dim, -dim has no effect
    bool preProjected = (loadVectorsTxtFmtName != "") || (loadVectorsBinFmtName != "");
    if (preProjected) {
        if (cmdLineParser.findOption("dim")->isSpecified()) {
            *errMsg = "When loading pre-projected data with -loadVectorsTxtFmt or "
                      "-loadVectorsBinFmt, -dim has no effect and should not be specified";
            return false;
        }

    }

    if (cmdLineParser.findOption("seedproj")->isSpecified()) {
        if (preProjected) {
            *errMsg = "When loading pre-projected data with -loadVectorsTxtFmt or "
                      "-loadVectorsBinFmt, -seedproj has no effect and should "
                      "not be specified";
            return false;
        }

        if ((loadProjMatrixTxtFmtName != "") || (loadProjMatrixBinFmtName != "")) {
            *errMsg = "Loading a projection matrix and specifying a projection "
                "seed are incompatible options";
            return false;
        }
    }


    // noProject has no effect with seedproj and loading pre-projected data
    if (useNoProjection) {
        if ((saveProjMatrixTxtFmtName != "") || (saveProjMatrixBinFmtName != "")) {
            *errMsg = "No projection matrix is used when using -dim noProject; "
                "-saveProjMatrixTxtFmt and -saveProjMatrixBinFmt have no effect";
            return false;
        }

        if (cmdLineParser.findOption("seedproj")->isSpecified()) {
            *errMsg = "No projection matrix is used when using -dim noProject; "
                "-seedproj has no effect";
            return false;
        }
    }

    // if loading simpoint-vector data, no projection matrix is used
    if (((loadProjMatrixTxtFmtName != "") || (loadProjMatrixBinFmtName != "") ||
         (saveProjMatrixTxtFmtName != "") || (saveProjMatrixBinFmtName != "")) &&
        ((loadVectorsTxtFmtName != "") || (loadVectorsBinFmtName != ""))) {
        *errMsg = "Cannot load or save a projection matrix when vectors are "
            "loaded with -loadVectorsTxtFmt or -loadVectorsBinFmt";
        return false;
    }


    // -FVDim and -numFVs must be specified together, and only with -loadFVFile
    bool fvd_spec = cmdLineParser.findOption("FVDim")->isSpecified();
    bool nfv_spec = cmdLineParser.findOption("numFVs")->isSpecified();
    if ((fvd_spec || nfv_spec) && preProjected) {
        *errMsg = "When loading with -loadVectorsTxtFmt or -loadVectorsBinFmt,"
                  "-numFVs and -FVDim have no effect and should not be specified";
        return false;
    }
    if ((fvd_spec && (! nfv_spec)) || ((! fvd_spec) && nfv_spec)) {
        *errMsg = "Both -numFVs and -FVDim must be specified together";
        return false;
    }

    // check that the user has not specified two projection matrices
    if ((loadProjMatrixTxtFmtName != "") && (loadProjMatrixBinFmtName != "")) {
        *errMsg = "Only one of -loadProjMatrixTxtFmt and -loadProjMatrixBinFmt "
            "may be specified";
        return false;
    }

    // check that the user has not specified a projection matrix with
    // pre-projected data
    if (preProjected && ((loadProjMatrixTxtFmtName != "") ||
                (loadProjMatrixBinFmtName != ""))) {
        *errMsg = "Loading a projection matrix has no effect on data loaded with "
                  "-loadVectorsTxtFmt or -loadVectorsBinFmt";
        return false;
    }

    // check that the user has not specified the k-means initialization type and
    // an initial set of labels
    if (cmdLineParser.findOption("initkm")->isSpecified() &&
        ((loadInitialLabelsName != "") || (loadInitialCentersName != ""))) {
        *errMsg = "Loading initial labels or centers is incompatible with "
                  "specifying the k-means initialization method";
        return false;
    }

    // loading initial centers and labels is incompatible
    if ((loadInitialLabelsName != "") && (loadInitialCentersName != "")) {
        *errMsg = "Cannot specify both -loadInitCtrs and -loadInitLabels";
        return false;
    }

    // fixed length vectors setting should be consistent with loading data from
    // pre-projected data files, which may have non-uniform weights
    if ((! cmdLineParser.findOption("fixedLength")->isSpecified()) && preProjected) {
        *errMsg = "You should specify the -fixedLength option (on or off) "
                  "explicitly when using -loadVectorsTxtFmt or -loadVectorsBinFmt";
        return false;
    }

    // assume that if the user provides weights, they want fixed-length = off
    if (loadVectorWeightsName != "") {
        fixedLength = "off";
    }

    // fixedLength and loadVectorWeights are incompatible
    if ((fixedLength == "on") && 
            cmdLineParser.findOption("loadVectorWeights")->isSpecified()) {
        *errMsg = "Fixed-length vectors (-fixedLength on) is incompatible "
                  "with -loadVectorWeights";
        return false;
    }

    // check that if the user specifies a sample seed, he also specifies the
    // sample size
    if (cmdLineParser.findOption("seedsample")->isSpecified() &&
            (! cmdLineParser.findOption("sampleSize")->isSpecified())) {
        *errMsg = "When specifying -seedsample, you must also specify -sampleSize";
        return false;
    }

    // coveragePct only has an effect on two options
    if (cmdLineParser.findOption("coveragePct")->isSpecified() &&
        ((saveSimpointWeightsName == "") && (saveSimpointsName == ""))) {
        *errMsg = "When specifying -coveragePct, you must also specify "
            "-saveSimpoints and/or -saveSimpointWeights";
        return false;
    }

    return true;
}


void SimpointOptions::usage(const char *myName) {
    Logger::log() << "usage: " << myName << " [options]\n";
    cmdLineParser.printExplanationsPretty(Logger::log());
}

void SimpointOptions::printOptionSettings(ostream &os) const {
    for (unsigned int i = 0; i < cmdLineParser.getNumOptions(); i++) {
        const CmdLineOption *opt = cmdLineParser.getOption(i);
        if (opt->isSpecified()) {
            os << "*** ";
        } else {
            os << "    ";
        }
        os << opt->getPrettyValue() << endl;
    }
}



/* Split the string s into multiple parts based on the character c */
vector<string> split(const string &s, char c) {
    vector<string> parts;
    unsigned long start = 0, end;
    while (start < s.size()) {
        end = s.find(c, start);
        if (end == string::npos) {
            parts.push_back(s.substr(start));
            start = s.size();
        } else {
            parts.push_back(s.substr(start, end - start));
            start = end + 1;
        }
    }

    return parts;
}

bool NumClustersCmdLineOption::parseArgumentSub(const string &argument) {
    *searchTarget = (argument == "search");

    if (! *searchTarget) {
        kValuesTarget->clear();
        int k;
        vector<string> ranges = split(argument, ',');
        for (unsigned int i = 0; i < ranges.size(); i++) {
            vector<string> kRange = split(ranges[i], ':');
            if (kRange.size() == 1) {
                k = atoi(kRange[0].c_str());
                if (k < 1) {
                    setParseError("k value cannot be less than 1");
                    return false;
                }
                kValuesTarget->push_back(k);
            } else if (kRange.size() == 2) {
                int start = atoi(kRange[0].c_str()), end = atoi(kRange[1].c_str());
                if ((start < 1) || (end < 1)) {
                    setParseError("k values cannot be less than 1");
                }
                if (start < end) {
                    for (k = start; k <= end; k++) { kValuesTarget->push_back(k); }
                } else {
                    for (k = start; k >= end; k--) { kValuesTarget->push_back(k); }
                }
            } else if (kRange.size() == 3) {
                int start = atoi(kRange[0].c_str()), step = atoi(kRange[1].c_str()),
                    end = atoi(kRange[2].c_str());
                if ((start < 1) || (end < 1)) {
                    setParseError("k values cannot be less than 1");
                    return false;
                } else if (step == 0) {
                    setParseError("step value cannot be 0");
                    return false;
                } else if (((start < end) && (step < 0)) ||
                           ((end < start) && (step > 0))) {
                    step = -step; // silently fix for the user
                }
                if (start < end) {
                    for (k = start; k <= end; k += step) { kValuesTarget->push_back(k); }
                } else {
                    for (k = start; k >= end; k += step) { kValuesTarget->push_back(k); }
                }
            } else {
                setParseError("invalid range specification");
                return false;
            }
        }
    }

    return true;
}


