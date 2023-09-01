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

#ifndef CMD_LINE_PARSER_H
#define CMD_LINE_PARSER_H


/***********************************************************************
 * File: CmdLineParser.h
 * Author: Greg Hamerly
 * Date: 5/31/2005
 *
 * The command line parser is used for parsing command-line arguments.
 ***********************************************************************/

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include "Utilities.h"

using namespace std;

/* A CmdLineOption is an abstract base class that specific command line
 * options should use for implementation. The intent of this class is to hold
 * relevant information about the option (e.g. name, whether it receives an
 * argument, its explanation) and to be able to parse any arguments that are
 * specified.
 */
class CmdLineOption {
    public:
        CmdLineOption(const string &option_name,
                bool requires_argument,
                const string &argument_name,
                const string &the_explanation) {
            name = option_name;
            argumentName = argument_name;
            explanation = the_explanation;
            argumentRequired = requires_argument;
            specified = false;
        }

        virtual ~CmdLineOption() {}

        const string &getName() const { return name; }
        const string &getArgumentName() const { return argumentName; }
        const string &getExplanation() const { return explanation; }

        // true if this option does require an argument
        bool requiresArgument() const { return argumentRequired; }

        // "specified" means that the user gave the option on the command line
        bool isSpecified() const { return specified; }

        // returns true if the argument parsed successfully, false otherwise
        // subclasses should override parseArgumentSub()
        bool parseArgument(const string &argument) {
            return requiresArgument() ? parseArgumentSub(argument) : false;
        }

        // tell this option that it has been specified
        virtual void setSpecified() { specified = true; }

        // gets a pretty, printable option-and-value string that describes the
        // state of the option 
        string getPrettyValue() const {
            return "-" + name + " : " + getPrettyValueSub();
        }

        // returns the parsing error message, if there was an error when
        // parsing the argument
        string getParseError() const { return parseErrorMessage; }

    protected:
        // subclasses should override to parse the given argument, and return
        // true on successful parse, false otherwise
        virtual bool parseArgumentSub(const string &argument) = 0;

        // subclasses should override to provide a "pretty" version of the
        // value
        virtual string getPrettyValueSub() const = 0;

        // internal function for setting the parse error message
        void setParseError(const string &msg) { parseErrorMessage = msg; }

    private:
        // The name is the actual value that the user specifies on the command
        // line, minus the "-" (e.g. "numberOfIterations").
        // The argumentName is a brief, pretty name that is printed to explain
        // the argument (e.g. "n" or "seed" or "filename").
        // The explanation is a long string that explains this option in detail.
        // The parseError is the error obtained while parsing the argument.
        string name, argumentName, explanation, parseErrorMessage;

        // argumentRequired is true if the option requires an argument
        // specified is true if the option was specified on the command line
        bool argumentRequired, specified;
};


/* A CmdLineParser class does several things. It holds all of the command line
 * options that can be used, it parses the command line and tells the
 * appropriate command line option objects when they have been specified, it
 * allows access to the various options, and it can print out all the options
 * in a readable fashion.
 */
class CmdLineParser {
    public:
        CmdLineParser() {}

        virtual ~CmdLineParser() {
            for (unsigned int i = 0; i < allOptions.size(); i++) {
                delete allOptions[i];
            }
            optionNameToNdxMap.clear();
        }

        // Adds the given option to the parser. The option must be allocated on
        // the heap. The parser assumes ownership of the memory and will delete
        // it.
        void addOption(CmdLineOption *option) {
            Utilities::check(option != NULL,
                    "CmdLineParser::addOption(): option cannnot be NULL");
            Utilities::check(findOption(option->getName()) == NULL, 
                    "CmdLineParser::addOption(): option " + option->getName() +
                    " is already added");
            optionNameToNdxMap[option->getName()] = allOptions.size();
            allOptions.push_back(option);
        }

        // Finds the command line option with the given name, returning a
        // pointer to the option (or NULL if not found).
        const CmdLineOption *findOption(const string &name) const {
            map<string, int>::const_iterator itr = optionNameToNdxMap.find(name);
            return (itr == optionNameToNdxMap.end()) ? NULL : allOptions[itr->second];
        }

        // Finds the command line option with the given name, returning a
        // pointer to the option (or NULL if not found).
        CmdLineOption *findOption(const string &name) {
            map<string, int>::iterator itr = optionNameToNdxMap.find(name);
            return (itr == optionNameToNdxMap.end()) ? NULL : allOptions[itr->second];
        }

        // This function allows the program to specify an option explicitly,
        // rather than letting the user provide it on the command line.
        bool specifyOption(const string &name, const string &argument = "");

        // Returns the number of options that have been added to this parser
        unsigned int getNumOptions() const { return allOptions.size(); }

        // Returns the option at the given index (where indexes go in the order
        // the options were added)
        const CmdLineOption *getOption(unsigned int ndx) const {
            Utilities::check((0 <= ndx) && (ndx < allOptions.size()),
                "CmdLineOption::getOption(unsigned int) const: ndx out of bounds");
            return allOptions[ndx];
        }

        // Returns the option at the given index (where indexes go in the order
        // the options were added)
        CmdLineOption *getOption(unsigned int ndx) {
            Utilities::check((0 <= ndx) && (ndx < allOptions.size()),
                "CmdLineOption::getOption(unsigned int): ndx out of bounds");
            return allOptions[ndx];
        }

        // This is the main function for this class, and it parses the command
        // line starting with argv[1]. For each option given on the command
        // line, the parser finds the corresponding option and tells it it was
        // specified, and tells it to parse its argument.
        bool parseCmdLine(int argc, char **argv);

        // Print all of the explanations for all the command line options, in
        // the order in which they were added. Useful for making a "help" text
        // for a program.
        void printExplanationsPretty(ostream &os) const;

        // Return any extra arguments, if there were any.
        const vector<string> &getExtraArguments() const { return extraArguments; }

        // Gets the error message (if any) that occurred while parsing the
        // command line options.
        const string &getErrorMsg() const { return parseErrorMessage; }

    private:
        vector<CmdLineOption *> allOptions;
        map<string, int> optionNameToNdxMap;
        vector<string> extraArguments;

        string parseErrorMessage;

        // finds the required prefix column width for pretty-printing the
        // explanations
        int calculatePrefixColumnWidth() const;

        // prints one option in a pretty format
        static void printExplanationPretty(const CmdLineOption *option, 
                                    unsigned int prefixLength, ostream &os,
                                    unsigned int lineWidth);

        // returns an option printed pretty 
        static string prettyOptionNameAndArg(const CmdLineOption *option);


        // sets the parser error message
        void parseError(const string &errMessage) {
            parseErrorMessage = errMessage;
        }
};


#endif

