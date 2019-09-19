//Copyright (c) 2016, 2017, 2018, 2019 Hitachi Vantara Corporation
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

#include "ivy_engine.h"

extern bool trace_evaluate;
extern std::ostream& trace_ostream;

std::pair<bool /*success*/, std::string /* message */>
ivy_engine::createWorkload(
    const std::string& workloadName,
    const std::string& select_string,
    const std::string& iosequencerName,
    const std::string& parameters)
{
    {
        std::ostringstream o;
        o << "ivy engine API create_workload("
            << "workload_name = \"" << workloadName << "\""
            << ", select = \"" << select_string << "\""
            << ", iosequencer = \"" << iosequencerName << "\""
            << ", parameters = \"" << parameters << "\""
            << ")" << std::endl;
        std::cout << o.str();
        log (m_s.masterlogfile,o.str());
        log(ivy_engine_calls_filename,o.str());
    }

	// Look at proposed workload name

	if (!looksLikeHostname(workloadName))
	{
                ostringstream o;
		o << "Invalid workload name \"" << workloadName << "\".  Workload names start with an alphabetic [a-zA-Z] character and the rest is all alphanumerics and underscores [a-zA-Z0-9_].";
        return std::make_pair(false,o.str());
	}

    JSON_select select;

    if (!select.compile_JSON_select(select_string))
    {
        std::ostringstream o;
        o << "<Error> create workload] failed - invalid \"select\" expression \"" << select_string << "\" - error message follows: " << select.error_message << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }

    { std::ostringstream o; o << "      select " << select << std::endl; std::cout << o.str(); log(m_s.masterlogfile,o.str()); }

	// Look at [select] clause and filter against all discovered test LUNs to define "createWorkloadLUNs".

	LUNpointerList createWorkloadLUNs;

	if (select.is_null())
	{
        createWorkloadLUNs.clone(m_s.availableTestLUNs);
	}
	else
	{
        createWorkloadLUNs.clear_and_set_filtered_version_of(m_s.availableTestLUNs,select);
	}

    if (createWorkloadLUNs.LUNpointers.size() == 0)
    {
        std::string msg = "<Error> [CreateWorkload] - [select] clause did not select any LUNs to create the workload on.\n";
        std::cout << msg;
        log(m_s.masterlogfile, msg);
        m_s.kill_subthreads_and_exit();
    }

	if (trace_evaluate) { trace_ostream << "createWorkloadLUNs contains:" << std::endl << createWorkloadLUNs.toString() << std::endl; }

	//LUN* p_sample_LUN = allDiscoveredLUNs.LUNpointers.front();

	// Look at iosequencer name

	std::unordered_map<std::string,IosequencerInput*>::iterator template_it;
	template_it=iosequencer_templates.find(iosequencerName);
	if (iosequencer_templates.end()==template_it)
	{
        ostringstream o;
        o << "Create workload - invalid [iosequencer] specification - no such I/O generator name found - \"" << iosequencerName << "\".";
        return std::make_pair(false,o.str());
	}

	// Make a copy of the selected iosequencer's default template as a starting point before
	// we apply the user's parameters to the default template to create a "stamp", an IosequencerInput object
	// that we will then clone for each WorkloadTracker that we create to track a particular test host workload thread.

	// Later on, the MeasureController can decide to update these parameters.

	// Because we keep a local copy in ivymaster in the WorkloadTracker object of the most recent update to a workload's
	// IosequencerInput object, we can also do things like "increase IOPS by 10%".

	IosequencerInput i_i;  // the "stamp"
	i_i.fromString((*template_it).second->toString(),masterlogfile);

	// now apply the user-provided parameter settings on top of the template


	if (0<parameters.length())
	{
		auto rv = i_i.setMultipleParameters(parameters);
		if (!rv.first)
		{
            ostringstream o;
            o << "Create workload - failure setting initial parameters - \"" << parameters << "\"." <<std::endl
                << rv.second << std::endl;
            return std::make_pair(false,o.str());
		}
	}


	// The same WorkloadID in text form is used as the lookup key
	//	- in ivymaster to lookup a WorkloadTracker object
	//	- in ivydriver to lookup an I/O driver subthread

	// sun159+/dev/sdc+poink

	// A valid WorkloadID in text form
	//	- is a single token [i.e. no spaces inside it] composed of three parts separated by two plus sign '+' characters
	//	- the first part is a valid test host name according to how you named the host in your ivyscript file.
	//		- canonical hostname, or alias or the IP address form
	//	- the second part is a valid OS name for an available test LUN on that host
	//		- could be /dev/sdxyz on Linux, or if this ever gets ported to using Windows asynchronous I/O, a physical drive name (or is it disk name - I can never remember.)
	//	- the third part is the workload name from the [CreateWorkload] statement
	//		- Valid workload names start with alphabetic [a-zA-Z] continue with alphanumerics & underscores [_a-zA-Z0-9]

	WorkloadID workloadID;

	// Check first that for all the test LUNs, the WorkloadIDs build as well-formed, and that none by those names already exist
	for (auto& p_LUN : createWorkloadLUNs.LUNpointers)
	{
		if
		(
			!workloadID.set
			(
				  p_LUN->attribute_value(std::string("ivyscript hostname"))
				+ std::string("+")
				+ p_LUN->attribute_value(std::string("LUN_name"))
				+ std::string("+")
				+ workloadName
			)
		)
		{
		        ostringstream o;
			o << "Create workload - internal programming error - invalid WorkloadID = \"" << workloadID <<   "\" generated that is not well formed as \"ivyscript_hostname+LUN_name+workload_name\".";
            return std::make_pair(false,o.str());
		}
		std::map<std::string, WorkloadTracker*>::iterator it = workloadTrackers.workloadTrackerPointers.find(workloadID.workloadID);
		if (it != workloadTrackers.workloadTrackerPointers.end())
		{
            ostringstream o;
            o << "Create workload - failed - workload ID = \"" << workloadID <<   "\" already exists.";
            return std::make_pair(false,o.str());
		}
	}

	createWorkloadExecutionTimeSeconds.clear();

	for (auto& p_LUN : createWorkloadLUNs.LUNpointers)  // maybe come back later and do this for a set of LUNs in parallel, like we do for editWorkload().
	{
		std::string ivyscript_hostname = p_LUN->attribute_value(std::string("ivyscript hostname"));

		workloadID.set
		(
			  ivyscript_hostname
			+ std::string("+")
			+ p_LUN->attribute_value(std::string("LUN_name"))
			+ std::string("+")
			+ workloadName
		);

		// First we need to lookup the pointer to the subthread for the host, so we can talk to it.

		std::unordered_map<std::string,pipe_driver_subthread*>::iterator it = host_subthread_pointers.find(ivyscript_hostname);

		if (it == host_subthread_pointers.end())
		{
            ostringstream o;
            o << "Create workload - internal programming error.  Failed looking up pipe_driver_subthread* for host=\"" << ivyscript_hostname <<   "\".";
            return std::make_pair(false,o.str());
		}

		pipe_driver_subthread* p_host = (*it).second;

		// Next we make a new WorkloadTracker object for the remote thread we are going to create on a test host.

		WorkloadTracker* pWorkloadTracker = new WorkloadTracker(workloadID.workloadID, iosequencerName, p_LUN);

		pWorkloadTracker->wT_IosequencerInput.copy(i_i);

		workloadTrackers.workloadTrackerPointers[workloadID.workloadID] = pWorkloadTracker;

		// We time how long it takes to make the thread
		{
			ostringstream o;
			o << "Creating workload on host \"" << p_host->ivyscript_hostname << "\", LUN \"" << p_LUN->attribute_value(std::string("LUN_name")) << "\", workload name \""
			  << workloadName << "\", iosequencer \"" << iosequencerName << "\", parameters \"" << i_i.toString() << "\".";
			log(masterlogfile,o.str());
		}

		ivytime beforeMakingThread;
		beforeMakingThread.setToNow();

		// get lock on talking to pipe_driver_subthread for this remote host
		{
			std::unique_lock<std::mutex> u_lk(p_host->master_slave_lk);
			// tell slave driver thread to talk to the other end and make the thread

			p_host->commandString=std::string("[CreateWorkload]");

			p_host->commandHost = p_LUN->attribute_value(std::string("ivyscript hostname"));
			p_host->commandLUN = p_LUN->attribute_value(std::string("LUN_name"));
			p_host->commandWorkloadID = workloadID.workloadID;
			p_host->commandIosequencerName = iosequencerName;
			p_host->commandIosequencerParameters = i_i.toString();

			p_host->command=true;
			p_host->commandSuccess=false;
			p_host->commandComplete=false;
			p_host->commandErrorMessage.clear();
		}
		p_host->master_slave_cv.notify_all();

		// wait for the outcome of making the remote thread
		{
			std::unique_lock<std::mutex> u_lk(p_host->master_slave_lk);
			while (!p_host->commandComplete) p_host->master_slave_cv.wait(u_lk);

			if (!p_host->commandSuccess)
			{
				ostringstream o;
				o << "Failed creating thread on host \"" << p_host->ivyscript_hostname << "\", LUN \"" << p_LUN->attribute_value(std::string("LUN_name")) << "\", workload name \""
				  << workloadName << "\", iosequencer \"" << iosequencerName << "\", parameters \"" << i_i.toString() << "\".";
                return std::make_pair(false,o.str());
			}
		}
		ivytime afterMakingThread;
		afterMakingThread.setToNow();
		ivytime createThreadExecutionTime = afterMakingThread - beforeMakingThread;
		createWorkloadExecutionTimeSeconds.push(createThreadExecutionTime.getlongdoubleseconds());
	}


	{
		std::ostringstream o;
		o << "      remote create workload execution time summary: total time " << createWorkloadExecutionTimeSeconds.sum()
			<<" seconds - count " <<   createWorkloadExecutionTimeSeconds.count()
			<< " - average " << createWorkloadExecutionTimeSeconds.avg()
			<< " seconds - minimum " << createWorkloadExecutionTimeSeconds.min()
			<< " seconds - maximum " << createWorkloadExecutionTimeSeconds.max()
			<< " seconds." << std::endl;

		log(masterlogfile,o.str());
	}


	return std::make_pair(true,std::string(""));
}

