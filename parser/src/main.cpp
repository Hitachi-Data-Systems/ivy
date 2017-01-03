//Copyright (c) 2016 Hitachi Data Systems, Inc.
//All Rights Reserved.
//
//   Licensed under the Apache License, Version 2.0 (the "License"); you may
//   not use this file except in compliance with the License. You may obtain
//   a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
//   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
//   License for the specific language governing permissions and limitations
//   under the License.
//
//Author: Allart Ian Vogelesang <ian.vogelesang@hds.com>
//
//Support:  "ivy" is not officially supported by Hitachi Data Systems.
//          Contact me (Ian) by email at ian.vogelesang@hds.com and as time permits, I'll help on a best efforts basis.
#include <iostream>
#include <stdio.h>
#include <regex>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "Ivy_pgm.h"
#include "ivy.parser.hh"
#include "ivyhelpers.h"
#include "master_stuff.h"
#include "MeasureDFC.h"

std::string startup_log_file {"~/ivy_startup.txt"};

std::ostringstream startup_ostream;

std::string inter_statement_divider {"=================================================================================================="};

bool routine_logging {false};

void usage_message(char* argv_0)
{
    std::cout << std::endl << "Usage: " << argv_0 << " [options] <ivyscript file name to open>" << std::endl << std::endl
        << "where \"[options]\" means zero or more of:" << std::endl << std::endl
        << "-log" << std::endl
        << "     Turns on logging of routine events." << std::endl << std::endl
        << "-trace_lexer" << std::endl
        << "     Log routine events and trace the \"lexer\" which breaks down the .ivyscript program into \"tokens\"." << std::endl << std::endl
        << "-trace_parser" << std::endl
        << "     Log routine events and trace the \"parser\" which recognizes the syntax of the stream of tokens as a program." << std::endl << std::endl
        << "-trace_evaluate" << std::endl
        << "     Log routine events and trace the execution of the compiled ivyscript program." << std::endl << std::endl
        << "-t" << std::endl
        << "     Same as -trace_lexer -trace_parser -trace_evaluate." << std::endl << std::endl
        << "The .ivyscript suffix on the name of the ivyscript program is optional." << std::endl << std::endl
        << "Examples:" << std::endl
        << "\t" << argv_0 << " xxxx.ivyscript" << std::endl
        << "\t" << argv_0 << " xxxx" << std::endl
        << "\t" << argv_0 << " -log xxxx" << std::endl
        << "\t" << argv_0 << " -t xxxx" << std::endl
        ;
}

