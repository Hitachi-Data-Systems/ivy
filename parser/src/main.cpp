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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
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
#include <sys/utsname.h>

#ifdef __GLIBC__
#include <gnu/libc-version.h>
#endif

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
bool one_thread_per_core {false};

void usage_message(char* argv_0)
{
    std::cout << std::endl << "Usage: " << argv_0 << " [options] <ivyscript file name to open>" << std::endl << std::endl
        << "where \"[options]\" means zero or more of:" << std::endl << std::endl
        << "-rest" << std::endl
        << "     Turns on REST API service (ignores ivyscript in command line)" << std::endl << std::endl
        << "-log" << std::endl
        << "     Turns on logging of routine events." << std::endl << std::endl
        << "-no_stepcsv" << std::endl
        << "     Sets the default for the \"stepcsv\" [Go] parameter to \"off\" instead of \"on\"." << std::endl
        << "     This suppresses creation of a \"step\" subfolder with by-subinterval csv files for rollups." << std::endl << std::endl
        << "-storcsv" << std::endl
        << "     Sets the default for the \"storcsv\" [Go] parameter to \"on\" instead of \"off\"." << std::endl
        << "     For each step (for each [Go]), if \"stepcsv\" is \"on\" to create a step subfolder with" << std::endl
        << "     by-subinterval csv files, if the \"storcsv\" [Go] parameter is also \"on\", for each" << std::endl
        << "     Hitachi subsystem with a command device connector, a nested subfolder is created containing" << std::endl
        << "     by-subinterval subsystem data csv files." << std::endl << std::endl
        << "-no_wrap" << std::endl
        << "     When opening ivy csv files with Excel, parity group names (e.g. 1-1)" << std::endl
        << "     are interpreted by Excel as dates and LDEV names (e.g. 10:00) as times."<<std::endl
        << "     Normally ivy encloses things like these in formulas, like =\"1-1\" or =\"10:00\"" << std::endl
        << "     so they will look OK in Excel. To supress this formula rapping, use -no_wrap." << std::endl << std::endl
        << "-no_cmd"<< std::endl
        << "     Don\'t use any command devices." << std::endl << std::endl
        << "-skip_LDEV"<< std::endl
        << "     If a command device is being used, set the [Go] parameter default for skip_LDEV" << std::endl
        << "     to \"skip_LDEV=on\" to skip collection of LDEV & PG data.  This makes gathers faster" << std::endl
        << "     if you don't need LDEV and PG data." << std::endl << std::endl
        << "-no_perf"<< std::endl
        << "     If a command device is being used, set the [Go] parameter default for no_perf" << std::endl
        << "     to \"no_perf=on\" to suppress the collection of subsystem performance data" << std::endl
        << "     while ivy is running driving I/O.  Collection of performance data resumes" << std::endl
        << "     with the second and subsequent cooldown subintervals at IOPS=0 to support" << std::endl
        << "     the cooldwon_by_wp and cooldown_by_MP_busy featueres." << std::endl << std::endl
        << "-spinloop" << std::endl
        << "     Used to make test host code continually check for work to do without ever waiting." << std::endl << std::endl
        << "-one_thread_per_core" << std::endl
        << "     Use -one_thread_per_core to have ivydriver start a workload subthread on only the first" << std::endl
        << "     hyperthread on each physical CPU core, instead of the default which is start a workload subthread"
        << "     on all hyperthreads on each core.  Either way, workload subthread(s) are never started on core 0." << std::endl << std::endl
//        << "-trace_lexer or -l" << std::endl
//        << "     Log routine events and trace the \"lexer\" which breaks down the .ivyscript program into \"tokens\"." << std::endl << std::endl
//        << "-trace_parser or -p" << std::endl
//        << "     Log routine events and trace the \"parser\" which recognizes the syntax of the stream of tokens as a program." << std::endl << std::endl
//        << "-trace_evaluate or -e" << std::endl
//        << "     Log routine events and trace the execution of the compiled ivyscript program." << std::endl << std::endl
//        << "-t" << std::endl
//        << "     Same as -trace_lexer -trace_parser -trace_evaluate." << std::endl << std::endl
        << "The .ivyscript suffix on the name of the ivyscript program is optional." << std::endl << std::endl
        << "Examples:" << std::endl
        << "\t" << argv_0 << " -rest" << std::endl
        << "\t" << argv_0 << " xxxx.ivyscript" << std::endl
        << "\t" << argv_0 << " xxxx" << std::endl
        << "\t" << argv_0 << " -no_wrap xxxx" << std::endl
        << "\t" << argv_0 << " -spinloop xxxx" << std::endl
        << "\t" << argv_0 << " -log xxxx" << std::endl
        //<< "\t" << argv_0 << " -t xxxx" << std::endl
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
    char hostname[HOST_NAME_MAX + 1];
    hostname[HOST_NAME_MAX] = 0x00;
    if (0 != gethostname(hostname,HOST_NAME_MAX))
    {
        std::cout << "<Error> internal programming error - gethostname(hostname,HOST_NAME_MAX)) failed errno " << errno << " - " << strerror(errno) << "." << std::endl;
        return -1;
    }

    std::string versions_message;
    {
        std::ostringstream o;
        o << "ivy version " << ivy_version << " build date " << IVYBUILDDATE << std::endl;

        std::cout << o.str();

#ifdef __GNUC__
        o << std::endl << "gcc version " << __GNUC__;
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
        o << std::endl;
#ifdef __GLIBC__
        o << "GNU libc compile-time version: " << __GLIBC__ << "." << __GLIBC_MINOR__;
        o << " - GNU libc runtime version: " << gnu_get_libc_version() << std::endl;
#endif
        o << GetStdoutFromCommand("openssl version") << std::endl;

        struct utsname utsname_buf;
        if (0 == uname(&utsname_buf))
        {
            o << utsname_buf.nodename
                << " - " << utsname_buf.sysname
                << " - " << utsname_buf.release
                << " - " << utsname_buf.version
                << " - " << utsname_buf.machine
#ifdef _GNU_SOURCE
                << " - " << utsname_buf.domainname
#endif
                << std::endl;
        }
        o << std::endl << "Attributes from lscpu command:" << std::endl << GetStdoutFromCommand("lscpu") << std::endl;
        versions_message = o.str();
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
    {{  // double braces to be really sure "item" is fresh every time.
        std::string item {argv[arg_index]};

        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-rest")))           { rest_api = true; continue; }
        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-log")))            { routine_logging = true; continue; }
        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-no_wrap")))        { m_s.formula_wrapping = false; continue; }
        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-no_stepcsv")))     { m_s.stepcsv_default = false; continue; }
        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-storcsv")))        { m_s.storcsv_default = true; continue; }
        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-t")))              { routine_logging = trace_lexer = trace_parser = trace_evaluate = true; continue; }
        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-trace_lexer"))
         || stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-l"))          )    { routine_logging = trace_lexer = true; continue; }
        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-trace_parser"))
         || stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-p"))           )   { routine_logging = trace_parser = true; continue; }
        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-trace_evaluate"))
        ||  stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-e"))             ) { routine_logging = trace_evaluate = true; continue; }
        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-no_cmd")))         { m_s.use_command_device = false; continue; }
        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-skip_LDEV")))      { m_s.skip_ldev_data_default = true; continue; }
        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-spinloop")))       { spinloop = true; continue; }
        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-one_thread_per_core"))) { one_thread_per_core = true; continue; }
        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-no_perf"))
         || stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-suppress_perf")))  { m_s.suppress_subsystem_perf_default = true; continue; }
        if (stringCaseInsensitiveEquality(remove_underscores(item), remove_underscores("-no_check_failed_component")))  { m_s.check_failed_component_default = false; continue; }

        if (arg_index != (argc-1)) { usage_message(argv[0]); return -1; }

        ivyscriptFilename = item;
    }}

    bool is_python_script {false};
    if (endsIn(ivyscriptFilename, ".py"))
    {
        rest_api = true;
        is_python_script = true;
    } else {
        if (!endsIn(ivyscriptFilename, ".ivyscript")) ivyscriptFilename += std::string(".ivyscript");
    }

    if (rest_api && !is_python_script)
    {
        // setup and start serving the REST API
        RestHandler rest_handler;

        RestHandler::wait_and_serve();
        return 0;
    }

    std::regex ivyscript_filename_regex(R"ivy((.*[/])?([^/\\]+)(\.ivyscript))ivy");
    std::regex python_filename_regex(R"ivy((.*[/])?([^/\\]+)(\.py))ivy");

    // Three sub matches
    // - optional path part, which if present ends with a forward slash / or
    // - test name part
    // - and the .ivyscript suffix, which was added if the user left it off

    std::smatch entire_match; // we will pick out the test name part - submatch 2, the identifier, possibly with embedded periods.
    std::ssub_match test_name_sub_match;

    if ((is_python_script ? !std::regex_match(ivyscriptFilename, entire_match, python_filename_regex)
                          : !std::regex_match(ivyscriptFilename, entire_match, ivyscript_filename_regex)))
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


    m_s.testName = test_name;

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

    m_s.test_start_time.setToNow();

    if (is_python_script)
    {
        // setup and start serving the REST API
        RestHandler rest_handler;

        Ivy_pgm  context(ivyscriptFilename,test_name);

        m_s.outputFolderRoot = context.output_folder_root;
        m_s.testName = context.test_name;
        m_s.ivyscript_filename = context.ivyscript_filename;

        // if a python script is provided - run script and ivy sandwiched together
        std::string cmd = "sleep 1; unset http_proxy https_proxy; python " + ivyscriptFilename;
        system(cmd.c_str());

        RestHandler::wait_and_serve(); // blocking
        return 0;
    }

    if( stat("/var",&struct_stat) || (!S_ISDIR(struct_stat.st_mode)) )
    {
        std::cout << "<Error> \"\var\" doesn\'t exist or is not a folder." << std::endl;
        return -1;
    }

    if( stat("/var/ivymaster_logs", &struct_stat) )
    {
        //  folder doesn't already exist
        int rc;
        if ((rc = mkdir("/var/ivymaster_logs", S_IRWXU | S_IRWXG | S_IRWXO)))
        {
            std::ostringstream o;
            o << "<Error> Failed trying to create ivymaster root log folder \"/var/ivymaster_logs\" - "
                << "mkdir() return code " << rc << ", errno = " << errno << " " << std::strerror(errno) << std::endl;
            std::cout << o.str();
            return -1;
        }
    }

    // Note:  In order to have ivy's log files temporarily stored somewhere else
    //        other than in /var, make a symlink in /var as /var/ivymaster_logs,
    //        and have the symlink point to the root folder such as /xxxxx/ivymaster_logs.

    m_s.var_ivymaster_logs_testName = "/var/ivymaster_logs/"s + m_s.testName;

	{
	    std::string cmd = "rm -rf "s + m_s.var_ivymaster_logs_testName;
	    system(cmd.c_str());
    }

    {
        int rc;
	    if ((rc = mkdir(m_s.var_ivymaster_logs_testName.c_str(), S_IRWXU | S_IRWXG | S_IRWXO)))
        {
            std::ostringstream o;
            o << "<Error> Failed trying to create ivymaster log folder \"" << m_s.var_ivymaster_logs_testName << "\" - "
                << "mkdir() return code " << rc << ", errno = " << errno << " " << std::strerror(errno) << std::endl;
            std::cout << o.str();
            return -1;
        }
    }

    m_s.masterlogfile.logfilename = m_s.var_ivymaster_logs_testName + "/log.ivymaster.txt"s;

    log(m_s.masterlogfile,versions_message);

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
        std::cout << "<Error> unsuccessful compile." << std::endl << std::endl;
        context.show_syntax_error_location();
    }

    ivytime finish_time; finish_time.setToNow();
    ivytime duration = finish_time - m_s.test_start_time;

    {

        std::ostringstream o;

        o << std::endl << "Step times summary:" << std::endl << m_s.step_duration_lines;

        o << std::endl << "********* ivy run complete." << std::endl;

        o << "********* Total run time    " << duration.format_as_duration_HMMSS()
            << " for test name \"" << m_s.testName << "\"" << std::endl << std::endl;

        if (m_s.warning_messages.size() > 0)
        {
            o << "**********************************************************************************" << std::endl;
            o << "***                                                                            ***" << std::endl;
            o << "*** Will now repeat <Warning> messages previously shown while ivy was running. ***" << std::endl;
            o << "***                                                                            ***" << std::endl;
            o << "**********************************************************************************" << std::endl << std::endl;
            for (auto s : m_s.warning_messages)
            {
                o << s << std::endl;
            }
        }
        std::cout << o.str();
        log(m_s.masterlogfile, o.str());

    }

    if (m_s.iosequencer_template_was_used) { m_s.print_iosequencer_template_deprecated_msg(); }

    std::pair<bool,std::string> strc = m_s.shutdown_subthreads();

	return 0; // in case compiler complains, but unreachable.

}

