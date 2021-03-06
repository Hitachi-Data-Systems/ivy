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
// ivydriver.cpp

#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include "ivytypes.h"
#include "ivyhelpers.h"
#include "logger.h"
#include "pipe_line_reader.h"

// get REG_EIP from ucontext.h
#ifndef __USE_GNU
#define __USE_GNU
#endif


#ifdef __GLIBC__
#include <gnu/libc-version.h>
#endif

#include "ivytime.h"
#include "ivydriver.h"
#include "pipe_line_reader.h"

IvyDriver ivydriver;

bool routine_logging {false};
bool subthread_per_hyperthread {true};  // sorry, this is backwards compared to the ivy main host's one_thread_per_core

std::string printable_ascii;

void invokeThread(WorkloadThread* T)
{
	T->WorkloadThreadRun();
}

void IvyDriver::say(std::string s)
{
    {
        // This is a bit lame, not ensuring that <Warning> messages are "said"
        // immediately by the main thread when a subthread issues a <Warning> message,
        // but at least we say them first when we go to say something else;

        std::lock_guard<std::mutex> lk_guard(ivydriver_main_mutex);

        for (auto& e : workload_thread_error_messages)
        {
            std::cout << e << std::flush;
        }
        workload_thread_error_messages.clear();

        for (auto& w : workload_thread_warning_messages)
        {
            std::cout << w << std::flush;
        }
        workload_thread_warning_messages.clear();
    }

 	if (s.length()==0 || s[s.length()-1] != '\n') s.push_back('\n');

    ivytime delta;

    if (ivydriver.last_message_time == ivytime_zero)
    {
        ivydriver.last_message_time.setToNow();
        delta.setToZero();
    }
    else
    {
        ivytime now; now.setToNow();
        delta = now - ivydriver.last_message_time;
        ivydriver.last_message_time = now;
    }

    if (routine_logging) log(ivydriver.slavelogfile,format_utterance("Slave", s, delta));

	std::cout << s << std::flush;
	return;
}


void IvyDriver::killAllSubthreads()
{
	for (auto& pear : all_workload_threads)
    {
        if (ThreadState::died == pear.second->state || ThreadState::exited_normally == pear.second->state)
        {
            std::ostringstream o;
            o << "IvyDriver::killAllSubthreads() - thread for " << pear.first
                << " was in state " << threadStateToString(pear.second->state) << std::endl;

            log(slavelogfile,o.str());

            // See if it's OK to say this during killAllSubthreads.  We're on the way out anyway.
            // I say them one at a time, because at least if I get part way through there will be a log message on the master host pipe_driver_subthread log.
        }
        else
		{
			std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);
			pear.second->ivydriver_main_posted_command=true;
			pear.second->ivydriver_main_says=MainThreadCommand::die;
			if (routine_logging)
			{
			    log(pear.second->slavethreadlogfile,"[logged here by ivydriver main thread] ivydriver main thread posted \"die\" command.");
            }
		}
		pear.second->slaveThreadConditionVariable.notify_all();
	}

	int threads=0;

	for (auto& pear : all_workload_threads) {
		pear.second->std_thread.join();
		threads++;
	}

	if (routine_logging)
	{
        std::ostringstream o;
        o << "killAllSubthreads() - commanded " << threads <<" iosequencer threads to die and harvested the remains.\n";
        log(slavelogfile, o.str());
	}
}


int main(int argc, char* argv[])
{
    return ivydriver.main(argc, argv);
}

void sig_handler(int sig, siginfo_t *p_siginfo, void *context);