int main(int argc, char* argv[])
{
	std::string ivyscriptFilename {};

    if (argc==1) { usage_message(argv[0]); return -1; }

    for (int arg_index = 1 /*skipping executable name*/ ; arg_index < argc ; arg_index++ )
    {
        std::string item {argv[arg_index]};

        if (item == "-log")            { routine_logging = true; continue; }
        if (item == "-t")              { routine_logging = trace_lexer = trace_parser = trace_evaluate = true; continue; }
        if (item == "-trace_lexer")    { routine_logging = trace_lexer = true; continue; }
        if (item == "-trace_parser")   { routine_logging = trace_parser = true; continue; }
        if (item == "-trace_evaluate") { routine_logging = trace_evaluate = true; continue; }

        if (arg_index != (argc-1)) { usage_message(argv[0]); return -1; }

        ivyscriptFilename = item;
    }

    auto pear = looksLikeFilename(ivyscriptFilename);

    if (!pear.first) // returns true if all alphanumerics, underscores, periods (dots), single slashes, or single backslashes
    {
        std::cout << pear.second;
        return -1;
    }

    if (!endsIn(ivyscriptFilename, ".ivyscript")) ivyscriptFilename += std::string(".ivyscript");

    std::regex ivyscript_filename_regex(R"ivy((.*[/])?([a-zA-Z][_\.[:alnum:]]*)(\.ivyscript))ivy");
    // Three sub matches
    // - optional path part,
    // - root part, which is an identifier, possibly with embedded periods,
    // - and the .ivyscript suffix, which was added if the user left it off
    std::smatch entire_match; // we will pick out the test name part - submatch 2, the identifier, possibly with embedded periods.
    std::ssub_match test_name_sub_match;

    if (!std::regex_match(ivyscriptFilename, entire_match, ivyscript_filename_regex))
    {
        std::cout << "ivyscript filename \"" + ivyscriptFilename + "\" - bad filename or programming error extracting test name portion of filename." << std::endl;
        return -1;
    }
    if (entire_match.size() != 4)
    {
        std::cout << "ivyscript filename \"" + ivyscriptFilename + "\" - programming error extracting test name portion of filename.  "
                  << std::endl << entire_match.size() << " submatches found - expecting 4." << std::endl;
        return -1;
    }

    test_name_sub_match = entire_match[2];

    std::string test_name = test_name_sub_match.str();

    if (!isValidTestNameIdentifier(test_name))
    {
        std::cout << "The ivy \"test name\" is the part of the filename before the optional .ivyscript." << std::endl
                  << "The test name must consist of entirely alphanumerics, underscores, and periods (dots)." << std::endl
                  << "Periods (dots) may not be used as first or last characters, and sequences of " << std::endl
                  << "two consecutive periods are not permitted." << std::endl;
        return -1;
    }

    struct stat struct_stat;
    if(stat(ivyscriptFilename.c_str(),&struct_stat))
    {
        std::cout << "<Error> ivyscript filename \"" + ivyscriptFilename + "\" does not exist." << std::endl;
        return -1;
    }

    if(!S_ISREG(struct_stat.st_mode))
    {
        std::cout << "<Error> ivyscript filename \"" + ivyscriptFilename  + "\" is not a regular file." << std::endl;
        return -1;
    }


    extern FILE* yyin;

    if( !(yyin = fopen(ivyscriptFilename.c_str(), "r")) )
    {
        std::cout << "<Error> Could not open \"" << ivyscriptFilename << "\" for in." << std::endl;
        return -1;
    }


    Ivy_pgm  context(ivyscriptFilename,test_name);

    p_Ivy_pgm = &context;


	yy::ivy parser(context);

	parser.parse();

    fclose(yyin);

    if (trace_parser) { context.snapshot(std::cout); }

	if (context.successful_compile && !context.unsuccessful_compile )
	{
        std::cout << "Successful compile, test name is \"" << test_name << "\"." << std::endl;


        try {
            context.stacktop = context.stack + context.next_avail_static;
            context.executing_frame = context.next_avail_static;

            if (trace_evaluate)
            {
                trace_ostream << "main.cpp: before invoking p_global_Block, context.next_avail_static = " << context.next_avail_static
                    << ", context.stack = " << &(*context.stack)
                    << ", stacktop offset from stack = " << (context.stacktop - context.stack)
                    << ", executing_frame = " << context.executing_frame
                    << std::endl;
            }

            context.p_global_Block()->invoke();
        }
        catch (...)
        {
            std::cout << "<Error> Whoops!  Something happened trying to run the ivy program." << std::endl;
            throw;
        }
        m_s.overall_success = true;
	}
	else
	{
        std::cout << "<Error> unsuccessful compile." << std::endl;
    }

    ivytime finish_time; finish_time.setToNow();
    ivytime duration = finish_time - m_s.test_start_time;

    {

        std::ostringstream o;
        o << std::endl << std::endl << "********* ivy run complete.  Total run time " << duration.format_as_duration_HMMSS() << " for test name \"" << m_s.testName << "\" *********" << std::endl << std::endl;
        o << m_s.step_times.str();
        std::cout << o.str();
        log(m_s.masterlogfile, o.str());
    }

    std::pair<bool,std::string> strc = m_s.shutdown_subthreads();

    {
        std::ostringstream o;
        o << "ivy engine shutdown subthreads ";
        if (strc.first) o << "successful"; else o << "unsuccessful";
        o << std::endl;
        std::cout << o.str();
        log (m_s.masterlogfile,o.str());
    }

	return 0; // in case compiler complains, but unreachable.

}