std::pair<bool /*success*/, std::string /* message */>
ivy_engine::deleteWorkload(
    const std::string& workloadName,
    const std::string& select_string)
{
    {
        std::ostringstream o;
        o << "ivy engine API delete_workload("
            << "workload name = \"" << workloadName << "\""
            << ", select = \"" << select_string << "\""
            << ")" << std::endl;
        std::cout << o.str();
        log (m_s.masterlogfile,o.str());
        log(ivy_engine_calls_filename,o.str());
   }

    std::list <WorkloadID> delete_list {};

    std::string s = workloadName;
    trim(s); // removes leading/trailing whitespace

    bool delete_all {false};

    if (  (s.size() == 0)  ||  (s == "*")  )   // blank workload name or asterisk * means delete all
    {
        delete_all = true;
    }
    else
    {
        // Look at proposed workload name

        if (!looksLikeHostname(s))
        {
            ostringstream o;
            o << "Invalid workload name \"" << workloadName << "\".  Workload names start with an alphabetic [a-zA-Z] character and the rest is all alphanumerics and underscores [a-zA-Z0-9_].";
            return std::make_pair(false,o.str());
        }
    }

    JSON_select select;

    if (!select.compile_JSON_select(select_string))
    {
        std::ostringstream o;
        o << "<Error> delete workload] failed - invalid \"select\" expression \"" << select_string << "\" - error message follows: " << select.error_message << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }
    { std::ostringstream o; o << "delete workload select " << select << std::endl; std::cout << o.str(); log(m_s.masterlogfile,o.str()); }

    for (auto& pear : workloadTrackers.workloadTrackerPointers)
    {
        const std::string& key = pear.first;
        WorkloadTracker* p_WorkloadTracker = pear.second;

        WorkloadID w;

        if (!w.set(key))
        {
		    ostringstream o;
		    o << "<Error> delete workload - internal programming error.  The key of an entry in workloadTrackers.workloadTrackerPointers did not parse as a valid WorkloadID.";
			std::cout << "Sorry if you are going to see this twice, but just in case: " << o.str() << std::endl;
            return std::make_pair(false,o.str());
        }

        if (delete_all)
        {
            delete_list.push_back(w);
        }
        else
        {
            if (stringCaseInsensitiveEquality(w.getWorkloadPart(),s))
            {
                // if the LUN behind this workload matches the Select clause, it's one we need to delete
                if (select.is_null())
                {
                    delete_list.push_back(w);
                }
                else
                {
                    if (select.matches(&(p_WorkloadTracker->workloadLUN)))
                    {
                        delete_list.push_back(w);
                    }
                }
            }
        }
    }

    if (delete_list.size() == 0)
    {
        std::ostringstream o;
        o << "<Error> [DeleteWorkload] - no matching instances of \"" << workloadName << "\" were available to delete.";
        std::cout << "Sorry if you are going to see this twice, but just in case: " << o.str() << std::endl;
        return std::make_pair(false,o.str());
    }

	deleteWorkloadExecutionTimeSeconds.clear();

	for (auto& wid : delete_list)  // judged not worth the effor to for a set of LUNs in parallel, like we do for editWorkload(), since this is when workloads are stopped.
	{
		std::string ivyscript_hostname = wid.getHostPart();

		// First we need to lookup the pointer to the subthread for the host, so we can talk to it.

		std::unordered_map<std::string,pipe_driver_subthread*>::iterator
            it = host_subthread_pointers.find(ivyscript_hostname);

		if (it == host_subthread_pointers.end())
		{
		    ostringstream o;
		    o << "<Error> [DeleteWorkload] - internal programming error.  Failed looking up pipe_driver_subthread* for host = \"" << ivyscript_hostname <<   "\".";
            return std::make_pair(false,o.str());
		}

		pipe_driver_subthread* p_host = it->second;

        auto wit = workloadTrackers.workloadTrackerPointers.find(wid.workloadID);

        if (wit == workloadTrackers.workloadTrackerPointers.end())
        {
		    ostringstream o;
		    o << "<Error> [DeleteWorkload] - internal programming error.  workloadTrackers.workloadTrackerPointers.find(\"" << wid.workloadID << "\") failed when about to delete it.";
            return std::make_pair(false,o.str());
        }

		// We time how long it takes to delete the thread
		{
			ostringstream o;
			o << "Deleting workload thread on host \"" << ivyscript_hostname
			  << "\", LUN \"" << wid.getLunPart()
			  << "\", workload \"" << wid.getWorkloadPart() << "\".";
			log(masterlogfile,o.str());
		}

		ivytime beforeDeletingThread; beforeDeletingThread.setToNow();

		// get lock on talking to pipe_driver_subthread for this remote host
		{
			std::unique_lock<std::mutex> u_lk(p_host->master_slave_lk);
			// tell slave driver thread to talk to the other end and make the thread

			p_host->commandString=std::string("[DeleteWorkload]");
			p_host->commandWorkloadID = wid.workloadID;

			p_host->command=true;
			p_host->commandSuccess=false;
			p_host->commandComplete=false;
			p_host->commandErrorMessage.clear();
		}
		p_host->master_slave_cv.notify_all();

		// wait for the outcome of deleting the remote thread
		{
			std::unique_lock<std::mutex> u_lk(p_host->master_slave_lk);
			while (!p_host->commandComplete) p_host->master_slave_cv.wait(u_lk);

			if (!p_host->commandSuccess)
			{
				ostringstream o;
				o << "Failed deleting workload ID " << wid.workloadID;
                return std::make_pair(false,o.str());
			}
		}
		ivytime afterDeletingThread; afterDeletingThread.setToNow();

		ivytime deleteThreadExecutionTime = afterDeletingThread - beforeDeletingThread;

		deleteWorkloadExecutionTimeSeconds.push(deleteThreadExecutionTime.getlongdoubleseconds());

		delete wit->second; // A struct really
		workloadTrackers.workloadTrackerPointers.erase(wit);
	}


	{
		std::ostringstream o;
		o << "      remote delete workload thread execution time summary: total time " << deleteWorkloadExecutionTimeSeconds.sum()
			<<" seconds - count " <<   deleteWorkloadExecutionTimeSeconds.count()
			<< " - average " << deleteWorkloadExecutionTimeSeconds.avg()
			<< " seconds - minimum " << deleteWorkloadExecutionTimeSeconds.min()
			<< " seconds - maximum " << deleteWorkloadExecutionTimeSeconds.max() << " seconds." << std::endl;

		log(masterlogfile,o.str());
	}


	return std::make_pair(true,std::string(""));
}

