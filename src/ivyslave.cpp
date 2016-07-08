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
// ivyslave.cpp

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <malloc.h>
#include <random>
#include <scsi/sg.h>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <linux/aio_abi.h>
#include <semaphore.h>


using namespace std;

#include "ivytime.h"
#include "iogenerator_stuff.h"
#include "WorkloadID.h"
#include "ListOfWorkloadIDs.h"
#include "LUN.h"
#include "ivydefines.h"
#include "discover_luns.h"
#include "LDEVset.h"
#include "ivyhelpers.h"
#include "ivybuilddate.h"
#include "ivylinuxcpubusy.h"
#include "IogeneratorInput.h"
#include "Eyeo.h"
#include "Iogenerator.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "SubintervalOutput.h"
#include "Subinterval.h"
#include "WorkloadThread.h"

// variables

std::unordered_map<std::string, WorkloadThread*>
	iogenerator_threads;  // Key looks like "workloadName:host:00FF:/dev/sdxx:2A"

ivytime
	test_start_time, subinterval_duration, master_thread_subinterval_end_time, lasttime;

int
	next_to_harvest_subinterval, otherSubinterval;

struct procstatcounters
	interval_start_CPU, interval_end_CPU;

std::string
	slavelogfile;

bool routine_logging {false};

std::string printable_ascii;

// methods

bool waitForSubintervalEndThenHarvest();


// code

void invokeThread(WorkloadThread* T) {
	T->WorkloadThreadRun();
}




void killAllSubthreads(std::string logfilename) {
	for (auto& pear : iogenerator_threads) {
		if (ThreadState::died != pear.second->state && ThreadState::exited_normally != pear.second->state)
		{
			std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);
			pear.second->ivyslave_main_posted_command=true;
			pear.second->ivyslave_main_says=MainThreadCommand::die;
//*debug*/pear.second->debug_command_log("ivyslave.cpp killAllSubthreads");
		}
		pear.second->slaveThreadConditionVariable.notify_all();
	}
	int threads=0;
	for (auto& pear : iogenerator_threads) {
		pear.second->std_thread.join();
		threads++;
	}

	if (routine_logging)
	{
        std::ostringstream o;
        o << "killAllSubthreads() - commanded " << threads <<" iogenerator threads to die and harvested the remains.\n";
        log(logfilename, o.str());
	}
}

void say(std::string s, std::string logfilename,ivytime &lasttime) {
 	if (s.length()==0 || s[s.length()-1] != '\n') s.push_back('\n');
	if (routine_logging) log(logfilename,format_utterance("Slave", s));
	std::cout << s << std::flush;
	return;
}

void initialize_io_time_clip_levels(); // Accumulators_by_io_type.cpp

