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

#include "Ivy_pgm.h"
#include "host_range.h"
#include "SelectClause.h"
#include "ivy.parser.hh"
#include "ivyhelpers.h"
#include "master_stuff.h"
#include "MeasureDFC.h"
#include "ivybuilddate.h"

std::string startup_log_file {"~/ivy_startup.txt"};

std::ostringstream startup_ostream;

std::string inter_statement_divider {"---------------------"};

bool routine_logging {false};
void initialize_io_time_clip_levels();

int main(int argc, char* argv[])
{
    m_s.test_start_time.setToNow();

    initialize_io_time_clip_levels();

    for (int arg_index = 1 /*skipping executable name*/ ; arg_index < argc ; arg_index++ )
    {
        std::string item {argv[arg_index]};

        if (item == "-log")            { routine_logging = true; continue; }
        if (item == "-t")              { trace_lexer = trace_parser = trace_evaluate = true; continue; }
        if (item == "-trace_lexer")    { trace_lexer = true; continue; }
        if (item == "-trace_parser")   { trace_parser = true; continue; }
        if (item == "-trace_evaluate") { trace_evaluate = true; continue; }
        if (arg_index != (argc-1))
        {
            std::cout << argv[0] << " - usage: " << argv[0] << " options <ivyscript file name to open>" << std::endl
                << " where \"options\" means zero or more of: -trace_lexer  -trace_parser  -trace_evaluate or -t which sets all 3 trace types on." << std::endl
                << "and where the default filename is " << m_s.ivyscriptFilename << std::endl;
                return -1;
        }
        m_s.ivyscriptFilename = item;
    }

    if (!looksLikeFilename(m_s.ivyscriptFilename)) // returns true if all alphanumerics, underscores, periods (dots), single slashes, or single backslashes
    {
        std::cout << "ivyscript filename \"" + m_s.ivyscriptFilename + "\" doesn't look like a filename." << std::endl;
        return -1;
    }

    if (!endsIn(m_s.ivyscriptFilename, ".ivyscript")) m_s.ivyscriptFilename += std::string(".ivyscript");

    std::regex ivyscript_filename_regex(R"ivy((.*[/\\])?([a-zA-Z][_\.[:alnum:]]*)(\.ivyscript))ivy");
    // Three sub matches
    // - optional path part,
    // - root part, which is an identifier, possibly with embedded periods,
    // - and the .ivyscript suffix, which was added if the user left it off
    std::smatch entire_match; // we will pick out the test name part - submatch 2, the identifier, possibly with embedded periods.
    std::ssub_match test_name_sub_match;

    if (!std::regex_match(m_s.ivyscriptFilename, entire_match, ivyscript_filename_regex))
    {
        std::cout << "ivyscript filename \"" + m_s.ivyscriptFilename + "\" - bad filename or programming error extracting test name portion of filename." << std::endl;
        return -1;
    }
    if (entire_match.size() != 4)
    {
        std::cout << "ivyscript filename \"" + m_s.ivyscriptFilename + "\" - programming error extracting test name portion of filename.  "
                  << std::endl << entire_match.size() << " submatches found - expecting 4." << std::endl;
        return -1;
    }

    test_name_sub_match = entire_match[2];
    m_s.testName = test_name_sub_match.str();

    if (!isValidTestNameIdentifier(m_s.testName))
    {
        std::cout << "The ivy \"test name\" is the part of the filename before the optional .ivyscript." << std::endl
                  << "The test name must consist of entirely alphanumerics, underscores, and periods (dots)." << std::endl
                  << "Periods (dots) may not be used as first or last characters, and sequences of " << std::endl
                  << "two consecutive periods are not permitted." << std::endl;
        return -1;
    }


    struct stat struct_stat;
    if(stat(m_s.ivyscriptFilename.c_str(),&struct_stat))
    {
        std::cout << "ivyscript filename \"" + m_s.ivyscriptFilename + "\" does not exist." << std::endl;
        return -1;
    }

    if(!S_ISREG(struct_stat.st_mode))
    {
        std::cout << "ivyscript filename \"" + m_s.ivyscriptFilename  + "\" is not a regular file." << std::endl;
        return -1;
    }


    extern FILE* yyin;

    if( !(yyin = fopen(m_s.ivyscriptFilename.c_str(), "r")) )
    {
        std::cout << "Could not open \"" << m_s.ivyscriptFilename << "\" for in." << std::endl;
        return -1;
    }

    m_s.availableControllers[toLower(std::string("measure"))] = &m_s.the_dfc;

    std::string my_error_message;

    if ( ! m_s.random_steady_template.setParameter(my_error_message, std::string("iogenerator=random_steady")))
    {
        std::ostringstream o;
        o << "<Error> dreaded internal programming error - ivymaster startup - failed trying to set the default random steady I/O generator template - " << my_error_message << std::endl;
        std::cout << o.str();
        return -1;
    }
    if ( ! m_s.random_independent_template.setParameter(my_error_message, std::string("iogenerator=random_independent")))
    {
        std::ostringstream o;
        o << "<Error> dreaded internal programming error - ivymaster startup - failed trying to set the default random independent I/O generator template - " << my_error_message << std::endl;
        std::cout << o.str();
        return -1;
    }
    if ( ! m_s.sequential_template.setParameter(my_error_message, std::string("iogenerator=sequential")))
    {
        std::ostringstream o;
        o << "<Error> dreaded internal programming error - ivymaster startup - failed trying to set the default sequential I/O generator template - " << my_error_message << std::endl;
        std::cout << o.str();
        return -1;
    }



    Ivy_pgm  context(m_s.ivyscriptFilename);

    p_Ivy_pgm = &context;


	yy::ivy parser(context);

	parser.parse();

    fclose(yyin);

    if (trace_parser) { context.snapshot(std::cout); }

	if (context.successful_compile && !context.unsuccessful_compile )
	{
        std::cout << std::endl << "Successful compile, test name is \"" << m_s.testName << "\"." << std::endl;

        std::cout
            << inter_statement_divider << std::endl
            << "[OutputFolderRoot] \"" << m_s.outputFolderRoot << "\";" << std::endl;

        struct stat struct_stat;

        if( stat(m_s.outputFolderRoot.c_str(),&struct_stat))
        {
            std::ostringstream o;
            o << "      <Error> directory \"" << m_s.outputFolderRoot << "\" does not exist." << std::endl;
            std::cout << o.str();
            return -1;
        }

        if(!S_ISDIR(struct_stat.st_mode))
        {
            std::ostringstream o;
            o << "      <Error> \"" << m_s.outputFolderRoot << "\" is not a directory." << std::endl;
            std::cout << o.str();
            return -1;
        }

        m_s.testFolder=m_s.outputFolderRoot + std::string("/") + m_s.testName;

        if(0==stat(m_s.testFolder.c_str(),&struct_stat))  // output folder already exists
        {
            if(!S_ISDIR(struct_stat.st_mode))
            {
                std::ostringstream o;
                o << "     <Error> Output folder for this test run \"" << m_s.testFolder << "\" already exists but is not a directory." << std::endl;
                std::cout << o.str();
                return -1;
            }
            // output folder already exists and is a directory, so we delete it to make a fresh one.
            if (0 == system((std::string("rm -rf ")+m_s.testFolder).c_str()))   // ugly but easy.
            {
                std::ostringstream o;
                o << "      Deleted pre-existing folder \"" << m_s.testFolder << "\"." << std::endl;
                std::cout << o.str();

            }
            else
            {
                std::ostringstream o;
                o << "      <Error> Failed trying to delete previously existing version of test run output folder \"" << m_s.testFolder << "\"." << std::endl;
                std::cout << o.str();
                return -1;
            }

        }

        if (mkdir(m_s.testFolder.c_str(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
        {
            std::ostringstream o;
            o << "      <Error> Failed trying to create output folder \"" << m_s.testFolder << "\" << errno = " << errno << " " << strerror(errno) << std::endl;
            std::cout << o.str();
            return -1;
        }

        std::cout << "      Created test run output folder \"" << m_s.testFolder << "\"." << std::endl;

        if (mkdir((m_s.testFolder+std::string("/logs")).c_str(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
        {
            std::ostringstream o;
            o << "      <Error> - Failed trying to create logs subfolder in output folder \"" << m_s.testFolder << "\" << errno = " << errno << " " << strerror(errno) << std::endl;
            std::cout << o.str();
            return -1;
        }

        m_s.masterlogfile = m_s.testFolder + std::string("/logs/log.ivymaster.txt");

        {
            std::ostringstream o;
            o << "ivymaster build date date " << IVYBUILDDATE << " - fireup." << std::endl;
            log(m_s.masterlogfile,o.str());
        }

        if (!routine_logging) log(m_s.masterlogfile,"For logging of routine (non-error) events, use the ivy -log command line option, like \"ivy -log a.ivyscript\".\n\n");

        std::string copyivyscriptcmd = std::string("cp -p ") + m_s.ivyscriptFilename + std::string(" ") +
                                       m_s.testFolder + std::string("/") + m_s.testName + std::string(".ivyscript");
        if (0!=system(copyivyscriptcmd.c_str()))   // now getting lazy, but purist maintainers could write C++ code to do this.
        {
            std::ostringstream o;
            o << "Failed trying to copy input ivyscript to output folder: \"" << copyivyscriptcmd << "\"." << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            return -1;
        }

        std::string rollupsInitializeMessage;
        if (!m_s.rollups.initialize(rollupsInitializeMessage))
        {
            std::ostringstream o;
            o << "Internal programming error - failed initializing rollups in ivymaster.cpp saying: " << rollupsInitializeMessage << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            return -1;
        }

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
            std::cout << "Whoops!  Something happened trying to run the ivy program." << std::endl;
            throw;
        }
	}
	else
	{
        std::cout << "unsuccessful compile." << std::endl;
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

    m_s.kill_subthreads_and_exit();  // doesn't return

	return 0; // in case compiler complains, but unreachable.

}