int IvyDriver::main(int argc, char* argv[])
{
//    struct sigaction sigactshun;
//    memset(&sigactshun, 0, sizeof(sigactshun));
//    sigactshun.sa_flags = SA_SIGINFO;  // three argument form of signal handler
//    sigactshun.sa_sigaction = sig_handler;

    //sigaction(SIGINT, &sigactshun, NULL);
    //sigaction(SIGHUP, &sigactshun, NULL);
    //sigaction(SIGCHLD, &sigactshun, NULL);
    //sigaction(SIGSEGV, &sigactshun, NULL);
    //sigaction(SIGUSR1, &sigactshun, NULL);
    //sigaction(SIGTERM, &sigactshun, NULL);

    int rc = system("stty -echo");
    if (!WIFEXITED(rc) || (0 != WEXITSTATUS(rc)))
    {
        std::cout << "<Error> Failed turn ing off stdin echo using \"stty -echo\" command.\n";
        return -1;
    }

    system ("modprobe msr"); // this may be needed for check_CPU_temperature() to work.

	initialize_io_time_clip_levels();

    for (int arg_index = 1 /*skipping executable name*/ ; arg_index < argc ; arg_index++ )
    {
        std::string item {argv[arg_index]};

        if (item == "-warn_on_critical_temp")  { warn_on_critical_temp     = true;  continue;}
        if (item == "-error_on_critical_temp") { warn_on_critical_temp     = false; continue;}
        if (item == "-log")                    { routine_logging           = true;  continue;}
        if (item == "-one_thread_per_core")    { subthread_per_hyperthread = false; continue;}

        if (arg_index != (argc-1))
        {
            std::cout << argv[0] << " - usage: " << argv[0] << " options <ivyscript_hostname>" << std::endl
                << " where \"options\" means zero or more of: -log -hyperthread" << std::endl
                << "and where ivyscript_hostname is either an identifier, a hostname or alias, or is an IPV4 dotted quad." << std::endl << std::endl
                << "-one_thread_per_core means start an I/O driving subthread on only the first hyperthread of each physical core." << std::endl
                << "Core 0 and its hyperthreads are never used for driving I/O." << std::endl << std::endl;
                return -1;
        }
        ivyscript_hostname = item;
    }

	struct stat struct_stat;

    if( stat("/var",&struct_stat) || (!S_ISDIR(struct_stat.st_mode)) )
    {
        //  folder doesn't already exist or it's not a directory
        std::cout << "<Error> " << "ivydriver log folder root \"/var\" doesn\'t exist or is not a folder." << std::endl;
        return -1;
    }

    // In the next bit where we create the subfolder of IVYDRIVERLOGFOLDERROOT, we use a semaphore
    // because if you are testing with two aliases for the same test host, only one ivydriver main thread
    // should be creating the subfolder.

    sem_t* p_semaphore = sem_open("/ivydriver_log_subfolder_create", O_CREAT,S_IRWXG|S_IRWXO|S_IRWXU,0);

    if (SEM_FAILED == p_semaphore)
    {
        std::ostringstream o;
        o << "<Error> Failed trying to open semaphore to create ivydriver log subfolder.  errno = " << errno
            << " - " << std::strerror(errno) << " at " << __FILE__  << " line " << __LINE__ << "." << std::endl;
        std::cout << o.str();
        return -1;
    }
    else
    {
        if( stat("/var/ivydriver_logs" ,&struct_stat) )
        {
            //  folder doesn't already exist
            int rc;
            if ((rc = mkdir("/var/ivydriver_logs", S_IRWXU | S_IRWXG | S_IRWXO)))
            {
                std::ostringstream o;
                o << "<Error> Failed trying to create ivydriver log folder \"/var/ivydriver_logs\" - "
                    << "mkdir() return code " << rc << ", errno = " << errno << " " << std::strerror(errno) << std::endl;
                std::cout << o.str();
                return -1;
            }
        }

        sem_close(p_semaphore);
    }

	std::string erase_earlier_log_files = "rm -f /var/ivydriver_logs/log.ivydriver."s + ivyscript_hostname + "*"s;
	system(erase_earlier_log_files.c_str());

    slavelogfile.logfilename = "/var/ivydriver_logs/log.ivydriver."s + ivyscript_hostname + ".txt"s;

    {
        std::ostringstream o;
        o << "ivydriver version " << ivy_version << " build date " << IVYBUILDDATE;

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

        char my_current_directory[PATH_MAX];
        char* p_my_current_driectory = getcwd(my_current_directory,PATH_MAX);

        if (p_my_current_driectory != nullptr)
        {
            o << "My current directory is " << my_current_directory << std::endl;
        }

        log(slavelogfile,o.str());
    }

	lasttime.setToNow();
	say(std::string("Hello, whirrled! from ")+myhostname());


    for (unsigned int c = 0; c < 128; c++) if (isprint((char)c)) printable_ascii.push_back((char)c);

    if (routine_logging) {std::ostringstream o; o << "printable_ascii = \"" << printable_ascii << "\"" << std::endl; log(slavelogfile,o.str());}

    std::vector<std::pair<unsigned int /* core */, unsigned int /* "processor" (hyperthread) */>> hyperthreads_to_start_subthreads_on {};

    std::map<unsigned int /* core */, std::vector<unsigned int /* processor (hyperthread) */>>  hyperthreads_per_core = get_processors_by_core();

    // The really weird thing I saw on one host is that core_id 7 was missing, whilst the "processor" numbers (hyperthread numbers) had no gaps, and jumped over core_id 7.


    // the size of active_cores never changes, but every time we redistribute TestLUNs over WorkloadThreads,
    // we first clear all the entries false and then turn on only those processor numbers that have WorkloadThreads on them which own TestLUNs.


    //    # cat /proc/cpuinfo | grep 'processor\|core id'
    //    processor	: 0
    //    core id		: 0
    //    processor	: 1
    //    core id		: 1
    //    processor	: 2
    //    core id		: 2
    //    processor	: 3
    //    core id		: 3
    //    processor	: 4
    //    core id		: 0
    //    processor	: 5
    //    core id		: 1
    //    processor	: 6
    //    core id		: 2
    //    processor	: 7
    //    core id		: 3
    //    processor	: 8
    //    core id		: 0
    //    processor	: 9
    //    core id		: 1
    //    processor	: 10
    //    core id		: 2
    //    processor	: 11
    //    core id		: 3
    //    processor	: 12
    //    core id		: 0
    //    processor	: 13
    //    core id		: 1
    //    processor	: 14
    //    core id		: 2
    //    processor	: 15
    //    core id		: 3

    //    What we see above is that the hyperthread or "processor" numbers are allocated on a round-robbin
    //    fashion across the core_ids.  Since all_workload_threads is a map with the hyperthread / processor
    //    number as the key, when we iterate over all_workload_threads, we iterate over the cores
    //    on a round-robbin basis.  This means that with the -hyperthread command line option,
    //    when we map workload LUNs to I/O driving subthreads, we allocate those LUNs round=robbing over the cores.

    //    std::map<unsigned int,WorkloadThread*> all_workload_threads {};

    if (routine_logging)
    {
        std::ostringstream o;

        for (const auto& v : hyperthreads_per_core)
        {
            o << std::endl << "core " << v.first << " has hyperthreads ";

            for (auto& p : v.second)
            {
                o << p << "  ";
            }

        }
        o << std::endl;

        log(slavelogfile,o.str());
    }

    if (subthread_per_hyperthread)
    {
        for (auto& pear : hyperthreads_per_core)
        {
            unsigned int hyperthread_count = 0;

            for (unsigned int& processor : pear.second)
            {
                hyperthread_count ++;

                if (pear.first != 0 || hyperthread_count > 2)
                {
                    hyperthreads_to_start_subthreads_on.push_back(std::make_pair(pear.first, processor));
                }
            }
        }
    }
    else
    {
        for (auto& pear : hyperthreads_per_core)
        {
            if (pear.first == 0) continue;  // leave this open for ivydriver master thread, or even the ivy central host subthreads.
                // Come back here and maybe leave another core free in case this test host is also the master host ..... XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

            hyperthreads_to_start_subthreads_on.push_back(std::make_pair(pear.first, pear.second[0]));
        }
    }

    for (auto& pear : hyperthreads_to_start_subthreads_on)
    {
        unsigned int physical_core = pear.first;
        unsigned int hyperthread = pear.second;

        WorkloadThread* p_WorkloadThread = new WorkloadThread(&ivydriver_main_mutex,physical_core,hyperthread);

        all_workload_threads[hyperthread]=p_WorkloadThread;

        {
            std::ostringstream o;

            o << "/var/ivydriver_logs/log.ivydriver." << ivyscript_hostname << ".core_" << physical_core << "_hyperthread_" << hyperthread;

            o << ".txt";

            p_WorkloadThread->slavethreadlogfile.logfilename = o.str();
        }

        // Invoke thread
        p_WorkloadThread->std_thread=std::thread(invokeThread,p_WorkloadThread);

        // wait for thread to wake up and post that it's ready for commands.
        {
            std::unique_lock<std::mutex> u_lk(p_WorkloadThread->slaveThreadMutex);

            if (!p_WorkloadThread->slaveThreadConditionVariable.wait_for(u_lk, 1000ms,
                       [&p_WorkloadThread] { return p_WorkloadThread->state == ThreadState::waiting_for_command; }))
            {
                std::ostringstream o;
                o << "<Error> ivydriver waited more than one second for workload thread for core "
                    << physical_core << " hyperthread " << hyperthread <<  " to start up." << std::endl
                    << "Source code reference " << __FILE__ << " line " << __LINE__ << std::endl;
                say(o.str());
                log(slavelogfile, o.str());
                u_lk.unlock();
                killAllSubthreads();
            }
        }

        // set processor affinity

        cpu_set_t aff_mask;
        CPU_ZERO(&aff_mask);
        CPU_SET(hyperthread, &aff_mask);

        if (0 !=  pthread_setaffinity_np
                    (
                        p_WorkloadThread->std_thread.native_handle()
                        , sizeof(cpu_set_t)
                        , &aff_mask
                    )
        )
        {
            std::ostringstream o;
            o << "<Warning> was unsuccessful to set CPU processor affinity for core "
                << physical_core << " hyperthread " << hyperthread << "." << std::endl
                << "Source code reference " << __FILE__ << " line " << __LINE__ << std::endl;
            say(render_string_harmless(o.str()));
            log(slavelogfile, o.str());
        }
//*debug*/ break; // only one WT
    }

    disco.discover();

	if(std::cin.eof()) {log(slavelogfile, "std::cin.eof()\n"); return 0;}

	pipe_line_reader getline {};

	getline.set_fd(0); // reading from stdin

	ivytime one_hour = ivytime(60*60);
        // one hour so that if ivydriver somehow would wait forever, this makes it explode after an hour.
        // Ivydriver gets a command every subinterval, so this would limit subinterval_seconds to one hour.

    std::smatch entire_set_command_match;
    std::ssub_match set_attribute_name, set_attribute_value;

	while(!std::cin.eof())
	{
	    if (time_error_reported != ivytime_zero)
	    {
	        ivytime now; now.setToNow();

	        ivytime time_since_error = now - time_error_reported;

	        if (time_since_error >= ivytime(60))
	        {
	            // we reported the error over a minute ago.
                killAllSubthreads();
                return -1;
	        }

	        one_hour = ivytime(60) - time_since_error;
	    }

        {
            std::lock_guard<std::mutex> lk_guard(ivydriver_main_mutex);

            for (auto& e : workload_thread_error_messages)
            {
                std::cout << e << std::flush;
                log(slavelogfile,"Slave: "s + e);
            }
            workload_thread_error_messages.clear();

            for (auto& w : workload_thread_warning_messages)
            {
                std::cout << w << std::flush;
                log(slavelogfile,"Slave: "s + w);
            }
            workload_thread_warning_messages.clear();
        }
		// get commands from ivymaster

		input_line = getline.get_reassembled_line(one_hour,"ivydriver reading a line from ivy");

		lasttime.setToNow();

        if (last_message_time == ivytime_zero)
        {
            last_message_time.setToNow();
        }
        else
        {
            ivytime now, delta;

            now.setToNow();
            delta = now - last_message_time;
            last_message_time = now;

            if (routine_logging) { log(slavelogfile,format_utterance("Master", input_line, delta)); }
        }

		trim(input_line);

		if (stringCaseInsensitiveEquality(input_line, std::string("[Die, Earthling!]")))
		{
			say(std::string("[what?]"));

            std::string s = print_logfile_stats(); // separated from next statement to avoid any issues with mutex locks.
			log(slavelogfile,s);

			// When ivymaster subthread encounters an error and is terminating abnormally
			killAllSubthreads();
			return -1;
		}
		// end of [Die, Earthling!]

		if (reported_error)
		{
		    std::string s = "<Error> " + ivyscript_hostname + " - ignoring \""s + input_line + "\" because I previously reported an error.\n"s;
		    say(s);
            std::string s2 = print_logfile_stats(); // separated from next statement to avoid any issues with mutex locks.
			log(slavelogfile,s2);
            continue;
		}


		else if (stringCaseInsensitiveEquality(input_line, std::string("send LUN header")))
		{
			disco.get_header_line(header_line);
			say("[LUNheader]Ivyscript Hostname,"+header_line);
		}
		// end of send LUN header
		else if (stringCaseInsensitiveEquality(input_line,std::string("send LUN")))
		{
			if (disco.get_next_line(disco_line))
			{
				LUN* pLUN = new LUN();
				if (!pLUN->loadcsvline(header_line,disco_line,slavelogfile))
				{
					std::ostringstream o;
					o << "<Error> Ivydriver main thread failed trying to load a LUN lister csv file line into a LUN object.  csv header line = \""
					<< header_line << "\", data line = \"" << disco_line << "\"." << std::endl ;
					log(slavelogfile,o.str());
					say (o.str());
					killAllSubthreads();
					return -1;
				}
				LUNpointers[pLUN->attribute_value(std::string("LUN name"))]=pLUN;
				say(std::string("[LUN]")+ivyscript_hostname+std::string(",")+disco_line);
			}
			else
			{
				say("[LUN]<eof>");

            }
		}
		// end of send LUN

		else if (startsWith("[CreateWorkload]",input_line,remainder))
		    { try {create_workload();} catch (std::exception& e) { post_error(e.what()); continue;} }

		else if (startsWith("[DeleteWorkload]",input_line,remainder))
		    { try {delete_workload();} catch (std::exception& e) { post_error(e.what()); continue;} }

		else if (startsWith("[EditWorkload]",  input_line,remainder))
		    { try {edit_workload();  } catch (std::exception& e) { post_error(e.what()); continue;} }

        else if (0 == input_line.compare(std::string("get subinterval result")))
            { try {waitForSubintervalEndThenHarvest(); } catch (std::exception& e) { post_error(e.what()); continue;} }
			// waitForSubintervalEndThenHarvest() increments the subinterval starting/ending times.

		else if (startsWith(std::string("Go!"),input_line,subinterval_duration_as_string))
		    { try {go();} catch (std::exception& e) { post_error(e.what()); continue;} }

		else if (0 == input_line.compare(std::string("continue")))
		    { try {continue_or_cooldown();} catch (std::exception& e) { post_error(e.what()); continue;} }

		else if (0==input_line.compare(std::string("stop")))
		    { try{stop();}  catch (std::exception& e) { post_error(e.what()); continue;} }

		else if ((input_line.length()>=bracketedExit.length())
			&& stringCaseInsensitiveEquality(input_line.substr(0,bracketedExit.length()),bracketedExit))
        {
			killAllSubthreads();
			return 0;
		}
        else if (std::regex_match(input_line,entire_set_command_match,set_command_regex))
        {
            set_attribute_name  = entire_set_command_match[1];
            set_attribute_value = entire_set_command_match[2];

            set_command( set_attribute_name.str(), set_attribute_value.str() );

            continue;
        }
		else
		{
		    std::string s = "ivydriver received command from ivymaster \""s + input_line + "\" that was not recognized."s;
			log(slavelogfile,s);
			post_error(s);
			continue;
		}
	}
	// at eof on std::cin

    std::string s = print_logfile_stats(); // separated from next statement to avoid any issues with mutex locks.
    log(slavelogfile,s);

	killAllSubthreads();

	return 0;
};


