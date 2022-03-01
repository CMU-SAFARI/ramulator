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


#ifndef SIMPOINT_OPTIONS_H
#define SIMPOINT_OPTIONS_H

/***********************************************************************
 * File: SimpointOptions.h
 * Author: Greg Hamerly
 * Date: 5/31/2005
 *
 * This file defines the command line option classes that are used in SimPoint
 * and the SimpointOptions class which is used to define the behavior of
 * SimPoint while it executes.
 ***********************************************************************/

#include "CmdLineParser.h"
#include "Utilities.h"
#include <climits>
#include <cfloat>
#include <set>

/*****************************************************************************/
/* Generic command line options                                              */
/*****************************************************************************/

/* This class represents a "flag" command line option -- one whose presence
 * indicates an option has been specified, but does not take an argument.
 */
class FlagCmdLineOption : public CmdLineOption {
    public:
        FlagCmdLineOption(const string &option_name, 
                const string &the_explanation, bool *flag_target) :
            CmdLineOption(option_name, false, string(""), the_explanation) {
            Utilities::check(flag_target != NULL,
                    "FlagCmdLineOption constructor: flag_target cannnot be NULL");
            flagTarget = flag_target;
            *flagTarget = false;
        }

        virtual void setSpecified() {
            *flagTarget = true;
            CmdLineOption::setSpecified();
        }

    protected:
        virtual bool parseArgumentSub(const string &argument) {
            setParseError("no argument should be specified");
            return false;
        }

        virtual string getPrettyValueSub() const {
            return isSpecified() ? "true" : "false";
        }

    private:
        bool *flagTarget;
};

/* An IntCmdLineOption is for a command line option that takes a single integer
 * as argument. This class will verify the validity of the argument as being in
 * a specified range (default is all integers).
 */
class IntCmdLineOption : public CmdLineOption {
    public:
        IntCmdLineOption(const string &option_name, const string &argument_name,
                const string &the_explanation, int *int_target, int defaultValue,
                int min_valid_value = INT_MIN,
                int max_valid_value = INT_MAX) : 
            CmdLineOption(option_name, true, argument_name, the_explanation) {
            Utilities::check(int_target != NULL,
                    "IntCmdLineOption constructor: int_target cannnot be NULL");
            intTarget = int_target;
            *intTarget = defaultValue;
            minValidValue = min_valid_value;
            maxValidValue = max_valid_value;
        }

        int getIntValue() const { return *intTarget; }

    protected:
        virtual bool parseArgumentSub(const string &argument) {
            int value = atoi(argument.c_str());
            if ((value < minValidValue) || (value > maxValidValue)) {
                setParseError("integer value is out of valid range of " 
                        + toString(minValidValue) + " to " 
                        + toString(maxValidValue));
                return false;
            }
            *intTarget = value;
            return true;
        }

        virtual string getPrettyValueSub() const {
            return toString(*intTarget);
        }

    private:
        int *intTarget;
        int maxValidValue, minValidValue;
};

/* A StringCmdLineOption is for a command line option that takes a single string
 * as argument. By default this class will accept any string, but it may also accept
 * a limited set of strings, specified to the constructor.
 */
class StringCmdLineOption : public CmdLineOption {
    public:
        StringCmdLineOption(const string &option_name, const string &argument_name,
                const string &the_explanation, string *string_target, 
                const string &defaultValue = string(),
                const set<string> arg_options = set<string>()) :
            CmdLineOption(option_name, true, argument_name, the_explanation) {
            Utilities::check(string_target != NULL,
                    "StringCmdLineOption constructor: string_target cannnot be NULL");
            stringTarget = string_target;
            *stringTarget = defaultValue;
            argumentOptions = arg_options;
        }

        const string &getStringValue() const { return *stringTarget; }

    protected:
        virtual bool parseArgumentSub(const string &argument) {
            if ((argumentOptions.size() > 0) &&
                    (argumentOptions.find(argument) == argumentOptions.end())) {
                setParseError("string value is not a valid option");
                return false;
            }
            *stringTarget = argument;
            return true;
        }

        virtual string getPrettyValueSub() const { return *stringTarget; }

    private:
        string *stringTarget;
        set<string> argumentOptions;
};


/* A DoubleCmdLineOption is for a command line option that takes a single double
 * as argument. This class will verify the validity of the argument as being in
 * a specified range (default is all real doubles).
 */