int main(int argc, char* argv[])
{
//*debug*/{std::ostringstream o; o << "fireup - argc = " << argc; for (int i=0;i<argc; i++){o << " argv[" << i << "]=\"" << argv[i] << "\"  ";} o << std::endl; fileappend("/home/ivogelesang/Desktop/eraseme.txt",o.str());}

	std::mutex ivyslave_main_mutex;

	initialize_io_time_clip_levels();

	std::string hostname;

    for (int arg_index = 1 /*skipping executable name*/ ; arg_index < argc ; arg_index++ )
    {
        std::string item {argv[arg_index]};

        if (item == "-log") { routine_logging = true; continue;}

        if (arg_index != (argc-1))
        {
            std::cout << argv[0] << " - usage: " << argv[0] << " options <ivyscript_hostname>" << std::endl
                << " where \"options\" means zero or more of: -log" << std::endl
                << "and where ivyscript_hostname is either an identifier, a hostname or alias, or is an IPV4 dotted quad." << std::endl;
                return -1;
        }
        hostname = item;
    }

	struct stat struct_stat;

    if( stat(IVYSLAVELOGFOLDERROOT,&struct_stat) || (!S_ISDIR(struct_stat.st_mode)) )
    {
        //  folder doesn't already exist or it's not a directory
        std::cout << "<Error> " << "ivyslave log folder root \"" << IVYSLAVELOGFOLDERROOT << "\" doesn\'t exist or is not a folder." << std::endl;
        return -1;
    }

    // In the next bit where we create the subfolder of IVYSLAVELOGFOLDERROOT, we use a semaphore
    // because if you are testing with two aliases for the same test host, only one ivyslave main thread
    // should be creating the subfolder.

    sem_t* p_semaphore = sem_open("/ivyslave_log_subfolder_create", O_CREAT,S_IRWXG|S_IRWXO|S_IRWXU,0);

    if (SEM_FAILED == p_semaphore)
    {
        std::ostringstream o;
        o << "<Error> Failed trying to open semaphore to create ivyslave log subfolder.  errno = " << errno
            << " - " << std::strerror(errno) << " at " << __FILE__  << " line " << __LINE__ << "." << std::endl;
        std::cout << o.str();
        return -1;
    }
    else
    {
//        if (0 != sem_wait(p_semaphore))
//        {
//            std::ostringstream o;
//            o << "<Error> Failed trying to lock semaphore to create ivyslave log subfolder.  errno = " << errno
//                << " - " << std::strerror(errno) << " at " << __FILE__  << " line " << __LINE__ << "." << std::endl;
//            std::cout << o.str();
//            return -1;
//        }

        if( stat(IVYSLAVELOGFOLDERROOT IVYSLAVELOGFOLDER ,&struct_stat) )
        {
            //  folder doesn't already exist
            int rc;
            if ((rc = mkdir(IVYSLAVELOGFOLDERROOT IVYSLAVELOGFOLDER, S_IRWXU | S_IRWXG | S_IRWXO)))
            {
                std::ostringstream o;
                o << "<Error> Failed trying to create ivyslave log folder \"" << IVYSLAVELOGFOLDERROOT IVYSLAVELOGFOLDER << "\" - "
                    << "mkdir() return code " << rc << ", errno = " << errno << " " << std::strerror(errno) << std::endl;
                std::cout << o.str();
                return -1;
            }
        }

//        if (0 != sem_post(p_semaphore))
//        {
//            std::ostringstream o;
//            o << "<Error> Failed trying to post semaphore after creating ivyslave log subfolder.  errno = " << errno
//                << " - " << std::strerror(errno) << " at " << __FILE__  << " line " << __LINE__ << "." << std::endl;
//            std::cout << o.str();
//            return -1;
//        }
        sem_close(p_semaphore);
    }

    if( stat(IVYSLAVELOGFOLDERROOT IVYSLAVELOGFOLDER,&struct_stat) || (!S_ISDIR(struct_stat.st_mode)) )
    {
            //  folder doesn't already exist or it's not a directory
        std::cout << "<Error> " << "ivyslave log folder \"" << IVYSLAVELOGFOLDERROOT IVYSLAVELOGFOLDER << "\" doesn\'t exist or is not a folder." << std::endl;
        return -1;
    }

	std::string erase_earlier_log_files( std::string("rm -f ") + std::string(IVYSLAVELOGFOLDERROOT IVYSLAVELOGFOLDER) + std::string("/log.ivyslave.") + hostname + std::string("*") );
	system(erase_earlier_log_files.c_str());

        slavelogfile = std::string(IVYSLAVELOGFOLDERROOT IVYSLAVELOGFOLDER) + std::string("/log.ivyslave.") + hostname + std::string(".txt");

        if (!routine_logging) log(slavelogfile,"For logging of routine (non-error) events, use the ivy -log command line option, like \"ivy -log a.ivyscript\".\n\n");

	//int maxTags;
	std::vector<std::string> luns;

	discovered_LUNs disco;
	disco.discover();
	std::string header_line{""};
	std::string disco_line{""};

	std::map<std::string, LUN*> LUNpointers;
		// The map is from "/dev/sdxxx" to LUN*
		// We don't know what our ivyscript hostname is, as that gets glued on at the master end.
		// But the WorkloadID for each workload thread is named ivyscript_hostname+/dev/sdxxx+workload_name, so you can see the ivyscript_hostname there if you need it.

	std::string bracketedExit{"[Exit]"};
	std::string bracketedCreateWorkload{"[CreateWorkload]"};
	std::string bracketedDeleteWorkload{"[DeleteWorkload]"};
	std::string remainder;
	std::string subinterval_duration_as_string;

	lasttime.setToNow();
	say(std::string("Hello, whirrled! from ")+myhostname(),slavelogfile,lasttime);


    for (unsigned int c = 0; c < 128; c++) if (isprint((char)c)) printable_ascii.push_back((char)c);

    if (routine_logging) {std::ostringstream o; o << "printable_ascii = \"" << printable_ascii << "\"" << std::endl; log(slavelogfile,o.str());}


	std::string input_line;
	if(std::cin.eof()) {log("std::cin.eof()\n",slavelogfile); return 0;}
	while(!std::cin.eof()) {
		// get commands from ivymaster
		std::getline(std::cin,input_line);
		lasttime.setToNow();

		if (routine_logging)
		{	std::ostringstream o;
			o << format_utterance("Host ",input_line);
			log(slavelogfile,o.str());
		}
		trim(input_line);
		if (stringCaseInsensitiveEquality(input_line, std::string("[Die, Earthling!]")))
		{
			say(std::string("[what?]"),slavelogfile,lasttime);
			// When ivymaster subthread encounters an error and is terminating abnormally
			killAllSubthreads(slavelogfile);
			return -1;
		}
		if (stringCaseInsensitiveEquality(input_line, std::string("send LUN header")))
		{
			disco.get_header_line(header_line);
			say("[LUNheader]ivyscript_hostname,"+header_line,slavelogfile,lasttime);
		}
		else if (stringCaseInsensitiveEquality(input_line,std::string("send LUN")))
		{
			if (disco.get_next_line(disco_line))
			{
				LUN* pLUN = new LUN();
				if (!pLUN->loadcsvline(header_line,disco_line,slavelogfile))
				{
					std::ostringstream o;
					o << "<Error> Ivyslave main thread failed trying to load a LUN lister csv file line into a LUN object.  csv header line = \""
					<< header_line << "\", data line = \"" << disco_line << "\"." << std::endl ;
					log(slavelogfile,o.str());
					say (o.str(),slavelogfile,lasttime);
					killAllSubthreads(slavelogfile);
					return -1;
				}
				LUNpointers[pLUN->attribute_value(std::string("LUN name"))]=pLUN;
				say(std::string("[LUN]")+hostname+std::string(",")+disco_line,slavelogfile,lasttime);
			}
			else
			{
				say("[LUN]<eof>",slavelogfile,lasttime);
			}
		}
		else if (0 == std::string("Stop!").compare(input_line))
		{
			// This "Stop!" (not "stop") means wait for all threads to transition (possibly from "stopping") to "waiting for command" state

			for (auto& pear : iogenerator_threads)
			{
				{
					std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);
					while (ThreadState::stopping == pear.second->state) pear.second->slaveThreadConditionVariable.wait(u_lk);
				}
			}

			say("<OK>",slavelogfile,lasttime);
		}




		else if (startsWith("[CreateWorkload]",input_line,remainder))
		{
			// We get "[CreateWorkload] myhostname+/dev/sdxy+workload_name [Parameters] subinterval_input.toString()

			// The myhostname+/dev/sdxy+workload_name part is a "WorkloadID"

			std::string workloadID, createThreadParameters;

			trim(remainder);
            unsigned int i=0;
			while (i < remainder.length() && remainder[i]!='[')
				i++;
			if (0==i || (!startsWith("[Parameters]",remainder.substr(i,remainder.length()-i),createThreadParameters)))
			{
				say("<Error> [CreateWorkload] did not have \"[Parameters]\"",slavelogfile,lasttime);
				killAllSubthreads(slavelogfile);
				return -1;
			}
			workloadID=remainder.substr(0,i);
			trim(workloadID);
			trim(createThreadParameters);

			// we get "[CreateWorkload] sun159+/dev/sdxx+ [Parameters]subinterval_input.toString()

/*debug*/if (routine_logging) log(slavelogfile,std::string("[CreateWorkload] WorkloadID is ")+workloadID+std::string("\n"));

			std::unordered_map<std::string, WorkloadThread*>::iterator trd=iogenerator_threads.find(workloadID);
			if (iogenerator_threads.end()!=trd) {
				std::ostringstream o;
				o << "<Error> [CreateWorkload] failed - thread \"" << workloadID << "\" already exists! Not supposed to happen.  ivymaster is supposed to know." << std::endl;
				say(o.str(),slavelogfile,lasttime);
				killAllSubthreads(slavelogfile);
				return -1;
			}

			WorkloadID wID(workloadID);
			if (!wID.isWellFormed)
			{
				std::ostringstream o;
				o << "<Error> [CreateWorkload] - workload ID \"" << workloadID << "\" did not look like hostname+/dev/sdxxx+workload_name." << std::endl;
				say(o.str(),slavelogfile,lasttime);
				killAllSubthreads(slavelogfile);
				return -1;
			}

			std::map<std::string, LUN*>::iterator nabIt = LUNpointers.find(wID.getLunPart());

			if (LUNpointers.end() == nabIt)
			{
				std::ostringstream o;
				o << "<Error> [CreateWorkload] - workload ID \"" << workloadID << "\"  - no such LUN \"" << wID.getLunPart() << "\"." << std::endl;
				say(o.str(),slavelogfile,lasttime);
				killAllSubthreads(slavelogfile);
				return -1;
			}

			LUN* pLUN = (*nabIt).second;

			//Now we validate & parse the maxLBA
			if (!pLUN->contains_attribute_name(std::string("Max LBA")))
			{
				std::ostringstream o;
				o << "<Error> [CreateWorkload] - workload ID \"" << workloadID <<  "\" - LUN \"" << wID.getLunPart() << "\" - does not have a \"Max LBA\" attribute." << std::endl;
				say(o.str(),slavelogfile,lasttime);
				killAllSubthreads(slavelogfile);
				return -1;
			}
			std::string maxLBAtext = pLUN->attribute_value(std::string("Max LBA"));
			trim(maxLBAtext);
			if (!isalldigits(maxLBAtext))
			{
				std::ostringstream o;
				o << "<Error> [CreateWorkload] - workload ID \"" << workloadID <<  "\" - LUN \"" << wID.getLunPart()
					<< "\" - \"Max LBA\" attribute value \"" << maxLBAtext << "\" is not all digits." << std::endl;
				say(o.str(),slavelogfile,lasttime);
				killAllSubthreads(slavelogfile);
				return -1;

			}
			long long int maxLBA;
			{
				std::istringstream is(maxLBAtext);
				is >> maxLBA;
			}

			WorkloadThread* p_WorkloadThread = new WorkloadThread(workloadID,(*nabIt).second, maxLBA, createThreadParameters,&ivyslave_main_mutex);

			iogenerator_threads[workloadID]=p_WorkloadThread;

	        	p_WorkloadThread->slavethreadlogfile = std::string(IVYSLAVELOGFOLDERROOT IVYSLAVELOGFOLDER) + std::string("/log.ivyslave.")
				+ convert_non_alphameric_or_hyphen_or_period_to_underscore(workloadID) + std::string(".txt");

			// we still need to set the iogenerator parameters in both subintervals
			p_WorkloadThread->subinterval_array[0].input.fromString(createThreadParameters,slavelogfile);
			p_WorkloadThread->subinterval_array[1].input.fromString(createThreadParameters,slavelogfile);

			// Invoke thread
			p_WorkloadThread->std_thread=std::thread(invokeThread,p_WorkloadThread);

			say("<OK>",slavelogfile,lasttime);

		}

		else if (startsWith("[DeleteWorkload]",input_line,remainder))
		{
			// We get "[DeleteWorkload]myhostname+/dev/sdxy+workload_name"

			// The myhostname+/dev/sdxy+workload_name part is a "WorkloadID"

			WorkloadID wid;

			trim(remainder);

			if (!wid.set(remainder))
			{
				std::ostringstream o;
				o << "<Error> at ivyslave [DeleteWorkload] failed - WorkloadID \"" << remainder << "\" is not well-formed like \"myhostname+/dev/sdxy+workload_name\"." << std::endl;
				say(o.str(),slavelogfile,lasttime);
				killAllSubthreads(slavelogfile);
				return -1;
			}

            std::unordered_map<std::string, WorkloadThread*>::iterator
                t_it = iogenerator_threads.find(wid.workloadID);

			if (iogenerator_threads.end() == t_it)
			{
				std::ostringstream o;
				o << "<Error> at ivyslave [DeleteWorkload] failed - no such WorkloadID \"" << remainder << "\"." << std::endl;
				say(o.str(),slavelogfile,lasttime);
				killAllSubthreads(slavelogfile);
				return -1;
			}

			WorkloadThread* p_WorkloadThread = t_it->second;

			if (p_WorkloadThread == nullptr)
			{
				std::ostringstream o;
				o << "<Error> at ivyslave [DeleteWorkload] failed - internal programming error WorkloadID \"" << remainder << "\" key exists but the associated WorkloadTracker pointer was nullptr." << std::endl;
				say(o.str(),slavelogfile,lasttime);
				killAllSubthreads(slavelogfile);
				return -1;
			}

            if (ThreadState::died != p_WorkloadThread->state && ThreadState::exited_normally != p_WorkloadThread->state)
            {
                std::unique_lock<std::mutex> u_lk(p_WorkloadThread->slaveThreadMutex);

                p_WorkloadThread->ivyslave_main_posted_command=true;
                p_WorkloadThread->ivyslave_main_says=MainThreadCommand::die;
            }

            p_WorkloadThread->slaveThreadConditionVariable.notify_all();


                //==================================
                //==================================
                        // need to come back and do the join() with a timeout, but without slowing down the join(), since we are deleting the workloads one by one.
                //==================================
                //==================================

            p_WorkloadThread->std_thread.join();

            if (routine_logging)
            {
                std::ostringstream o;
                o << "[DeleteWorkload] workload thread died upon command - " << wid.workloadID << std::endl;
                log(slavelogfile, o.str());
            }

            iogenerator_threads.erase(t_it);

			say("<OK>",slavelogfile,lasttime);
		}

		else if (startsWith("[EditWorkload]",input_line,remainder))
		{
			// We get "[EditWorkload] <myhostname+/dev/sdxy+workload_name, myhostname+/dev/sdxz+workload_name> [Parameters] IOPS = *1.25

			// The <myhostname+/dev/sdxy+workload_name, myhostname+/dev/sdxz+workload_name> part is the toString()/fromString() format for ListOfWorkloadIDs.

			// The [parameters] field has already been validated by having successfully been applied to the up to date synchronized copy
			// of our "next subinterval" iogenerator_input object.  ivymaster keeps this copy in the WorkloadTracker object.

			// So as long as the ivy engine itself isn't broken, the workload IDs are for valid existing workload threads, and we already
			// have shown that the parameters apply cleanly to the current iogenerator_input object.

/*debug*/if (routine_logging) log( slavelogfile,std::string("[EditWorkload]") + remainder + std::string("\n") );

			std::string listOfWorkloadIDsText, parametersText;

			trim(remainder);
			unsigned int i=0;
			while (i < remainder.length() && remainder[i]!='[')
				i++;
			if (0==i || (!startsWith("[Parameters]",remainder.substr(i,remainder.length()-i),parametersText)))
			{
				say("<Error> [EditWorkload] did not have \"[Parameters]\"",slavelogfile,lasttime);
				killAllSubthreads(slavelogfile);
				return -1;
			}
			listOfWorkloadIDsText=remainder.substr(0,i);
			trim(listOfWorkloadIDsText);
			trim(parametersText);

			// we get "[CreateWorkload] sun159+/dev/sdxx+ [Parameters]subinterval_input.toString()

/*debug*/if(routine_logging) log(slavelogfile,std::string("[EditWorkload] WorkloadIDs are ")+listOfWorkloadIDsText+std::string(" and parametersText=\"")+parametersText+std::string("\".\n"));

			ListOfWorkloadIDs lowi;

			if (!lowi.fromString(&listOfWorkloadIDsText))
			{
				say(std::string("<Error> [EditWorkload] invalid list of WorkloadIDs, should look like <myhostname+/dev/sdxy+workload_name,myhostname+/dev/sdxz+workload_name> - \"")
					+ listOfWorkloadIDsText + std::string("\"."),slavelogfile,lasttime);
				killAllSubthreads(slavelogfile);
				return -1;
			}

			for (auto& wID : lowi.workloadIDs)
			{
				//std::unordered_map<std::string, WorkloadThread*>  iogenerator_threads;  // Key looks like "workloadName:host:00FF:/dev/sdxx:2A"

				auto it = iogenerator_threads.find(wID.workloadID);

				if (iogenerator_threads.end() == it)
				{
					say(std::string("<Error> [EditWorkload] no such WorkloadID, should look like myhostname+/dev/sdxy+workload_name - \"")
						+ wID.workloadID + std::string("\"."),slavelogfile,lasttime);
					killAllSubthreads(slavelogfile);
					return -1;
				}

				WorkloadThread* p_WorkloadThread = (*it).second;


				// If the workload thread is running, we update the next subinterval.
				// If it is stopped, we update both subintervals.

				{
					// if we can't get the lock without waiting, we abort - we should only be doing this at a time when it's not locked.

					if (!p_WorkloadThread->slaveThreadMutex.try_lock())
					{
						say(std::string("<Error> [EditWorkload] subthread was already locked - aborting - \"")
							+ wID.workloadID + std::string("\"."),slavelogfile,lasttime);
						killAllSubthreads(slavelogfile);
						return -1;
					}
					else
					{
						// we have the lock

						// Note that there is no interlock between threads nor any posting of events
						// since this just updates the iogenerator_input parameters for a subinterval that is waiting to be run.

						if ( ThreadState::running  == p_WorkloadThread->state || ThreadState::waiting_for_command == p_WorkloadThread->state )
						{
///*debug*/{std::ostringstream o;  o << std::endl <<  "Workload thread \"" << p_WorkloadThread->workloadID.workloadID << "\" is stopped, setting into both, parameters = \"" << parametersText<< "\".";   fileappend(slavelogfile, o.str());}
							std::string error_message;

							if (!p_WorkloadThread->subinterval_array[0].input.setMultipleParameters(error_message, parametersText))
							{
								std::ostringstream o;
								o << "<Error> Internal programming error since we already successfully set these parameters into the corresponding WorkloadTracker objects in ivymaster - failed setting parameters \""
								  << parametersText << "\" into subinterval_array[0].input iogenerator_input object for WorkloadID \""
								  << wID.workloadID << "\" - "
								  << error_message;
								say(o.str(),slavelogfile,lasttime);
								killAllSubthreads(slavelogfile);
								return -1;
							}
							if (!p_WorkloadThread->subinterval_array[1].input.setMultipleParameters(error_message, parametersText))
							{
								std::ostringstream o;
								o << "<Error> Internal programming error since we already successfully set these parameters into the corresponding WorkloadTracker objects in ivymaster - failed setting parameters \""
								  << parametersText << "\" into subinterval_array[1].input iogenerator_input object for WorkloadID \""
								  << wID.workloadID << "\" - "
								  << error_message;
								say(o.str(),slavelogfile,lasttime);
								killAllSubthreads(slavelogfile);
								return -1;
							}
						}
						else
						{
							std::ostringstream o;
							o << "<Error> [EditWorkload] thread " << wID.workloadID << " was not STOPPED or RUNNING, but rather was " << threadStateToString(p_WorkloadThread->state) << " aborting." << std::endl;
							say(o.str(),slavelogfile,lasttime);
							killAllSubthreads(slavelogfile);
							return -1;
						}

						// we release the lock

						p_WorkloadThread->slaveThreadMutex.unlock();
						p_WorkloadThread->slaveThreadConditionVariable.notify_all();
					}
				}
			}
			say("<OK>",slavelogfile,lasttime);
		}

		else  if (startsWith(std::string("Go!"),input_line,subinterval_duration_as_string))
		{
			test_start_time.setToNow();
			next_to_harvest_subinterval=0; otherSubinterval=1;
			rtrim(subinterval_duration_as_string);  // can't remember if it's necessary, but won't hurt
			if (!subinterval_duration.fromString(subinterval_duration_as_string))
			{
				ostringstream o;
				o << "<Error> received command \"" << rtrim(input_line) << "\", saw \"Go!\" with a subinterval duration ivytime string representation of \""
					<< subinterval_duration_as_string << "\", but this failed to convert to an ivytime when calling \"fromString()\".  "
					<< "Valid ivytime string representations look like \"<seconds,nanoseconds>\"." << std::endl;
				say(o.str(),slavelogfile,lasttime);
				killAllSubthreads(slavelogfile);
				return -1;
			}
			master_thread_subinterval_end_time = test_start_time + subinterval_duration;


			// harvest CPU counters starting time first subinterval

			if (0!=getprocstat(&interval_start_CPU,slavelogfile))
			{
				say("<Error> ivyslave main thread: procstatcounters::read_CPU_counters() call to get first subinterval starting CPU counters failed.",
					slavelogfile,lasttime);
			}
			for (auto& pear : iogenerator_threads)
			{
				{
					std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);
					if (ThreadState::waiting_for_command != pear.second->state)
					{
						ostringstream o;
						o << "<Error> received \"Go!\" command, but thread \"" << pear.first << "\" was in " << threadStateToString(pear.second->state) << " state, not in \"waiting_for_command\" state.  Aborting." << std::endl;
						say(o.str(),slavelogfile,lasttime);
						killAllSubthreads(slavelogfile);
						return -1;
					}

					// At this point, both subinterval input objects already have their iogenerator_input objects set.
						// Either this happened from a [CreateWorkload] or when a previous run stopped, the settings
						// from the last subinterval to run were copied to the other subinterval, or finally
						// both were set by a "[ModifyWorkload]" command that ran while the iogenerator thread was stopped.
//*debug*/{ostringstream o; o << "Posting IVYSLAVE_SAYS_RUN command for first subinterval for " << pear.first << std::endl; log(slavelogfile,o.str());}


// NOTE: Originally I thought I would be switching back and forth between two IogeneratorInput objects, but now they are just clones of each other.
//       I was being overly careful but by accident discovered I was writing updates into the "active" subinterval IogeneratorInput object instead of the inactive one.
//       Since the running workload thread only reads from the IogeneratorInput object there were no race conditions / bad things happening.
//       Eventually should cut back to only having  one IogeneratorInput object ... maybe, unless there are pairs of input parameters that must both take effect simultaneously ...



                    if (pear.second->subinterval_array[0].input.dedupe > 1.0 && pear.second->subinterval_array[0].input.fractionRead == 1.0)
					{
						ostringstream o;
						o << "<Error> Internal programming error - ivy master failed earlier to detect dedupe > 1.0 with fractionRead = 100%.  Aborting." << std::endl;
						say(o.str(),slavelogfile,lasttime);
						killAllSubthreads(slavelogfile);
						return -1;
					}

                    if (pear.second->subinterval_array[0].input.fractionRead < 1.0)
                    {
                        pear.second->have_writes = true;
                        pear.second->pat = pear.second->subinterval_array[0].input.pat;
                        pear.second->compressibility = pear.second->subinterval_array[0].input.compressibility;
                    }
                    else
                    {
                        pear.second->have_writes = false;
                    }

                    if (pear.second->subinterval_array[0].input.dedupe == 1.0)
                    {
                        pear.second->doing_dedupe=false;
                        pear.second->block_seed = ( (uint64_t) std::hash<std::string>{}(pear.first) ) ^ test_start_time.getAsNanoseconds();
                    }
                    else
                    {
                        pear.second->doing_dedupe=true;
                        pear.second->threads_in_workload_name = (pattern_float_type) pear.second->subinterval_array[0].input.threads_in_workload_name;
                        pear.second->serpentine_number = 1.0 + ( (pattern_float_type) pear.second->subinterval_array[0].input.this_thread_in_workload );
                        pear.second->serpentine_number -= pear.second->threads_in_workload_name; // this is because we increment the serpentine number by threads_in_workload before using it.
                        pear.second->serpentine_multiplier =
                            ( 1.0 - ( (pattern_float_type)  pear.second->subinterval_array[0].input.fractionRead ) )
                            /       ( (pattern_float_type)  pear.second->subinterval_array[0].input.dedupe );

                        pear.second->pattern_seed = pear.second->subinterval_array[0].input.pattern_seed;
                        pear.second->pattern_number = 0;
                    }
                    pear.second->write_io_count = 0;

					pear.second->subinterval_array[0].start_time=test_start_time;
					pear.second->subinterval_array[0].end_time=test_start_time + subinterval_duration;
					pear.second->subinterval_array[1].start_time=pear.second->subinterval_array[0].end_time;
					pear.second->subinterval_array[1].end_time=pear.second->subinterval_array[1].start_time + subinterval_duration;
					pear.second->subinterval_array[0].output.clear();  // later if energetic figure out if these must already be cleared.
					pear.second->subinterval_array[1].output.clear();
					pear.second->subinterval_array[0].subinterval_status=IVY_SUBINTERVAL_READY_TO_RUN;
					pear.second->subinterval_array[1].subinterval_status=IVY_SUBINTERVAL_READY_TO_RUN;
					pear.second->ivyslave_main_posted_command=true;
					pear.second->ivyslave_main_says=MainThreadCommand::run;
//*debug*/pear.second->debug_command_log("ivyslave.cpp posting first subinterval MainThreadCommand::run.");
                    pear.second->cooldown = false;

				}
				pear.second->slaveThreadConditionVariable.notify_all();
			}

			// Although we have prepared two subintervals for the iogenerator thread to use,
			// when the iogenerator threads get to the end of the first subinterval, they are going to
			// expect us to have posted another MainThreadCommand::run command.

			for (auto& pear : iogenerator_threads)
			{
				{
					// Wait for the iogenerator thread to have consumed the first "IVYSLAVEMAIN_SAYS_RUN" command
					// then post another one.
					std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);
					while (true == pear.second->ivyslave_main_posted_command)
						pear.second->slaveThreadConditionVariable.wait(u_lk);
//*debug*/{ostringstream o; o << "Posting IVYSLAVE_SAYS_RUN command for second subinterval for " << pear.first  << std::endl; log(slavelogfile,o.str());}
					pear.second->ivyslave_main_posted_command=true;
					pear.second->ivyslave_main_says=MainThreadCommand::run;
//*debug*/pear.second->debug_command_log("ivyslave.cpp posting second subinterval MainThreadCommand::run");
				}
				pear.second->slaveThreadConditionVariable.notify_all();
			}

			if (!waitForSubintervalEndThenHarvest()) return -1;  // ivymaster can't tell us anything until after digesting the results of the first subinterval
		}
		else if ( 0 == input_line.compare(std::string("continue")) || 0 == input_line.compare(std::string("cooldown")) )
		{
//*debug*/{ostringstream o; o << "Got \"continue\" or  \"cooldown\". " << std::endl; log(slavelogfile,o.str());}
            bool cooldown_flag;
            if (0 == input_line.compare(std::string("cooldown"))) {cooldown_flag=true;}
            else                                                  {cooldown_flag=false;}
			for (auto& pear : iogenerator_threads)
			{
				{ // lock
					std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);
//*debug*/{ostringstream o; o << "Got the lock for " << pear.first << std::endl; log(slavelogfile,o.str());}
					if (ThreadState::running != pear.second->state)
					{
						ostringstream o;
						o << "<Error> received \"continue\" or \"cooldown\" command, but thread \"" << pear.first << "\" was in " << threadStateToString(pear.second->state) << " state, not in \"running\" state.  Aborting." << std::endl;
						say(o.str(),slavelogfile,lasttime);
						killAllSubthreads(slavelogfile);
						return -1;
					}

					// At this point, ivy master has made any modifications to the input parameters for the next subinterval
					// and the "other" subsystem has been harvested and we need to turn it around to get ready to run

					pear.second->ivyslave_main_posted_command=true;
					pear.second->ivyslave_main_says=MainThreadCommand::run;
					pear.second->cooldown = cooldown_flag;
//*debug*/pear.second->debug_command_log("ivyslave.cpp posting MainThreadCommand::run");
//*debug*/{ostringstream o; o << "Posted MainThreadCommand::run command to " << pear.first << std::endl; log(slavelogfile,o.str());}
				}
				pear.second->slaveThreadConditionVariable.notify_all();
//*debug*/{ostringstream o; o << "Dropped the lock for " << pear.first << std::endl; log(slavelogfile,o.str());}
			}