void IvyDriver::set_command(const std::string& attribute, const std::string& value)
{

    if (attribute == "log"s)
    {
        try
        {
            routine_logging = parse_boolean(value);
            say("<OK>");
        }
        catch (std::exception& e)
        {
            std::ostringstream o;
            o << "<Error> set \"" << attribute << "\" to \"" << value << "\" - invalid value for log.  Only yes/no, on/off, or true/false are accepted." << std::endl;
            say(o.str());
        }
        return;
    }

    if (attribute == "track_long_running_IOs"s)
    {
        try
        {
            track_long_running_IOs = parse_boolean(value);
            say("<OK>");
        }
        catch (std::exception& e)
        {
            std::ostringstream o;
            o << "<Error> set \"" << attribute << "\" to \"" << value << "\" - invalid value for track_long_running_IOs.  Only yes/no, on/off, or true/false are accepted." << std::endl;
            say(o.str());
        }
        return;
    }

    if (attribute == "spinloop"s)
    {
        try
        {
            spinloop = parse_boolean(value);
            say("<OK>");
        }
        catch (std::exception& e)
        {
            std::ostringstream o;
            o << "<Error> set \"" << attribute << "\" to \"" << value << "\" - invalid value for \"" << attribute << "\".  Only yes/no, on/off, or true/false are accepted." << std::endl;
            say(o.str());
        }
        return;
    }

    if (attribute == "fruitless_passes_before_wait"s)
    {
        try
        {
            fruitless_passes_before_wait = unsigned_long(value);
            say("<OK>");
        }
        catch (std::exception& e)
        {
            std::ostringstream o;
            o << "<Error> set \"" << attribute << "\" to \"" << value << "\" - invalid value for fruitless_passes_before_wait.  Must be a positive value expressed as one or more digits 0-9 like \"56\" or \"2\"." << std::endl;
            say(o.str());
        }
        return;
    }

    if (attribute == "sqes_per_submit_limit"s)
    {
        try
        {
            sqes_per_submit_limit = unsigned_int(value);
            if (sqes_per_submit_limit == 0) throw std::invalid_argument("sqes_per_submit_limit must be set to a positive value.");
            say("<OK>");
        }
        catch (std::exception& e)
        {
            std::ostringstream o;
            o << "<Error> set \"" << attribute << "\" to \"" << value << "\" - invalid value for " << attribute << ".  Must be a positive value expressed as one or more digits 0-9 like \"56\" or \"2\"." << std::endl;
            say(o.str());
        }
        return;
    }

    if (attribute == "generate_at_a_time_multiplier"s)
    {
        try
        {
            generate_at_a_time_multiplier = unsigned_int(value);
            say("<OK>");
        }
        catch (std::exception& e)
        {
            std::ostringstream o;
            o << "<Error> set \"" << attribute << "\" to \"" << value << "\" - invalid value for generate_at_a_time_multiplier.  Must be a number greater than zero and less than or equal to 1.0 like \"1\" or \"0.5\"." << std::endl;
            say(o.str());
        }
        return;
    }

    if (attribute == "max_wait_seconds"s)
    {
        ivy_float float_value;

        try
        {
            float_value = number_optional_trailing_percent(value);
        }
        catch (std::exception& e)
        {
            std::ostringstream o;
            o << "<Error> set \"" << attribute << "\" to \"" << value << "\" - invalid value. Must be a positive value with optional trailing %, like \"6\" or \"45.2%\"." << std::endl;
            say(o.str());
            return;
        }

        if (float_value <= 0.0)
        {
            std::ostringstream o;
            o << "<Error> set \"" << attribute << "\" to \"" << value << "\" - invalid value. Must be a positive value with optional trailing %, like \"6\" or \"45.2%\"." << std::endl;
            say(o.str());
        }
        else
        {
            max_wait = ivytime(float_value);
            say("<OK>");
        }

        return;
    }


    {
        std::ostringstream o;
        o << "<Error> set \"" << attribute << "\" to \"" << value << "\" - \"" << attribute << "\" is not recognized as something that can be set in ivydriver." << std::endl;
        say(o.str());
        return;
    }
}


