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
#include <sys/stat.h>

#include "master_stuff.h"
#include "DynamicFeedbackController.h"
#include "RollupInstance.h"

extern bool routine_logging;
extern bool trace_lexer;
extern bool trace_parser;
extern bool trace_evaluate;

void run_subinterval_sequence(DynamicFeedbackController* p_DynamicFeedbackController)
{
    // This is the generalized dynamic feedback control mechanism engine.

    {
        std::ostringstream o;
        o << "run_subinterval_sequence() starting for step number " << m_s.stepNNNN << " name " << m_s.stepName << std::endl;
        log(m_s.masterlogfile,o.str());
    }

    // We make a test step subfolder in the overall test output folder

    m_s.stepFolder = m_s.testFolder + std::string("/") + m_s.stepNNNN + std::string(".") + m_s.stepName;

    if (mkdir(m_s.stepFolder.c_str(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
    {
        std::ostringstream o;
        o << std::endl << "writhe and fail, gasping \"Died trying to make test step folder \"" << m_s.stepFolder << "\".   Uuuugg." << std::endl;
        log(m_s.masterlogfile,o.str());
        m_s.kill_subthreads_and_exit();
        exit(-1);
    }

    /*debug*/
    {
        std::ostringstream o;
        o << "Made test step folder \"" << m_s.stepFolder << "\"." << std::endl;
        std::cout << o.str();
        log(m_s.masterlogfile,o.str());
    }

    // First we clean out the existing subinterval sequences in the rollups in case it was still full of stuff from a previous test step.
    m_s.running_subinterval = -1;
    m_s.last_gain_adjustment_subinterval = -1;
    m_s.rollups.resetSubintervalSequence();
    m_s.cpu_by_subinterval.clear();

    m_s.measureDFC_failure_point = -1;  // where cooldown extends to wait for WP to empty, this is the point where we declared failure and stopped driving I/O.

//    m_s.have_best_of_wurst = false;
    m_s.have_timeout_rollup = false;


    m_s.min_sequential_fill_progress = 1.0;
    m_s.keep_filling = false;

    // Clear out Subsystem data from a possible earlier subinterval sequence,
    // and then make the first gather for t=0, recording how long it takes.

    m_s.overallGatherTimeSeconds.clear();

    ivytime t0_gather_start, t0_gather_complete, t0_gather_time;

    if (m_s.haveCmdDev)
    {
        t0_gather_start.setToNow();

        std::chrono::system_clock::time_point leftnow {std::chrono::system_clock::now()};
        int timeout_seconds {2};

        for (auto& pear : m_s.subsystems)
        {
            pear.second->gathers.clear();

            if (0 == std::string("Hitachi RAID").compare(pear.second->type()))
            {
                Hitachi_RAID_subsystem* pHitachiRAID = (Hitachi_RAID_subsystem*) pear.second;
                pipe_driver_subthread* p_pds = pHitachiRAID->pRMLIBthread;

                {
                    std::unique_lock<std::mutex> u_lk(p_pds->master_slave_lk);
                    p_pds->commandString = std::string("gather");
                    p_pds->command=true;
                    p_pds->commandComplete=false;
                    p_pds->commandSuccess=false;
                    p_pds->commandErrorMessage.clear();

                    {
                        std::ostringstream o;
                        o << "Posted \"gather\" to thread for subsystem serial " << pHitachiRAID->serial_number << " managed on host " << p_pds->ivyscript_hostname << '.' << std::endl;
                        log(m_s.masterlogfile,o.str());
                    }
                }
                p_pds->master_slave_cv.notify_all();
            }
        }

        for (auto& pear : m_s.subsystems)
        {
            if (0 == std::string("Hitachi RAID").compare(pear.second->type()))
            {
                Hitachi_RAID_subsystem* pHitachiRAID = (Hitachi_RAID_subsystem*) pear.second;
                pipe_driver_subthread* p_pds = pHitachiRAID->pRMLIBthread;

                {
                    std::unique_lock<std::mutex> u_lk(p_pds->master_slave_lk);

                    if (
                            p_pds->master_slave_cv.wait_until
                            (
                                u_lk,
                                leftnow + std::chrono::seconds(timeout_seconds),
                                [&p_pds]() { return p_pds->commandComplete; }
                            )
                        )
                    {
                        if (!p_pds->commandSuccess)
                        {
                            std::ostringstream o;
                            o << "\"gather\" to thread for subsystem serial " << pHitachiRAID->serial_number << " managed on host " << p_pds->ivyscript_hostname << " failed.  Aborting." << std::endl;
                            log(m_s.masterlogfile,o.str());
                            m_s.kill_subthreads_and_exit();
                            exit(-1);
                        }

                        {
                            std::ostringstream o;
                            o << "\"gather\" reported complete with duration " << p_pds->duration.format_as_duration_HMMSSns() << " by thread for subsystem serial " << pHitachiRAID->serial_number << " managed on host " << p_pds->ivyscript_hostname << '.' << std::endl;
                            log(m_s.masterlogfile,o.str());
                        }
                    }
                    else // timeout
                    {
                        std::ostringstream o;
                        o << "Aborting - timeout waiting " << timeout_seconds << " seconds for pipe_driver_subthread for subsystem " << pear.first << " to complete a gather." <<  std::endl;
                        std::cout << o.str();
                        log(m_s.masterlogfile,o.str());
                        u_lk.unlock();
                        m_s.kill_subthreads_and_exit();
                        exit ( -1);
                    }
                }
            }
        }

        t0_gather_complete.setToNow();
        ivytime t0_gather_time {t0_gather_complete - t0_gather_start};
        m_s.overallGatherTimeSeconds.push(t0_gather_time.getlongdoubleseconds());
        {
            std::ostringstream o;
            o << std::endl << "t=0 overall gather time over all subsystems was " << std::fixed << std::setprecision(1) << (1000.*t0_gather_time.getlongdoubleseconds()) << " ms - " << m_s.getWPthumbnail(-1)<< std::endl << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile,o.str());
        }

    }

    m_s.have_timeout_rollup = false;

    if (m_s.have_pid || m_s.have_measure)
    {
        if (m_s.p_focus_rollup == nullptr) throw std::runtime_error("<Error> internal programming error - run_subinterval_sequence() - have_pid and/or have_measure are set, but m_s.p_focus_rollup == nullptr.");

        for (auto& pear : m_s.p_focus_rollup->instances)
        {
            pear.second->reset();
        }
    }

    m_s.in_cooldown_mode = false;

    // On the other end, iosequencer threads are waiting to start.  This is because when you first
    // create the thread, it sets up the first two subintervals with the parameters specified,
    // and then when a subinterval sequence ends, once ivyslave stops upon command and sends the last
    // subinterval detail lines, it will set up the first two subintervals to have identical subinterval
    // input parameters to the last completed subinterval.  This is a starting point for any ModifyWorkload
    // commands that may follow.

    // post "Go!" command to start the iosequencer threads running
    for (auto& pear : m_s.host_subthread_pointers)
    {
        {
            std::unique_lock<std::mutex> u_lk(pear.second->master_slave_lk);
            std::ostringstream o;
            o << "Go!" << m_s.subintervalLength.toString() << ';' << std::fixed << m_s.catnap_time_seconds << ';' << std::fixed << m_s.post_time_limit_seconds;
            pear.second->commandString = o.str();
            pear.second->command=true;
            pear.second->commandComplete=false;
            pear.second->commandSuccess=false;
            pear.second->commandErrorMessage.clear();

            {
                std::ostringstream o;
                o << "Posted \"Go!\" for subintervalLength = " << m_s.subintervalLength.toString() << "seconds, "
                    << "catnap_time_seconds = " << std::fixed << m_s.catnap_time_seconds << "seconds, and "
                    << "post_time_limit_seconds = " << std::fixed << m_s.post_time_limit_seconds << "seconds "
                    << "to " << pear.second->ivyscript_hostname << std::endl;
                log(m_s.masterlogfile,o.str());
            }
        }
        pear.second->master_slave_cv.notify_all();
    }

    m_s.lastEvaluateSubintervalReturnCode=EVALUATE_SUBINTERVAL_CONTINUE;
    Subinterval_CPU s;




    // Now the iosequencer threads are running the first subinterval

    while (true) // loop starts where we wait for iosequencer threads to have posted the results of a subinterval
    {
        m_s.running_subinterval++;



        if ( -1 == m_s.rollups.current_index() )
        {
            m_s.subintervalStart.setToNow();
        }
        else
        {
            m_s.subintervalStart = m_s.rollups.starting_ending_times[m_s.rollups.current_index() ].second;  // ending time from previous subinterval
        }

        m_s.subintervalEnd = m_s.subintervalStart + m_s.subintervalLength;
        m_s.nextSubintervalEnd = m_s.subintervalEnd + m_s.subintervalLength;


        {
            std::ostringstream o;

            o << std::endl << "--------------------------------------------------------------------------------------------------" << std::endl
                << "Test name = \"" << m_s.testName << "\""
                << ", step number = \"" << m_s.stepNNNN << "\""
                << ", step name = \"" << m_s.stepName << "\""
                << " - Top of subinterval " << (m_s.rollups.current_index()+1)
                << " from " << m_s.subintervalStart.format_as_datetime_with_ns()
                <<  " to  " << m_s.subintervalEnd.format_as_datetime_with_ns() << std::endl;
            std::cout << o.str();
            if (routine_logging) log(m_s.masterlogfile,o.str());
        }

        m_s.rollups.startNewSubinterval(m_s.subintervalStart,m_s.subintervalEnd);

        m_s.cpu_by_subinterval.push_back(s);  // master_stuff: std::vector<Subinterval_CPU> cpu_by_subinterval;

        m_s.rollups.not_participating.push_back(subsystem_summary_data());
        for (auto& rt_pair : m_s.rollups.rollups)
        {
            for (auto& ri_pair : rt_pair.second->instances)
            {
                ri_pair.second->subsystem_data_by_subinterval.push_back(subsystem_summary_data());
            }
        }

        if (routine_logging) {
            std::ostringstream o;
            o<< "Waiting for step name " << m_s.stepName
             << ", subinterval " << m_s.rollups.current_index() << " to complete. "
             << "  Start " << (*( m_s.rollups.p_current_start_time() )).format_as_datetime_with_ns()
             << ", end " << (*( m_s.rollups.p_current_end_time() )).format_as_datetime_with_ns()
             << "  Duration " << m_s.subintervalLength.format_as_duration_HMMSSns() << std::endl;
            log(m_s.masterlogfile,o.str());
            //std::cout << o.str();
        }



        // Start of code to perform a "gather()" near the end of the subsystem so the resulting data will be fresh.
        // It's assumed the gather time is comfortably smaller than the subinterval length.  We will see in the log entries.

        ivytime gather_schedule;

        ivytime tn_gather_start, tn_gather_complete, tn_gather_time;


        if (m_s.haveCmdDev)
        {
            gather_schedule = m_s.subintervalEnd - ivytime( (long double) ( (1.0 + gather_lead_time_safety_margin) * m_s.overallGatherTimeSeconds.avg()) );
            gather_schedule.waitUntilThisTime();

            std::system("clear");

            tn_gather_start.setToNow();

            std::chrono::system_clock::time_point leftnow {std::chrono::system_clock::now()};
            int timeout_seconds {2};

            for (auto& pear : m_s.subsystems)
            {
                if (0 == std::string("Hitachi RAID").compare(pear.second->type()))
                {
                    Hitachi_RAID_subsystem* pHitachiRAID = (Hitachi_RAID_subsystem*) pear.second;
                    pipe_driver_subthread* p_pds = pHitachiRAID->pRMLIBthread;

                    {
                        std::unique_lock<std::mutex> u_lk(p_pds->master_slave_lk);
                        p_pds->commandString = std::string("gather");
                        p_pds->command=true;
                        p_pds->commandComplete=false;
                        p_pds->commandSuccess=false;
                        p_pds->commandErrorMessage.clear();

                        {
                            std::ostringstream o;
                            o << "Gathering from " << pHitachiRAID->command_device_description;
                            log(m_s.masterlogfile,o.str());
                            std::cout << o.str();
                        }
                    }
                    p_pds->master_slave_cv.notify_all();
                }
            }

            for (auto& pear : m_s.subsystems)
            {
                if (0 == std::string("Hitachi RAID").compare(pear.second->type()))
                {
                    Hitachi_RAID_subsystem* pHitachiRAID = (Hitachi_RAID_subsystem*) pear.second;
                    pipe_driver_subthread* p_pds = pHitachiRAID->pRMLIBthread;

                    {
                        std::unique_lock<std::mutex> u_lk(p_pds->master_slave_lk);

                        if  (
                                p_pds->master_slave_cv.wait_until
                                (
                                    u_lk,
                                    leftnow + std::chrono::seconds(timeout_seconds),
                                    [&p_pds]() { return p_pds->commandComplete; }
                                )
                            )
                        {
                            if (!p_pds->commandSuccess)
                            {
                                std::ostringstream o;
                                o << "\"gather\" to thread for subsystem serial " << pHitachiRAID->serial_number << " managed on host " << p_pds->ivyscript_hostname << " failed.  Aborting." << std::endl;
                                log(m_s.masterlogfile,o.str());
                                m_s.kill_subthreads_and_exit();
                                exit(-1);
                            }

                            {
                                std::ostringstream o;
                                o << "\"gather\" complete by thread for subsystem serial " << pHitachiRAID->serial_number << " managed on host " << p_pds->ivyscript_hostname << '.' << std::endl;
                                log(m_s.masterlogfile,o.str());
                            }
                        }
                        else // timeout
                        {
                            std::ostringstream o;
                            o << "Aborting - timeout waiting " << timeout_seconds << " seconds for pipe_driver_subthread for subsystem " << pear.first << " to complete a gather." <<  std::endl;
                            std::cout << o.str();
                            log(m_s.masterlogfile,o.str());
                            u_lk.unlock();
                            m_s.kill_subthreads_and_exit();
                            exit ( -1);
                        }
                    }
                }
            }
            tn_gather_complete.setToNow();

            ivytime tn_gather_time {tn_gather_complete - tn_gather_start};
            m_s.overallGatherTimeSeconds.push(tn_gather_time.getlongdoubleseconds());
            {
                std::ostringstream o;
                o << "\"" << m_s.testName << "\" " << m_s.stepNNNN << " \"" << m_s.stepName << "\" bottom of subinterval " << m_s.rollups.current_index() << " - overall gather time was " << tn_gather_time.format_as_duration_HMMSSns()
                  << "     " << m_s.getWPthumbnail(m_s.rollups.current_index())
                  << std::endl
                  << std::endl
                  << "\"all=all\" rollup\'s configuration under test:" << std::endl
                  << m_s.rollups.get_all_equals_all_instance()->test_config_thumbnail
                  << "\"all=all\" rollup\'s subsystem_summary_data.thumbnail(): "
                        << m_s.rollups.get_all_equals_all_instance()->subsystem_data_by_subinterval.back().thumbnail()
                  << std::endl;
                if (routine_logging) log(m_s.masterlogfile,o.str());
                std::cout << o.str();
            }
        }
        // end of code for gathering real-time data from subsystems.

        //{ostringstream o; o << "done gathering real-time data from subsystems, if any.\n"; std::cout << o.str(); log(m_s.masterlogfile,o.str());}

        RunningStat<ivy_float, ivy_int> time_in_hand;
        time_in_hand.clear();

        for (auto& pear : m_s.host_subthread_pointers)
        {
            {
                std::unique_lock<std::mutex> u_lk(pear.second->master_slave_lk);

                // Wait for each subtask to post "subinterval complete".
                while (!pear.second->commandComplete) pear.second->master_slave_cv.wait(u_lk);

                // Note that it is the subtask for each host that does the posting of iosequencer detail lines
                // into the various rollups.  It does this after grabbing the lock to serialize access to the
                // "master_stuff" structure the rollups are kept in that is shared by the main thread and all
                // the host driver subthreads.

                if (!pear.second->commandSuccess)
                {
                    std::ostringstream o;
                    o << std::endl << std::endl << "When waiting for subinterval complete, subthread for " << pear.first << " posted an error, saying "  << pear.second->commandErrorMessage << std::endl;
                    std::cout << o.str();
                    log (m_s.masterlogfile,o.str());
                    m_s.kill_subthreads_and_exit();
                }

                time_in_hand.push(pear.second->time_in_hand_before_subinterval_end.getlongdoubleseconds());

                pear.second->commandComplete=false; // reset for next pass
                pear.second->commandSuccess=false;
                pear.second->commandErrorMessage.clear();
            }
        }

        if (!m_s.haveCmdDev)
        {
            std::system("clear");
        }

        if (routine_logging)
        {
            std::ostringstream o;
            o << "Received notification of delivery to all workload threads of \"" << m_s.host_subthread_pointers.begin()->second->commandString << "\" command from ivyslave instances "
                << "earliest " << std::fixed << std::setprecision(3) << time_in_hand.max() << " seconds, "
                << "latest " << std::fixed << std::setprecision(3) << time_in_hand.min() << " seconds "
                << "before end of subinterval." << std::endl << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile, o.str());
        }

        if (routine_logging)
        {
            std::ostringstream o;
            o << "All host subthreads posted subinterval complete."
            << " - " << "ivyslave catnap_time_seconds = " << std::fixed << m_s.catnap_time_seconds
            << " seconds after subinterval end"
            << " - " << "post_time_limit_seconds = " << std::fixed << m_s.post_time_limit_seconds
            << " seconds after subinterval end."
            << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str() << std::endl;
        }


        auto allTypeIterator = m_s.rollups.rollups.find(std::string("all"));

        if ( m_s.rollups.rollups.end() == allTypeIterator )
        {
            fileappend(m_s.masterlogfile,std::string("Couldn\'t find the \"all\" RollupType to print the subinterval thumbnail line.\n"));
        }
        else
        {
            auto allAllIterator = (*allTypeIterator).second->instances.find(std::string("all"));

            if ( (*allTypeIterator).second->instances.end() == allAllIterator )
            {
                fileappend(m_s.masterlogfile,std::string("Found the \"all\" RollupType, but couldn\'t find the \"all\" RollupInstance to print the subinterval thumbnail line.\n"));
            }
            else
            {
                SubintervalRollup& allAllSubintervalRollup = (*allAllIterator).second->subintervals.lastSubintervalRollup();
                SubintervalOutput& allAllSubintervalOutput = allAllSubintervalRollup.outputRollup;

                ivytime n; n.setToNow();
                ivytime rollup_time = n - m_s.subintervalEnd;


                ivytime test_duration = n - m_s.test_start_time;
                ivytime step_duration = n - m_s.get_go;

                std::ostringstream o;
                o << "At " << test_duration.format_as_duration_HMMSS() << " into test \"" << m_s.testName << "\" and "
                  << "at " << step_duration.format_as_duration_HMMSS() << " into " << m_s.stepNNNN
                  << " \"" << m_s.stepName << "\" "
                  << " rollups complete at " << rollup_time.format_as_duration_HMMSSns() << " after subinterval end." << std::endl;

                if (m_s.haveCmdDev)
                {
                    o << m_s.getWPthumbnail(((*allAllIterator).second->subintervals.sequence.size())-1);
                }

                o << allAllSubintervalOutput.thumbnail(allAllSubintervalRollup.durationSeconds()) << std::endl;

                std::cout << o.str();
                if (routine_logging) log(m_s.masterlogfile,o.str());
            }
        }

        if ( m_s.min_sequential_fill_progress < 1.0  && m_s.sequential_fill)
        {
            m_s.keep_filling = true;
        }
        else
        {
            m_s.keep_filling = false;
        }

        if ( m_s.min_sequential_fill_progress < 1.0 )
        {
            std::ostringstream o;

            o << "Sequential fill progress is "
                << std::fixed << std::setprecision(2)
                << (100*m_s.min_sequential_fill_progress) << "%";

            ivytime now;
            now.setToNow();

            ivytime duration = now - m_s.get_go;
            o << " after " << duration.format_as_duration_HMMSS() << ".";

            auto duration_seconds = duration.getlongdoubleseconds();

            ivytime estimated_total_duration( duration_seconds / m_s.min_sequential_fill_progress);

            ivytime estimated_completion(m_s.get_go + estimated_total_duration);

            ivytime remaining = estimated_completion - now;

            o << "  Estimated remaining " << remaining.format_as_duration_HMMSS()
                << " to complete fill at " << estimated_completion.format_as_datetime() << "."
                << std::endl
                << std::endl;

            std::cout << o.str();
            log(m_s.masterlogfile,o.str());
        }

        m_s.min_sequential_fill_progress = 1.0; // reset for next subinterval


        if (!(EVALUATE_SUBINTERVAL_CONTINUE==m_s.lastEvaluateSubintervalReturnCode)) break;

        // if we said "stop" the last time around, we still go through the top of the loop
        // again to wait for the last subinterval to finish, and we break from the loop here.

        // A subinterval is now complete, including posting all the iosequencer detail lines
        // into the various rollups.

        // This next bit of code generalizes the concept of dynamic feedback control and
        // uses a "DynamicFeedbackController" pure virtual class with an "evaluateSubinterval()" function
        // that each type of feedback control has to implement.

        // At the end of each subinterval and with the rollups ready to analyze, we
        // invoke the DynamicFeedbackController's evaluateSubinterval() routine.

        // The evaluateSubinterval routine analyzes what to do, looking at the sequence of
        // subinterval input and output data so far.  Overall rollups as well as by any rollups
        // you found convenient.  (By whatever you are controlling.)

        // Note that the DFC is called once at the end of a subinterval once results
        // have been posted into all rollups.

        // When ivy was first written, there were separate fixed, measure, and PID DFCs, but now
        // 1) The cooldown_by_wp function was moved from the measure DFC to run_subinterval_sequence
        //     so that it works no matter which DFC is used.
        // 2) The concept of the focus metric used by both PID and measure=on was introduced.
        //    Now, run_subinterval_sequence calls for the focus metric to be gathered after each subinterval
        //    if the have_pid or have_measure flags are on.
        // 3) The PID loop function was moved from the PID_DFC, which was removed, to the rollup instance
        //    where the focus_metric vector is.  If the go statement specified dfc=pid, when
        //    run_subinterval_sequence gathers the focus metric value, it also does a PID loop calculation
        //    of a new total_IOPS and uses edit rollup to send to that rollup instance.
        // 3) Since the focus metric vector by subinterval is by rollup instance, the function to evaluate the
        //    statistical validity of a subsequence was put in the rollup instance that had the data.
        // 4) This left only MeasureDFC standing, as the function of the Fixed_DFC became an if
        //    statement in the Measure DFC.

        // So paradoxically, after the dust settled and we can freely mix fine-grained PID loop and measure
        // functionality where when you say [go] dfc=pid, you are selecting something that run_subinterval_sequence does,
        // and when you say measure=on, it calles the one thing derived from the DFC class.
        // Maybe come back and take another look at terminology at least later.

        // The DFC signals that it is done issuing any more editRollup commands
        // by saying either "continue", or "stop".

        // This gets propagated out to all workload threads and they behave accordingling
        // once they get to the end of the current subinterval and look to see what
        // instruction had been posted for them.

        // Well, the DFC doesn't say "stop".  It says one of two things, both of which
        // are propagated as "stop" to all the lower layers.  A test host workload thread
        // when it stops goes into "waiting for command" state.

        // The basic DFC has a csv file structure that is along the lines of the vdbench "summary.html"
        // concept.  A subinterval sequence, with a mid-stream measurement period sample averaged
        // the measurement period subintervals only ignoring the "warmup" and "cooldown"

        // Each "test step" has a step number like step0003 and a step name (which you are encouraged
        // to highlight the salient point by using).

        // When the ivyscript "Go!" command is executed, the workload threads have whatever most recent
        // input parameters that were set.

        // At the end of each subinterval, and after all the the rollups have been performed,
        // ivymaster prints out a csv line for that subinterval in the csv file for that rollup instance.
        // Rollup instances occur in rollup type folders.

        // There are two places you find the set of rollup folders by rollup type.

        // For each test step a subfolder is created, and within that test step subfolder
        // we find the rollup folder structure within which each rollup instance like "Port=2A"
        // has a csvfile line for each subinterval.

        // The data by subinterval in the test step folders is raw data, meaning that
        // we haven't yet said we made a successful valid repeatable measurement.

        // If the DFC said "success", it makes available the subinterval sequence
        // "first" and "last" indexes of the "measurement period".

        // Analogous to "summary.html", we now build a set of rollup folders in the main
        // output folder for the test, in the same folder that has the individual subfolders by step.

        // For each rollup instance, like "host=sun159", we compute the "measurement" as the average
        // from to the first to last measurement period subintervals this test step [subfolder].
        // So this writes one "measurement" line in the csv files for each test step.
        // If you describe in the step name the thing that is varying with this step, it can make it easier to read the data in Excel.

        // There is a default rollup type "all" that only has one instance "all".

        // So instead of looking for summary.html, look in the output folder for the "all" folder and click on the all=all item.

        // On the other hand, if various data validation mechanisms fail, we won't get a measurement
        // for that step, and appropriate error mechanisms are written.

        // If the DFC says "failure", we print the appropriate csv file data line whose ID preamble
        // is normal, but then which payload part has an error message.



        // Here please peruse some earlier material ...



        // Note that at the point the evaluateSubinterval() routine decides to stop, the next
        // and last subinterval has already started, and the iosequencer thread won't stop
        // until it reaches the end of that last subinterval.  The data from the last subinterval
        // will show in the subinterval-by-subinterval detail csv files, but it cannot become
        // part of valid measurement data because we always require at least one subinterval
        // worth of data be dropped from the end.  (We need to drop at least one subinterval
        // from the end because tests start only as fast as TCP/IP delivers the "Go!" command
        // to each test host, and in this way we don't need NTP synchronisation between test hosts,
        // but we have perhaps a jitter of 1/10 second between subinterval start/end times across
        // the various test hosts. So we have to drop the last subinterval, because otherwise
        // you would see the effects on some hosts of other hosts stopping a tenth of a second
        // before the end of a subinterval.)

        // When we get "success", the DynamicFeedbackController object is ready for you to call its
        // firstMeasurementSubintervalIndex() and lastMeasurementSubintervalIndex() methods which define the set of consecutive subintevals
        // to roll up to get the overall test step results to post in the summary.csv file.

        // A return code of EVALUATE_SUBINTERVAL_FAILURE also means stop at the end of the currently
        // running subinterval, but in this case we are abandoning the test without a range
        // of consecutive subintervals comprising valid measurement data.

        // "Failure" means the test didn't stabilize with a time limit.


// design change 2015-12-07

// Sorry, the terminology has changed.  What is called a DFC now only decides when we have "seen enough".

// What's different is that now the DFC has been broken into three separate functions

// 1) PID loop, 2) Seen enough and stop, 3) cooldown until WP is empty

// Both the PID loop and the "measure" or "seen enough & stop" feature look at the "focus metric",
// so for each rollup instance in the focus rollup, we first collect the focus metric.

// Then if we have PID, we run the PID loop on each focus rollup instance.

// Then we run either the fixed or the measure DFC.

// If the DFC resports SUCCESS or FAILURE, then if cooldown_by_WP is on, and WP is not empty, we turn on
// cooldown mode and run another subinterval.

// Now that you have seen that explanation, before we do any of the above, if we are already in cooldown
// mode we keep doing that until WP is empty.  The measure DFC already has set the first/last measurement
// subinterval values.

        if (m_s.have_pid || m_s.have_measure)
        {
            if (m_s.p_focus_rollup == nullptr) throw std::runtime_error("<Error> internal programming error - run_subinterval_sequence() - have_pid and or have_measure are set, but m_s.p_focus_rollup == nullptr.");

            for (auto& pear : m_s.p_focus_rollup->instances)
            {
                pear.second->collect_and_push_focus_metric();
                if (m_s.have_pid) pear.second->perform_PID();
            }
            if (m_s.have_pid)
            {
                // this puts a blank line to separate from the PID loops' [EditRollup} lines.
                std::ostringstream o;
                o << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
            }

            std::string s = m_s.focus_caption();
            std::cout << s;
            if (s.size()>0 && routine_logging) log(m_s.masterlogfile, s);
        }

        if (m_s.in_cooldown_mode)
        {
            ivytime now; now.setToNow();

            ivytime cooldown_time = now - m_s.cooldown_start;

            if (m_s.some_cooldown_WP_not_empty())
            {
                m_s.lastEvaluateSubintervalReturnCode = EVALUATE_SUBINTERVAL_CONTINUE;

                std::ostringstream o;
                o << "Cooldown duration " << cooldown_time.format_as_duration_HMMSS() << "  not complete, will do another cooldown subinterval." << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
            }
            else
            {
                std::ostringstream o;
                o << "Cooldown duration " << cooldown_time.format_as_duration_HMMSS() << " complete, posting DFC return code from last test subinterval before running cooldown subintervals." << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
                m_s.lastEvaluateSubintervalReturnCode = m_s.eventualEvaluateSubintervalReturnCode;
                    // which is success or failure
            }
        }
        else
        {
            m_s.lastEvaluateSubintervalReturnCode = p_DynamicFeedbackController->evaluateSubinterval();

            {
                std:: ostringstream o;

                o << "For subinterval " << m_s.rollups.current_index() << " evaluateSubinterval() returned ";

                if (EVALUATE_SUBINTERVAL_CONTINUE==m_s.lastEvaluateSubintervalReturnCode)
                    o << "EVALUATE_SUBINTERVAL_CONTINUE";
                else if (EVALUATE_SUBINTERVAL_FAILURE==m_s.lastEvaluateSubintervalReturnCode)
                    o << "EVALUATE_SUBINTERVAL_FAILURE";
                else if (EVALUATE_SUBINTERVAL_SUCCESS==m_s.lastEvaluateSubintervalReturnCode)
                    o << "EVALUATE_SUBINTERVAL_SUCCESS";
                else
                    o << "unexpected return code " << m_s.lastEvaluateSubintervalReturnCode;

                o << std::endl;
                std::cout << o.str();
                if (routine_logging) log(m_s.masterlogfile,o.str());
            }

            if (
                    (
                        m_s.lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_SUCCESS
                        ||
                        m_s.lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_FAILURE
                    )
                &&
                    m_s.cooldown_by_wp
                &&
                    m_s.some_cooldown_WP_not_empty()
            )
            {
                m_s.eventualEvaluateSubintervalReturnCode = m_s.lastEvaluateSubintervalReturnCode;
                m_s.lastEvaluateSubintervalReturnCode = EVALUATE_SUBINTERVAL_CONTINUE;

                m_s.in_cooldown_mode = true;
                m_s.cooldown_start.setToNow();

                std::ostringstream o;
                o << "DFC posted SUCCESS or FAILURE, but cooldown_by_wp is on and we have at least one available test CLPR we can see that is not empty."
                    << "  Entering cooldown mode." << std::endl;

                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
            }
        }


        // Iterate over all hosts, posting command to run another subinterval, or to stop at the end of the current subinterval.
        for (auto& pear : m_s.host_subthread_pointers)
        {
            {
                std::unique_lock<std::mutex> u_lk(pear.second->master_slave_lk);
                if (EVALUATE_SUBINTERVAL_CONTINUE == m_s.lastEvaluateSubintervalReturnCode)
                {
                    if (m_s.in_cooldown_mode)
                    {
                        pear.second->commandString = "cooldown";
                    }
                    else
                    {
                        pear.second->commandString = "continue";
                    }
                }
                else
                {
                    pear.second->commandString = "stop";
                }
                pear.second->command=true;
                pear.second->commandComplete=false;
                pear.second->commandSuccess=false;
                pear.second->commandErrorMessage.clear();
            }
            pear.second->master_slave_cv.notify_all();
        }
    }

//*debug*/{std::ostringstream o; o << "After subinterval sequence but before calculating measurement interval stuff." << std::endl; std::cout << o.str(); log(m_s.masterlogfile,o.str());}

    ivytime warmup_start, warmup_duration, measurement_start, measurement_duration, cooldown_start, cooldown_end, cooldown_duration;






    // print csv files with real-time subsystem data

    // NOTE: this was done after the subinterval sequence is complete because I didn't want to avoid doing processing in real time if I could do it later.
    // But later if we want to see this data as we go along, it would not too hard to move this to "run_subinterval_sequence()" and do it in real time.

    // HOWEVER - need to think about column titles when a metric doesn't appear until after the first gather.

    for (auto& pear : m_s.subsystems)
    {
        pear.second->print_subinterval_csv_line_set( pear.second->serial_number + std::string(".perf"));
    }

    unsigned int first, last;



    if (EVALUATE_SUBINTERVAL_SUCCESS == m_s.lastEvaluateSubintervalReturnCode
     || EVALUATE_SUBINTERVAL_FAILURE == m_s.lastEvaluateSubintervalReturnCode)
    {

        if (EVALUATE_SUBINTERVAL_SUCCESS == m_s.lastEvaluateSubintervalReturnCode)
        {
            {
                std::ostringstream o;
                o << "Successful valid measurement, either fixed duration, or using the \"measure\" feature." << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
            }
            first = p_DynamicFeedbackController->firstMeasurementSubintervalIndex();
            last  = p_DynamicFeedbackController->lastMeasurementSubintervalIndex();
        }
        else
        {
            if (nullptr == m_s.p_focus_rollup)
            {
                std::ostringstream o;
                o << std::endl << "Writhe and fail, gasping that although the test result was FAILURE, we didn\'t have a pointer to the focus rollup." << std::endl
                    << "Error occurred in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
                m_s.kill_subthreads_and_exit();
                exit(-1);
            }

            {
                std::ostringstream o;
                o << "Measurement timed out without reaching target accuracy.  Making measurement rollup for best subsequence that got the closest to the target for all instances of the focus rollup." << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
            }

            if (!m_s.p_focus_rollup->p_wurst_RollupInstance->best_first_last_valid)
            {
                std::ostringstream o;
                o << std::endl << "Dreaded internal programming error.  Writhe and fail, gasping that the wurst RollupInstance didn\'t have valid best subsequence first and last indexes." << std::endl
                    << "Error occurred in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
                m_s.kill_subthreads_and_exit();
                exit(-1);
            }

            first = m_s.p_focus_rollup->p_wurst_RollupInstance->best_first;
            last  = m_s.p_focus_rollup->p_wurst_RollupInstance->best_last;
            m_s.have_timeout_rollup = true;
        }

        {
            std::ostringstream o;
            o << "Making measurement rollups from subinterval " << first << " to " << last << "." << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile,o.str());
        }
        // Dropping the first few and last few subintervals, build the measurement rollup set

        auto retval = m_s.rollups.makeMeasurementRollup(first, last);
        if (!retval.first)
        {
            std::ostringstream o;
            o << std::endl << "writhe and fail, gasping that the RollupSet::makeMeasurementRollup()\'s last words were - " << retval.second << std::endl
                << std::endl << "Source code reference: function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
            exit(-1);
        }

        retval = m_s.make_measurement_rollup_CPU(first, last);
        if (!retval.first)
        {
            std::ostringstream o;
            o << std::endl << "writhe and fail, gasping that master_stuff::make_measurement_rollup_CPU()\'s last words were - " << retval.second << std::endl
                << std::endl << "Source code reference: function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
            exit(-1);
        }

    }


    std::cout << "Printing csv files." << std::endl;

    // Now we write out the test step result csv files.

    // All ivy output is by rollup instance (e.g. "1A+CLPR0") csv file in a rollup type (e.g. "Port+CLPR") subfolder.
    // If you want to see the overall result, look in the "all" rollup folder at the only entry "all".

    // For the overall ivyscript file test run csv files that have one line per test step (one line per subinterval sequence)
    // the RollupType subfolders "measurementRollupFolder" are put in the root folder for this ivyscript run - testFolder.

    // For the csv files that have one line per subinterval, the RollupType subfolders "stepRollupFolder"
    // are in the stepFolder subfolder of TestFolder.
    // For example, in the "step0000.Fluffy" subfolder of testFolder, we could have a "Port+CLPR"
    // subfolder containing a csv file for the subinterval sequence for "1A+CLPR0"

    struct stat struct_stat;

    for (auto& pear : m_s.rollups.rollups)  // For each RollupType
    {
        std::string rollupTypeName = pear.second->attributeNameCombo.attributeNameComboID;
        RollupType* pRollupType = pear.second;

        /*debug*/
        {
            std::ostringstream o;
            o << std::endl << "Making csv files for rollup key= \"" << pear.first << "\" - rollupTypeName=\"" << rollupTypeName << "\", pRollupType->attributeNameCombo.attributeNameComboID=\""  << pRollupType->attributeNameCombo.attributeNameComboID << "\"." << std::endl;
            //std::cout << o.str();
            log(m_s.masterlogfile,o.str());
        }

        // Make the RollupInstance's RollupType subfolder if necessary
        m_s.measurementRollupFolder = m_s.testFolder + std::string("/") + rollupTypeName;

        if(0==stat(m_s.measurementRollupFolder.c_str(),&struct_stat))
        {
            // folder name already exists
            if(!S_ISDIR(struct_stat.st_mode))
            {
                std::ostringstream o;
                o << std::endl << "writhe and fail, gasping measurement rollup folder \"" << m_s.measurementRollupFolder << "\" exists but is not a directory.   Uuuugg." << std::endl;
                log(m_s.masterlogfile,o.str());
                m_s.kill_subthreads_and_exit();
                exit(-1);
            }
        }
        else if (mkdir(m_s.measurementRollupFolder.c_str(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
        {
            std::ostringstream o;
            o << std::endl << "writhe and fail, gasping couldn\'t make measurement rollup folder \"" << m_s.measurementRollupFolder << "\".   Uuuugg." << std::endl;
            log(m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
            exit(-1);
        }


        // print data validation csv headerline if necessary

        {
            // but first figure out the data validation csv filename
            std::ostringstream data_validation_filename_stream;

            data_validation_filename_stream << m_s.measurementRollupFolder << "/" << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(m_s.testName)
                                            << '.' << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(rollupTypeName) << ".data_validation.csv";

            m_s.measurement_rollup_data_validation_csv_filename = data_validation_filename_stream.str();
        }

        if ( 0 != stat(m_s.measurement_rollup_data_validation_csv_filename.c_str(),&struct_stat))
        {
            // we need to print the header line.


            std::ostringstream o;

            o << "Test Name,Step Number,Step Name,Rollup," << RollupType::getDataValidationCsvTitles();

            fileappend( m_s.measurement_rollup_data_validation_csv_filename, o.str() );
        }

        // print data validation csv data line

        {
            std::ostringstream o;
            o 	<< m_s.testName << ',' << m_s.stepNNNN << ',' << m_s.stepName
                << ',' << pRollupType->attributeNameCombo.attributeNameComboID
                << ',' << pRollupType->getDataValidationCsvValues();

            {
                std::ostringstream g; g << quote_wrap_csvline_except_numbers(o.str()) << std::endl;
                fileappend(m_s.measurement_rollup_data_validation_csv_filename,g.str());
            }

        }

        for (auto& peer : pRollupType->instances) // For each RollupInstance
        {
            std::string rollupInstanceName = peer.first;
            RollupInstance* pRollupInstance = peer.second;

            if (routine_logging)
            {
                std::ostringstream o;
                o << std::endl << "Making csv files for rollup instance key= \"" << pear.first << "\" - pRollupInstance->attributeNameComboID = \"" << pRollupInstance->attributeNameComboID << "\", RollupInstance->rollupInstanceID = \"" << pRollupInstance->rollupInstanceID << "\"." << std::endl;
                log(m_s.masterlogfile,o.str());
            }

            // First print the by-test_step measurement period csv file lines in the appropriate RollupType subfolder of output folder
            // Write the csv header line if necessary
            // Write the csv data line for the measurement rollup
            {
                std::string instanceFilename = rollupInstanceName;
                std::replace(instanceFilename.begin(),instanceFilename.end(),'/','_');
                std::replace(instanceFilename.begin(),instanceFilename.end(),'\\','_');

                std::ostringstream filename_prefix;
                filename_prefix << m_s.measurementRollupFolder << "/"
                                << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(m_s.testName )
                                << '.'
                                << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(rollupTypeName)
                                << '='
                                << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(instanceFilename)
                                << '.';

                std::ostringstream type_filename_prefix;
                type_filename_prefix << m_s.measurementRollupFolder << "/"
                                << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(m_s.testName )
                                << '.'
                                << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(rollupTypeName)
                                << '.';

                m_s.measurement_rollup_by_test_step_csv_filename = filename_prefix.str() + std::string("summary.csv");
                m_s.measurement_rollup_by_test_step_csv_type_filename = type_filename_prefix.str() + std::string("summary.csv");

                /*debug*/
                {
                    std::ostringstream o;
                    o << std::endl << "m_s.measurement_rollup_by_test_step_csv_filename=\"" << m_s.measurement_rollup_by_test_step_csv_filename << "\"." << std::endl;
                    o << std::endl << "m_s.measurement_rollup_by_test_step_csv_type_filename=\"" << m_s.measurement_rollup_by_test_step_csv_type_filename << "\"." << std::endl;
//                    std::cout << o.str();
                    log(m_s.masterlogfile,o.str());
                }


                bool need_header = (0 != stat(m_s.measurement_rollup_by_test_step_csv_filename.c_str(),&struct_stat));
                bool need_type_header = (0 != stat(m_s.measurement_rollup_by_test_step_csv_type_filename.c_str(),&struct_stat));

                if ( need_header || need_type_header)
                {
                    // we need to print the header line.
                    {
                        std::ostringstream o;

                        o << "Test Name,Step Number,Step Name,Start,Warmup,Duration,Cooldown,Write Pending,valid or invalid,invalid reason,Rollup Type,Rollup Instance";

                        if (m_s.ivymaster_RMLIB_threads.size()>0) { o << pRollupInstance->test_config_thumbnail.csv_headers(); }
                        o << IosequencerInputRollup::CSVcolumnTitles();
                        o << m_s.measurement_rollup_CPU.csvTitles();

                        if (m_s.ivymaster_RMLIB_threads.size()>0)
                        {
                            o << subsystem_summary_data::csvHeadersPartOne();
                            o << ",host IOPS per drive,host decimal MB/s per drive, application service time Littles law q depth per drive";
                            o << ",Subsystem IOPS as % of application IOPS,Subsystem MB/s as % of application MB/s,Subsystem service time as % of application service time,Path latency = application minus subsystem service time (ms)";
                        }

                        o << ",non random sample correction factor";
                        o << ",plus minus series statistical confidence";

                        o << SubintervalOutput::csvTitles(true);
                        if (m_s.ivymaster_RMLIB_threads.size()>0)
                        {
                            o << subsystem_summary_data::csvHeadersPartTwo();
                            o << subsystem_summary_data::csvHeadersPartOne("non-participating ");
                            o << subsystem_summary_data::csvHeadersPartTwo("non-participating ");
                        }
                        o << ",service time true IOPS histogram:";
                        for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                        o << ",service time histogram normalized by step total IOPS:";
                        for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                        o << ",service time histogram scaled by bucket width:";
                        for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                        o << ",service time histogram scaled by bucket width and normalized by step total IOPS:";
                        for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);

                        o << ",random read service time true IOPS histogram:";
                        for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                        o << ",random write service time true IOPS histogram:";
                        for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                        o << ",sequential read service time true IOPS histogram:";
                        for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                        o << ",sequential write service time true IOPS histogram:";
                        for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                        o << std::endl;

                        if (need_header) fileappend( m_s.measurement_rollup_by_test_step_csv_filename, o.str() );
                        if (need_type_header) fileappend( m_s.measurement_rollup_by_test_step_csv_type_filename, o.str() );
                    }
                }

                if (EVALUATE_SUBINTERVAL_SUCCESS == m_s.lastEvaluateSubintervalReturnCode || m_s.have_timeout_rollup)
                {
                    std::ostringstream csvline;

                    csvline << m_s.testName << ',' << m_s.stepNNNN << ',' << m_s.stepName;

                    ivytime warmup_start    = m_s.rollups.starting_ending_times[0].first;
                    ivytime warmup_complete = m_s.rollups.starting_ending_times[first-1].second;
                    ivytime warmup_duration = warmup_complete - warmup_start;

                    ivytime start    = m_s.rollups.starting_ending_times[first].first;
                    ivytime duration = m_s.rollups.starting_ending_times[last].second - start;

                    ivytime cooldown_start = m_s.rollups.starting_ending_times[last+1].first;
                    ivytime cooldown_complete = m_s.rollups.starting_ending_times[ m_s.rollups.starting_ending_times.size() - 1 ].second;
                    ivytime cooldown_duration = cooldown_complete - cooldown_start;

                    csvline << ',' << start.format_as_datetime();
                    csvline << ',' << warmup_duration.format_as_duration_HMMSS();
                    csvline << ',' << duration.format_as_duration_HMMSS();
                    csvline << ',' << cooldown_duration.format_as_duration_HMMSS();
                    csvline << ','; // Write Pending only shows in by-subinterval csv lines
                    csvline << ',';
                    if ( m_s.have_timeout_rollup || !m_s.rollups.passesDataVariationValidation().first)
                    {
                        csvline << "invalid"; // subintervalIndex;
                    }
                    else
                    {
                        csvline << "valid"; // subintervalIndex;
                    }

                    std::string validation_errors {};

                    if (m_s.have_timeout_rollup) validation_errors = "[measurement timeout]";

                    validation_errors += m_s.rollups.passesDataVariationValidation().second;

                    csvline << ',' << validation_errors;

                    csvline << ',' << pRollupType->attributeNameCombo.attributeNameComboID;
                    csvline << ',' << pRollupInstance->rollupInstanceID ; // rollupInstanceID;

                    if (m_s.ivymaster_RMLIB_threads.size()>0) { csvline << pRollupInstance->test_config_thumbnail.csv_columns(); }

                    csvline << pRollupInstance->measurementRollup.inputRollup.CSVcolumnValues(true); // true shows how many occurences of each value, false shows "multiple"

                    csvline << ','; // CPU - 2 columns
                    if (stringCaseInsensitiveEquality(std::string("host"),pRollupType->attributeNameCombo.attributeNameComboID))
                    {
                        csvline << m_s.measurement_rollup_CPU.csvValues(toLower(pRollupInstance->rollupInstanceID));
                    }
                    else
                    {
                        csvline << m_s.measurement_rollup_CPU.csvValuesAvgOverHosts();
                    }

                    if (m_s.ivymaster_RMLIB_threads.size()>0)
                    {
                        csvline << pRollupInstance->measurement_subsystem_data.csvValuesPartOne( m_s.rollups.measurement_last_index + 1 - m_s.rollups.measurement_first_index );

                        unsigned int overall_drive_count = pRollupInstance->test_config_thumbnail.total_drives();

                        if (overall_drive_count == 0)
                        {
                            csvline << ",,,";
                        }
                        else
                        {
                            RunningStat<ivy_float,ivy_int> st = pRollupInstance->measurementRollup.outputRollup.overall_service_time_RS();
                            RunningStat<ivy_float,ivy_int> bt = pRollupInstance->measurementRollup.outputRollup.overall_bytes_transferred_RS();

                            ivy_float secs=pRollupInstance->measurementRollup.durationSeconds();
                            ivy_float iops_per_drive, decMBps_per_drive;

                            iops_per_drive = (((ivy_float) st.count()) / secs ) / ((ivy_float) overall_drive_count);
                            decMBps_per_drive = 1e-6 * (bt.sum()/secs)/((ivy_float) overall_drive_count);
                            csvline << "," << std::fixed << std::setprecision(1) <<  iops_per_drive;
                            csvline << "," << std::fixed << std::setprecision(1) <<  decMBps_per_drive;

                            //, application service time Littles law q depth per drive
                            csvline << "," << std::fixed << std::setprecision(1) << (st.avg()*iops_per_drive);
                        }
                    }

                        {
                            ivy_float seconds = pRollupInstance->measurementRollup.durationSeconds();

                            if (m_s.ivymaster_RMLIB_threads.size()>0)
                            {
                                // print the comparisions between application & subsystem IOPS, MB/s, service time

                                ivy_float subsystem_IOPS = pRollupInstance->measurement_subsystem_data.IOPS();
                                ivy_float subsystem_decimal_MB_per_second = pRollupInstance->measurement_subsystem_data.decimal_MB_per_second();
                                ivy_float subsystem_service_time_ms = pRollupInstance->measurement_subsystem_data.service_time_ms();

                                // ",Subsystem IOPS as % of application IOPS,Subsystem MB/s as % of application MB/s,Subsystem service time as % of application service time,Path latency = application minus subsystem service time (ms)";

                                csvline << ','; // subsystem IOPS as % of application IOPS
                                if (subsystem_IOPS > 0.0)
                                {
                                    RunningStat<ivy_float,ivy_int> st = pRollupInstance->measurementRollup.outputRollup.overall_service_time_RS();
                                    ivy_float application_IOPS = st.count() / seconds;
                                    if (application_IOPS > 0.0)
                                    {
                                        csvline << std::fixed << std::setprecision(3) << (subsystem_IOPS / application_IOPS) * 100.0 << '%';
                                    }
                                }

                                csvline << ','; // Subsystem MB/s as % of application MB/s
                                if (subsystem_decimal_MB_per_second > 0.0)
                                {
                                    RunningStat<ivy_float,ivy_int> bt = pRollupInstance->measurementRollup.outputRollup.overall_bytes_transferred_RS();
                                    ivy_float application_decimal_MB_per_second = 1E-6 * bt.sum() / seconds;
                                    if (application_decimal_MB_per_second > 0.0)
                                    {
                                        csvline << std::fixed << std::setprecision(3) << (subsystem_decimal_MB_per_second / application_decimal_MB_per_second) * 100.0 << '%';
                                    }
                                }

                                csvline << ','; // Subsystem service time as % of application service time
                                if (subsystem_service_time_ms > 0.0)
                                {
                                    RunningStat<ivy_float,ivy_int> st = pRollupInstance->measurementRollup.outputRollup.overall_service_time_RS();
                                    ivy_float application_service_time_ms = 1000.* st.avg();
                                    if (application_service_time_ms > 0.0)
                                    {
                                        csvline << std::fixed << std::setprecision(3) << (subsystem_service_time_ms / application_service_time_ms) * 100.0 << '%';
                                    }
                                }

                                csvline << ','; // Path latency = application minus subsystem service time (ms)
                                if (subsystem_service_time_ms > 0.0)
                                {
                                    RunningStat<ivy_float,ivy_int> st = pRollupInstance->measurementRollup.outputRollup.overall_service_time_RS();
                                    ivy_float application_service_time_ms = 1000. * st.avg();
                                    if (application_service_time_ms > 0.0)
                                    {
                                        csvline << std::fixed << std::setprecision(3) << (application_service_time_ms - subsystem_service_time_ms);
                                    }
                                }
                            }

                            csvline << "," << m_s.non_random_sample_correction_factor;
                            csvline << "," << plus_minus_series_confidence_default;

                            csvline << pRollupInstance->measurementRollup.outputRollup.csvValues(seconds,&(pRollupInstance->measurementRollup),m_s.non_random_sample_correction_factor);

                            if (m_s.ivymaster_RMLIB_threads.size() > 0)
                            {
                                csvline << pRollupInstance->measurement_subsystem_data.csvValuesPartTwo( m_s.rollups.measurement_last_index + 1 - m_s.rollups.measurement_first_index );
                                csvline << m_s.rollups.not_participating_measurement.csvValuesPartOne(   m_s.rollups.measurement_last_index + 1 - m_s.rollups.measurement_first_index );
                                csvline << m_s.rollups.not_participating_measurement.csvValuesPartTwo();
                            }

                            // histogram_s
                            {
                                RunningStat<ivy_float,ivy_int> st = pRollupInstance->measurementRollup.outputRollup.overall_service_time_RS();
                                ivy_float total_IOPS = st.count() / seconds;

                                // ",service time true IOPS histogram:";
                                csvline << "," << m_s.stepName;
                                for (int i=0; i < io_time_buckets; i++)
                                {
                                    csvline << ',';
                                    RunningStat<ivy_float,ivy_int>
                                    rs  = pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[0][0][i];
                                    rs += pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[0][1][i];
                                    rs += pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[1][0][i];
                                    rs += pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[1][1][i];
                                    if (rs.count()>0) csvline << ((ivy_float) rs.count() / seconds);
                                }

                                // ",service time histogram normalized by step total IOPS:";
                                csvline << "," << m_s.stepName;
                                for (int i=0; i < io_time_buckets; i++)
                                {
                                    csvline << ',';

                                    if (total_IOPS > 0.)
                                    {
                                        RunningStat<ivy_float,ivy_int>
                                        rs  = pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[0][0][i];
                                        rs += pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[0][1][i];
                                        rs += pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[1][0][i];
                                        rs += pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[1][1][i];
                                        if (rs.count()>0) csvline << ( ((ivy_float) rs.count() / seconds) / total_IOPS);
                                    }
                                }
                                // ",service time histogram scaled by bucket width:";

                                csvline << "," << m_s.stepName;
                                for (int i=0; i < io_time_buckets; i++)
                                {
                                    csvline << ',';
                                    RunningStat<ivy_float,ivy_int>
                                    rs  = pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[0][0][i];
                                    rs += pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[0][1][i];
                                    rs += pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[1][0][i];
                                    rs += pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[1][1][i];
                                    if (rs.count()>0) csvline << (histogram_bucket_scale_factor(i) * (ivy_float) rs.count() / seconds);
                                }

                                // ",service time histogram scaled by bucket width and normalized by step total IOPS:";
                                csvline << "," << m_s.stepName;
                                for (int i=0; i < io_time_buckets; i++)
                                {
                                    csvline << ',';

                                    if (total_IOPS > 0.)
                                    {
                                        RunningStat<ivy_float,ivy_int>
                                        rs  = pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[0][0][i];
                                        rs += pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[0][1][i];
                                        rs += pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[1][0][i];
                                        rs += pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[1][1][i];
                                        if (rs.count()>0) csvline << ( (histogram_bucket_scale_factor(i) * (ivy_float) rs.count() / seconds) / total_IOPS);
                                    }
                                }
                            }


                            // ",random read service time true IOPS histogram:";
                            csvline << "," << m_s.stepName;
                            for (int i=0; i < io_time_buckets; i++)
                            {
                                csvline << ',';
                                RunningStat<ivy_float,ivy_int> rs = pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[0][0][i];
                                if (rs.count()>0) csvline << (histogram_bucket_scale_factor(i) * (ivy_float) rs.count() / seconds);
                            }

                            // ",random write service time true IOPS histogram:";
                            csvline << "," << m_s.stepName;
                            for (int i=0; i < io_time_buckets; i++)
                            {
                                csvline << ',';
                                RunningStat<ivy_float,ivy_int> rs = pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[0][1][i];
                                if (rs.count()>0) csvline << (histogram_bucket_scale_factor(i) * (ivy_float) rs.count() / seconds);
                            }

                            // ",sequential read service time true IOPS histogram:";
                            csvline << "," << m_s.stepName;
                            for (int i=0; i < io_time_buckets; i++)
                            {
                                csvline << ',';
                                RunningStat<ivy_float,ivy_int> rs = pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[1][0][i];
                                if (rs.count()>0) csvline << (histogram_bucket_scale_factor(i) * (ivy_float) rs.count() / seconds);
                            }

                            // ",sequential write service time true IOPS histogram:";
                            csvline << "," << m_s.stepName;
                            for (int i=0; i < io_time_buckets; i++)
                            {
                                csvline << ',';
                                RunningStat<ivy_float,ivy_int> rs = pRollupInstance->measurementRollup.outputRollup.u.a.service_time.rs_array[1][1][i];
                                if (rs.count()>0) csvline << (histogram_bucket_scale_factor(i) * (ivy_float) rs.count() / seconds);
                            }

                        }


                    {
                        std::ostringstream o; o << quote_wrap_csvline_except_numbers(csvline.str()) << std::endl;
                        fileappend(m_s.measurement_rollup_by_test_step_csv_filename,o.str());
                        fileappend(m_s.measurement_rollup_by_test_step_csv_type_filename,o.str());
                    }

                    if (0 == std::string("all").compare(pRollupInstance->attributeNameComboID)
                            && 0 == std::string("all").compare(pRollupInstance->rollupInstanceID)    )
                    {
                        std::ostringstream o;

                        o << "Measurement summary for \"all=all\" rollup: " << pRollupInstance->measurementRollup.outputRollup.thumbnail(pRollupInstance->measurementRollup.durationSeconds()) << std::endl;

                        std::cout << o.str();
                        log(m_s.masterlogfile,o.str());
                    }
                }
                else
                {
                    // failed  to declare success with a measurement

                    std::ostringstream csvline;

                    csvline << m_s.testName << ',' << m_s.stepNNNN << ',' << m_s.stepName;
                    csvline << ',' ; // << start.format_as_datetime();

                    ivytime warmup_start, warmup_complete, warmup_duration, cooldown_start, cooldown_complete, cooldown_duration;
                    if (-1 == m_s.measureDFC_failure_point)
                    {
                        warmup_start = m_s.rollups.starting_ending_times[0].first;
                        warmup_complete = m_s.rollups.starting_ending_times[m_s.rollups.starting_ending_times.size() - 1 ].second;
                        warmup_duration = warmup_complete - warmup_start;
                        cooldown_duration = ivytime(0);
                    }
                    else
                    {
                        warmup_start = m_s.rollups.starting_ending_times[0].first;
                        warmup_complete = m_s.rollups.starting_ending_times[m_s.measureDFC_failure_point].second;
                        warmup_duration = warmup_complete - warmup_start;
                        if (warmup_complete == m_s.rollups.starting_ending_times[m_s.rollups.starting_ending_times.size()-1].second)
                        {
                            cooldown_duration = ivytime(0);
                        }
                        else
                        {
                            cooldown_start = m_s.rollups.starting_ending_times[m_s.measureDFC_failure_point].second;
                            cooldown_complete = m_s.rollups.starting_ending_times[m_s.rollups.starting_ending_times.size()-1].second;
                            cooldown_duration = cooldown_complete - cooldown_start;
                        }
                    }

                    csvline << ',' << warmup_duration.format_as_duration_HMMSSns();
                    csvline << ',' ; // << duration.format_as_duration_HMMSSns();
                    csvline << ',' << cooldown_duration.format_as_duration_HMMSSns();
                    csvline << ',' ; // Write Pending only shows in by-subinterval csv lines
                    csvline << ',' << "failed - timed out"; // subintervalIndex;
                    csvline << ',' << "no measurement"; // phase
                    csvline << ',' << pRollupType->attributeNameCombo.attributeNameComboID;
                    csvline << ',' << pRollupInstance->rollupInstanceID ; // rollupInstanceID;

                    {
                        std::ostringstream o; o << quote_wrap_csvline_except_numbers(csvline.str()) << std::endl;
                        fileappend(m_s.measurement_rollup_by_test_step_csv_filename,o.str());
                    }

                }
            }

            // For stepFolder for this RollupInstance
            {
                // Make the RollupInstance's RollupType subfolder if needed in stepFolder

                // For each csv file data line format {measurement_summary, IOPS_MBpS_service_response_blocksize}
                // Write the csv header line if necessary
                // Write the csv data line

                m_s.stepRollupFolder = m_s.stepFolder + std::string("/") + rollupTypeName;

                if(0==stat(m_s.stepRollupFolder.c_str(),&struct_stat))
                {
                    // folder name already exists
                    if(!S_ISDIR(struct_stat.st_mode))
                    {
                        std::ostringstream o;
                        o << std::endl << "writhe and fail, gasping step rollup folder \"" << m_s.stepRollupFolder << "\" exists but is not a director.   Uuuugg." << std::endl;
                        log(m_s.masterlogfile,o.str());
                        m_s.kill_subthreads_and_exit();
                        exit(-1);
                    }
                }
                else if (mkdir(m_s.stepRollupFolder.c_str(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
                {
                    std::ostringstream o;
                    o << std::endl << "writhe and fail, gasping couldn\'t make step rollup folder \"" << m_s.stepRollupFolder << "\".   Uuuugg." << std::endl;
                    log(m_s.masterlogfile,o.str());
                    m_s.kill_subthreads_and_exit();
                    exit(-1);
                }


                // For each by by-subinterval csv file in this step folder
                // Write the csv header line if necessary
                // Write the csv data line for the measurement rollup
                {
                    std::string instanceFilename = rollupInstanceName;
                    std::replace(instanceFilename.begin(),instanceFilename.end(),'/','_');
                    std::replace(instanceFilename.begin(),instanceFilename.end(),'\\','_');

                    std::ostringstream filename_prefix;
                    filename_prefix << m_s.stepRollupFolder << "/"
                                    << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(m_s.testName)
                                    << '.'
                                    << m_s.stepNNNN
                                    << '.'
                                    << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(m_s.stepName)
                                    << '.'
                                    << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(rollupTypeName)
                                    << '='
                                    << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(instanceFilename)
                                    << '.';


                    m_s.step_detail_by_subinterval_filename = filename_prefix.str() + std::string("csv");

                    // print the header line.
                    {
                        std::ostringstream o;
                        o << "Test Name,Step Number,Step Name,Start,Warmup,Duration,Cooldown,Write Pending,Subinterval Number,Phase,Rollup Type,Rollup Instance";
                        if (m_s.ivymaster_RMLIB_threads.size()>0) { o << Test_config_thumbnail::csv_headers(); }
                        o << IosequencerInputRollup::CSVcolumnTitles();
                        o << avgcpubusypercent::csvTitles();

                        if (m_s.ivymaster_RMLIB_threads.size()>0)
                        {
                            o << subsystem_summary_data::csvHeadersPartOne();
                            o << ",host IOPS per drive,host decimal MB/s per drive,application service time Littles law q depth per drive";
                            o << ",Subsystem IOPS as % of application IOPS,Subsystem MB/s as % of application MB/s,Subsystem service time as % of application service time,Path latency = application minus subsystem service time (ms)";
                        }

                        o << SubintervalOutput::csvTitles();

                        if (m_s.ivymaster_RMLIB_threads.size()>0)
                        {
                            o << subsystem_summary_data::csvHeadersPartTwo();
                            o << subsystem_summary_data::csvHeadersPartOne( "non-participating ");
                            o << subsystem_summary_data::csvHeadersPartTwo( "non-participating ");
                        }

                        o << ",IOPS histogram by service time (ms):";
                        for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                        o << ",IOPS histogram by random read service time (ms):";
                        for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                        o << ",IOPS histogram by random write service time (ms):";
                        for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                        o << ",IOPS histogram by sequential read service time (ms):";
                        for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                        o << ",IOPS histogram by sequential write service time (ms):";
                        for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);

                        o << std::endl;
                        fileappend( m_s.step_detail_by_subinterval_filename, o.str() );
                    }

                    // print the detail lines for each subinterval
                    for (unsigned int i = 0; i < (pRollupInstance->subintervals.sequence).size(); i++)
                    {
                        {
                            std::ostringstream o;
                            o << pRollupInstance->test_config_thumbnail;
                            ofstream ofs(filename_prefix.str() + std::string("config_thumbnail.txt"));
                            ofs << o.str();
                            ofs.close();

                        }
                        {
                            std::ostringstream csvline;

                            csvline << m_s.testName << ',' << m_s.stepNNNN << ',' << m_s.stepName;

                            ivytime start, duration;
                            start = m_s.rollups.starting_ending_times[i].first;
                            duration = m_s.rollups.starting_ending_times[i].second - start;

                            csvline << ',' << start.format_as_datetime()
                                    << ',' // warmup duration
                                    << ',' << duration.format_as_duration_HMMSSns()
                                    << ',' // cooldown duration
                                    << ',' ; // beginning of the Write Pending field

                            if (m_s.cooldown_WP_watch_set.size())
                            {
                                try
                                {
                                    csvline << m_s.getWPthumbnail(i);
                                }
                                catch (std::invalid_argument& iae)
                                {
                                    std::ostringstream o;
                                    o << std::endl << "writhe and fail, gasping that when printing csv line, when getting Write Pending thumbnail, failed saying - " << iae.what() << std::endl;
                                    std::cout << o.str();
                                    log(m_s.masterlogfile,o.str());
                                    m_s.kill_subthreads_and_exit();
                                    exit(-1);
                                }
                            }

                            csvline << ',' << i;

                            {
                                csvline << ',';
                                if ( EVALUATE_SUBINTERVAL_SUCCESS == m_s.lastEvaluateSubintervalReturnCode )
                                {
                                    if ( i < p_DynamicFeedbackController->firstMeasurementSubintervalIndex() )
                                        csvline << "warmup";
                                    else if (i > p_DynamicFeedbackController->lastMeasurementSubintervalIndex() )
                                        csvline << "cooldown";
                                    else
                                        csvline << "measure";
                                }
                                else
                                {
                                    if (-1 == m_s.measureDFC_failure_point || ((int)i) <= m_s.measureDFC_failure_point) csvline << "warmup"
                                                ;
                                    else csvline << "cooldown";
                                }
                            }

                            csvline << ',' << pRollupType->attributeNameCombo.attributeNameComboID;

                            csvline << ',' << pRollupInstance->rollupInstanceID;

                            if (m_s.ivymaster_RMLIB_threads.size()>0) { csvline << pRollupInstance->test_config_thumbnail.csv_columns(); }

                            csvline << pRollupInstance->subintervals.sequence[i].inputRollup.CSVcolumnValues(true); // true shows how many occurences of each value, false shows "multiple"

                            {
                                csvline << ','; // host CPU % busy - cols

                                if (stringCaseInsensitiveEquality(std::string("host"),pRollupType->attributeNameCombo.attributeNameComboID))
                                {
                                    csvline << m_s.cpu_by_subinterval[i].csvValues(toLower(pRollupInstance->rollupInstanceID));
                                }
                                else
                                {
                                    csvline << m_s.cpu_by_subinterval[i].csvValuesAvgOverHosts();
                                }
                            }

                            ivy_float seconds = pRollupInstance->subintervals.sequence[m_s.rollups.current_index()].durationSeconds();

                            if (m_s.ivymaster_RMLIB_threads.size() > 0)
                            {
                                csvline << pRollupInstance->subsystem_data_by_subinterval[i].csvValuesPartOne(1);


                                unsigned int overall_drive_count = pRollupInstance->test_config_thumbnail.total_drives();

                                if (overall_drive_count == 0)
                                {
                                    csvline << ",,,";
                                }
                                else
                                {
                                    RunningStat<ivy_float,ivy_int> st = pRollupInstance->subintervals.sequence[i].outputRollup.overall_service_time_RS();
                                    RunningStat<ivy_float,ivy_int> bt = pRollupInstance->subintervals.sequence[i].outputRollup.overall_bytes_transferred_RS();
                                    ivy_float iops_per_drive = (st.count()/seconds) /  ((ivy_float) overall_drive_count);
                                    csvline << "," <<  iops_per_drive;
                                    csvline << "," <<  (   1e-6 /* decimal MB/s */ * (bt.sum()/seconds)   /   ((ivy_float) overall_drive_count)     );

                                    //, host service time Littles law q depth per drive
                                    csvline << "," << std::fixed << std::setprecision(1) << (st.avg()*iops_per_drive);
                                }

                                {
                                    // print the comparisions between application & subsystem IOPS, MB/s, service time

                                    ivy_float subsystem_IOPS = pRollupInstance->subsystem_data_by_subinterval[i].IOPS();
                                    ivy_float subsystem_decimal_MB_per_second = pRollupInstance->subsystem_data_by_subinterval[i].decimal_MB_per_second();
                                    ivy_float subsystem_service_time_ms = pRollupInstance->subsystem_data_by_subinterval[i].service_time_ms();

                                    // ",Subsystem IOPS as % of application IOPS,Subsystem MB/s as % of application MB/s,Subsystem service time as % of application service time,Path latency = application minus subsystem service time (ms)";

                                    csvline << ','; // subsystem IOPS as % of application IOPS
                                    if (subsystem_IOPS > 0.0)
                                    {
                                        RunningStat<ivy_float,ivy_int> st = pRollupInstance->subintervals.sequence[i].outputRollup.overall_service_time_RS();
                                        ivy_float application_IOPS = st.count() / seconds;
                                        if (application_IOPS > 0.0)
                                        {
                                            csvline << std::fixed << std::setprecision(2) << (subsystem_IOPS / application_IOPS) * 100.0 << '%';
                                        }
                                    }

                                    csvline << ','; // Subsystem MB/s as % of application MB/s
                                    if (subsystem_decimal_MB_per_second > 0.0)
                                    {
                                        RunningStat<ivy_float,ivy_int> bt = pRollupInstance->subintervals.sequence[i].outputRollup.overall_bytes_transferred_RS();
                                        ivy_float application_decimal_MB_per_second = 1E-6 * bt.sum() / seconds;
                                        if (application_decimal_MB_per_second > 0.0)
                                        {
                                            csvline << std::fixed << std::setprecision(2) << (subsystem_decimal_MB_per_second / application_decimal_MB_per_second) * 100.0 << '%';
                                        }
                                    }

                                    csvline << ','; // Subsystem service time as % of application service time
                                    if (subsystem_service_time_ms > 0.0)
                                    {
                                        RunningStat<ivy_float,ivy_int> st = pRollupInstance->subintervals.sequence[i].outputRollup.overall_service_time_RS();
                                        ivy_float application_service_time_ms = 1000.* st.avg();
                                        if (application_service_time_ms > 0.0)
                                        {
                                            csvline << std::fixed << std::setprecision(2) << (subsystem_service_time_ms / application_service_time_ms) * 100.0 << '%';
                                        }
                                    }

                                    csvline << ','; // Path latency = application minus subsystem service time (ms)
                                    if (subsystem_service_time_ms > 0.0)
                                    {
                                        RunningStat<ivy_float,ivy_int> st = pRollupInstance->subintervals.sequence[i].outputRollup.overall_service_time_RS();
                                        ivy_float application_service_time_ms = 1000. * st.avg();
                                        if (application_service_time_ms > 0.0)
                                        {
                                            csvline << std::fixed << std::setprecision(2) << (application_service_time_ms - subsystem_service_time_ms);
                                        }
                                    }
                                }
                            }

                            csvline << pRollupInstance->subintervals.sequence[i].outputRollup.csvValues( seconds );

                            if (m_s.ivymaster_RMLIB_threads.size() > 0)
                            {
                                csvline << pRollupInstance->subsystem_data_by_subinterval[i].csvValuesPartTwo();
                                csvline << m_s.rollups.not_participating[i].csvValuesPartOne();
                                csvline << m_s.rollups.not_participating[i].csvValuesPartTwo();
                            }

                            csvline << ",subinterval " << i;
                            for (int bucket=0; bucket < io_time_buckets; bucket++)
                            {
                                csvline << ',';
                                RunningStat<ivy_float,ivy_int>
                                rs  = pRollupInstance->subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[0][0][bucket];
                                rs += pRollupInstance->subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[0][1][bucket];
                                rs += pRollupInstance->subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[1][0][bucket];
                                rs += pRollupInstance->subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[1][1][bucket];
                                if (rs.count()>0) csvline << (histogram_bucket_scale_factor(bucket) * (ivy_float) rs.count() / seconds);
                            }

                            csvline << ",subinterval " << i;
                            for (int bucket=0; bucket < io_time_buckets; bucket++)
                            {
                                csvline << ',';
                                RunningStat<ivy_float,ivy_int> rs = pRollupInstance->subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[0][0][bucket];
                                if (rs.count()>0) csvline << (histogram_bucket_scale_factor(bucket) * (ivy_float) rs.count() / seconds);
                            }

                            csvline << ",subinterval " << i;
                            for (int bucket=0; bucket < io_time_buckets; bucket++)
                            {
                                csvline << ',';
                                RunningStat<ivy_float,ivy_int> rs = pRollupInstance->subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[0][1][bucket];
                                if (rs.count()>0) csvline << (histogram_bucket_scale_factor(bucket) * (ivy_float) rs.count() / seconds);
                            }

                            csvline << ",subinterval " << i;
                            for (int bucket=0; bucket < io_time_buckets; bucket++)
                            {
                                csvline << ',';
                                RunningStat<ivy_float,ivy_int> rs = pRollupInstance->subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[1][0][bucket];
                                if (rs.count()>0) csvline << (histogram_bucket_scale_factor(bucket) * (ivy_float) rs.count() / seconds);
                            }

                            csvline << ",subinterval " << i;
                            for (int bucket=0; bucket < io_time_buckets; bucket++)
                            {
                                csvline << ',';
                                RunningStat<ivy_float,ivy_int> rs = pRollupInstance->subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[1][1][bucket];
                                if (rs.count()>0) csvline << (histogram_bucket_scale_factor(bucket) * (ivy_float) rs.count() / seconds);
                            }

                            {
                                std::ostringstream o; o << quote_wrap_csvline_except_numbers(csvline.str()) << std::endl;
                                fileappend(m_s.step_detail_by_subinterval_filename,o.str());
                            }
                        }
                    }
                }
            }
        }
    }

    if (m_s.have_pid)
    {
        if (m_s.p_focus_rollup == nullptr) throw std::runtime_error("<Error> internal programming error - run_subinterval_sequence() - have_pid when printing pid csv files, but m_s.p_focus_rollup == nullptr.");

        for (auto& pear : m_s.p_focus_rollup->instances)
        {
            pear.second->print_pid_csv_files();
        }
    }

    std::ostringstream m45d;
    m45d << m_s.stepNNNN;
    if (m_s.stepName != m_s.stepNNNN) m45d << " " << m_s.stepName;
    m45d << " csv file print now complete." << std::endl << std::endl;
    std::cout << m45d.str();
    log(m_s.masterlogfile, m45d.str());

    // post "Catch in flight I/Os" command to wait for in-flight I/Os at end of last subinterval to finish.

    if (trace_evaluate){
        std::ostringstream o;
        o << "Going to post \"Catch in flight I/Os\" command to each host." << std::endl;
        log(m_s.masterlogfile,o.str());
    }

    ivytime stopstartivytime;
    stopstartivytime.setToNow();

    for (auto& pear : m_s.host_subthread_pointers)
    {
        {
            std::unique_lock<std::mutex> u_lk(pear.second->master_slave_lk);
            pear.second->commandString = std::string("Catch in flight I/Os");
            pear.second->command=true;
            pear.second->commandComplete=false;
            pear.second->commandSuccess=false;
            pear.second->commandErrorMessage.clear();

//*debug*/{ std::ostringstream o; o << "Posted \"Catch in flight I/Os\" to " << pear.second->ivyscript_hostname << std::endl; log(m_s.masterlogfile,o.str()); }
        }
        pear.second->master_slave_cv.notify_all();
    }

    for (auto& pear : m_s.host_subthread_pointers)
    {
        {
            std::unique_lock<std::mutex> u_lk(pear.second->master_slave_lk);
            while (!pear.second->commandComplete) pear.second->master_slave_cv.wait(u_lk);
            if (!pear.second->commandSuccess)
            {
                    std::ostringstream o;
                    o << "When waiting for \"Catch in flight I/Os\" to complete after the final subinterval, subthread for "
                        << pear.first << " posted an error, saying "  << pear.second->commandErrorMessage << std::endl;
                    std::cout << o.str();
                    log (m_s.masterlogfile,o.str());
                    m_s.kill_subthreads_and_exit();
            }
        }
    }

    ivytime stopendivytime;
    stopendivytime.setToNow();
    ivytime stopdur = stopendivytime - stopstartivytime;

    /*debug*/
    {
        std::ostringstream o;
        o << "\"Catch in flight I/Os\" command to wait for in-flight I/Os at end of last subinterval complete, duration =  " << stopdur.format_as_duration_HMMSSns()
            << ".  run_subinterval_sequence() complete." << std::endl;
        log(m_s.masterlogfile,o.str());
    }

    return;
}