class DoubleCmdLineOption : public CmdLineOption {
    public:
        DoubleCmdLineOption(const string &option_name, const string &argument_name,
                const string &the_explanation, double *double_target, 
                double defaultValue,
                double min_valid_value = -DBL_MAX, 
                double max_valid_value = DBL_MAX) :
            CmdLineOption(option_name, true, argument_name, the_explanation) {
            Utilities::check(double_target != NULL,
                    "DoubleCmdLineOption constructor: double_target cannnot be NULL");
            doubleTarget = double_target;
            *doubleTarget = defaultValue;
            minValidValue = min_valid_value;
            maxValidValue = max_valid_value;
        }

        double getDoubleValue() const { return *doubleTarget; }

    protected:
        virtual bool parseArgumentSub(const string &argument) {
            double value = atof(argument.c_str());
            if ((value < minValidValue) || (value > maxValidValue)) {
                setParseError("double value is out of valid range of " 
                        + toString(minValidValue) + " to " 
                        + toString(maxValidValue));
                return false;
            }
            *doubleTarget = value;
            return true;
        }

        virtual string getPrettyValueSub() const {
            return toString(*doubleTarget);
        }

    private:
        double *doubleTarget;
        double minValidValue, maxValidValue;
};

/*****************************************************************************/
/* Specific command line options                                             */
/*****************************************************************************/

/* This class can recognize a particular regular expression for the purpose of 
 * finding the way the user wants to search through a range of values for k
 * (either with "search" or a specified set of ranges).
 */
class NumClustersCmdLineOption : public CmdLineOption {
    public:
        NumClustersCmdLineOption(const string &option_name,
                const string &argument_name, const string &the_explanation,
                bool *search_target,
                vector<int> *kvalues_target) :
            CmdLineOption(option_name, true, argument_name, the_explanation) {
            Utilities::check(search_target != NULL,
                "NumClustersCmdLineOption constructor: search_target cannnot be NULL");
            Utilities::check(kvalues_target != NULL,
                "NumClustersCmdLineOption constructor: kvalues_target cannnot be NULL");
            searchTarget = search_target;
            kValuesTarget = kvalues_target;
            *searchTarget = true;
        }

        bool isSearch() const { return *searchTarget; }
        const vector<int> &getKValues() const { return *kValuesTarget; }

    protected:
        virtual bool parseArgumentSub(const string &argument);

        virtual string getPrettyValueSub() const {
            string s;
            if (*searchTarget) {
                s = "search";
            } else {
                for (unsigned int i = 0; i < kValuesTarget->size(); i++) {
                    if (i > 0) { s += ","; }
                    s += toString((*kValuesTarget)[i]);
                }
            }
            return s;
        }

    private:
        bool *searchTarget;
        vector<int> *kValuesTarget;
};

/* This class can recognize either the word "noProject" or a single integer,
 * which indicates the dimension that should be used to project down to.
 */
class DimensionCmdLineOption : public IntCmdLineOption {
    public:
        DimensionCmdLineOption(const string &option_name,
                const string &argument_name, const string &the_explanation,
                bool *noproject_target,
                int *dimension_target, int defaultDimension) :
            IntCmdLineOption(option_name, argument_name, the_explanation,
                    dimension_target, defaultDimension, 1) {
            Utilities::check(noproject_target != NULL,
                "DimensionCmdLineOption constructor: noproject_target cannnot be NULL");
            noProjectTarget = noproject_target;
            *noProjectTarget = false;
        }

        bool usesNoProject() const { return *noProjectTarget; }

    protected:
        virtual bool parseArgumentSub(const string &argument) {
            if (argument == string("noProject")) {
                *noProjectTarget = true;
                return true;
            }
            return IntCmdLineOption::parseArgumentSub(argument);
        }

        virtual string getPrettyValueSub() const {
            string s;
            if (*noProjectTarget) { s = "noProject"; }
            else { s = toString(getIntValue()); }
            return s;
        }

    private:
        bool *noProjectTarget;
};

/* This class can recognize either the word "off" or a single integer,
 * which indicates the maximum number of k-means iterations that should be
 * performed.
 */
class NumItersCmdLineOption : public IntCmdLineOption {
    public:
        NumItersCmdLineOption(const string &option_name,
                const string &argument_name, const string &the_explanation,
                bool *no_iter_limit_target, int *num_iters_target,
                int defaultNumIters) :
            IntCmdLineOption(option_name, argument_name, the_explanation,
                    num_iters_target, defaultNumIters, 0) {
            Utilities::check(no_iter_limit_target != NULL,
                "NumItersCmdLineOption constructor: no_iter_limit_target cannnot be NULL");
            noIterLimitTarget = no_iter_limit_target;
            *noIterLimitTarget = false;
        }