void IvyDriver::waitForSubintervalEndThenHarvest()
{
    // wait for subinterval ending time

    ivytime now; now.setToNow();
    if (now > ivydriver_view_subinterval_end)
	{
        std::ostringstream o;
        o << "subinterval_seconds too short - ivymaster told ivydriver main thread to wait for the end of the subinterval and then harvest results, but subinterval was already over.";
        o << "  This can also be caused if an ivy command device is on a subsystem port that is saturated with other (ivy) activity, making communication with the command device run very slowly.";
        o << "ivydriver_view_subinterval_end = " << ivydriver_view_subinterval_end.format_as_datetime_with_ns() << ", now = " << now.format_as_datetime_with_ns();
        throw std::runtime_error(o.str());
 	}

	try {
		ivydriver_view_subinterval_end.waitUntilThisTime();
	}
	catch (std::exception& e)
	{
		std::ostringstream o;
		o << "waitForSubintervalEndThenHarvest() - exception during waitUntilThisTime() - " << e.what() << std::endl;
        throw std::runtime_error(o.str());
	}

    ivytime subinterval_ending_time;
            subinterval_ending_time.setToNow();

    ivytime limit_time = subinterval_ending_time + ivytime(0.75 * subinterval_duration.getlongdoubleseconds());

	// harvest CPU counters and send CPU line.

	check_CPU_temperature();

	int rc;

	rc = getprocstat(&interval_end_CPU, slavelogfile);

	if ( 0 != rc )
	{
		std::ostringstream o;
		o << "ivydriver main thread routine waitForSubintervalEndThenHarvest(): getprocstat(&interval_end_CPU, slavelogfile) call to get subinterval ending CPU counters failed.\n";
        throw std::runtime_error(o.str());
	}

	struct    cpubusypercent cpubusydetail;
	struct avgcpubusypercent cpubusysummary;

	if ( 0 != computecpubusy (
		&interval_start_CPU, // counters at start of interval
		&interval_end_CPU,
		&cpubusydetail, // this gets filled in as output
		&cpubusysummary, // this gets filled in as output
		active_cores,
		slavelogfile
	))
	{
        std::ostringstream o;
        o << "ivydriver main thread routine waitForSubintervalEndThenHarvest(): computecpubusy() call to get subinterval ending CPU % busies failed.";
        throw std::runtime_error(o.str());
 	}

    if (routine_logging)
 	{
 	    std::ostringstream o;
 	    o << "struct avgcpubusypercent :" << cpubusysummary.toString() << std::endl;
 	    o << cpubusydetail << std::endl;log(slavelogfile, o.str());
    }

    RunningStat_double_long_int* p_rs = (RunningStat_double_long_int*) &digital_readouts;
	say(std::string("[CPU]")+cpubusysummary.toString() + std::string("+") + p_rs->toString()); // this p_rs is to use snprintf instead as it's faster.

	interval_start_CPU.copy(interval_end_CPU);

    ivy_float min_seq_fill_fraction = 1.0;

    RunningStat<double, long int> dispatching_latency_seconds_accumulator;  // These are a new instance each pass through, so don't need clearing.
    RunningStat<double, long int> lock_aquisition_latency_seconds_accumulator;
    RunningStat<double, long int> switchover_completion_latency_seconds_accumulator;

    RunningStat<double, long int> distribution_over_workloads_of_avg_dispatching_latency;
    RunningStat<double, long int> distribution_over_workloads_of_avg_lock_acquisition;
    RunningStat<double, long int> distribution_over_workloads_of_avg_switchover;

    RunningStat<double, long int> subinterval_input_print_time_ms {};
    RunningStat<double, long int> subinterval_output_print_time_ms {};

    ivytime time_x {}, time_y {}, time_z {};

    size_t number_of_IOs_running_at_end_of_subinterval {0};

    for (auto& pear : workload_threads)
    {
        ivytime n; n.setToNow();

        ivytime thread_limit_time = limit_time;

        if (MainThreadCommand::stop == pear.second->ivydriver_main_says)
        {
            // when we are stopping, we need some extra time in the last subinterval to handle catching in-flight I/Os
            // and in any case, there is no problem if it takes somewhat longer as there is no deadline - no next subinterval.
            thread_limit_time = thread_limit_time + subinterval_duration;
            // catch_in_flight_IOs() at the end of the last subinterval cancels any still-running I/Os
            // after waiting up to half a subinterval for I/Os to end.
        }

        if ( n > thread_limit_time)
        {
            std::ostringstream o;
            o << "Interlock failure - over 3/4 of the way through the next subinterval when still sending up workload detail lines for the previous subinterval."
                << "  If there is a huge number of workloads, consider running a longer subinterval_seconds." << std::endl;
            throw std::runtime_error(o.str());
        }

        ivytime togo = thread_limit_time - n;
        std::chrono::nanoseconds time_to_limit ( (int64_t) togo.getAsNanoseconds() );

        {
            WorkloadThread &wlt = (*pear.second);  // the nested block gets a fresh reference

            {
                std::unique_lock<std::mutex> slavethread_lk(wlt.slaveThreadMutex);

                if (!wlt.slaveThreadConditionVariable.wait_for(slavethread_lk, time_to_limit,
                           [&wlt] { return wlt.ivydriver_main_posted_command == false; }))  // WorkloadThread turns this off when switching to a new subinterval
                {
                    std::ostringstream o;
                    o << "Interlock failure - over 3/4 of the way through the next subinterval when still sending up workload detail lines for the previous subinterval."
                        << "  If there is a huge number of workloads, consider running a longer subinterval_seconds." << std::endl;
                    throw std::runtime_error(o.str());
                }

                // now we keep the lock while we are processing this subthread

                number_of_IOs_running_at_end_of_subinterval += wlt.number_of_IOs_running_at_end_of_subinterval;  // zero unless track_long_running_IOs

                dispatching_latency_seconds_accumulator           += pear.second->dispatching_latency_seconds;
                lock_aquisition_latency_seconds_accumulator       += pear.second->lock_aquisition_latency_seconds;
                switchover_completion_latency_seconds_accumulator += pear.second->switchover_completion_latency_seconds;

                distribution_over_workloads_of_avg_dispatching_latency.push(pear.second->dispatching_latency_seconds.avg());
                distribution_over_workloads_of_avg_lock_acquisition   .push(pear.second->lock_aquisition_latency_seconds.avg());
                distribution_over_workloads_of_avg_switchover         .push(pear.second->switchover_completion_latency_seconds.avg());

                for (auto& pTestLUN : wlt.pTestLUNs)
                {
                    for (auto& peach : pTestLUN->workloads)
                    {
                        {
                            // Inner code block to get fresh references

                            const std::string& workloadID = peach.first;
                            Workload& workload = peach.second;

                            if (workload.sequential_fill_fraction < min_seq_fill_fraction)
                            {
                                min_seq_fill_fraction = workload.sequential_fill_fraction;
                            }

                            if(    (workload.subinterval_array)[next_to_harvest_subinterval].subinterval_status
                                == subinterval_state::ready_to_send)
                            {
                                (workload.subinterval_array)[next_to_harvest_subinterval].subinterval_status
                                    = subinterval_state::sending;
                            }
                            else
                            {
                                std::ostringstream o;
                                o << "ivydriver main thread routine waitForSubintervalEndThenHarvest(): "
                                    << " WorkloadThread for physical core " << wlt.physical_core << " hyperthread " << wlt.hyperthread
                                    << " workload ID " << workloadID
                                    << " interlocked at subinterval end, but WorkloadThread hadn\'t marked subinterval ready-to-send.";
                                throw std::runtime_error(o.str());
                            }

                            // indent level with lock held
                            {
                                ostringstream o;

                                time_x.setToNow();

                                if (!(workload.subinterval_array)[next_to_harvest_subinterval].output.toBuffer(subintervalOutput_buffer, sizeof(subintervalOutput_buffer)))
                                {
                                    o << "<Error> Internal programming error - output buffer too small to contain serialized SubintervalOutput object." << std::endl;
                                }
                                else
                                {
                                    time_y.setToNow(); subinterval_output_print_time_ms.push( 1000. * ((time_y-time_x).getlongdoubleseconds()) );

                                    o << '<' << workloadID << '>'
                                        << (workload.subinterval_array)[next_to_harvest_subinterval].input.toString()
                                        << subintervalOutput_buffer
                                        << std::endl;
                                    time_z.setToNow(); subinterval_input_print_time_ms.push( 1000. * ((time_z-time_y).getlongdoubleseconds()) );
                                }

                                say(o.str());
                            }

                            // Copy subinterval object from running subinterval to inactive subinterval.
                            (workload.subinterval_array)[next_to_harvest_subinterval].input.copy(  (workload.subinterval_array)[otherSubinterval].input);

                            // Zero out inactive subinterval output object.
                            (workload.subinterval_array)[next_to_harvest_subinterval].output.clear();

                            (workload.subinterval_array)[next_to_harvest_subinterval].subinterval_status
                                = subinterval_state::ready_to_run;
                        }
                    }
                }
            } // lock drops here as exit this block

            // Note that we sent data up to pipe_driver_subthread,
            // and we have cleared out and prepared the other subinterval to be marked "ready to run",
            // but we have not posted a command to WorkloadThread, so it will ignore any condition variable notifications until then.

            pear.second->slaveThreadConditionVariable.notify_all();  // probably superfluous

            // (The command to the workload will be posted later, after we get "continue" or "stop".)
        }
        // end nested block for fresh WorkloadThread reference
    }
    // end of loop over workload threads

	say(std::string("<end>"));  // end of workload detail lines

    {
        std::ostringstream o;
        o << "min_seq_fill_fraction = " << min_seq_fill_fraction;
        say(o.str());
    }

    {
        std::ostringstream o;
        o << "latencies: "
            << dispatching_latency_seconds_accumulator.toString()
            << lock_aquisition_latency_seconds_accumulator.toString()
            << switchover_completion_latency_seconds_accumulator.toString()
            << distribution_over_workloads_of_avg_dispatching_latency.toString()
            << distribution_over_workloads_of_avg_lock_acquisition.toString()
            << distribution_over_workloads_of_avg_switchover.toString()
            << subinterval_input_print_time_ms.toString()
            << subinterval_output_print_time_ms.toString()
            ;

        say(o.str());
    }

    {
        std::ostringstream o;
        o << "running IOs: " << number_of_IOs_running_at_end_of_subinterval << std::endl;
        say(o.str());
    }

	if (0 == next_to_harvest_subinterval)
	{
		next_to_harvest_subinterval = 1;
		otherSubinterval = 0;
	}
	else
	{
		next_to_harvest_subinterval = 0;
		otherSubinterval = 1;
	}

	ivydriver_view_subinterval_start = ivydriver_view_subinterval_start + subinterval_duration;
	ivydriver_view_subinterval_end   = ivydriver_view_subinterval_end   + subinterval_duration;
	// These are just used to print a message without looking into a particular WorkloadThread.

	return;
}
// end of waitForSubintervalEndThenHarvest()