//*debug*/{ostringstream o; o << "About to waitForSubintervalEndThenHarvest() " << std::endl; log(slavelogfile,o.str());}

			if (!waitForSubintervalEndThenHarvest()) return -1;
//*debug*/log(slavelogfile,"Looping back to get next command from ivymaster.");
		}
		else if (0==input_line.compare(std::string("stop")))
		{
			for (auto& pear : iogenerator_threads)
			{
				{ // lock
					std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);
					if (ThreadState::running != pear.second->state)
					{
						ostringstream o;
						o << "<Error> received \"stop\" command, but thread \"" << pear.first << "\" was in " << threadStateToString(pear.second->state) << " state, not in \"running\" state.  Aborting." << std::endl;
						say(o.str(),slavelogfile,lasttime);
						killAllSubthreads(slavelogfile);
						return -1;
					}

					// at this point, what we do is copy the input parameters from the running subinterval
					// to the other one, to get it ready for the next test run as if the thread had just been created

					pear.second->ivyslave_main_posted_command=true;
					pear.second->ivyslave_main_says=MainThreadCommand::stop;
//*debug*/pear.second->debug_command_log("ivyslave.cpp posting MainThreadCommand::stop");

				}
				pear.second->slaveThreadConditionVariable.notify_all();
			}
			if (!waitForSubintervalEndThenHarvest()) return -1;

		} else if ((input_line.length()>=bracketedExit.length())
			&& stringCaseInsensitiveEquality(input_line.substr(0,bracketedExit.length()),bracketedExit)) {
			killAllSubthreads(slavelogfile);
			return 0;
		} else {
			log(slavelogfile,std::string("ivyslave received command from ivymaster \"") + input_line + std::string("\" that was not recognized.  Aborting.\n"));
			killAllSubthreads(slavelogfile);
			return 0;
		}
	}
	killAllSubthreads(slavelogfile);
	return 0;
};


