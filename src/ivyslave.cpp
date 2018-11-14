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
// ivyslave.cpp


#include "ivyslave.h"
// This is the ONLY include for ivyslave.h.  including it again somewhere else would blow up in link edit.

// methods

bool waitForSubintervalEndThenHarvest();



// code

void invokeThread(WorkloadThread* T) {
	T->WorkloadThreadRun();
}

void say(std::string s) {
 	if (s.length()==0 || s[s.length()-1] != '\n') s.push_back('\n');

    if (last_message_time == ivytime(0))
    {
        last_message_time.setToNow();
    }
    else
    {
        ivytime now, delta;

        now.setToNow();
        delta = now - last_message_time;
        last_message_time = now;

    	if (routine_logging) log(slavelogfile,format_utterance("Slave", s, delta));
    }

	std::cout << s << std::flush;
	return;
}


void killAllSubthreads(std::string logfilename) {
	for (auto& pear : workload_threads)
    {
        if (ThreadState::died == pear.second->state || ThreadState::exited_normally == pear.second->state)
        {
            std::ostringstream o;
            o << "<Error> Thread for " << pear.first << " in state " << threadStateToString(pear.second->state) << " whose dying_words were \"" << pear.second->dying_words << "\"." << std::endl;
            WorkloadThread_dying_words << o.str();

            if (!routine_logging) { log(slavelogfile,o.str());}
            say(o.str());

            // See if it's OK to say this during killAllSubthreads.  We're on the way out anyway.
            // I say them one at a time, because at least if I get part way through there will be a log message on the master host pipe_driver_subthread log.
        }
        else
		{
			std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);
			pear.second->ivyslave_main_posted_command=true;
			pear.second->ivyslave_main_says=MainThreadCommand::die;
			log(pear.second->slavethreadlogfile,"ivyslave main thread posted \"die\" command.");
		}
		pear.second->slaveThreadConditionVariable.notify_all();
	}
	int threads=0;
	for (auto& pear : workload_threads) {
		pear.second->std_thread.join();
		threads++;
	}

	if (routine_logging)
	{
        std::ostringstream o;
        o << "killAllSubthreads() - commanded " << threads <<" iosequencer threads to die and harvested the remains.\n";
        log(logfilename, o.str());
	}
}


void initialize_io_time_clip_levels(); // Accumulators_by_io_type.cpp


