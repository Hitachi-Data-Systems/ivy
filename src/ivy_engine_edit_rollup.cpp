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
#include "ListOfNameEqualsValueList.h"
#include "skew_weight.h"

bool rewrite_total_IOPS(std::string& parametersText, ivy_float fraction_of_total_IOPS)
{
	// rewrites the first instance of "total_IOPS = 5000" to "IOPS=1000.000000000000" by dividing total_IOPS by instances.

	// total_IOPS number forms recognized:  1 .2 3. 4.4 1.0e3 1e+3 1.e-3 3E4

	// returns true if it re-wrote, false if it didn't see "total_IOPS"

	std::string total_IOPS {"total_IOPS"};
	std::string totalIOPS {"totalIOPS"};

	if (total_IOPS.length() > parametersText.length()) return false;

	int start_point, number_start_point, last_plus_one_point;

	for (unsigned int i=0; i < (parametersText.length()-(total_IOPS.length()-1)); i++)
	{
        size_t l = 0;

        if (stringCaseInsensitiveEquality(total_IOPS,parametersText.substr(i,total_IOPS.length())))
        {
            l = total_IOPS.length();
        }
        else if (stringCaseInsensitiveEquality(totalIOPS,parametersText.substr(i,totalIOPS.length())))
        {
            l = totalIOPS.length();
        }

		if ( l > 0 )
		{
			start_point=i;
			i += l;

			while (i < parametersText.length()  && isspace(parametersText[i])) i++;

			if (i < parametersText.length()  && '=' == parametersText[i])
			{
				i++;
				while (i < parametersText.length()  && isspace(parametersText[i])) i++;

                char starting_quote = parametersText[i];

                if ( (i < (parametersText.length()-1)) && ( '\'' == starting_quote || '\"' == starting_quote))
                {   // number is in quotes
                    i++;
                    unsigned int quoted_string_start = i;

                    while ((parametersText[i] != starting_quote) && (i < (parametersText.length()-1)))
                    {
                        i++;
                    }
                    if (parametersText[i] != starting_quote)
                    {
                        std::ostringstream o;
                        o << "[EditRollup] total_IOPS specification missing closing quote: " << parametersText << std::endl;
                        m_s.error(o.str());
                    }
                    last_plus_one_point = i+1;
                    std::string s = parametersText.substr(quoted_string_start,i-quoted_string_start);
                    trim(s);
                    if (!std::regex_match(s,float_number_regex))
                    {
                        std::ostringstream o;
                        o << "[EditRollup] total_IOPS quoted string value \"" << s << "\" does not look like a floating point number." << std::endl;
                        m_s.error(o.str());
                    }
                    std::istringstream n(s);
                    ivy_float v;
                    if (! (n >> v))
                    {
                        std::ostringstream o;
                        o << "[EditRollup] total_IOPS quoted string value \"" << s << "\" did not parse as a floating point number." << std::endl;
                        m_s.error(o.str());
                    }
                    v *= fraction_of_total_IOPS;
                    std::ostringstream newv;
                    newv << "IOPS=" << std::fixed << std::setprecision(12) << v;

                    parametersText.replace(start_point,last_plus_one_point-start_point,newv.str());

                    return true;
                }
                else
                {   // number is not in quotes
                    number_start_point=i;

                    // This next bit must have been written before std::regex was working in g++/libstdc++

                    if
                    ((
                        i < parametersText.length() && isdigit(parametersText[i])
                    ) || (
                        i < (parametersText.length()-1) && '.' == parametersText[i] && isdigit(parametersText[i+1])
                    ))
                    {
                        // we have found a "total_IOPS = 4.5" that we will edit
                        bool have_decimal_point {false};
                        while (i < parametersText.length())
                        {
                            if (isdigit(parametersText[i]))
                            {
                                i++;
                                continue;
                            }
                            if ('.' == parametersText[i])
                            {
                                if (have_decimal_point) break;
                                i++;
                                have_decimal_point=true;
                                continue;
                            }
                            break;
                        }

                        if ( i < (parametersText.length()-1) && ('e' == parametersText[i] || 'E' == parametersText[i]) && isdigit(parametersText[i+1]))
                        {
                            i+=2;
                            while (i < parametersText.length() && isdigit(parametersText[i])) i++;
                        }
                        else if ( i < (parametersText.length()-2) && ('e' == parametersText[i] || 'E' == parametersText[i]) && ('+' == parametersText[i+1] || '-' == parametersText[i+1]) && isdigit(parametersText[i+2]))
                        {
                            i+=3;
                            while (i < parametersText.length() && isdigit(parametersText[i])) i++;
                        }

                        last_plus_one_point=i;

                        std::istringstream isn(parametersText.substr(number_start_point,last_plus_one_point-number_start_point));

                        ivy_float value;

                        isn >> value;

                        value *= fraction_of_total_IOPS;

                        std::ostringstream o; o << "IOPS=" << std::fixed << std::setprecision(12) << value;

                        parametersText.replace(start_point,last_plus_one_point-start_point,o.str());

                        return true;
                    }
				}
			}
		}
	}
	return false; // return point if no match.
}