bool waitForSubintervalEndThenHarvest()
{
//*debug*/log(slavelogfile,"Entered waitForSubintervalEndThenHarvest().\n");
	// wait for subinterval ending time

//*debug*/{std::ostringstream o; ivytime now; now.setToNow(); ivytime dur = master_thread_subinterval_end_time - now;
//*debug*/	o << "waitForSubintervalEndThenHarvest() Going to wait from now " << now.format_as_datetime_with_ns() << " until subinterval_ending_time " << master_thread_subinterval_end_time.format_as_datetime_with_ns() 	<< " duration " << dur.format_as_duration_HMMSSns() << std::endl; log(slavelogfile, o.str());}

	try {

		master_thread_subinterval_end_time.waitUntilThisTime();

	}
	catch (std::exception& e)
	{
		std::ostringstream o;
		o << "waitForSubintervalEndThenHarvest() - exception during waitUntilThisTime() - " << e.what() << std::endl;
		log(slavelogfile, o.str());
	}


//*debug*/log(slavelogfile, std::string("Have waited until subinterval_ending_time ")+master_thread_subinterval_end_time.format_as_datetime_with_ns());
	// harvest CPU counters and send CPU line.
//*debug*/log(slavelogfile,std::string("slavelogfile=")+slavelogfile);
	int rc;
	rc=getprocstat(&interval_end_CPU, slavelogfile);
//*debug*/{ostringstream o; o << "waitForSubintervalEndThenHarvest() - getprocstat(&interval_end_CPU, slavelogfile) returned " << rc; log(slavelogfile,o.str());}
	if (0!=rc)
	{
		say(std::string("<Error> ivyslave main thread routine waitForSubintervalEndThenHarvest(): getprocstat(&interval_end_CPU, slavelogfile) call to get subinterval ending CPU counters failed.\n"),slavelogfile,lasttime);
		killAllSubthreads(slavelogfile);
		return false;
	}

	struct cpubusypercent cpubusydetail;
	struct avgcpubusypercent cpubusysummary;
	if (0!=computecpubusy(
		&interval_start_CPU, // counters at start of interval
		&interval_end_CPU,
		&cpubusydetail, // this gets filled in as output
		&cpubusysummary, // this gets filled in as output
		slavelogfile
	))
	{
		say(std::string("<Error> ivyslave main thread routine waitForSubintervalEndThenHarvest(): computecpubusy() call to get subinterval ending CPU % busies failed."),slavelogfile,lasttime);
		killAllSubthreads(slavelogfile);
		return false;
 	}


	say(std::string("[CPU]")+cpubusysummary.toString(),slavelogfile,lasttime);
//*debug*/log(slavelogfile,"said [CPU] line.\n");
	interval_start_CPU.copyFrom(interval_end_CPU,std::string("turning over for next subinterval in waitForSubintervalEndThenHarvest()"),slavelogfile);

	// At the end of the subinterval, if the iogenerator thread has to wait to get the lock, it aborts,
	// as it operates in real time driving and harvesting I/Os and cannot afford to wait.

	// So before we get the lock to see if the iogenerator thread has posted the subinterval as complete,
	// we are going to do is wait for a second just to make sure we dont try to get the lock
	// while the iogenerator threads might still be switching over to the next subinterval.

	// If the iogenerator thread hasn't posted the last subinterval as complete by then, we abort.

	ivytime afterCatnap = master_thread_subinterval_end_time + ivytime(catnapTimeSeconds);
	afterCatnap.waitUntilThisTime();

//*debug*/log(slavelogfile,"after catnap.\n");

	// Now we go through the iogenerator threads, harvesting the input & output objects and sending them
	// to ivymaster's host driver thread.  We send the key for each iogenerator thread to make it easy
	// to do the rollups in ivymaster, as the key has the workload name (from the [CreateWorkload] statement),
	// the LDEV name, the LUN name, and the port name. (The iogenerator type name is in the subinterval input object.)

	for (auto& pear : iogenerator_threads)
	{
//*debug*/log(slavelogfile,std::string("About to get the lock to wait for the iogenerator thread to post result of a subinterval")+pear.first+std::string("\n"));
		{
//*debug*/log(slavelogfile,std::string("Got the lock to process result of a subinterval for \"")+pear.first+std::string("\".\n"));

			std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);

			if ((pear.second->subinterval_array)[next_to_harvest_subinterval].subinterval_status != IVY_SUBINTERVAL_READY_TO_SEND)
			{
				ostringstream o;
				o << "<Error> ivyslave main thread - iogenerator thread \"" << pear.first
					<< "\" failed to post subinterval status = IVY_SUBINTERVAL_READY_TO_SEND within " << catnapTimeSeconds << " seconds of subinterval ending time for subinterval index = " << next_to_harvest_subinterval
					 << ".  Status actually was ";
                switch ((pear.second->subinterval_array)[next_to_harvest_subinterval].subinterval_status)
                {
                    case IVY_SUBINTERVAL_STATUS_UNDEFINED:
                        o << "IVY_SUBINTERVAL_STATUS_UNDEFINED (0)";
                        break;
                    case IVY_SUBINTERVAL_READY_TO_RUN:
                        o << "IVY_SUBINTERVAL_READY_TO_RUN (1)";
                        break;
                    case IVY_SUBINTERVAL_READY_TO_SEND:
                        o << "IVY_SUBINTERVAL_READY_TO_SEND (2)";
                        break;
                    case IVY_SUBINTERVAL_STOP:
                        o << "IVY_SUBINTERVAL_STOP (3)";
                        break;
                    default:
                        o << (pear.second->subinterval_array)[next_to_harvest_subinterval].subinterval_status;
                }
                o << std::endl;
				say(o.str(),slavelogfile,lasttime);
				killAllSubthreads(slavelogfile);
				return false;
			}


//*debug*/log(slavelogfile,std::string("It\'s posted ready to send.")+pear.first+std::string("\n"));
			// Send iogenerator subinterval detail line to ivymaster
			//	- WorkloadID
			//	- Subinterval input object toString()
			//	- Subinterval output object toString()
			lasttime.setToNow();
			ostringstream o;
			o << '<' << pear.second->workloadID.workloadID << '>'
				<< (pear.second->subinterval_array)[next_to_harvest_subinterval].input.toString()
				<< (pear.second->subinterval_array)[next_to_harvest_subinterval].output.toString() << std::endl;
			say(o.str(),slavelogfile,lasttime);

			// Copy subinterval object from running subinterval to inactive subinterval.
			(pear.second->subinterval_array)[next_to_harvest_subinterval].input.copy(  (pear.second->subinterval_array)[otherSubinterval].input);
//*debug*/{ostringstream o; o << pear.first << " just harvested [next_to_harvest_subinterval = " << next_to_harvest_subinterval << "] " << pear.second->subinterval_array[next_to_harvest_subinterval].start_time.format_as_datetime_with_ns() << " to " << pear.second->subinterval_array[next_to_harvest_subinterval].end_time.format_as_datetime_with_ns() << std::endl; log(slavelogfile,o.str()); }
//*debug*/{ostringstream o; o << pear.first << " basing times on [otherSubinterval = " << otherSubinterval << "] " << pear.second->subinterval_array[otherSubinterval].start_time.format_as_datetime_with_ns() << " to " << pear.second->subinterval_array[otherSubinterval].end_time.format_as_datetime_with_ns() << std::endl; log(slavelogfile,o.str()); }
			// Zero out inactive subinterval output object.
			(pear.second->subinterval_array)[next_to_harvest_subinterval].output.clear();

			pear.second->subinterval_array[next_to_harvest_subinterval].start_time
				= pear.second->subinterval_array[otherSubinterval].end_time;

			pear.second->subinterval_array[next_to_harvest_subinterval].end_time
				= pear.second->subinterval_array[next_to_harvest_subinterval].start_time + subinterval_duration;
//*debug*/{ostringstream o; o << pear.first << " have now set up [next_to_harvest_subinterval = " << next_to_harvest_subinterval << "] " << pear.second->subinterval_array[next_to_harvest_subinterval].start_time.format_as_datetime_with_ns() << " to " << pear.second->subinterval_array[next_to_harvest_subinterval].end_time.format_as_datetime_with_ns() << std::endl; log(slavelogfile,o.str()); }
			(pear.second->subinterval_array)[next_to_harvest_subinterval].subinterval_status = IVY_SUBINTERVAL_READY_TO_RUN;
		}
		pear.second->slaveThreadConditionVariable.notify_all();
//*debug*/log(slavelogfile,std::string("dropped the lock to process result of a subinterval for \"")+pear.first+std::string("\".\n"));
	}

	lasttime.setToNow();
	say(std::string("<end>"),slavelogfile,lasttime);

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

	master_thread_subinterval_end_time = master_thread_subinterval_end_time + subinterval_duration;

//*debug*/log(slavelogfile,"Exiting from waitForSubintervalEndThenHarvest()");
	return true;
}