int main(int argc, char* argv[])
{
	std::mutex ivyslave_main_mutex;

	initialize_io_time_clip_levels();

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

    {
        std::ostringstream o;
        o << "ivyslave version " << ivy_version << " build date " << IVYBUILDDATE << " starting." << std::endl;
        log(slavelogfile,o.str());
    }

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
	say(std::string("Hello, whirrled! from ")+myhostname());


    for (unsigned int c = 0; c < 128; c++) if (isprint((char)c)) printable_ascii.push_back((char)c);

    if (routine_logging) {std::ostringstream o; o << "printable_ascii = \"" << printable_ascii << "\"" << std::endl; log(slavelogfile,o.str());}

	std::string input_line;
	if(std::cin.eof()) {log("std::cin.eof()\n",slavelogfile); return 0;}
	while(!std::cin.eof())
	{
		// get commands from ivymaster
		std::getline(std::cin,input_line);
		lasttime.setToNow();

        if (last_message_time == ivytime(0))
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
			// When ivymaster subthread encounters an error and is terminating abnormally
			killAllSubthreads(slavelogfile);
			return -1;
		}
		// end of [Die, Earthling!]

		else if (stringCaseInsensitiveEquality(input_line, std::string("send LUN header")))
		{
			disco.get_header_line(header_line);
			say("[LUNheader]ivyscript_hostname,"+header_line);
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
					o << "<Error> Ivyslave main thread failed trying to load a LUN lister csv file line into a LUN object.  csv header line = \""
					<< header_line << "\", data line = \"" << disco_line << "\"." << std::endl ;
					log(slavelogfile,o.str());
					say (o.str());
					killAllSubthreads(slavelogfile);
					return -1;
				}
				LUNpointers[pLUN->attribute_value(std::string("LUN name"))]=pLUN;
				say(std::string("[LUN]")+hostname+std::string(",")+disco_line);
			}
			else
			{
				say("[LUN]<eof>");
			}
		}
		// end of send LUN

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
				say("<Error> [CreateWorkload] did not have \"[Parameters]\"");
				killAllSubthreads(slavelogfile);
				return -1;
			}
			workloadID=remainder.substr(0,i);
			trim(workloadID);
			trim(createThreadParameters);

			// we get "[CreateWorkload] sun159+/dev/sdxx+ [Parameters]subinterval_input.toString()

            if (routine_logging) log(slavelogfile,std::string("[CreateWorkload] WorkloadID is ")+workloadID+std::string("\n"));

			std::unordered_map<std::string, WorkloadThread*>::iterator trd=workload_threads.find(workloadID);
			if (workload_threads.end()!=trd) {
				std::ostringstream o;
				o << "<Error> [CreateWorkload] failed - thread \"" << workloadID << "\" already exists! Not supposed to happen.  ivymaster is supposed to know." << std::endl;
				say(o.str());
				killAllSubthreads(slavelogfile);
				return -1;
			}

			WorkloadID wID(workloadID);
			if (!wID.isWellFormed)
			{
				std::ostringstream o;
				o << "<Error> [CreateWorkload] - workload ID \"" << workloadID << "\" did not look like hostname+/dev/sdxxx+workload_name." << std::endl;
				say(o.str());
				killAllSubthreads(slavelogfile);
				return -1;
			}

			std::map<std::string, LUN*>::iterator nabIt = LUNpointers.find(wID.getLunPart());

			if (LUNpointers.end() == nabIt)
			{
				std::ostringstream o;
				o << "<Error> [CreateWorkload] - workload ID \"" << workloadID << "\"  - no such LUN \"" << wID.getLunPart() << "\"." << std::endl;
				say(o.str());
				killAllSubthreads(slavelogfile);
				return -1;
			}

			LUN* pLUN = (*nabIt).second;

			//Now we validate & parse the maxLBA
			if (!pLUN->contains_attribute_name(std::string("Max LBA")))
			{
				std::ostringstream o;
				o << "<Error> [CreateWorkload] - workload ID \"" << workloadID <<  "\" - LUN \"" << wID.getLunPart() << "\" - does not have a \"Max LBA\" attribute." << std::endl;
				say(o.str());
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
				say(o.str());
				killAllSubthreads(slavelogfile);
				return -1;

			}
			long long int maxLBA;
			{
				std::istringstream is(maxLBAtext);
				is >> maxLBA;
			}

			WorkloadThread* p_WorkloadThread = new WorkloadThread(workloadID,(*nabIt).second, maxLBA, createThreadParameters,&ivyslave_main_mutex);

			workload_threads[workloadID]=p_WorkloadThread;

	        	p_WorkloadThread->slavethreadlogfile = std::string(IVYSLAVELOGFOLDERROOT IVYSLAVELOGFOLDER) + std::string("/log.ivyslave.")
				+ convert_non_alphameric_or_hyphen_or_period_to_underscore(workloadID) + std::string(".txt");

			// we still need to set the iosequencer parameters in both subintervals
			p_WorkloadThread->subinterval_array[0].input.fromString(createThreadParameters,slavelogfile);
			p_WorkloadThread->subinterval_array[1].input.fromString(createThreadParameters,slavelogfile);

			//ivy_float target_dedupe = p_WorkloadThread->subinterval_array[0].input.dedupe;
			//p_WorkloadThread->dedupe_regulator = new DedupePatternRegulator(target_dedupe);
                        //log(p_WorkloadThread->slavethreadlogfile, p_WorkloadThread->dedupe_regulator->logmsg());

			// Invoke thread
			p_WorkloadThread->std_thread=std::thread(invokeThread,p_WorkloadThread);

            { // wait one second for workload thread to start.
                std::unique_lock<std::mutex> u_lk(p_WorkloadThread->slaveThreadMutex);

                if (!p_WorkloadThread->slaveThreadConditionVariable.wait_for(u_lk, 1000ms,
                           [&p_WorkloadThread] { return p_WorkloadThread->state == ThreadState::waiting_for_command; }))
                {
                    std::ostringstream o;
                    o << "<Error> ivyslave waited more than one second for workload thread \""
                        << p_WorkloadThread->workloadID.workloadID << "\" to start up." << std::endl
                        << "Source code reference " << __FILE__ << " line " << __LINE__ << std::endl;
                    say(o.str());
                    log(slavelogfile, o.str());
                    u_lk.unlock();
                    killAllSubthreads(slavelogfile);
                }
            }

			say("<OK>");
		}
        // end of [CreateWorkload]

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
				say(o.str());
				killAllSubthreads(slavelogfile);
				return -1;
			}

            std::unordered_map<std::string, WorkloadThread*>::iterator
                t_it = workload_threads.find(wid.workloadID);

			if (workload_threads.end() == t_it)
			{
				std::ostringstream o;
				o << "<Error> at ivyslave [DeleteWorkload] failed - no such WorkloadID \"" << remainder << "\"." << std::endl;
				say(o.str());
				killAllSubthreads(slavelogfile);
				return -1;
			}

			WorkloadThread* p_WorkloadThread = t_it->second;

			if (p_WorkloadThread == nullptr)
			{
				std::ostringstream o;
				o << "<Error> at ivyslave [DeleteWorkload] failed - internal programming error WorkloadID \"" << remainder << "\" key exists but the associated WorkloadTracker pointer was nullptr." << std::endl;
				say(o.str());
				killAllSubthreads(slavelogfile);
				return -1;
			}

            if (ThreadState::died != p_WorkloadThread->state && ThreadState::exited_normally != p_WorkloadThread->state)
            {
                std::unique_lock<std::mutex> u_lk(p_WorkloadThread->slaveThreadMutex);

                p_WorkloadThread->ivyslave_main_posted_command=true;
                p_WorkloadThread->ivyslave_main_says=MainThreadCommand::die;
                log(p_WorkloadThread->slavethreadlogfile,"ivyslave main thread posted \"die\" command.");
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

            workload_threads.erase(t_it);

			say("<OK>");
		}
        // end of [DeleteWorkload]

		else if (startsWith("[EditWorkload]",input_line,remainder))
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
				say("<Error> [EditWorkload] did not have \"[Parameters]\"");
				killAllSubthreads(slavelogfile);
				return -1;
			}
			listOfWorkloadIDsText=remainder.substr(0,i);
			trim(listOfWorkloadIDsText);
			trim(parametersText);

			// we get "[CreateWorkload] sun159+/dev/sdxx+ [Parameters]subinterval_input.toString()

            if(routine_logging) log(slavelogfile,std::string("[EditWorkload] WorkloadIDs are ")+listOfWorkloadIDsText+std::string(" and parametersText=\"")+parametersText+std::string("\".\n"));

			ListOfWorkloadIDs lowi;

			if (!lowi.fromString(&listOfWorkloadIDsText))
			{
				say(std::string("<Error> [EditWorkload] invalid list of WorkloadIDs, should look like <myhostname+/dev/sdxy+workload_name,myhostname+/dev/sdxz+workload_name> - \"")
					+ listOfWorkloadIDsText + std::string("\"."));
				killAllSubthreads(slavelogfile);
				return -1;
			}

			for (auto& wID : lowi.workloadIDs)
			{
				//std::unordered_map<std::string, WorkloadThread*>  workload_threads;  // Key looks like "workloadName:host:00FF:/dev/sdxx:2A"

				auto it = workload_threads.find(wID.workloadID);

				if (workload_threads.end() == it)
				{
					say(std::string("<Error> [EditWorkload] no such WorkloadID, should look like myhostname+/dev/sdxy+workload_name - \"")
						+ wID.workloadID + std::string("\"."));
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
							+ wID.workloadID + std::string("\"."));
						killAllSubthreads(slavelogfile);
						return -1;
					}
					else
					{
						// we have the lock

						// Note that there is no interlock between threads nor any posting of events
						// since this just updates the iosequencer_input parameters for a subinterval that is waiting to be run.

						if ( ThreadState::running  == p_WorkloadThread->state || ThreadState::waiting_for_command == p_WorkloadThread->state )
						{
							auto rv = p_WorkloadThread->subinterval_array[0].input.setMultipleParameters(parametersText);

							if (!rv.first)
							{
								std::ostringstream o;
								o << "<Error> Internal programming error since we already successfully set these parameters into the corresponding WorkloadTracker objects in ivymaster - failed setting parameters \""
								  << parametersText << "\" into subinterval_array[0].input iosequencer_input object for WorkloadID \""
								  << wID.workloadID << "\" - "
								  << rv.second;
								say(o.str());
								killAllSubthreads(slavelogfile);
								return -1;
							}

							rv = p_WorkloadThread->subinterval_array[1].input.setMultipleParameters(parametersText);

							if (!rv.first)
							{
								std::ostringstream o;
								o << "<Error> Internal programming error since we already successfully set these parameters into the corresponding WorkloadTracker objects in ivymaster - failed setting parameters \""
								  << parametersText << "\" into subinterval_array[1].input iosequencer_input object for WorkloadID \""
								  << wID.workloadID << "\" - "
								  << rv.second;
								say(o.str());
								killAllSubthreads(slavelogfile);
								return -1;
							}
						}
						else
						{
							std::ostringstream o;
							o << "<Error> [EditWorkload] thread " << wID.workloadID << " was not STOPPED or RUNNING, but rather was " << threadStateToString(p_WorkloadThread->state) << " aborting." << std::endl;
							say(o.str());
							killAllSubthreads(slavelogfile);
							return -1;
						}

						// we release the lock

						p_WorkloadThread->slaveThreadMutex.unlock();
						p_WorkloadThread->slaveThreadConditionVariable.notify_all();
					}
				}
			}
			say("<OK>");
		}
        // end of [EditWorkload]

        else if (0 == input_line.compare(std::string("get subinterval result")))
        {
			if (!waitForSubintervalEndThenHarvest()) return -1;
        }
        // end of get subinterval result

		else  if (startsWith(std::string("Go!"),input_line,subinterval_duration_as_string))
		{
			test_start_time.setToNow();
			next_to_harvest_subinterval=0; otherSubinterval=1;

			if (!subinterval_duration.fromString(subinterval_duration_as_string))
			{
				ostringstream o;
				o << "<Error> received command \"" << rtrim(input_line) << "\", saw \"Go!\" with a subinterval duration ivytime string representation of \""
					<< subinterval_duration_as_string << "\", but this failed to convert to an ivytime when calling \"fromString()\".  "
					<< "Valid ivytime string representations look like \"<seconds,nanoseconds>\"." << std::endl;
				say(o.str());
				killAllSubthreads(slavelogfile);
				return -1;
			}

			master_thread_subinterval_end_time      = test_start_time + subinterval_duration;

			harvest_subinterval_number = 0;
			harvest_start = test_start_time;
			harvest_end   = harvest_start + subinterval_duration;

			// harvest CPU counters starting time first subinterval

			if (0!=getprocstat(&interval_start_CPU,slavelogfile))
			{
				say("<Error> ivyslave main thread: procstatcounters::read_CPU_counters() call to get first subinterval starting CPU counters failed.");
			}

			for (auto& pear : workload_threads)
			{
				{
					std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);
					if (ThreadState::waiting_for_command != pear.second->state)
					{
						ostringstream o;
						o << "<Error> received \"Go!\" command, but thread \"" << pear.first << "\" was in " << threadStateToString(pear.second->state) << " state, not in \"waiting_for_command\" state."
						    << "The workload thread\'s dying words were \"" << pear.second->dying_words << "\"." << std::endl;
						say(o.str());
						killAllSubthreads(slavelogfile);
						return -1;
					}

					// At this point, both subinterval input objects already have their iosequencer_input objects set.
						// Either this happened from a [CreateWorkload] or when a previous run stopped, the settings
						// from the last subinterval to run were copied to the other subinterval, or finally
						// both were set by a "[ModifyWorkload]" command that ran while the iosequencer thread was stopped.


// NOTE: Originally I thought I would be switching back and forth between two IosequencerInput objects, but now they are just clones of each other.
//       I was being overly careful but by accident discovered I was writing updates into the "active" subinterval IosequencerInput object instead of the inactive one.
//       Since the running workload thread only reads from the IosequencerInput object there were no race conditions / bad things happening.
//       Eventually should cut back to only having  one IosequencerInput object ... maybe, unless there are pairs of input parameters that must both take effect simultaneously ...


                    if (pear.second->subinterval_array[0].input.fractionRead == 1.0)
                    {
                        pear.second->have_writes = false;
                        pear.second->doing_dedupe = false;
                    }
                    else
                    {
                        pear.second->have_writes = true;
                        pear.second->pat = pear.second->subinterval_array[0].input.pat;
                        pear.second->compressibility = pear.second->subinterval_array[0].input.compressibility;

                        if ( pear.second->subinterval_array[0].input.dedupe == 1.0 )
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

                            //universal fixed seed or random
                            bool use_universal_seed {true};
                            if (use_universal_seed)
                            {
                                WorkloadID wid(pear.first);
                                //pear.second->block_seed = ((uint64_t) std::hash<std::string>{}(pear.first));
                                pear.second->block_seed = ((uint64_t) std::hash<std::string>{}(wid.getLunPart()));
                                pear.second->pattern_seed = universal_seed ^ pear.second->block_seed;
                                //pear.second->pattern_seed = universal_seed;
                                //pear.second->block_seed = pear.second->pattern_seed;
                            } else
                            {
                                pear.second->block_seed = ( (uint64_t) std::hash<std::string>{}(pear.first) ) ^ test_start_time.getAsNanoseconds();
                                pear.second->pattern_seed = pear.second->subinterval_array[0].input.pattern_seed;
                            }
                            pear.second->pattern_number = 0;
                        }
                    }

                    pear.second->write_io_count = 0;

					pear.second->subinterval_array[0].start_time=test_start_time;
					pear.second->subinterval_array[0].end_time=test_start_time + subinterval_duration;
					pear.second->subinterval_array[1].start_time=pear.second->subinterval_array[0].end_time;
					pear.second->subinterval_array[1].end_time=pear.second->subinterval_array[1].start_time + subinterval_duration;
					pear.second->subinterval_array[0].output.clear();  // later if energetic figure out if these must already be cleared.
					pear.second->subinterval_array[1].output.clear();
					pear.second->subinterval_array[0].subinterval_status=subinterval_state::ready_to_run;
					pear.second->subinterval_array[1].subinterval_status=subinterval_state::ready_to_run;
					pear.second->ivyslave_main_posted_command=true;
					pear.second->ivyslave_main_says=MainThreadCommand::start;
					log(pear.second->slavethreadlogfile,"ivyslave main thread posted \"start\" command.");

                    pear.second->cooldown = false;

				}
				pear.second->slaveThreadConditionVariable.notify_all();
			}

			// Although we have prepared two subintervals for the iosequencer thread to use,
			// when the iosequencer threads get to the end of the first subinterval, they are going to
			// expect us to have posted another MainThreadCommand::start command.

			for (auto& pear : workload_threads)
			{
				{
					// Wait for the workload thread to have consumed the "start" command
					// then post a "keep going" command
					std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);
					while (true == pear.second->ivyslave_main_posted_command)        // should have a timeout?? How long?
						pear.second->slaveThreadConditionVariable.wait(u_lk);
					pear.second->ivyslave_main_posted_command=true;
					pear.second->ivyslave_main_says=MainThreadCommand::keep_going;
					if (routine_logging) { log(pear.second->slavethreadlogfile,"ivyslave main thread posted \"keep_going\" command."); }
				}
				pear.second->slaveThreadConditionVariable.notify_all();
			}

			say("<OK>");
		}
		// end of Go!

		else if ( 0 == input_line.compare(std::string("continue")) || 0 == input_line.compare(std::string("cooldown")) )
		{
            bool cooldown_flag;
            if (0 == input_line.compare(std::string("cooldown")))
            {
                cooldown_flag=true;
            }
            else
            {
                cooldown_flag=false;
            }

            input_line = "continue"; //  remove later - grasping at straws

			for (auto& pear : workload_threads)
			{
				{ // lock
					std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);

					if (ThreadState::running != pear.second->state)
					{
						ostringstream o;
						o << "<Error> received \"continue\" or \"cooldown\" command, but thread \"" << pear.first
						    << "\" was in " << threadStateToString(pear.second->state)
						    << " state, not in \"running\" state."
						    << "  dying_words = \"" << pear.second->dying_words << "\"." << std::endl;
						say(o.str());
						killAllSubthreads(slavelogfile);
						return -1;
					}

					// At this point, ivy master has made any modifications to the input parameters for the next subinterval
					// and the "other" subsystem has been harvested and we need to turn it around to get ready to run

					pear.second->ivyslave_main_posted_command=true;
					pear.second->ivyslave_main_says=MainThreadCommand::keep_going;
					pear.second->cooldown = cooldown_flag;

					if (routine_logging)
					{
					    std::ostringstream o;
					    o << "ivyslave main thread posted \"keep_going\" command with cooldown_flag = "
                            << (cooldown_flag ? "true" : "false") << ".";
					    log(pear.second->slavethreadlogfile,o.str());
                    }
				}
				pear.second->slaveThreadConditionVariable.notify_all();
			}
			say("<OK>");
		}
		// end of continue or cooldown

		else if (0==input_line.compare(std::string("stop")))
		{
			for (auto& pear : workload_threads)
			{
				{ // lock
					std::unique_lock<std::mutex> u_lk(pear.second->slaveThreadMutex);
					if (ThreadState::running != pear.second->state)
					{
						ostringstream o;
						o << "<Error> received \"stop\" command, but thread \"" << pear.first
						    << "\" was in " << threadStateToString(pear.second->state) << " state, not in \"running\" state."
                            << "  dying_words = \"" << pear.second->dying_words << "\"." << std::endl;
						say(o.str());
						killAllSubthreads(slavelogfile);
						return -1;
					}

					// at this point, what we do is copy the input parameters from the running subinterval
					// to the other one, to get it ready for the next test run as if the thread had just been created

					pear.second->ivyslave_main_posted_command=true;
					pear.second->ivyslave_main_says=MainThreadCommand::stop;
                    log(pear.second->slavethreadlogfile,"ivyslave main thread posted \"stop\" command.");
				}
				pear.second->slaveThreadConditionVariable.notify_all();
			}
			say("<OK>");
		}
		// end of stop

		else if ((input_line.length()>=bracketedExit.length())
			&& stringCaseInsensitiveEquality(input_line.substr(0,bracketedExit.length()),bracketedExit))
        {
			killAllSubthreads(slavelogfile);
			return 0;
		}
		else
		{
			log(slavelogfile,std::string("ivyslave received command from ivymaster \"") + input_line + std::string("\" that was not recognized.  Aborting.\n"));
			killAllSubthreads(slavelogfile);
			return 0;
		}
	}
	// at eof on std::cin

	killAllSubthreads(slavelogfile);
	return 0;
};


