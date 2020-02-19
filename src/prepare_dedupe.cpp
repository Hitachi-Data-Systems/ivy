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

//Gets called by go_statement()
//
//Mechanism to handle dedupe thread numbering.
//
//Multiple separate "create workload" statements can be used to build workloads with the same name on different LUNs.
//
//Not all workload threads of the same thread may have the same parameter settings.
//
//Therefore, when we get a "go", we spin through all workload IDs to make sure that every workload ID of the same workload name
//has the same "dedupe", "pattern", and "compressibility" parameter settings - error if not.
//
//Then for a workload name where the dedupe setting is not equal to 1.0
// - we come up with a starting pattern hash value
// - for each workload ID with that workload name, we do an EditWorkload using the
//   back-end mechanism of EditRollup to send out one by one
//   - "you are thread x of y and use pattern seed s".
// - This does mean that we need an interlock between the ivy master thread, pipe-drive subthread, and remote host
//   for each workload ID that is participating in a dedupe group.  Hope this scales OK ...
//
//Each workload thread when it gets "go" sets a "have_dedupe" flag to (dedupe != 1.0).
//
//If have_dedupe, the ivy master thread will have set the "x" of "y" threads numbers and the initial seed
//common to all threads in that workload name.
//
//If !have_dedupe, the thread establishes its own starting seed. (Something like hash of workload ID xored with timestamp)
//


#include <ivy_engine.h>

class dedupe_workload
{
public:
    std::list<WorkloadTracker*> workload_threads;

    ivy_float dedupe_setting;
    pattern   pattern_setting;
    ivy_float compressibility_setting;
    ivy_float fractionRead_setting;
};