void IvyDriver::distribute_TestLUNs_over_cores()
{
    for (auto& pear : active_cores)
    {
        pear.second = false;
    }

    for (auto& pear : all_workload_threads)
    {
        pear.second->pTestLUNs.clear();
    }

    if (testLUNs.size() == 0)
    {
        std::ostringstream o;
        o << "\"Go\" command received, but there are no TestLUNs, meaning there are no Workloads to run." << std::endl;
        throw std::runtime_error(o.str());
    }

    auto wt_iter = all_workload_threads.begin();

    for (auto& pear : testLUNs)
    {
        TestLUN* pTestLUN = &pear.second;

        active_cores[wt_iter->first] = true;  // later when computing CPU busy this filters for those processors that are running driving I/O.

        WorkloadThread* pWorkloadThread = wt_iter->second;

        pWorkloadThread->pTestLUNs.push_back(pTestLUN);

        pTestLUN->pWorkloadThread = pWorkloadThread;

        for (auto& peach : pTestLUN->workloads)
        {
            Workload* pWorkload = &peach.second;
            pWorkload->pWorkloadThread = pWorkloadThread;
            pWorkload->pTestLUN = pTestLUN;
        }

        wt_iter++;
        if (wt_iter == all_workload_threads.end())
        {
            wt_iter = all_workload_threads.begin();
        }
    }

    workload_threads.clear();

    for (auto& pear : all_workload_threads)
    {
        unsigned int    core            = pear.first;
        WorkloadThread* pWorkloadThread = pear.second;

        if (pWorkloadThread->pTestLUNs.size() > 0)
        {
            workload_threads[core] = pWorkloadThread;
        }
    }

    // This next bit initializes starting point iterators that marks the starting
    // point for iterating over all TestLUNs managed by each core.

    // After every use, the starting point bookmark is incremented (with wraparound)
    // so that every instance of the thing the bookmark gets pointed to takes
    // an equal turn being first.

    for (auto& pear : workload_threads)
    {
        //unsigned int core_number = pear.first;
        WorkloadThread* pWorkloadThread = pear.second;

        if ( pWorkloadThread->pTestLUNs.size() == 0 )
        {
            pWorkloadThread->pTestLUN_reap_IOs_bookmark =
            pWorkloadThread->pTestLUN_pop_bookmark =
            pWorkloadThread->pTestLUN_generate_bookmark =
            pWorkloadThread->pTestLUN_populate_sqes_bookmark =
                pWorkloadThread->pTestLUNs.end();
        }
        else
        {
            pWorkloadThread->pTestLUN_reap_IOs_bookmark =
            pWorkloadThread->pTestLUN_pop_bookmark =
            pWorkloadThread->pTestLUN_generate_bookmark =
            pWorkloadThread->pTestLUN_populate_sqes_bookmark =
                pWorkloadThread->pTestLUNs.begin();
        }
    }

    // Now we initialize iterators but for Workloads within each LUN
    for (auto& pear : testLUNs)
    {
        {
            TestLUN& testLUN = pear.second;
            if (testLUN.workloads.size() == 0)
            {
                testLUN.start_IOs_Workload_bookmark =
                testLUN.pop_one_bookmark            =
                testLUN.generate_one_bookmark       =
                    testLUN.workloads.end();
            }
            else // this is just to give some comfort.  If workloads.size() is 0, then .begin() has the value .end().
            {
                testLUN.start_IOs_Workload_bookmark =
                testLUN.pop_one_bookmark            =
                testLUN.generate_one_bookmark       =
                    testLUN.workloads.begin();
            }
        }
    }

    return;
}


void IvyDriver::create_workload()
{
    // We get "[CreateWorkload] myhostname+/dev/sdxy+workload_name [Parameters] subinterval_input.toString()

    // The myhostname+/dev/sdxy+workload_name part is a "WorkloadID"

    std::string workloadID, newWorkloadParameters;

    trim(remainder);
    unsigned int i=0;
    while (i < remainder.length() && remainder[i]!='[')
        i++;
    if (0==i || (!startsWith("[Parameters]",remainder.substr(i,remainder.length()-i),newWorkloadParameters)))
    {
        throw std::runtime_error("CreateWorkload] did not have \"[Parameters]\"");
    }
    workloadID=remainder.substr(0,i);
    trim(workloadID);
    trim(newWorkloadParameters);

    // we get "[CreateWorkload] sun159+/dev/sdxx+bork [Parameters]subinterval_input.toString()

    if (routine_logging) log(slavelogfile,std::string("[CreateWorkload] WorkloadID is ")+workloadID+std::string("\n"));

    WorkloadID wID(workloadID);
    if (!wID.isWellFormed)
    {
        std::ostringstream o;
        o << "[CreateWorkload] - workload ID \"" << workloadID << "\" did not look like hostname+/dev/sdxxx+workload_name." << std::endl;
        throw std::runtime_error(o.str());
    }

    std::string lun_part = wID.getHostLunPart();

    TestLUN* pTestLUN;

    auto itr = testLUNs.find(lun_part); // This is independent of ownership of TestLUNs by WorkloadTheads
    if (testLUNs.end() == itr)
    {
        pTestLUN = &(testLUNs[lun_part]); // create a new one

        std::map<std::string, LUN*>::iterator nabIt = LUNpointers.find(wID.getLunPart());

        if (LUNpointers.end() == nabIt)
        {
            std::ostringstream o;
            o << "[CreateWorkload] - workload ID \"" << workloadID << "\"  - no such LUN \"" << wID.getLunPart() << "\"." << std::endl;
            throw std::runtime_error(o.str());
        }

        LUN* pL = (*nabIt).second;
        pTestLUN->pLUN = pL;

        //Now we validate & parse the maxLBA
        if (!pL->contains_attribute_name(std::string("Max LBA")))
        {
            std::ostringstream o;
            o << "[CreateWorkload] - workload ID \"" << workloadID <<  "\" - LUN \"" << wID.getLunPart() << "\" - does not have a \"Max LBA\" attribute." << std::endl;
            throw std::runtime_error(o.str());
        }

        std::string maxLBAtext = pL->attribute_value(std::string("Max LBA"));
        trim(maxLBAtext);
        if (!isalldigits(maxLBAtext))
        {
            std::ostringstream o;
            o << "[CreateWorkload] - workload ID \"" << workloadID <<  "\" - LUN \"" << wID.getLunPart()
                << "\" - \"Max LBA\" attribute value \"" << maxLBAtext << "\" is not all digits." << std::endl;
            throw std::runtime_error(o.str());

        }
        long long int maxLBA;
        {
            std::istringstream is(maxLBAtext);
            is >> maxLBA;
        }

        pTestLUN->maxLBA = maxLBA;
    }
    else
    {
        pTestLUN = &(itr->second);

        auto& map = pTestLUN->workloads;
        if (map.find(workloadID) != map.end())
        {
            std::ostringstream o;
            o << "[CreateWorkload] failed - workloadID \"" << workloadID << "\" already exists! Not supposed to happen.  ivymaster is supposed to know." << std::endl;
            throw std::runtime_error(o.str());
        }
    }

    pTestLUN->LUN_size_bytes = (1 + pTestLUN->maxLBA) * pTestLUN->sector_size;

    pTestLUN->host_plus_lun = lun_part;

    Workload* pWorkload = &((pTestLUN->workloads)[workloadID]);

    pWorkload->workloadID = wID;

    pWorkload->uint64_t_hash_of_host_plus_LUN = (uint64_t) std::hash<std::string>{}(pTestLUN->host_plus_lun);

    pWorkload->iosequencerParameters = newWorkloadParameters;

    pWorkload->subinterval_array[0].input.fromString(newWorkloadParameters,slavelogfile);
    pWorkload->subinterval_array[1].input.fromString(newWorkloadParameters,slavelogfile);

    pWorkload->pTestLUN = pTestLUN;

    say("<OK>");

    return;
}

void IvyDriver::edit_workload()
{
    // We get "[EditWorkload] <myhostname+/dev/sdxy+workload_name, myhostname+/dev/sdxz+workload_name> [Parameters] IOPS = *1.25

    // The <myhostname+/dev/sdxy+workload_name, myhostname+/dev/sdxz+workload_name> part is the toString()/fromString() format for ListOfWorkloadIDs.

    // The [parameters] field has already been validated by having successfully been applied to the up to date synchronized copy
    // of our "next subinterval" iosequencer_input object.  ivymaster keeps this copy in the WorkloadTracker object.

    // So as long as the ivy engine itself isn't broken, the workload IDs are for valid existing workload threads, and we already
    // have shown that the parameters apply cleanly to the current iosequencer_input object.

    if (routine_logging) log( slavelogfile,std::string("[EditWorkload]") + remainder + std::string("\n") );

    std::string listOfWorkloadIDsText, parametersText;

    trim(remainder);
    unsigned int i=0;
    while (i < remainder.length() && remainder[i]!='[')
        i++;
    if (0==i || (!startsWith("[Parameters]",remainder.substr(i,remainder.length()-i),parametersText)))
    {
        throw std::runtime_error("[EditWorkload] did not have \"[Parameters]\"");
    }
    listOfWorkloadIDsText=remainder.substr(0,i);
    trim(listOfWorkloadIDsText);
    trim(parametersText);

    if(routine_logging) log(slavelogfile,std::string("[EditWorkload] WorkloadIDs are ")+listOfWorkloadIDsText+std::string(" and parametersText=\"")+parametersText+std::string("\".\n"));

    ListOfWorkloadIDs lowi;

    if (!lowi.fromString(&listOfWorkloadIDsText))
    {
        throw std::runtime_error(std::string("<Error> [EditWorkload] invalid list of WorkloadIDs, should look like <myhostname+/dev/sdxy+workload_name,myhostname+/dev/sdxz+workload_name> - \"")
            + listOfWorkloadIDsText + std::string("\"."));
    }

    for (auto& wID : lowi.workloadIDs)
    {
        //std::unordered_map<std::string, WorkloadThread*>  workload_threads;  // Key looks like "workloadName:host:00FF:/dev/sdxx:2A"

        std::string host_plus_lun = wID.getHostLunPart();

        auto it = testLUNs.find(host_plus_lun);

        if (testLUNs.end() == it)
        {
            std::ostringstream o;
            o << "ivydriver for host \"" << ivyscript_hostname << "\""
                << " - [EditWorkload] no such WorkloadID \"" << wID.workloadID << "\""
                << " - did not find TestLUN \"" << host_plus_lun << "\"." << std::endl
                << "Source code reference line " << __LINE__ << " of " << __FILE__ << ".\n";
            throw std::runtime_error(o.str());
        }

        TestLUN* pTestLUN = &(it->second);

        auto w_itr = pTestLUN->workloads.find(wID.workloadID);

        if (w_itr == pTestLUN->workloads.end())
        {
            std::ostringstream o;
            o << "ivydriver for host \"" << ivyscript_hostname << "\""
                << " - [EditWorkload] no such WorkloadID \"" << wID.workloadID << "\""
                << " within TestLUN \"" << host_plus_lun << "\"." << std::endl
                << "Source code reference line " << __LINE__ << " of " << __FILE__ << ".\n";
            throw std::runtime_error(o.str());
        }

        Workload* pWorkload = &(w_itr->second);


        //Original comment describing design idea:
        // If the workload thread is running, we update the next subinterval.
        // If it is stopped, we update both subintervals.

        // Now we update both running and not-running subintervals,
        // but we keep having a separate IosequencerInput object
        // for both running and non-running subintervals, so that
        // in future, we could go back to the originally conceived way
        // in case we need to make an atomic (all-at-once) change.
        // However, this would affect DFC, since the changes don't
        // take effect right away.  Thus we might want to use
        // today's behaviour for a simple IOPS change, but the
        // atomic method for changes to those parameters requiring it.

        auto rv = pWorkload->subinterval_array[0].input.setMultipleParameters(parametersText);

        if (!rv.first)
        {
            std::ostringstream o;
            o << "Internal programming error since we already successfully set these parameters into the corresponding WorkloadTracker objects in ivymaster - failed setting parameters \""
              << parametersText << "\" into subinterval_array[0].input iosequencer_input object for WorkloadID \""
              << wID.workloadID << "\" - "
              << rv.second;
            throw std::runtime_error(o.str());
        }

        rv = pWorkload->subinterval_array[1].input.setMultipleParameters(parametersText);

        if (!rv.first)
        {
            std::ostringstream o;
            o << "Internal programming error since we already successfully set these parameters into the corresponding WorkloadTracker objects in ivymaster - failed setting parameters \""
              << parametersText << "\" into subinterval_array[1].input iosequencer_input object for WorkloadID \""
              << wID.workloadID << "\" - "
              << rv.second;
            throw std::runtime_error(o.str());
        }
    }

    say("<OK>");
}