bool waitForSubintervalEndThenHarvest()
{
    // wait for subinterval ending time

    ivytime now; now.setToNow();
    if (now > master_thread_subinterval_end_time)
	{
        std::ostringstream o;
        o << "<Error> " << __FILE__ << " line " << __LINE__   << " - subinterval_seconds too short - ivymaster told ivyslave main thread to wait for the end of the subinterval and then harvest results, but subinterval was already over.";
        o << "  This can also be caused if an ivy command device is on a subsystem port that is saturated with other (ivy) activity, making communication with the command device run very slowly.";
        o << "master_thread_subinterval_end_time = " << master_thread_subinterval_end_time.format_as_datetime_with_ns() << ", now = " << now.format_as_datetime_with_ns();
        say(o.str());
        say(o.str());
		killAllSubthreads(slavelogfile);
		return false;
 	}

	try {
		master_thread_subinterval_end_time.waitUntilThisTime();
	}
	catch (std::exception& e)
	{
		std::ostringstream o;
		o << "<Error> " << __FILE__ << " line " << __LINE__ << " - waitForSubintervalEndThenHarvest() - exception during waitUntilThisTime() - " << e.what() << std::endl;
		say(o.str());
		log(slavelogfile, o.str());

		throw;
	}

    ivytime subinterval_ending_time;
            subinterval_ending_time.setToNow();

    ivytime quarter_subinterval = ivytime(0.25 * subinterval_duration.getlongdoubleseconds());

    ivytime limit_time = subinterval_ending_time + quarter_subinterval;

//    ivytime end_of_running_subinterval;
//    end_of_running_subinterval = master_thread_subinterval_end_time + subinterval_duration;

	// harvest CPU counters and send CPU line.

	int rc;

	rc = getprocstat(&interval_end_CPU, slavelogfile);

	if ( 0 != rc )
	{
		std::ostringstream o;
		o << "<Error> " << __FILE__ << " line " << __LINE__ << " - ivyslave main thread routine waitForSubintervalEndThenHarvest(): getprocstat(&interval_end_CPU, slavelogfile) call to get subinterval ending CPU counters failed.\n";
		say(o.str());
		killAllSubthreads(slavelogfile);
		return false;
	}

	struct cpubusypercent cpubusydetail;
	struct avgcpubusypercent cpubusysummary;
	if ( 0 != computecpubusy (
		&interval_start_CPU, // counters at start of interval
		&interval_end_CPU,
		&cpubusydetail, // this gets filled in as output
		&cpubusysummary, // this gets filled in as output
		slavelogfile
	))
	{
        std::ostringstream o;
        o << "<Error> " << __FILE__ << " line " << __LINE__ << " - ivyslave main thread routine waitForSubintervalEndThenHarvest(): computecpubusy() call to get subinterval ending CPU % busies failed.";
        say(o.str());
		killAllSubthreads(slavelogfile);
		return false;
 	}


	say(std::string("[CPU]")+cpubusysummary.toString());

	interval_start_CPU.copyFrom(interval_end_CPU,std::string("turning over for next subinterval in waitForSubintervalEndThenHarvest()"),slavelogfile);

	// At the end of the subinterval, if the iosequencer thread has to wait for than a few ms get the lock, it aborts,
	// as it operates in real time driving and harvesting I/Os and cannot afford to wait.

	// So before we get the lock to see if the iosequencer thread has posted the subinterval as complete,
	// we are going to do is wait for a catnap just to make sure we dont try to get the lock
	// while the iosequencer threads might still be switching over to the next subinterval.

	// If the iosequencer thread hasn't posted the last subinterval as complete by then, we abort.

    // optimal catnap - wait for all the workload threads for ready_to_send

    unsigned int numthreads = workload_threads.size();
    unsigned int thread_number {0};

    ivy_float min_seq_fill_fraction = 1.0;

    RunningStat<ivy_float,ivy_int> dispatching_latency_seconds_accumulator;  // These are a new instance each pass through, so don't need clearing.
    RunningStat<ivy_float,ivy_int> lock_aquisition_latency_seconds_accumulator;
    RunningStat<ivy_float,ivy_int> switchover_completion_latency_seconds_accumulator;

    RunningStat<ivy_float,ivy_int> distribution_over_workloads_of_avg_dispatching_latency;
    RunningStat<ivy_float,ivy_int> distribution_over_workloads_of_avg_lock_acquisition;
    RunningStat<ivy_float,ivy_int> distribution_over_workloads_of_avg_switchover;

    for (auto& pear : workload_threads)
    {
        thread_number++;

        ivytime n; n.setToNow();

        // REST API correction -- add half second to limit_time to ensure ivyslave
        // thread checks after all the workload threads are just in time.
        if ( n > (ivytime(0.5) + limit_time))
        {
            std::ostringstream o;
            o << "<Error> " << __FILE__ << " line " << __LINE__ << " - ivyslave main thread routine waitForSubintervalEndThenHarvest(): "
                << " over 1/4 of the way through the subinterval and still workload thread number "
                << thread_number << " of " << numthreads << " \"" << pear.first << "\" hadn't posted ready-to-send.";
            say(o.str());
            killAllSubthreads(slavelogfile);
            return false;
        }

        ivytime togo = limit_time - n;
        std::chrono::nanoseconds time_to_limit ( (int64_t) togo.getAsNanoseconds() );

        {
            WorkloadThread &wlt = (*pear.second);  // the nested block gets a fresh reference

            {
                std::unique_lock<std::mutex> slavethread_lk(wlt.slaveThreadMutex);

                if (!wlt.slaveThreadConditionVariable.wait_for(slavethread_lk, time_to_limit,
                           [&wlt] { return wlt.ivyslave_main_posted_command == false; }))  // WorkloadThread turns this off when switching to a new subingerval
                {
                    std::ostringstream o;
                    o << "<Error> " << __FILE__ << " line " << __LINE__ << " - ivyslave main thread routine waitForSubintervalEndThenHarvest(): "
                        << " over 1/4 of the way through the subinterval and still workload thread number "
                        << thread_number << " of " << numthreads << " \"" << pear.first << "\" hadn't posted ready-to-send.";
                    say(o.str());
                    log(slavelogfile, o.str());
                    slavethread_lk.unlock();
                    killAllSubthreads(slavelogfile);
                }

                // now we keep the lock while we are processing this subthread

                dispatching_latency_seconds_accumulator           += pear.second->dispatching_latency_seconds;
                lock_aquisition_latency_seconds_accumulator       += pear.second->lock_aquisition_latency_seconds;
                switchover_completion_latency_seconds_accumulator += pear.second->switchover_completion_latency_seconds;

                distribution_over_workloads_of_avg_dispatching_latency.push(pear.second->dispatching_latency_seconds.avg());
                distribution_over_workloads_of_avg_lock_acquisition   .push(pear.second->lock_aquisition_latency_seconds.avg());
                distribution_over_workloads_of_avg_switchover         .push(pear.second->switchover_completion_latency_seconds.avg());

                if (pear.second->sequential_fill_fraction < min_seq_fill_fraction)
                {
                    min_seq_fill_fraction = pear.second->sequential_fill_fraction;
                }

                if((pear.second->subinterval_array)[next_to_harvest_subinterval].subinterval_status == subinterval_state::ready_to_send)
                {
                    (pear.second->subinterval_array)[next_to_harvest_subinterval].subinterval_status = subinterval_state::sending;
                }
                else
                {
                    std::ostringstream o;
                    o << "<Error> " << __FILE__ << " line " << __LINE__ << " - ivyslave main thread routine waitForSubintervalEndThenHarvest(): "
                        << " WorkloadThread " << pear.first << " interlocked at subinterval end, but WorkloadThread hadn't marked subinterval ready-to-send.";
                    say(o.str());
                    log(slavelogfile, o.str());
                    slavethread_lk.unlock();
                    pear.second->slaveThreadConditionVariable.notify_all();
                    killAllSubthreads(slavelogfile);
                }

                // indent level with lock held
                {
                    // send the workload detail data to pipe_driver_subthread

                    ostringstream o;
                    o << '<' << pear.second->workloadID.workloadID << '>'
                        << (pear.second->subinterval_array)[next_to_harvest_subinterval].input.toString()
                        << (pear.second->subinterval_array)[next_to_harvest_subinterval].output.toString() << std::endl;

                    say(o.str());
                }

                // Copy subinterval object from running subinterval to inactive subinterval.
                (pear.second->subinterval_array)[next_to_harvest_subinterval].input.copy(  (pear.second->subinterval_array)[otherSubinterval].input);

                // Zero out inactive subinterval output object.
                (pear.second->subinterval_array)[next_to_harvest_subinterval].output.clear();

                pear.second->subinterval_array[next_to_harvest_subinterval].start_time
                    = pear.second->subinterval_array[otherSubinterval].end_time;

                pear.second->subinterval_array[next_to_harvest_subinterval].end_time
                    = pear.second->subinterval_array[next_to_harvest_subinterval].start_time + subinterval_duration;

                (pear.second->subinterval_array)[next_to_harvest_subinterval].subinterval_status = subinterval_state::ready_to_run;


            }
            // here is where we have dropped the lock

            // Note that we sent data up to pipe_driver_subthread,
            // and we have cleared out and prepared the other subinterval to be marked "ready to run",
            // but we have not posted a command to WorkloadThread, so it will ignore any condition variable notifications until then.

            pear.second->slaveThreadConditionVariable.notify_all();  // probably superfluous

            // (The command to the workload will be posted later, after we get "continue", "cooldown", or "stop".)

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
            ;

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


	master_thread_subinterval_end_time = master_thread_subinterval_end_time + subinterval_duration;

	return true;
}
// end of waitForSubintervalEndThenHarvest()