void prepare_dedupe()
{
    std::map<std::string /* workload name */, dedupe_workload> workloads;

    if (m_s.workloadTrackers.workloadTrackerPointers.size() == 0)
    {
        std::ostringstream o;
        o << "No workload threads found - in prepare_dedupe() at line " << __LINE__ << " of " << __FILE__;
        m_s.error(o.str());
    }

    for (auto& pear : m_s.workloadTrackers.workloadTrackerPointers)
    {
        WorkloadID wID {pear.first};
        WorkloadTracker* p_WorkloadTracker = pear.second;

        std::string workload_name = wID.getWorkloadPart();

//        if (p_WorkloadTracker->wT_IosequencerInput.dedupe > 1.0 && p_WorkloadTracker->wT_IosequencerInput.fractionRead == 1.0)
//        {
//            std::ostringstream o;
//            o << "Workload \"" << workload_name << "\" thread with WorkloadID = \"" << pear.first
//                << "\" forbidden to have dedupe = " << p_WorkloadTracker->wT_IosequencerInput.dedupe
//                << " with fractionRead = "          << p_WorkloadTracker->wT_IosequencerInput.fractionRead
//                << ".  dedupe > 1.0 not permitted for read-only workloads - in prepare_dedupe() at line " << __LINE__ << " of " << __FILE__;
//            m_s.error(o.str());
//        }

        auto peach = workloads.find(workload_name);
        if (workloads.end() == peach)
        {
            dedupe_workload& new_workload = workloads[workload_name];
            new_workload.workload_threads.push_back(p_WorkloadTracker);
            new_workload.dedupe_setting = p_WorkloadTracker->wT_IosequencerInput.dedupe;
            new_workload.pattern_setting = p_WorkloadTracker->wT_IosequencerInput.pat;
            new_workload.compressibility_setting = p_WorkloadTracker->wT_IosequencerInput.compressibility;
            new_workload.fractionRead_setting = p_WorkloadTracker->wT_IosequencerInput.fractionRead;
        }
        else
        {
            dedupe_workload& existing_workload = (*peach).second;
            existing_workload.workload_threads.push_back(p_WorkloadTracker);
            if (existing_workload.dedupe_setting != p_WorkloadTracker->wT_IosequencerInput.dedupe)
            {
                std::ostringstream o;
                o << "Not all workload threads with the name \"" << workload_name << "\" have the same \"dedupe\" setting - in prepare_dedupe() at line " << __LINE__ << " of " << __FILE__;
                m_s.error(o.str());
            }
            if (existing_workload.dedupe_setting > 1.0)
            {
                if (existing_workload.pattern_setting != p_WorkloadTracker->wT_IosequencerInput.pat)
                {
                    std::ostringstream o;
                    o << "Not all workload threads with the name \"" << workload_name << "\" with dedupe=" << existing_workload.dedupe_setting << " have the same \"pattern\" setting - in prepare_dedupe() at line " << __LINE__ << " of " << __FILE__;
                    m_s.error(o.str());
                }
                if (existing_workload.compressibility_setting != p_WorkloadTracker->wT_IosequencerInput.compressibility)
                {
                    std::ostringstream o;
                    o << "Not all workload threads with the name \"" << workload_name << "\" with dedupe=" << existing_workload.dedupe_setting << " have the same \"compressibility\" setting - in prepare_dedupe() at line " << __LINE__ << " of " << __FILE__;
                    m_s.error(o.str());
                }
                if (existing_workload.fractionRead_setting != p_WorkloadTracker->wT_IosequencerInput.fractionRead)
                {
                    std::ostringstream o;
                    o << "Not all workload threads with the name \"" << workload_name << "\" with dedupe=" << existing_workload.dedupe_setting << " have the same \"fractionRead\" setting - in prepare_dedupe() at line " << __LINE__ << " of " << __FILE__;
                    m_s.error(o.str());
                }
            }
        }
    }

    for (auto& pear : workloads)
    {
        if ( pear.second.dedupe_setting > 1.0 && ((pear.second.fractionRead_setting) < 1.0) )
        {
            unsigned int total_threads = pear.second.workload_threads.size();
            unsigned int this_thread = 0;

            ivytime t;
            t.setToNow();

            uint64_t s = ( (uint64_t) std::hash<std::string>{}(pear.first) ) ^ t.getAsNanoseconds();

            for (auto& p_WorkloadTracker : pear.second.workload_threads)
            {
                std::ostringstream o;
                o << "pattern_seed=" << s << ",threads_in_workload_name=" << total_threads << ",this_thread_in_workload=" << this_thread++;
                std::string parms = o.str();

                // now first set in local iosequencer_input, then set in remote

                auto rv = p_WorkloadTracker->wT_IosequencerInput.setMultipleParameters(parms);
                if (!rv.first)
				{
					std::ostringstream o;
					o << "Internal programming error - failed setting parameters \"" << parms << "\" into local WorkloadTracker object for WorkloadID = \"" << p_WorkloadTracker->workloadID.workloadID
                        << "\" saying \"" << rv.second << "\" in prepare_dedupe() at line " << __LINE__ << " of " << __FILE__;
                    m_s.error(o.str());
				}

                // then set in remote

                auto host_it = m_s.host_subthread_pointers.find(p_WorkloadTracker->workloadID.getHostPart());
                if (host_it == m_s.host_subthread_pointers.end())
                {
					std::ostringstream o;
					o << "Internal programming error - failed looking up host thread for WorkloadID = \"" << p_WorkloadTracker->workloadID.workloadID
                        << " in prepare_dedupe() at line " << __LINE__ << " of " << __FILE__;
                    m_s.error(o.str());
                }

                pipe_driver_subthread* p_host = host_it->second;

                // get lock on talking to pipe_driver_subthread for this remote host
                {
                    std::unique_lock<std::mutex> u_lk(p_host->master_slave_lk);

                    p_host->commandListOfWorkloadIDs.workloadIDs.clear();
                    p_host->commandListOfWorkloadIDs.workloadIDs.push_back(p_WorkloadTracker->workloadID);
                    p_host->p_edit_workload_IDs = & p_host->commandListOfWorkloadIDs;

                    p_host->commandString=std::string("[EditWorkload]");
                    p_host->commandIosequencerParameters = parms;
                    p_host->commandErrorMessage.clear();

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
                        o << "Internal programming error - failed setting dedupe parameters \"" << parms << "\" in remote test host for workloadID = \"" << p_WorkloadTracker->workloadID.workloadID
                            << " saying \"" << p_host->commandErrorMessage << "\" in prepare_dedupe() at line " << __LINE__ << " of " << __FILE__;
                        m_s.error(o.str());
                    }
                }
            }
        }
    }
    return;
}