void IvyDriver::delete_workload()
{
    // We get "[DeleteWorkload]myhostname+/dev/sdxy+workload_name"

    // The myhostname+/dev/sdxy+workload_name part is a "WorkloadID"

    WorkloadID wid;

    trim(remainder);

    if (!wid.set(remainder))
    {
        std::ostringstream o;
        o << "[DeleteWorkload] failed - WorkloadID \"" << remainder << "\" is not well-formed like \"myhostname+/dev/sdxy+workload_name\"." << std::endl;
        throw std::runtime_error(o.str());
    }

    std::string host_plus_lun = wid.getHostLunPart();

    auto itr = testLUNs.find(host_plus_lun);
    if ( itr == testLUNs.end() )
    {
        std::ostringstream o;
        o << "[DeleteWorkload] for \"" << wid.workloadID
            << "\" failed - LUN \"" << host_plus_lun << "\" does not exist." << std::endl;
        throw std::runtime_error(o.str());
    }

    auto rit = itr->second.workloads.find(wid.workloadID);
    if ( rit == itr->second.workloads.end() )
    {
        std::ostringstream o;
        o << "[DeleteWorkload] for \"" << wid.workloadID << "\" failed - workload not found within test LUN." << std::endl;
        throw std::runtime_error(o.str());
    }

    itr->second.workloads.erase(rit);

    if (itr->second.workloads.size() == 0)
    {
        testLUNs.erase(itr);
    }

    if (routine_logging)
    {
        std::ostringstream o;
        o << "[DeleteWorkload] " << wid.workloadID << " successful." << std::endl;
        log(slavelogfile, o.str());
    }

    say("<OK>");
}

void IvyDriver::go()
{
    std::string dash_spinloop {"-spinloop"};

    if (endsWith(subinterval_duration_as_string,dash_spinloop))
    {
        spinloop = true;
        subinterval_duration_as_string.erase
        (
            subinterval_duration_as_string.size()-dash_spinloop.size(),
            dash_spinloop.size()
        );
    }

    distribute_TestLUNs_over_cores();

    if (routine_logging)
    {
        log_TestLUN_ownership();
    }

    step_number++;

    test_start_time.setToNow();
    next_to_harvest_subinterval=0; otherSubinterval=1;

    if (!subinterval_duration.durationFromString(subinterval_duration_as_string))
    {
        ostringstream o;
        o << "received command \"" << rtrim(input_line) << "\", saw \"Go!\" with a subinterval duration ivytime string representation of \""
            << subinterval_duration_as_string << "\", but this failed to convert to an ivytime when calling \"fromString()\".  "
            << "Valid ivytime string representations look like \"<seconds,nanoseconds>\"." << std::endl;
        throw std::runtime_error(o.str());
    }

    ivydriver_view_subinterval_start = test_start_time;
    ivydriver_view_subinterval_end = test_start_time + subinterval_duration;

    harvest_subinterval_number = 0;
    harvest_start = test_start_time;
    harvest_end   = harvest_start + subinterval_duration;

    // harvest CPU counters starting time first subinterval

    if (0!=getprocstat(&interval_start_CPU,slavelogfile))
    {
        say("<Error> ivydriver main thread: procstatcounters::read_CPU_counters() call to get first subinterval starting CPU counters failed.");
    }

    for (auto& pear : workload_threads)
    {
        {
            WorkloadThread& wrkldThread = *(pear.second);

            std::unique_lock<std::mutex> u_lk(wrkldThread.slaveThreadMutex);
            if (ThreadState::waiting_for_command != wrkldThread.state)
            {
                ostringstream o;
                o << "received \"Go!\" command, but thread for core " << pear.first << " was in " << threadStateToString(wrkldThread.state) << " state, not in \"waiting_for_command\" state." << std::endl;
                throw std::runtime_error(o.str());
            }

            wrkldThread.ivydriver_main_posted_command = true;
            wrkldThread.ivydriver_main_says = MainThreadCommand::start;
            if(routine_logging) {log(wrkldThread.slavethreadlogfile,"[logged here by ivydriver main thread] ivydriver main thread posted \"start\" command.");}

            wrkldThread.slaveThreadConditionVariable.notify_all();
        }
    }
{std::ostringstream o; o << " after checking if workload threads are in waiting for command state and posting start command."; log(slavelogfile,o.str());}

    // Although we have prepared two subintervals for the iosequencer thread to use,
    // when the iosequencer threads get to the end of the first subinterval, they are going to
    // expect us to have posted another MainThreadCommand::start command.

    for (auto& pear : workload_threads)
    {
        {
            // Wait for the workload thread to have consumed the "start" command
            // then post a "keep going" command
            std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);
            while (true == pear.second->ivydriver_main_posted_command)        // should have a timeout?? How long?
                pear.second->slaveThreadConditionVariable.wait(u_lk);
            pear.second->ivydriver_main_posted_command=true;
            pear.second->ivydriver_main_says=MainThreadCommand::keep_going;
            if (routine_logging) { log(pear.second->slavethreadlogfile,"[logged here by ivydriver main thread] ivydriver main thread posted \"keep_going\" command."); }
        }
        pear.second->slaveThreadConditionVariable.notify_all();
    }
{std::ostringstream o; o << " after waiting for workload threads to consume start command and then posting keep going command."; log(slavelogfile,o.str());}

    say("<OK>");
}

void IvyDriver::continue_or_cooldown()
{
    for (auto& pear : workload_threads)
    {
        { // lock
            std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);

            if (ThreadState::running != pear.second->state)
            {
                ostringstream o;
                o << "received \"continue\" command, but thread for core " << pear.first
                    << " was in " << threadStateToString(pear.second->state)
                    << " state, not in \"running\" state." << std::endl;
                throw std::runtime_error(o.str());
            }

            // At this point, ivy master has made any modifications to the input parameters for the next subinterval
            // and the "other" subsystem has been harvested and we need to turn it around to get ready to run

            pear.second->ivydriver_main_posted_command=true;
            pear.second->ivydriver_main_says=MainThreadCommand::keep_going;

            if (routine_logging)
            {
                std::ostringstream o;
                o << "[logged here by ivydriver main thread] ivydriver main thread posted \"keep_going\" command.";
                log(pear.second->slavethreadlogfile,o.str());
            }
        }
        pear.second->slaveThreadConditionVariable.notify_all();
    }
    say("<OK>");
}


void IvyDriver::stop()
{
    for (auto& pear : workload_threads)
    {
        { // lock
            std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);
            if (ThreadState::running != pear.second->state)
            {
                ostringstream o;
                o << "received \"stop\" command, but thread for core " << pear.first
                    << " was in " << threadStateToString(pear.second->state) << " state, not in \"running\" state." << std::endl;
                throw std::runtime_error(o.str());
            }

            // at this point, what we do is copy the input parameters from the running subinterval
            // to the other one, to get it ready for the next test run as if the thread had just been created

            pear.second->ivydriver_main_posted_command=true;
            pear.second->ivydriver_main_says=MainThreadCommand::stop;
            if(routine_logging) { log(pear.second->slavethreadlogfile,"[logged here by ivydriver main thread] ivydriver main thread posted \"stop\" command."); }
        }
        pear.second->slaveThreadConditionVariable.notify_all();
    }
    say("<OK>");
}