        bool usesNoIterLimit() const { return *noIterLimitTarget; }

    protected:
        virtual bool parseArgumentSub(const string &argument) {
            if (argument == string("off")) {
                *noIterLimitTarget = true;
                return true;
            }
            return IntCmdLineOption::parseArgumentSub(argument);
        }

        virtual string getPrettyValueSub() const {
            return *noIterLimitTarget ? "off" : toString(getIntValue());
        }

    private:
        bool *noIterLimitTarget;
};



class SimpointOptions {
    public:
        // METHODS/CONSTRUCTOR
        SimpointOptions();

        bool parseOptions(int argc, char **argv);
        void usage(const char *name);
        void printOptionSettings(ostream &os) const;

        // CLASS CONSTANTS (PROGRAM DEFAULTS)
        static const int DEFAULT_KMEANS_ITERATIONS;
        static const int DEFAULT_DIMENSIONS;
        static const int DEFAULT_RANDSEED_KMEANS_INIT;
        static const int DEFAULT_RANDSEED_PROJECTION;
        static const int DEFAULT_RANDSEED_SAMPLE;
        static const int DEFAULT_NUM_FREQ_VECTORS;
        static const int DEFAULT_NUM_FREQ_DIMS;
        static const int DEFAULT_SAMPLE_SIZE;
        static const int DEFAULT_MAX_K;
        static const int DEFAULT_NUM_INIT_SEEDS;
        static const string DEFAULT_KMEANS_INIT_TYPE;
        static const string DEFAULT_FIXED_LENGTH;
        static const double DEFAULT_COVERAGE_PERCENTAGE;
        static const double DEFAULT_BIC_THRESHOLD;


        // THE OPTIONS
        string frequencyVectorFileName;  // the filename to load frequency vectors
        bool useNoIterationLimit;        // if true, then use no iteration limit
        int numKMeansIterations;         // the max number of iterations to limit k-means
        bool useNoProjection;            // if true, then do not project frequency vectors
        int projectionDimension;         // the number of dims to project frequency vectors to
        bool useBinarySearch;            // if true, then use binary search to find best clustering
        bool learnKFromFile;             // if true, then find the k value from the loaded user initialization
        vector<int> kValues;             // the k-values to search over
        int randSeedKMeansInit;          // the random seed value for k-means initialization
        int randSeedProjection;          // the random seed value for projection
        int randSeedSample;              // the random seed value for sub-sampling
        int numFreqVectors;              // number of frequency vectors (when loading frequency vector file)
        int numFVDims;                   // number of frequency vector dimensions (when loading frequency vector file)
        int sampleSize;                  // the number of intervals to take as a sample prior to clustering
        int max_k;                       // the maximum k value to use when using binary search
        int numInitSeeds;                // the number of k-means initializations to try
        int verboseLevel;                // the level of verbosity (for the Logger)
        bool saveAll;                    // if true, then save specified outputs for all k values tried
        bool inputVectorsAreGzipped;     // if true, then the input vectors should be decompressed with gzip
        double coveragePct;              // the fraction of weights that should be reported (0...1)
        double bicThreshold;             // the BIC threshold (0...1)

        string kMeansInitType;           // "ff" = furthest-first, "samp" = random sample

        string fixedLength;              // if "on", then all vectors treated
                                         //with equal weight, otherwise weights may vary

        string saveLabelsName;           // file names for saving labels,
        string saveSimpointWeightsName;  // simpoint weights, vector weights,
        string saveVectorWeightsName;    // and simpoints
        string saveSimpointsName;

        string saveInitialCentersName;   // file names for saving initial
        string saveFinalCentersName;     // centers, final centers, vectors
        string saveVectorsTxtFmtName;    // (text or binary format), and
        string saveVectorsBinFmtName;    // projection matrix (text or binary
        string saveProjMatrixTxtFmtName; // format)
        string saveProjMatrixBinFmtName;

        string loadInitialCentersName;   // file names for loading initial
        string loadInitialLabelsName;    // centers, initial labels, vectors
        string loadVectorsTxtFmtName;    // (text or binary fromat), projection
        string loadVectorsBinFmtName;    // matrix (text or binary format), and
        string loadProjMatrixBinFmtName; // vector weights
        string loadProjMatrixTxtFmtName;
        string loadVectorWeightsName;

    private:
        // validates and potentially reassigns option values to make sure that
        // they correspond to correct use of SimPoint. Returns true if values
        // are fine, false otherwise.
        bool validateOptions(string *errMsg);

        // the command line parser used to get and set the options for this
        // class
        CmdLineParser cmdLineParser;
};


#endif

