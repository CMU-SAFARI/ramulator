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


#include "CmdLineParser.h"

bool CmdLineParser::parseCmdLine(int argc, char **argv) {
    for (int argNdx = 0; argNdx < argc; argNdx++) {
        const char *arg = argv[argNdx];
        Utilities::check(arg != NULL, 
                "CmdLineParser::parseCmdLine(): found NULL argument");
        if (arg[0] == '\0') { // change by shen
            parseError("received a command line option that was empty");
            return false;
        }

        if (arg[0] == '-') {
            string optionName(arg + 1);
            CmdLineOption *option = findOption(optionName);
            if (option == NULL) {
                parseError(string("Unknown option: ") + optionName);
                return false;
            }

            // tell this option that it was specified
            option->setSpecified();

            string argument;
            if (option->requiresArgument()) {
                if (argNdx >= argc - 1) {
                    parseError(string("Argument required for option ") +
                               option->getName());
                    return false;
                }

                // don't check if the next token is actually another option; in
                // this case the user must figure it out...
                argNdx++;
                argument = string(argv[argNdx]);

                if (! option->parseArgument(argument)) {
                    parseError("Parsing argument -" + option->getName() + 
                                " : " + option->getParseError());
                    return false;
                }
            }
        } else {
            extraArguments.push_back(string(arg));
        }
    }

    return true;
}

void CmdLineParser::printExplanationsPretty(ostream &os) const {
    int prefixWidth = this->calculatePrefixColumnWidth();
    const unsigned int LINE_WIDTH = 80;
    for (unsigned int i = 0; i < allOptions.size(); i++) {
        printExplanationPretty(allOptions[i], prefixWidth, os, LINE_WIDTH);
    }
}

void CmdLineParser::printExplanationPretty(const CmdLineOption *option,
        unsigned int prefixWidth, ostream &os, unsigned int lineWidth) {
    prefixWidth += 3;  // to account for the string " : " below
    string nameAndArg = prettyOptionNameAndArg(option);
    os << nameAndArg;
    if (nameAndArg.size() < prefixWidth - 3) { os << ' '; }
    for (unsigned int i = nameAndArg.size() + 1; i < prefixWidth - 3; i++) {
        os << '.';
    }
    os << " : ";

    // strip the explanation of leading, trailing, and duplicate spaces
    string expl, explanation = option->getExplanation();
    unsigned int start = 0, end = explanation.size() - 1;
    while ((start < end) && (explanation[start] == ' ')) { start++; }
    while ((start < end) && (explanation[end] == ' ')) { end--; }
    for (unsigned int i = start; i <= end; i++) {
        expl += explanation[i];
        if (explanation[i] == ' ') {
            while ((i <= end) && (explanation[i+1] == ' ')) {
                i++;
            }
        }
    }

    unsigned int position = 0;
    bool firstLine = true;
    while (expl.size() - position > lineWidth - prefixWidth) {
        if (! firstLine) {
            for (unsigned int i = 0; i < prefixWidth; i++) {
                os << ' ';
            }
        }

        // find the first index of a space that is earlier than the limit of the
        // line width
        unsigned long searchStart = min((unsigned long)position + lineWidth - prefixWidth, 
                                        (unsigned long)expl.size() - 1);
        unsigned long spaceNdx = expl.rfind(' ', searchStart);
        if ((spaceNdx == string::npos) || (spaceNdx <= position)) {
            spaceNdx = expl.find(' ', searchStart);
            if (spaceNdx == string::npos) {
                spaceNdx = expl.size();
            }
        }
        os << expl.substr(position, spaceNdx - position) << endl;
        position = spaceNdx + 1;
        firstLine = false;
    }

    if (position < expl.size()) {
        if (! firstLine) {
            for (unsigned int i = 0; i < prefixWidth; i++) {
                os << ' ';
            }
        }
        os << expl.substr(position) << endl;
    }
}

int CmdLineParser::calculatePrefixColumnWidth() const {
    int prefixWidth = 0;
    for (unsigned int i = 0; i < allOptions.size(); i++) {
        prefixWidth = max(prefixWidth,
                          (int)prettyOptionNameAndArg(allOptions[i]).size());
    }
    return prefixWidth;
}


string CmdLineParser::prettyOptionNameAndArg(const CmdLineOption *option) {
    string nameAndArg = string("  -") + option->getName();
    if (option->requiresArgument()) {
        nameAndArg = nameAndArg + string(" ") + option->getArgumentName();
    }

    return nameAndArg;
}


bool CmdLineParser::specifyOption(const string &name, const string &argument) {
    CmdLineOption *opt = findOption(name);
    if (opt == NULL) { return false; }

    opt->setSpecified();
    if (opt->requiresArgument()) {
        if (argument == "") {
            parseError("CmdLineOption::specifyOption() need an argument, but "
                    "none was specified");
            return false;
        }

        if (! opt->parseArgument(argument)) {
            parseError("Parsing argument -" + opt->getName() + 
                        " : " + opt->getParseError());
            return false;
        }
    } else {
        Utilities::check(argument == "", "CmdLineOption::specifyOption() "
                "argument given, but none required");
        if (argument != "") {
            parseError("CmdLineOption::specifyOption() argument given, but "
                    "none allowed");
            return false;
        }
    }

    return true;
}