// Parts of signal handler adapted from :
// https://www.linuxjournal.com/files/linuxjournal.com/linuxjournal/articles/063/6391/6391l3.html

void sig_handler(int sig, siginfo_t *p_siginfo, void *context)
{
    std::ostringstream o;

    if (routine_logging) { o << "<Warning> signal " << sig << "(" << strsignal(sig) << ") received"; }

    switch (sig)
    {
        case SIGILL:
            switch (p_siginfo->si_code)
            {
                // The following values can be placed in si_code for a SIGILL signal:
                case ILL_ILLOPC: o << " with code ILL_OPC - Illegal opcode." << std::endl; goto past_si_code;
                case ILL_ILLOPN: o << " with code ILLOPN - Illegal operand." << std::endl; goto past_si_code;
                case ILL_ILLADR: o << " with code ILL_ILLADR - Illegal addressing mode." << std::endl; goto past_si_code;
                case ILL_ILLTRP: o << " with code ILL_ILLTRP - Illegal trap." << std::endl; goto past_si_code;
                case ILL_PRVOPC: o << " with code ILL_PRVOPC - Privileged opcode." << std::endl; goto past_si_code;
                case ILL_PRVREG: o << " with code ILL_PRVREG - Privileged register." << std::endl; goto past_si_code;
                case ILL_COPROC: o << " with code ILL_COPROC - Coprocessor error." << std::endl; goto past_si_code;
                case ILL_BADSTK: o << " with code ILL_COPROC - Internal stack error." << std::endl; goto past_si_code;

                default: o << " with unknown SIGILL code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
            }
        case SIGFPE:
            switch (p_siginfo->si_code)
            {
                // The following values can be placed in si_code for a SIGFPE signal:
                case FPE_INTDIV: o << " with code FPE_INTDIV - Integer divide by zero." << std::endl; goto past_si_code;
                case FPE_INTOVF: o << " with code FPE_INTOVF - Integer overflow." << std::endl; goto past_si_code;
                case FPE_FLTDIV: o << " with code FPE_FLTDIV - Floating-point divide by zero." << std::endl; goto past_si_code;
                case FPE_FLTOVF: o << " with code FPE_FLTOVF - Floating-point overflow." << std::endl; goto past_si_code;
                case FPE_FLTUND: o << " with code FPE_FLTUND - Floating-point underflow." << std::endl; goto past_si_code;
                case FPE_FLTRES: o << " with code FPE_FLTRES - Floating-point inexact result." << std::endl; goto past_si_code;
                case FPE_FLTINV: o << " with code FPE_FLTINV - Floating-point invalid operation." << std::endl; goto past_si_code;
                case FPE_FLTSUB: o << " with code FPE_FLTSUB - Subscript out of range." << std::endl; goto past_si_code;

                default: o << " with unknown SIGFPE code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
            }
        case SIGSEGV:
            switch (p_siginfo->si_code)
            {
                // The following values can be placed in si_code for a SIGSEGV signal:
                case SEGV_MAPERR: o << " with code SEGV_MAPERR - Address not mapped to object." << std::endl; goto past_si_code;
                case SEGV_ACCERR: o << " with code SEGV_ACCERR - Invalid permissions for mapped object." << std::endl; goto past_si_code;
                //case SEGV_BNDERR: o << " with code SEGV_BNDERR - Failed address bound checks." << std::endl; goto past_si_code;
                //case SEGV_PKUERR: o << " with code SEGV_PKUERR - Access was denied by memory protection keys.  See pkeys(7).  The protection key which applied to this access is available via si_pkey." << std::endl; goto past_si_code;

                default: o << " with unknown SIGSEGV code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
            }
        case SIGBUS:
            switch (p_siginfo->si_code)
            {
                // The following values can be placed in si_code for a SIGBUS signal:
                case BUS_ADRALN: o << " with code BUS_ADRALN - Invalid address alignment." << std::endl; goto past_si_code;
                case BUS_ADRERR: o << " with code BUS_ADRERR - Nonexistent physical address." << std::endl; goto past_si_code;
                case BUS_OBJERR: o << " with code BUS_OBJERR - Object-specific hardware error." << std::endl; goto past_si_code;
                //case BUS_MCEERR_AR: o << " with code BUS_MCEERR_AR - Hardware memory error consumed on a machine check; action required." << std::endl; goto past_si_code;
                //case BUS_MCEERR_AO: o << " with code BUS_MCEERR_AO - Hardware memory error detected in process but not consumed; action optional." << std::endl; goto past_si_code;

                default: o << " with unknown SIGBUS code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
            }
        case SIGTRAP:
            switch (p_siginfo->si_code)
            {
                // The following values can be placed in si_code for a SIGTRAP signal:
                case TRAP_BRKPT: o << " with code TRAP_BRKPT - Process breakpoint." << std::endl; goto past_si_code;
                case TRAP_TRACE: o << " with code TRAP_TRACE - Process trace trap." << std::endl; goto past_si_code;
                //case TRAP_BRANCH: o << " with code TRAP_BRANCH - Process taken branch trap." << std::endl; goto past_si_code;
                //case TRAP_HWBKPT: o << " with code TRAP_HWBKPT - (IA64 only) Hardware breakpoint/watchpoint." << std::endl; goto past_si_code;

                default: o << " with unknown SIGTRAP code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
            }
        case SIGCHLD:
            switch (p_siginfo->si_code)
            {
                // The following values can be placed in si_code for a SIGCHLD signal:
                case CLD_EXITED: o << " with code CLD_EXITED - Child has exited." << std::endl; goto past_si_code;
                case CLD_KILLED: o << " with code CLD_KILLED - Child was killed." << std::endl; goto past_si_code;
                case CLD_DUMPED: o << " with code CLD_DUMPED - Child terminated abnormally." << std::endl; goto past_si_code;
                case CLD_TRAPPED: o << " with code CLD_TRAPPED - Traced child has trapped." << std::endl; goto past_si_code;
                case CLD_STOPPED: o << " with code CLD_STOPPED - Child has stopped." << std::endl; goto past_si_code;
                case CLD_CONTINUED: o << " with code CLD_CONTINUED - Stopped child has continued." << std::endl; goto past_si_code;

                default: o << " with unknown SIGCHLD code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
            }
        case SIGIO: // and case SIGPOLL, which has the same signal number (alias)
            switch (p_siginfo->si_code)
            {
                // The following values can be placed in si_code for a SIGIO/SIGPOLL signal:
                case POLL_IN: o << " with code POLL_IN - Data input available." << std::endl; goto past_si_code;
                case POLL_OUT: o << " with code POLL_OUT - Output buffers available." << std::endl; goto past_si_code;
                case POLL_MSG: o << " with code POLL_MSG - Input message available." << std::endl; goto past_si_code;
                case POLL_ERR: o << " with code POLL_ERR - I/O error." << std::endl; goto past_si_code;
                case POLL_PRI: o << " with code POLL_PRI - High priority input available." << std::endl; goto past_si_code;
                case POLL_HUP: o << " with code POLL_HUP - Device disconnected." << std::endl; goto past_si_code;

                default: o << " with unknown SIGIO/SIGPOLL code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
            }
//        case SIGSYS:
//            switch (p_siginfo->si_code)
//            {
//                // The following value can be placed in si_code for a SIGSYS signal:
//                case SYS_SECCOMP: o << " with code SYS_SECCOMP - Triggered by a seccomp(2) filter rule." << std::endl; goto past_si_code;
//
//                default: o << " with unknown SIGSYS code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
//            }
        default:
            switch (p_siginfo->si_code)
            {
                case SI_USER:    o << " with code SI_USER - kill."                 << std::endl; goto past_si_code;
                case SI_KERNEL:  o << " with code SI_KERNEL - Sent by the kernel." << std::endl; goto past_si_code;
                case SI_QUEUE:   o << " with code SI_QUEUE - sigqueue."            << std::endl; goto past_si_code;
                case SI_TIMER:   o << " with code SI_TIMER - POSIX timer expired." << std::endl; goto past_si_code;
                case SI_MESGQ:   o << " with code SI_MESGQ - POSIX message queue state changed; see mq_notify(3)." << std::endl; goto past_si_code;
                case SI_ASYNCIO: o << " with code SI_ASYNCIO - AIO completed." << std::endl; goto past_si_code;
                case SI_SIGIO:   o << " with code SI_SIGIO - SIGIO/SIGPOLL fills in si_code." << std::endl; goto past_si_code;
                case SI_TKILL:   o << " with code SI_TKILL - tkill(2) or tgkill(2)." << std::endl; goto past_si_code;

                default: o << " with unknown code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
            }
    }
past_si_code:

    auto pid = getpid();
    auto tid = syscall(SYS_gettid);

    if (routine_logging) { o << "getpid() = " << pid << ", gettid() = " << tid << ", getpgid(getpid()) = " << getpgid(getpid()) << std::endl; }

    switch(sig)
    {
        case SIGINT:
            o << "Control-C was pressed." << std::endl;
            log(ivydriver.slavelogfile, o.str());
            _exit(0);

        case SIGCHLD:
            switch (p_siginfo->si_code)
            {
                case CLD_EXITED:
                    o << "Child exited." << std::endl;
                    break;

                case CLD_KILLED:
                    o << "Child was killed." << std::endl;
                    break;

                case CLD_DUMPED:
                    o << "Child has terminated abnormally and created a core file." << std::endl;
                    break;

                case CLD_TRAPPED:
                    o << "Traced child has trapped." << std::endl;
                    break;

                case CLD_STOPPED:
                    o << "Child has stopped." << std::endl;
                    break;

                case CLD_CONTINUED:
                    o << "Stopped child has continued." << std::endl;
                    break;

                default:
                    o << "Unknown sub-code " << p_siginfo->si_code << " for SIGCHLD." << std::endl;
                    break;
            }
            o << "Child\'s pid = " << p_siginfo->si_pid << ", real uid = " << p_siginfo->si_uid << ", exit status or signal = " << p_siginfo->si_status << std::endl;
            break;

        case SIGHUP:
            if (pid == tid) // if I'm ivydriver main thread
            {
                std::ostringstream o;
                o << "<Error> Connection from ivy master host was broken.";
                log(ivydriver.slavelogfile, o.str());
                ivydriver.killAllSubthreads();
                exit(-1);
            }
            break;

        case SIGUSR1:
            break;

        case SIGSEGV:
            log(ivydriver.slavelogfile, o.str());
            _exit(0);

    }

    if (routine_logging) { log (ivydriver.slavelogfile,o.str()); }

    return;
}



