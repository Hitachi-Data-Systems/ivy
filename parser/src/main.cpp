//Copyright (c) 2016, 2017, 2018 Hitachi Vantara Corporation
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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#include <iostream>
#include <stdio.h>
#include <regex>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

#include "Ivy_pgm.h"
#include "ivy.parser.hh"
#include "ivyhelpers.h"
#include "ivy_engine.h"
#include "MeasureCtlr.h"
#include "ivybuilddate.h"
#include "RestHandler.h"

std::string startup_log_file {"~/ivy_startup.txt"};

std::ostringstream startup_ostream;

std::string inter_statement_divider {"=================================================================================================="};

bool routine_logging {false};
bool rest_api {false};
bool spinloop {false};

void usage_message(char* argv_0)
{
    std::cout << std::endl << "Usage: " << argv_0 << " [options] <ivyscript file name to open>" << std::endl << std::endl
        << "where \"[options]\" means zero or more of:" << std::endl << std::endl
        << "-rest" << std::endl
        << "     Turns on REST API service (ignores ivyscript in command line)" << std::endl << std::endl
        << "-log" << std::endl
        << "     Turns on logging of routine events." << std::endl << std::endl
        << "-no_cmd"<< std::endl
        << "     Don\'t use any command devices." << std::endl << std::endl
        << "-trace_lexer or -l" << std::endl
        << "     Log routine events and trace the \"lexer\" which breaks down the .ivyscript program into \"tokens\"." << std::endl << std::endl
        << "-trace_parser or -p" << std::endl
        << "     Log routine events and trace the \"parser\" which recognizes the syntax of the stream of tokens as a program." << std::endl << std::endl
        << "-trace_evaluate or -e" << std::endl
        << "     Log routine events and trace the execution of the compiled ivyscript program." << std::endl << std::endl
        << "-t" << std::endl
        << "     Same as -trace_lexer -trace_parser -trace_evaluate." << std::endl << std::endl
        << "The .ivyscript suffix on the name of the ivyscript program is optional." << std::endl << std::endl
        << "Examples:" << std::endl
        << "\t" << argv_0 << " -rest" << std::endl
        << "\t" << argv_0 << " xxxx.ivyscript" << std::endl
        << "\t" << argv_0 << " xxxx" << std::endl
        << "\t" << argv_0 << " -log xxxx" << std::endl
        << "\t" << argv_0 << " -t xxxx" << std::endl
        ;
}


std::string get_my_path_part()
{
#define MAX_FULLY_QUALIFIED_PATHNAME 511
    const size_t pathname_max_length_with_null = MAX_FULLY_QUALIFIED_PATHNAME + 1;
    char pathname_char[pathname_max_length_with_null];

    // Read the symbolic link '/proc/self/exe'.
    const char *proc_self_exe = "/proc/self/exe";
    const int readlink_rc = int(readlink(proc_self_exe, pathname_char, MAX_FULLY_QUALIFIED_PATHNAME));

    std::string fully_qualified {};

    if (readlink_rc <= 0) { return ""; }

    fully_qualified = pathname_char;

    std::string path_part_regex_string { R"ivy((.*/)([^/]+))ivy" };
    std::regex path_part_regex( path_part_regex_string );

    std::smatch entire_match;
    std::ssub_match path_part;

    if (std::regex_match(fully_qualified, entire_match, path_part_regex))
    {
        path_part = entire_match[1];
        return path_part.str();
    }

    return "";
}

std::string get_running_user()
{
    uid_t u;
    struct passwd *p;

    u = geteuid ();
    p = getpwuid (u);

    if (nullptr != p)
    {
         return std::string(p->pw_name);
    }
    return std::string("");
}

int main(int argc, char* argv[])
{

    {
        std::ostringstream o;
        o << "ivy version " << ivy_version << " build date " << IVYBUILDDATE ;

#ifdef __GNUC__
    o << "  - gcc version " << __GNUC__;
#endif

#ifdef __GNUC_MINOR__
    o << "." << __GNUC_MINOR__;
#endif

#ifdef __GNUC_PATCHLEVEL__
    o << "." << __GNUC_PATCHLEVEL__;
#endif

#ifdef __GLIBCPP__
    o << " - libstdc++ date " << __GLIBCPP__;
#endif

#ifdef __GLIBCXX__
    o << " - libstdc++ date " << __GLIBCXX__;
#endif
        o << " starting." << std::endl << std::endl;
        std::cout << o.str();
    }

//    std::cout << "argc = " << argc << " ";
//    for (int i=0;i<argc;i++) {std::cout << "argv["<<i<<"] = \"" << argv[i] << "\" ";}
//    std::cout << std::endl;

    std::string u = get_running_user();

    if (0 != u.compare(std::string("root")))
    {
        std::cout << "ivy is running as user " << u << ".  Sorry, ivy must be running as root." << std::endl;
        return -1;
    }


    m_s.path_to_ivy_executable = get_my_path_part();

	std::string ivyscriptFilename {};

    if (argc==1) { usage_message(argv[0]); return -1; }

    for (int arg_index = 1 /*skipping executable name*/ ; arg_index < argc ; arg_index++ )
    {
        std::string item {argv[arg_index]};

        if (item == "-rest")                           { rest_api = true; continue; }
        if (item == "-log")                            { routine_logging = true; continue; }
        if (item == "-t")                              { routine_logging = trace_lexer = trace_parser = trace_evaluate = true; continue; }
        if (item == "-trace_lexer"    || item == "-l") { routine_logging = trace_lexer = true; continue; }
        if (item == "-trace_parser"   || item == "-p") { routine_logging = trace_parser = true; continue; }
        if (item == "-trace_evaluate" || item == "-e") { routine_logging = trace_evaluate = true; continue; }
        if (item == "-no_cmd")                         { m_s.use_command_device = false; continue; }
        if (item == "-spinloop")                       { spinloop = true; continue; }

        if (arg_index != (argc-1)) { usage_message(argv[0]); return -1; }

        ivyscriptFilename = item;
    }

    if (rest_api)
    {
        // setup and start serving the REST API
        RestHandler rest_handler;

        RestHandler::wait_and_serve();
        return 0;
    }

    if (!endsIn(ivyscriptFilename, ".ivyscript")) ivyscriptFilename += std::string(".ivyscript");

    std::regex ivyscript_filename_regex(R"ivy((.*[/])?([^/\\]+)(\.ivyscript))ivy");

    // Three sub matches
    // - optional path part, which if present ends with a forward slash / or
    // - test name part
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

    auto isValid = isValidTestNameIdentifier(test_name);
    if (!isValid.first)
    {
        std::cout << isValid.second;
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