std::pair<bool /*success*/, std::string /* message */>
ivy_engine::edit_rollup(const std::string& rollupText, const std::string& original_parametersText, bool do_not_log_API_call)
// returns true on success
{
    {
        std::ostringstream o;
        o << "ivy engine API edit_rollup("
            << "rollup_name = \"" << rollupText << "\""
            << ", parameters = \"" << original_parametersText << "\""
            << ")" << std::endl;
        std::cout << o.str()
            << std::endl;  // The extra new line groups fine-grained DFC lines showing first what happened in English, and then here show setting the resulting new IOPS.
        log (m_s.masterlogfile,o.str());
        if (!do_not_log_API_call) { log(ivy_engine_logfile,o.str()); }
    }

//variables
	ListOfNameEqualsValueList listOfNameEqualsValueList(rollupText);
	int fieldsInAttributeNameCombo,fieldsInAttributeValueCombo;
	ivytime beforeEditRollup;  // it's called editRollup at the ivymaster level, but once we build the list of edits by host, at the host level it's editWorkload

    std::string parametersText = original_parametersText;
//code

	beforeEditRollup.setToNow();

	if (!listOfNameEqualsValueList.parsedOK())
	{
		std::ostringstream o;

		o << "Parse error.  Rollup specification \"" << rollupText << "\" should look like \"host=sun159\" or \"serial_number+port = { 410321+1A, 410321+2A }\" - "
		  << listOfNameEqualsValueList.getParseErrorMessage();

		return std::make_pair(false,o.str());
	}

	if (1 < listOfNameEqualsValueList.clauses.size())
	{
		return std::make_pair(false,
            "Rollup specifications with more than one attribute = value list clause are not implemented at this time.\n"
			"Should multiple clauses do a union or an intersection of the WorkloadIDs selected in each clause?\n");
	}

	int command_workload_IDs {0};

	if (0 == listOfNameEqualsValueList.clauses.size())
	{
	    for (auto& pear: host_subthread_pointers)
	    {
	        pear.second->commandListOfWorkloadIDs.clear();
	    }

	    // The default is to apply to all workloads

	    for (auto& pear : workloadTrackers.workloadTrackerPointers)
	    {
	        WorkloadID wID;
	        wID.set(pear.first);
	        if (!wID.isWellFormed)
            {
                std::ostringstream o;
                o << "Internal programming error at line " << __LINE__ << " of " << __FILE__ << "bad WorkloadID \"" << pear.first << "\"" << std::endl;
                throw std::runtime_error(o.str());
            }

            auto host_iter = host_subthread_pointers.find(wID.ivyscript_hostname);
            if (host_iter == host_subthread_pointers.end())
            {
                std::ostringstream o;
                o << "Internal programming error at line " << __LINE__ << " of " << __FILE__ << "bad hostname part \"" << wID.ivyscript_hostname << "\" in WorkloadID \"" << pear.first << "\"" << std::endl;
                throw std::runtime_error(o.str());
            }
            host_iter->second->commandListOfWorkloadIDs.workloadIDs.push_back(wID);
            command_workload_IDs++;
	    }
	}
	else
	{
		// validate the "serial_number+Port" part of "serial_number+Port = { 410321+1A, 410321+2A }"

		std::string rollupsKey;
		std::string my_error_message;

		AttributeNameCombo anc;
		auto rc = anc.set(listOfNameEqualsValueList.clauses.front()->name_token, &m_s.TheSampleLUN);
		if (!rc.first)
		{
		    std::ostringstream o;
		    o << "Edit rollup - invalid atrribute name combo \"" << listOfNameEqualsValueList.clauses.front()->name_token << "\" - " << rc.second;
		    return std::make_pair(false,o.str());
		}

		rollupsKey = anc.attributeNameComboID;

		if (!isPlusSignCombo(my_error_message, fieldsInAttributeNameCombo, rollupsKey ))
		{
			std::ostringstream o;
			o << "rollup type \"" << listOfNameEqualsValueList.clauses.front()->name_token << "\" is malformed - " << my_error_message
			  << " - The rollup type is the \"serial_number+Port\" part of \"serial_number+Port = { 410321+1A, 410321+2A }\".";
            return std::make_pair(false,o.str());
		}

		auto it = rollups.rollups.find(rollupsKey);  // thank you, C++11.

		if (rollups.rollups.end() == it)
		{
			std::ostringstream o;
/*debug*/		o << " rollupsKey=\"" << rollupsKey << "\" ";
			o << "No such rollup \"" << listOfNameEqualsValueList.clauses.front()->name_token << "\".  Valid choices are:";
			for (auto& pear : rollups.rollups)
			{
/*debug*/			o << " key=\"" << pear.first << "\"";
				o << " " << pear.second->attributeNameCombo.attributeNameComboID;
			}
            return std::make_pair(false,o.str());
		}

		RollupType* pRollupType = (*it).second;

		// Before we start doing any matching, return an error message if the user's value combo tokens don't have the same number of fields as the rollup name key

		for (auto& matchToken : listOfNameEqualsValueList.clauses.front()->value_tokens)
		{
			if (!isPlusSignCombo(my_error_message, fieldsInAttributeValueCombo, matchToken))
			{
				std::ostringstream o;
				o << "rollup instance \"" << matchToken << "\" is malformed - " << my_error_message
				  << " - A rollup instance in \"serial_number+Port = { 410321+1A, 410321+2A }\" is \"410321+1A\".";
                return std::make_pair(false,o.str());
			}
			if (fieldsInAttributeNameCombo != fieldsInAttributeValueCombo)
			{
				std::ostringstream o;
				o << "rollup instance \"" << matchToken << "\" does not have the same number of \'+\'-delimited fields as the rollup type \"" << listOfNameEqualsValueList.clauses.front()->name_token << "\".";
                return std::make_pair(false,o.str());
			}
		}

		// We make a ListOfWorkloadIDs for each test host, as this has the toString() and fromString() methods to send over the pipe to ivydriver at the other end;
		// Then on the other end these are also the keys for looking up the iosequencer threads to post the parameter updates to.

//   XXXXXXX  And we are going to need to add "shorthand" for ranges of LDEVs, or hostnames, or PGs, ...

		// Then once we have the map ListOfWorkloadIDs[ivyscript_hostname] all built successfully
		// before we start sending any updates to ivydriver hosts we first apply the parameter updates
		// on the local copy we keep of the "next" subinterval IosequencerInput objects.

		// We fail out with an error message if there is any problem applying the parameters locally.

		// Then when we know that these particular parameters apply successfully to these particular WorkloadIDs,
		// and we do local applys in parallel, of course, only then do we send the command to ivydriver to
		// update parameters.

		// We send out exactly the same command whether ivydriver is running or whether it's stopped waiting for commands.
		// - if it's running, the parameters will be applied to both the running IosequencerInput and the non-running one.
		// - if it's stopped, the parameters will be applied to both subinterval objects which will be subintervals 0 and 1.

		for (auto& pear : host_subthread_pointers)
		{
			pear.second->commandListOfWorkloadIDs.clear();
			// we don't need to get a lock to do this because the slave driver thread only looks at this when we are waiting for it to perform an action on a set of WorkloadIDs.
		}

		for (auto& matchToken : listOfNameEqualsValueList.clauses.front()->value_tokens)
		{
			// 410321+1A

			auto rollupInstance_it = pRollupType/*serial_number+Port*/->instances.find(toLower(matchToken));

			if ( pRollupType->instances.end() == rollupInstance_it )
			{
				std::ostringstream o;
				o << "rollup instance \"" << matchToken << "\" not found.  Valid choices are:";
				for (auto& pear : pRollupType->instances)
				{
					o << "  " << pear.second->rollupInstanceID;
				}
                return std::make_pair(false,o.str());
			}

			ListOfWorkloadIDs& listofWIDs = (*rollupInstance_it).second->workloadIDs;

			for (auto& wID : listofWIDs.workloadIDs)
			{
				host_subthread_pointers[wID.getHostPart()]->commandListOfWorkloadIDs.workloadIDs.push_back(wID);
				command_workload_IDs++;
			}
		}

    }

    if (0 == command_workload_IDs) { return std::make_pair(false,string("edit_rollup() failed - zero test host workload threads selected\n")); }

    // OK now we have built the individual lists of affected workload IDs for each test host for the upcoming command


    // iterate over all the affected workload IDs to populate the "skew" data.

    // This was added later, and to make it easier to code & maintain, it is kept apart.

    skew_data s_d {};
    s_d.clear();

    for (auto& pear : host_subthread_pointers)
    {
        for (auto& wID : pear.second->commandListOfWorkloadIDs.workloadIDs)
        {
            auto wit = workloadTrackers.workloadTrackerPointers.find(wID.workloadID);
            if (workloadTrackers.workloadTrackerPointers.end() == wit)
            {
                std::ostringstream o;
                o << "ivy_engine::editRollup() - dreaded internal programming error - at the last moment the WorkloadTracker pointer lookup failed for workloadID = \"" << wID.workloadID << "\"" << std::endl;
                return std::make_pair(false,o.str());
            }

            WorkloadTracker* pWT = (*wit).second;

            ivy_float skew_w = pWT->wT_IosequencerInput.skew_weight;

            s_d.push(wID, skew_w);
        }
    }

    trim(parametersText);

    if (0 == parametersText.length())
    {
        return std::make_pair(false,std::string("edit_rollup() failed - called with an empty string as parameter updates.\n"));
    }

    for (auto& pear : host_subthread_pointers)
    {
        pipe_driver_subthread* p_host = pear.second;
        p_host->workloads_by_text.clear();
    }

    // Now we first verify that user parameters apply successfully to the "next subinterval" IosequencerInput objects in the affected WorkloadTrackers

    for (auto& pear : host_subthread_pointers)
    {
        pipe_driver_subthread* p_host = pear.second;

        for (auto& wID : pear.second->commandListOfWorkloadIDs.workloadIDs)
        {
            auto wit = workloadTrackers.workloadTrackerPointers.find(wID.workloadID);
            if (workloadTrackers.workloadTrackerPointers.end() == wit)
            {
                std::ostringstream o;
                o << "ivy_engine::editRollup() - dreaded internal programming error - at the last moment the WorkloadTracker pointer lookup failed for workloadID = \"" << wID.workloadID << "\"" << std::endl;
                return std::make_pair(false,o.str());
            }

            WorkloadTracker* pWT = (*wit).second;

            std::string edited_text = parametersText;

            rewrite_total_IOPS(edited_text, s_d.fraction_of_total_IOPS(wID.workloadID));

            // rewrites the first instance of "total_IOPS = 5000" to "IOPS= xxxx where the total_IOPS
            // is first divided equally across all represented LUNs on all hosts,
            // and then within a LUN, the IOPS is allocated according to the "skew" parameter value (default 1.0).
            // The skew parameter is treated as a weight.  Using weights adding to 1.0 or 100 work the way you expect.

            // total_IOPS number forms recognized:  1 .2 3. 4.4 1.0e3 1e+3 1.e-3 3E4, which is perhaps more flexible than with IOPS, which may only take fixed point numbers
            // returns true if it re-wrote, false if it didn't see "total_IOPS".

            auto rv = pWT->wT_IosequencerInput.setMultipleParameters(edited_text);
            if (!rv.first)
            {
                std::ostringstream o;
                o << "<Error> Failed setting parameters \"" << edited_text
                    << "\" into local WorkloadTracker object for WorkloadID = \"" << wID.workloadID << "\"" << std::endl << rv.second;
                return std::make_pair(false,o.str());
            }

            p_host->workloads_by_text[edited_text].workloadIDs.push_back(wID);

        }
    }

    // Originally, when there was only one command for each host, they were launched
    // in parallel.  Now with multiple versions of IOPS for total_IOPS + skew,
    // serializing all commands one host at a time, one command at a time.

    // If this takes too long, come back here and figure out how to launch in parallel.

    editWorkloadInterlockTimeSeconds.clear();
    ivytime before_edit_workload_interlock;
    ivytime after_edit_workoad_interlock;

    for (auto& pear : host_subthread_pointers)
    {
        pipe_driver_subthread* p_host = pear.second;

        for (auto& peach : p_host->workloads_by_text)
        {
            before_edit_workload_interlock.setToNow();
            {
                std::unique_lock<std::mutex> u_lk(p_host->master_slave_lk);

                // Already set earlier: ListOfWorkloadIDs p_host->commandListOfWorkloadIDs;
                const std::string& parameters = peach.first;
                p_host->commandIosequencerParameters = parameters;
                p_host->p_edit_workload_IDs = &peach.second;
                p_host->commandStart.setToNow();
                p_host->commandString=std::string("[EditWorkload]");
                p_host->command=true;
                p_host->commandSuccess=false;
                p_host->commandComplete=false;
            }
            p_host->master_slave_cv.notify_all();


            // wait for the slave driver thread to say commandComplete
            {
                std::unique_lock<std::mutex> u_lk(p_host->master_slave_lk);
                while (!p_host->commandComplete) p_host->master_slave_cv.wait(u_lk);

                if (!p_host->commandSuccess)
                {
                    std::ostringstream o;
                    o << "ivymaster main thread notified by host driver subthread of failure to set parameters \""
                      << p_host->commandIosequencerParameters << "\" in WorkloadIDs " << p_host->p_edit_workload_IDs->toString() << std::endl
                      << p_host->commandErrorMessage;  // don't forget to clear this when we get there
                    return std::make_pair(false,o.str());
                }
            }

            after_edit_workoad_interlock.setToNow();

            ivytime edit_worklock_interlock = after_edit_workoad_interlock - before_edit_workload_interlock;

            editWorkloadInterlockTimeSeconds.push(edit_worklock_interlock.getlongdoubleseconds());
        }
    }

    ivytime editRollupExecutionTime = after_edit_workoad_interlock - beforeEditRollup;

    if (routine_logging)
    {
        std::ostringstream o;
        o << "edit rollup - overall execution time = " << std::fixed << std::setprecision(1) << (100.*editRollupExecutionTime.getlongdoubleseconds()) << " ms "
            << ", edit workload interlock time "
            << " - average " << std::fixed << std::setprecision(1) << (100.*editWorkloadInterlockTimeSeconds.avg()) << " ms"
            << " - min " << std::fixed << std::setprecision(1) << (100.*editWorkloadInterlockTimeSeconds.min()) << " ms"
            << " - max " << std::fixed << std::setprecision(1) << (100.*editWorkloadInterlockTimeSeconds.max()) << " ms"
            << std::endl << std::endl;
        log(masterlogfile,o.str());
        std::cout<< o.str();
    }

    return std::make_pair(true,"");

}