void IvyDriver::log_TestLUN_ownership()
{
    std::ostringstream o;

    unsigned int testLUN_count{0}, workload_count {0};

    o << "There are " << workload_threads.size() << " workload threads with TestLUNs assigned." << std::endl;

    for (auto& pear : workload_threads)
    {
        WorkloadThread* pWorkloadThread = pear.second;

        o << std::endl << "physical core " << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << " owns ";

        if (pWorkloadThread->pTestLUNs.size() == 0)
        {
            o << "no TestLUNs" << std::endl;
        }
        else
        {
            unsigned int wt_LUNs      = 0;
            unsigned int wt_workloads = 0;

            for (auto& pTestLUN : pWorkloadThread->pTestLUNs)
            {
                testLUN_count++;
                wt_LUNs++;

                o << pTestLUN->host_plus_lun << " ";

                wt_workloads   += pTestLUN->workloads.size();
                workload_count += pTestLUN->workloads.size();

//                for (auto& pear : pTestLUN->workloads)
//                {
//                    o << "      " << pear.first << " - "<< pear.second.brief_status() << std::endl;
//
//                    for (unsigned int i = 0; i <= 1; i++)
//                    {
//                         o << "         subinterval_array[" << i << "]'s IosequencerInput is " << pear.second.subinterval_array[i].input.toStringFull() << endl;
//                    }
//
//
//                }
            }
            o << wt_LUNs << " TestLUNs with a total of " << wt_workloads << " workloads." << std::endl;
        }
    }

    o << "Total assignments: " << testLUN_count << " TestLUNs and " << workload_count << " Workloads." << std::endl;

    o << "This compares with ivydriver having " << testLUNs.size() << " TestLUNs." << std::endl;

    log(slavelogfile,o.str());
}



void IvyDriver::log_iosequencer_settings(const std::string& s)
{
    std::ostringstream o;

    o << s << std::endl;

    if (testLUNs.size() == 0 )
    {
        o << "No TestLUNs exist.";
    }
    else
    {
        for (auto& pear : testLUNs)
        {
            TestLUN* pTestLUN = & pear.second;

            o << "TestLUN - " << pTestLUN->host_plus_lun << std::endl;

            for (auto& peach : pTestLUN->workloads)
            {
                Workload* pWorkload = & peach.second;

                o << "   Workload - " << pWorkload->workloadID << std::endl;

                for (unsigned int i = 0; i <= 1; i++)
                {
                    o << "      subinterval_array[" << i << "]'s IosequencerInput is " << pWorkload->subinterval_array[i].input.toStringFull() << endl;
                }

                o << "      p_current_IosequencerInput points to - " << pWorkload->p_current_IosequencerInput->toStringFull() << std::endl;

            }
        }
    }

    o << std::endl;

    log(slavelogfile, o.str());

    return;
}

void IvyDriver::check_CPU_temperature()
{
    unsigned int cpu = 0;

    digital_readouts.clear();

    while(true)
    {
        {
            std::ostringstream msr;

            msr << "/dev/cpu/" << cpu << "/msr";

            int fd = open(msr.str().c_str(), O_RDONLY);

            if (fd < 0) break;

            uint64_t IA32_THERM_STATUS {0};

            ssize_t rc = pread(fd,&IA32_THERM_STATUS, 8, 0x19C);

            if (8 != rc)
            {
                std::ostringstream o;

                o << "Failed trying to pread(fd = " << fd << ",&IA32_THERM_STATUS, 8, 0x19C) from \"" << msr.str() << "\".  return code was " << rc;

                if (rc <0) { o << " errno = " << errno << " - " << strerror(errno); }

                o << "." << std::endl;

                log(slavelogfile, o.str());

                return;
            }

            close(fd);

            cpu++;

            uint32_t x = (uint32_t) IA32_THERM_STATUS & 0x00000000FFFFFFFF;

            x = x >> 16;

            uint32_t digital_readout = x & 0x0000007F;

            x = x >> 15;

            uint32_t reading_valid = x & 0x00000001;

            if (reading_valid != 0 ) digital_readouts.push((double) digital_readout);
        }
    }

    if (digital_readouts.count() > 0)
    {
        if (digital_readouts.min() == 0.0)
        {
            if (warn_on_critical_temp)
            {
                std::cout << "<Warning> CPU temperature has hit the critical limit and CPU operation has been throttled.  Test data is invalid.  Check dmesg & /var/log/messages."
                    << "  You may need to run with a longer subinterval_seconds if the throttling causes ivy interlock protocol timeouts." << std::endl;
            }
            else
            {
                std::ostringstream o; o << "<Error> CPU temperature has hit the critical limit and CPU operation has been throttled.  Test aborting." << std::endl;
                throw std::runtime_error(o.str());
            }
        }
        else if (digital_readouts.min() <= 5.0)
        {
            std::cout << "<Warning> CPU temperature reached " << digital_readouts.min() << " degrees C of the critical limit.  Machine checks have been observed in this range.  Check dmesg & /var/log/messages."
                << "  You may need to run with a longer subinterval_seconds if CPU throttling causes ivy interlock protocol timeouts." << std::endl;
        }
    }

    return ;
}



void IvyDriver::post_error(const std::string& msg)
{
    std::string s;

    {
        std::ostringstream o;
        o << "<Error> internal programming error - "s << ivydriver.ivyscript_hostname
            << " - " << msg;
        s = o.str();
    }

    if (s[s.size()-1] != '\n') { s += "\n"; }

    log(slavelogfile,s);

    if (!reported_error) {time_error_reported.setToNow();}
    reported_error = true;

    say(s);

    return;
}


std::string IvyDriver::print_logfile_stats()
{
    RunningStat<long double, long> overall_durations {};

    std::ostringstream o;

    o << "log file time to write a log entry:" << std::endl;

    {
        std::unique_lock<std::mutex> m_lk(slavelogfile.now_speaks);

        const auto& rs = slavelogfile.durations;

        if (rs.count() > 0)
        {
            o << "count " << rs.count()
                << ", avg " << (1000.0 * rs.avg()) << " ms"
                << ", min " << (1000.0 * rs.min()) << " ms"
                << ", max " << (1000.0 * rs.max()) << " ms"
                << " - " << slavelogfile.logfilename << std::endl;

            overall_durations += slavelogfile.durations;
        }
    }

    for (auto& pear : all_workload_threads)
    {
        {
            logger& bunyan = pear.second->slavethreadlogfile;

            std::unique_lock<std::mutex> m_lk(bunyan.now_speaks);

            const auto& rs = bunyan.durations;

            if (rs.count() > 0)
            {
                o << "count " << rs.count()
                    << ", avg " << (1000.0 * rs.avg()) << " ms"
                    << ", min " << (1000.0 * rs.min()) << " ms"
                    << ", max " << (1000.0 * rs.max()) << " ms"
                    << " - " << bunyan.logfilename << std::endl;

                overall_durations += bunyan.durations;
            }
        }
    }

    {
        const auto& rs = overall_durations;

        o << "count " << rs.count()
            << ", avg " << (1000.0 * rs.avg()) << " ms"
            << ", min " << (1000.0 * rs.min()) << " ms"
            << ", max " << (1000.0 * rs.max()) << " ms"
            << " - " << "< Overall >" << std::endl;
    }

    return o.str();
}














