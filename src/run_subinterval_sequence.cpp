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

#include <sys/stat.h>
#include <chrono>
#include <ctime>

#include "ivy_engine.h"
#include "MeasureController.h"
#include "RollupInstance.h"

extern bool routine_logging;
extern bool trace_lexer;
extern bool trace_parser;
extern bool trace_evaluate;
extern bool spinloop;

extern void say_and_log(const std::string& s);

//#define TRACE_RSS

//========================================================================================
//
// NOTE: Open up doc/run_subinterval_sequence_outline.txt along side, and keep it updated.
//
//========================================================================================

void process_completed_measurement()
{
    measurement& m = m_s.current_measurement();

    for (int i = m.first_subinterval; i <= m.last_subinterval; i++)
    {
        m_s.measurement_by_subinterval.push_back(m_s.measurements.size() -1);
    }

    auto retval = m_s.rollups.makeMeasurementRollup();
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

    retval = m.make_measurement_rollup_CPU();
    if (!retval.first)
    {
        std::ostringstream o;
        o << std::endl << "writhe and fail, gasping that ivy_engine::make_measurement_rollup_CPU()\'s last words were - " << retval.second << std::endl
            << std::endl << "Source code reference: function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
        std::cout << o.str();
        log(m_s.masterlogfile,o.str());
        m_s.kill_subthreads_and_exit();
        exit(-1);
    }
}

bool process_successful_measurement() // returns true if we have started the next measurement, false if we are done and the subinterval sequence is ending.
{
    {
        std::ostringstream o;
        o << "Successful valid measurement, either fixed duration, or using the \"measure\" feature." << std::endl;
        std::cout << o.str();
        log(m_s.masterlogfile,o.str());
    }

    measurement& m = m_s.current_measurement();

    m.success = true;
    process_completed_measurement();

    return m_s.start_new_measurement();
}

void process_failed_measurement()
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

    m_s.current_measurement().firstMeasurementIndex = m_s.p_focus_rollup->p_wurst_RollupInstance->best_first;
    m_s.current_measurement().lastMeasurementIndex  = m_s.p_focus_rollup->p_wurst_RollupInstance->best_last;
    m_s.current_measurement().have_timeout_rollup = true;

    measurement& m = m_s.current_measurement();

    m.success = false;
    process_completed_measurement();
}

void run_subinterval_sequence(MeasureController* p_MeasureController)
{
    {
        std::ostringstream o;
        o << "run_subinterval_sequence() starting for step number " << m_s.stepNNNN << " name " << m_s.stepName << std::endl;
        log(m_s.masterlogfile,o.str());
    }

    // We make a test step subfolder in the overall test output folder

    m_s.stepFolder = m_s.testFolder + std::string("/") + m_s.stepNNNN + std::string(".") + m_s.stepName;

    if (m_s.stepcsv && mkdir(m_s.stepFolder.c_str(),S_IRWXU | S_IRWXG | S_IRWXO))
    {
        std::ostringstream o;
        o << std::endl << "writhe and fail, gasping \"Died trying to make test step folder \"" << m_s.stepFolder << "\".   Uuuugg." << std::endl;
        log(m_s.masterlogfile,o.str());
        m_s.kill_subthreads_and_exit();
        exit(-1);
    }

    // First we clean out the existing subinterval sequences in the rollups in case it was still full of stuff from a previous test step.

    m_s.clear_measurements();
    m_s.start_new_measurement();

    m_s.harvesting_subinterval = -1;
    m_s.last_gain_adjustment_subinterval = -1;
    m_s.rollups.resetSubintervalSequence();
    m_s.cpu_by_subinterval.clear();
    m_s.cpu_degrees_C_from_critical_temp_by_subinterval.clear();

    m_s.MeasureCtlr_failure_point = -1;  // where cooldown extends to wait for WP to empty, this is the point where we declared failure and stopped driving I/O.

    m_s.current_measurement().have_timeout_rollup = false;

    m_s.cooldown_by_MP_busy_stay_down_count = 0;

    m_s.min_sequential_fill_progress = 1.0;
    m_s.keep_filling = false;

    m_s.subsystem_thumbnail.clear();

    // Clear out Subsystem data from a possible earlier subinterval sequence,
    // and then make the first gather for t=0, recording how long it takes.

    m_s.protocolTimeSeconds.clear();
    m_s.hostSendupTimeSeconds.clear();
    m_s.sendupTimeSeconds.clear();
    m_s.centralProcessingTimeSeconds.clear();
    m_s.gatherBeforeSubintervalEndSeconds.clear();
    m_s.earliest_gather_before_subinterval_end = 0.0;
    m_s.overallGatherTimeSeconds.clear();
    m_s.cruiseSeconds.clear();
    m_s.continueCooldownStopAckSeconds.clear();
    m_s.subinterval_start_end_times.clear();

    m_s.now_doing_suppress_perf_cooldown = false;
    m_s.suppress_perf_cooldown_subinterval_count = 0;

    m_s.measurement_by_subinterval.clear();

    // Check for failed subsystem components
    if (m_s.haveCmdDev & m_s.check_failed_component)
    {
        m_s.have_failed_component = false;
        m_s.failed_component_message.clear();

        for (auto& pear : m_s.command_device_subthread_pointers)
        {
            {
                pipe_driver_subthread*& p_pds = pear.second;

                if (p_pds->pCmdDevLUN == nullptr) continue;

                std::unique_lock<std::mutex> u_lk(p_pds->master_slave_lk);
                p_pds->commandString = std::string("get failed_component");
                p_pds->command=true;
                p_pds->commandComplete=false;
                p_pds->commandSuccess=false;
                p_pds->commandErrorMessage.clear();
            }
            pear.second->master_slave_cv.notify_all();
        }

        std::chrono::system_clock::time_point leftnow {std::chrono::system_clock::now()};

        for (auto& pear : m_s.command_device_subthread_pointers)
        {
            {
                pipe_driver_subthread*& p_pds = pear.second;

                if (p_pds->pCmdDevLUN == nullptr) continue;

                {
                    std::unique_lock<std::mutex> u_lk(p_pds->master_slave_lk);

                    if (
                            p_pds->master_slave_cv.wait_until
                            (
                                u_lk,
                                leftnow + std::chrono::milliseconds(5000),
                                [&p_pds]() { return p_pds->commandComplete; }
                            )
                        )
                    {
                        if (!p_pds->commandSuccess)
                        {
                            m_s.have_failed_component = true;

                            std::ostringstream o;
                            o << "<Error> \"check_failed_component\" = \"on\" and found failed component on subsystem serial " << pear.first << " managed on host " << p_pds->ivyscript_hostname
                                << ", saying \"" << p_pds->commandErrorMessage << "\"." << std::endl;

                            m_s.failed_component_message += o.str();
                            log(m_s.masterlogfile,o.str());
                        }

                        if (routine_logging)
                        {
                            std::ostringstream o;
                            o << "\"check_failed_component\" = \"on\" and subsystem serial " << pear.first << " managed on host " << p_pds->ivyscript_hostname << " does not have a failed component." << std::endl;
                            log(m_s.masterlogfile,o.str());
                        }
                    }
                    else // timeout
                    {
                        std::ostringstream o;
                        o << "Aborting - timeout waiting for pipe_driver_subthread for subsystem " << pear.first << " to check for a failed component." <<  std::endl;
                        std::cout << o.str();
                        log(m_s.masterlogfile,o.str());
                        u_lk.unlock();
                        m_s.kill_subthreads_and_exit();
                        exit ( -1);
                    }
                }
                p_pds->master_slave_cv.notify_all();
            }
            // lock dropped
        }

        if (m_s.have_failed_component)
        {
            std::cout << m_s.failed_component_message;
            m_s.kill_subthreads_and_exit();
        }
    }

    // Restore saved IOPS if a previous step had set IOPS=pause for cooldown

    auto restore_rc = m_s.edit_rollup("all=all","IOPS=restore",true);

    if (!restore_rc.first)
    {
        std::ostringstream o;
        o << "<Error> internal programming error - edit_rollup(\"all=all\",\"IOPS=restore\" failed - trying to restore IOPS after possible previous step cooldown - " << restore_rc.second <<  std::endl;
        std::cout << o.str();
        log(m_s.masterlogfile,o.str());
        m_s.kill_subthreads_and_exit();
        exit ( -1);
    }

    // If we have command devices, we perform a t=0 gather.

    // Most Hitachi RAID metrics take the form of cumulative rollover event counters.
    // The T=0 gather is the baseline for the number of events during a subinterval.
    // The t=0 gather time also informs us where to do the gather in subinterval 0,
    // the gather that will be rolled up at the beginning of subinterval 1.

    // Even when we have the -suppress_perf command line option, we still perform the t=0 gather
    // before I/O starts running, because
    // 1) some config data appears in performance data - ["subsystem"],["subsystem"]
    //    "max_MP_io_buffers" & "total_LDEV_capacity_512_sectors", and this gets
    //    copied to the configuration data from the t=0 gather, and
    // 2) with both -suppress_perf and (cooldown_by_wp and/or cooldown by _MP_busy) we are
    //    going to "fake out" collection while I/O is being gathered, pushing empty
    //    gather_data objects for each subinterval while I/O is running so that all
    //    the data structures have the right number of gathers when we turn gathers
    //    back on after we send the "cooldown" command to ivydriver.  We then run
    //    for at least 2 more cooldown subintervals with real subsystem gathers
    //    turned back on, so that we can keep extending until WP is empty and/or
    //    MP activity has wound down.  The fake gathers will appear to have the same
    //    gather time as the t=0 gather.

    ivytime t0_gather_start, t0_gather_complete, t0_gather_time;

    m_s.doing_t0_gather = true;

    if (m_s.haveCmdDev)
    {
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: About to perform t=0 gather.\n"; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif
        t0_gather_start.setToNow();

        std::chrono::system_clock::time_point leftnow {std::chrono::system_clock::now()};
        int gather_timeout_ms { 1000* ((int)(0.75 * m_s.subinterval_seconds)) };

        for (auto& pear : m_s.subsystems)
        {
            pear.second->gathers.clear(); // vector of GatherData objects

            if (0 == std::string("Hitachi RAID").compare(pear.second->type()))
            {
                Hitachi_RAID_subsystem* pHitachiRAID = (Hitachi_RAID_subsystem*) pear.second;

                pipe_driver_subthread* p_pds = pHitachiRAID->pRMLIBthread;

                if (p_pds == nullptr) continue;

                if (p_pds->pCmdDevLUN == nullptr) continue;

                p_pds->perSubsystemGatherTime.clear();
                p_pds->getCLPRDetailTime.clear();
                p_pds->getMPbusyTime.clear();
                p_pds->getLDEVIOTime.clear();
                p_pds->getPORTIOTime.clear();
                p_pds->getUR_JnlTime.clear();

                {
                    std::unique_lock<std::mutex> u_lk(p_pds->master_slave_lk);
                    p_pds->commandString = std::string("t=0 gather");
                    p_pds->command=true;
                    p_pds->commandComplete=false;
                    p_pds->commandSuccess=false;
                    p_pds->commandErrorMessage.clear();
                    p_pds->gather_scheduled_start_time.setToNow();

                    if (routine_logging)
                    {
                        std::ostringstream o;
                        o << "Posted \"gather\" to thread for subsystem serial " << pHitachiRAID->serial_number << " managed on host " << p_pds->ivyscript_hostname << '.' << std::endl;
                        log(m_s.masterlogfile,o.str());
                    }
                }
                p_pds->master_slave_cv.notify_all();
            }
        }

        for (auto& pear : m_s.command_device_subthread_pointers)
        {
            {
                pipe_driver_subthread*& p_pds = pear.second;

                if (p_pds->pCmdDevLUN == nullptr) continue;

                {
                    std::unique_lock<std::mutex> u_lk(p_pds->master_slave_lk);

                    if (
                            p_pds->master_slave_cv.wait_until
                            (
                                u_lk,
                                leftnow + std::chrono::milliseconds(gather_timeout_ms),
                                [&p_pds]() { return p_pds->commandComplete; }
                            )
                        )
                    {
                        if (!p_pds->commandSuccess)
                        {
                            std::ostringstream o;
                            o << "\"gather\" to thread for subsystem serial " << pear.first << " managed on host " << p_pds->ivyscript_hostname << " failed.  Aborting." << std::endl;
                            log(m_s.masterlogfile,o.str());
                            m_s.kill_subthreads_and_exit();
                            exit(-1);
                        }

                        if (routine_logging)
                        {
                            std::ostringstream o;
                            o << "\"gather\" reported complete with duration " << p_pds->duration.format_as_duration_HMMSSns() << " by thread for subsystem serial " << pear.first << " managed on host " << p_pds->ivyscript_hostname << '.' << std::endl;
                            log(m_s.masterlogfile,o.str());
                        }
                    }
                    else // timeout
                    {
                        std::ostringstream o;
                        o << "Aborting - timeout waiting 3/4 of a subinterval = " << (((ivy_float)gather_timeout_ms) / 1000.0)<< " seconds for pipe_driver_subthread for subsystem " << pear.first << " to complete a gather." <<  std::endl;
                        std::cout << o.str();
                        log(m_s.masterlogfile,o.str());
                        u_lk.unlock();
                        m_s.kill_subthreads_and_exit();
                        exit ( -1);
                    }
                }
                p_pds->master_slave_cv.notify_all();
            }
            // lock dropped
            if (pear.second->duration.getlongdoubleseconds() > m_s.earliest_gather_before_subinterval_end)
            {
                m_s.earliest_gather_before_subinterval_end = pear.second->duration.getlongdoubleseconds();
            }
        }

        t0_gather_complete.setToNow();
        ivytime t0_gather_time {t0_gather_complete - t0_gather_start};

        {
            std::ostringstream o;
            o << std::endl << "t=0 overall gather time over all subsystems was " << std::fixed << std::setprecision(1) << (1000.*t0_gather_time.getlongdoubleseconds()) << " ms - " << m_s.getWPthumbnail(-1)<< std::endl << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile,o.str());
        }

    }
    m_s.doing_t0_gather=false;

    // end of t=0 gather


    if (m_s.have_pid || m_s.have_measure)
    {
        if (m_s.p_focus_rollup == nullptr) throw std::runtime_error("<Error> internal programming error - run_subinterval_sequence() - have_pid and/or have_measure are set, but m_s.p_focus_rollup == nullptr.");

        for (auto& pear : m_s.p_focus_rollup->instances)
        {
            pear.second->reset();
        }
    }

    m_s.in_cooldown_mode = false;

    m_s.rollups.make_step_rollup_subfolders();

    // On the other end, iosequencer threads are waiting to start.  This is because when you first
    // create a workload, it sets up the first two subintervals with the parameters specified,
    // and then when a subinterval sequence ends, once ivydriver stops upon command and sends the last
    // subinterval detail lines, it will set up the first two subintervals to have identical subinterval
    // input parameters to the last completed subinterval.  This is a starting point for any EditWorkload
    // commands that may follow.  EditWorkload is the command that gets sent to ivydriver as a result
    // of an EditRollup at the ivy engine API level.

    // post "Go!" command to start the iosequencer threads running

#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: About to post Go!\n"; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif
    m_s.current_measurement().warmup_start.setToNow();

    m_s.subintervalStart = m_s.current_measurement().warmup_start;
    m_s.subintervalEnd     = m_s.subintervalStart + m_s.subintervalLength;
    m_s.nextSubintervalEnd = m_s.subintervalEnd   + m_s.subintervalLength;

    for (auto& pear : m_s.host_subthread_pointers)
    {
        {
            std::unique_lock<std::mutex> u_lk(pear.second->master_slave_lk);

            std::ostringstream o;
            o << "Go!" << m_s.subintervalLength.toString();
            if (spinloop) { o << "-spinloop"; }
            pear.second->commandString = o.str();
            pear.second->command=true;
            pear.second->commandComplete=false;
            pear.second->commandSuccess=false;
            pear.second->commandErrorMessage.clear();

            {
                std::ostringstream o;
                o << "Posted \"Go!\" for subintervalLength = " << m_s.subintervalLength.toString() << " as <seconds/nanoseconds> "
                    << "to " << pear.second->ivyscript_hostname << std::endl;
                log(m_s.masterlogfile,o.str());
            }
        }
        pear.second->master_slave_cv.notify_all();
    }

    m_s.lastEvaluateSubintervalReturnCode=EVALUATE_SUBINTERVAL_CONTINUE;
    Subinterval_CPU s;

    // Now the iosequencer threads are running the subinterval zero.

    // As soon as they post "Go!" to be a complete success,
    // which is in a few ms, we set a new command "continue".

    // But there was no retrieval of the -1 subinterval ... straight from Go! to continue.

    // retrieve confirmation of the Go! command.

#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: About to get confirmation from pipe_driver_subthread of ivydriver\'s OK for the Go!\n"; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

    for (auto& pear : m_s.host_subthread_pointers)
    {
        {
            std::unique_lock<std::mutex> s_lk(pear.second->master_slave_lk);

            if
            ( ! pear.second->master_slave_cv.wait_for
                (
                    s_lk,
                    std::chrono::milliseconds( (unsigned int)(1000 * m_s.subinterval_seconds)),
                    [&]() { return pear.second->commandComplete; }
                )
            )
            {
                // timed out.

                std::ostringstream o;
                o << "<Error> Timed out waiting to get acknowledgement from ivydriver of Go! - host " << pear.first
                    << " at line " << __LINE__ << " of " << __FILE__ << "." << std::endl;
                log(m_s.masterlogfile,o.str());
                m_s.kill_subthreads_and_exit();
            }

            if (!pear.second->commandSuccess)
            {
                ostringstream o;
                o << "<Error> \"" << pear.second->commandString << "\" failed - " << pear.second->commandErrorMessage;
                log(m_s.masterlogfile,o.str());
                m_s.kill_subthreads_and_exit();
            }
            // Note that we don't reset commandComplete, etc.
        }
        pear.second->master_slave_cv.notify_all();
    }
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: Received confirmations of Go!\n"; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

    // Wait 1/4 of a subinterval
    std::this_thread::sleep_for(std::chrono::milliseconds((unsigned int)(0.25 * 1000 * m_s.subinterval_seconds)));

#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: About to send \"continue\" 1/4 of the way through subinterval zero..\n"; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

    // Send "continue" command
    for (auto& pear : m_s.host_subthread_pointers)
    {
        {
            std::unique_lock<std::mutex> s_lk(pear.second->master_slave_lk);

            if
            ( ! pear.second->master_slave_cv.wait_for
                (
                    s_lk,
                    std::chrono::milliseconds( (unsigned int)(1000 * m_s.subinterval_seconds)),
                    [&]() { return pear.second->commandComplete; }
                )
            )
            {
                // timed out.

                std::ostringstream o;
                o << "<Error> Timed out waiting to get lock to post first continue command - host " << pear.first
                    << " at line " << __LINE__ << " of " << __FILE__ << "." << std::endl;
                log(m_s.masterlogfile,o.str());
                m_s.kill_subthreads_and_exit();
            }

            pear.second->commandString = "continue";
            pear.second->command = true;
            pear.second->commandComplete = false;
            pear.second->commandSuccess = false;
            pear.second->commandErrorMessage.clear();
        }
        pear.second->master_slave_cv.notify_all();
    }

    // Issue a gather timed to finish at the end of the first subinterval (#0)
    // based on how long the t=0 gather took.

    for (auto& pear : m_s.command_device_subthread_pointers)
    {
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: About to issue gather command to " << pear.first << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

        pear.second->gather_scheduled_start_time = m_s.subintervalEnd - ivytime(pear.second->perSubsystemGatherTime.avg());

        {
            std::unique_lock<std::mutex> u_lk(pear.second->master_slave_lk);
            pear.second->commandString = std::string("gather");
            pear.second->command=true;
            pear.second->commandComplete=false;
            pear.second->commandSuccess=false;
            pear.second->commandErrorMessage.clear();
            if (routine_logging)
            {
                std::ostringstream o;
                o << "Posted subinterval zero \"gather\" to thread for subsystem serial " << pear.first << " managed on host " << pear.second->ivyscript_hostname << '.' << std::endl;
                log(m_s.masterlogfile,o.str());
            }
        }
        pear.second->master_slave_cv.notify_all();
    }

    // wait to get until all test hosts confirm receipt of "continue"

    for (auto& pear : m_s.host_subthread_pointers)
    {
        {
            std::unique_lock<std::mutex> s_lk(pear.second->master_slave_lk);

            if
            ( ! pear.second->master_slave_cv.wait_for
                (
                    s_lk,
                    std::chrono::milliseconds( (unsigned int)(1000 * m_s.subinterval_seconds)),
                    [&]() { return pear.second->commandComplete; }
                )
            )
            {
                // timed out.

                std::ostringstream o;
                o << "<Error> Timed out waiting to get acknowledgement from ivydriver of command \"" << pear.second->commandString
                    << "\" - host " << pear.first << std::endl
                    << "Source code reference line " << __LINE__ << " of " << __FILE__ << "." << std::endl;
                log(m_s.masterlogfile,o.str());
                m_s.kill_subthreads_and_exit();
            }

            if (!pear.second->commandSuccess)
            {
                ostringstream o;
                o << "<Error> \"" << pear.second->commandString << "\" failed - " << pear.second->commandErrorMessage << std::endl
                    << "Source code reference line " << __LINE__ << " of " << __FILE__ << "." << std::endl;
                log(m_s.masterlogfile,o.str());
                m_s.kill_subthreads_and_exit();
            }
            pear.second->command=false;
            pear.second->commandComplete=false;
            pear.second->commandSuccess=false;
            pear.second->commandErrorMessage.clear();
        }
        pear.second->master_slave_cv.notify_all();
    }
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: All hosts confirmed receipt of \"continue\" during subinterval zero.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

    // We are approaching the end of subinterval zero.

    while (true) // loop starts where we have received confirmation of continue/stop and are waiting for the end of a subinterval
    {

        m_s.harvesting_subinterval++;
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: Top of loop and about to send out \"get subinterval result\" for subinterval " << m_s.harvesting_subinterval << ".\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

        ivytime now; now.setToNow();

        if (m_s.harvesting_subinterval > 0)
        {
            m_s.subintervalStart   = m_s.subintervalEnd;
            m_s.subintervalEnd     = m_s.nextSubintervalEnd;
            m_s.nextSubintervalEnd = m_s.subintervalEnd + m_s.subintervalLength;
        }

        // send out "get subinterval result"
        for (auto& pear : m_s.host_subthread_pointers)
        {
            {
                if (pear.second->dead)
                {
                    std::ostringstream o;
                    o << "<Error> pipe_driver_subthread for " << pear.first << " was found dead when about to send \"get subinterval result\"." << std::endl;
                    std::cout << o.str();
                    log(m_s.masterlogfile, o.str());
                    m_s.kill_subthreads_and_exit();
                }

                {
                    std::unique_lock<std::mutex> s_lk(pear.second->master_slave_lk);
                    pear.second->command=true;
                    pear.second->commandString = "get subinterval result";
                    pear.second->commandComplete = false;
                    pear.second->commandSuccess = false;
                }
            }
            pear.second->master_slave_cv.notify_all();

        }


        {
            std::ostringstream o;

            o << "--------------------------------------------------------------------------------------------------" << std::endl;

            std::cout << o.str();
            if (routine_logging) log(m_s.masterlogfile,o.str());
        }

        m_s.subinterval_start_end_times.push_back(std::make_pair(m_s.subintervalStart,m_s.subintervalEnd));

        m_s.rollups.startNewSubinterval(m_s.subintervalStart,m_s.subintervalEnd);

        m_s.cpu_by_subinterval.push_back(s);  // ivy_engine: std::vector<Subinterval_CPU> cpu_by_subinterval;
        m_s.cpu_degrees_C_from_critical_temp_by_subinterval.emplace_back();

        m_s.rollups.not_participating.push_back(subsystem_summary_data());
        for (auto& rt_pair : m_s.rollups.rollups)
        {
            for (auto& ri_pair : rt_pair.second->instances)
            {
                ri_pair.second->subsystem_data_by_subinterval.push_back(subsystem_summary_data());
            }
        }

        {
            std::ostringstream o;
            o<< "Waiting for step name " << m_s.stepName
             << ", subinterval " << m_s.rollups.current_index() << " to complete. " << std::endl;
            std::cout << o.str();
            if (routine_logging) { log(m_s.masterlogfile,o.str()); }
        }

        m_s.number_of_IOs_running_at_end_of_subinterval = 0;

        for (auto& pear : m_s.host_subthread_pointers)
        {
            {
                std::unique_lock<std::mutex> u_lk(pear.second->master_slave_lk);

                // Wait for each subtask to post "command complete", for "get subinterval result".

                while (!pear.second->commandComplete) pear.second->master_slave_cv.wait(u_lk);

                // Note that it is the subtask for each host that does the posting of iosequencer detail lines
                // into the various rollups.  It does this after grabbing the lock to serialize access to the
                // "ivy_engine" structure the rollups are kept in that is shared by the main thread and all
                // the host driver subthreads.

                if (!pear.second->commandSuccess)
                {
                    std::ostringstream o;
                    o << std::endl << std::endl << "When waiting for \"get subinterval result\" to complete, subthread for "
                        << pear.first << " posted an error, saying:" << std::endl
                        << pear.second->commandErrorMessage << std::endl;
                    std::cout << o.str();
                    log (m_s.masterlogfile,o.str());
                    m_s.kill_subthreads_and_exit();
                }

                m_s.number_of_IOs_running_at_end_of_subinterval += pear.second->number_of_IOs_running_at_end_of_subinterval;

                pear.second->commandComplete=false; // reset for next pass
                pear.second->commandSuccess=false;
                pear.second->commandErrorMessage.clear();
            }
            pear.second->master_slave_cv.notify_all();
        }
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: All hosts confirmed \"get subinterval result\" complete.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

        ivytime hosts_sent_up;
        hosts_sent_up.setToNow();

        long double host_sendup = ivytime(hosts_sent_up - m_s.subintervalEnd).getlongdoubleseconds();

///*debug*/{ std::cout << std::endl
//                     << "========================================================" << std::endl
//                     << "Debug: there were " << m_s.number_of_IOs_running_at_end_of_subinterval << " I/Os running at the end of the last subinterval." << std::endl
//                     << "========================================================" << std::endl << std::endl; }

        if (m_s.haveCmdDev)
        {
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: About to wait for subsystem gathers to complete.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif
            std::chrono::system_clock::time_point leftnow {std::chrono::system_clock::now()};
            int timeout_seconds {2};
            for (auto& pear : m_s.command_device_subthread_pointers)
            {
                {
                    pipe_driver_subthread*& p_pds = pear.second;

                    if (p_pds->pCmdDevLUN == nullptr) continue;

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
                                o << "\"gather\" to thread for subsystem serial " << pear.first << " managed on host " << p_pds->ivyscript_hostname << " failed.  Aborting." << std::endl;
                                log(m_s.masterlogfile,o.str());
                                m_s.kill_subthreads_and_exit();
                                exit(-1);
                            }

                            if (routine_logging)
                            {
                                std::ostringstream o;
                                o << "\"gather\" complete by thread for subsystem serial " << pear.first << " managed on host " << p_pds->ivyscript_hostname << '.' << std::endl;
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
                    p_pds->master_slave_cv.notify_all();
                }
            }

            {
                std::ostringstream o;

                o << m_s.rollups.get_all_equals_all_instance()->subsystem_data_by_subinterval.back().thumbnail() << std::endl;

                m_s.subsystem_thumbnail = o.str();

                if (routine_logging)
                {
                    log(m_s.masterlogfile,o.str());
                }
            }
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: Subsystem gathers complete.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif
	    }

        // Barrier 1
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: Barrier 1.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

        ivytime barrier_1_time;

        barrier_1_time.setToNow();

        // Note that subintervalStart, subintervalEnd refer to the subinterval we are now rolling up.
        // "Now" is in the "next" subinterval.

        long double sendup_time = ivytime(barrier_1_time - m_s.subintervalEnd).getlongdoubleseconds();

        // print the by-subinterval "provisional" csv line with a blank "warmup" / "measurement" / "cooldown" column
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: About to print the by-subinterval \"provisional\" csv line.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif
        if (m_s.stepcsv) for (auto& pear: m_s.rollups.rollups)
        {
            for (auto& peach : pear.second->instances)
            {
                peach.second->print_subinterval_csv_line(m_s.harvesting_subinterval,true);
            }
        }

        if (routine_logging)
        {
            std::ostringstream o;
            o << "All host subthreads posted subinterval complete." << std::endl;
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
                ivytime rollup_time = n - m_s.subintervalStart;

                ivytime test_duration = n - m_s.test_start_time;
                ivytime step_duration = n - m_s.get_go;

                std::time_t t = std::time(nullptr);

                std::ostringstream o;
                o << test_duration.format_as_duration_HMMSS() << " into test \"" << m_s.testName << "\" at " << std::put_time(std::localtime(&t), "%c %Z") << " local time." << std::endl;
                o << step_duration.format_as_duration_HMMSS() << " into " << m_s.stepNNNN << " \"" << m_s.stepName << "\" at subinterval " << m_s.harvesting_subinterval << std::endl << std::endl;

                o << allAllSubintervalOutput.thumbnail(allAllSubintervalRollup.durationSeconds()) << std::endl;

                if (m_s.haveCmdDev && ( (!m_s.suppress_subsystem_perf) || (m_s.now_doing_suppress_perf_cooldown && m_s.suppress_perf_cooldown_subinterval_count >= 2) ) )
                {
                    o << m_s.getWPthumbnail(((*allAllIterator).second->subintervals.sequence.size())-1) << std::endl;
                    o << m_s.subsystem_thumbnail;
                }

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

        if ( m_s.min_sequential_fill_progress > 0.0 && m_s.min_sequential_fill_progress < 1.0 )
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


        if (!(EVALUATE_SUBINTERVAL_CONTINUE==m_s.lastEvaluateSubintervalReturnCode))
        {
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: Breaking from loop because m_s.lastEvaluateSubintervalReturnCode is not CONTINUE.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif
            // this completes processing of the final subinterval.
            break;
        }

        if (m_s.have_pid || m_s.have_measure)
        {
            if (m_s.p_focus_rollup == nullptr) throw std::runtime_error("<Error> internal programming error - run_subinterval_sequence() - have_pid and or have_measure are set, but m_s.p_focus_rollup == nullptr.");

            for (auto& pear : m_s.p_focus_rollup->instances)
            {
                pear.second->collect_and_push_focus_metric();
                if (m_s.have_pid) pear.second->perform_PID(); // this uses edit_rollup(), which sends out the IOPS adjustment & receives confirmation.
            }

            std::string s = m_s.focus_caption();
            std::cout << s;
            if (s.size()>0 && routine_logging) log(m_s.masterlogfile, s);
        }

        if (m_s.in_cooldown_mode)
        {
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: In cooldown mode.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif
            p_MeasureController->step_over_cooldown_subinterval();

            m_s.current_measurement().cooldown_end     = m_s.subintervalEnd;
            m_s.current_measurement().last_subinterval = m_s.harvesting_subinterval;

            ivytime now; now.setToNow();
            ivytime cooldown_duration = now - m_s.start_of_cooldown;

            if (m_s.suppress_subsystem_perf)
            {
                m_s.suppress_perf_cooldown_subinterval_count++;
                m_s.now_doing_suppress_perf_cooldown = true;
            }

            if (m_s.suppress_subsystem_perf
                && ((m_s.cooldown_by_wp != cooldown_mode::off) || (m_s.cooldown_by_MP_busy != cooldown_mode::off))
                && m_s.suppress_perf_cooldown_subinterval_count <= 2 )
            {
                std::ostringstream o;
                o << "With -no_perf this is IOPS=0 cooldown subinterval " << (m_s.suppress_perf_cooldown_subinterval_count)
                    << " with subsystem gathers turned back on." << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
            }
            else if
            (
                ( (m_s.cooldown_by_MP_busy != cooldown_mode::off)       && m_s.some_subsystem_still_busy()  )  // this clause has to go first to make sure we are counting the number of subintervals MP busy stays below the limit.
             || ( (m_s.cooldown_by_wp      != cooldown_mode::off)       && m_s.some_cooldown_WP_not_empty() )
             || ( (m_s.number_of_IOs_running_at_end_of_subinterval > 0) && (cooldown_duration < ivytime(60)) )
             || ( (m_s.cooldown_seconds > ivytime(0))                   && (cooldown_duration < m_s.cooldown_seconds) )
            )
            {
                m_s.lastEvaluateSubintervalReturnCode = EVALUATE_SUBINTERVAL_CONTINUE;

                std::ostringstream o;
                o << "Cooldown duration " << m_s.current_measurement().cooldown_duration().format_as_duration_HMMSS() << "  not complete, will do another cooldown subinterval." << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());

                if ( (m_s.number_of_IOs_running_at_end_of_subinterval > 0) && (cooldown_duration > ivytime(10)) )
                {
                    std::ostringstream oo;
                    oo << "<Warning> over 10 seconds into cooldown at IOPS = 0 and some I/Os are still running." << std::endl;
                    std::cout << o.str();
                    log(m_s.masterlogfile,o.str());
                }
            }
            else
            {
                std::ostringstream o;
                o << "Cooldown duration " << m_s.current_measurement().cooldown_duration().format_as_duration_HMMSS() << " complete, posting DFC return code from before cooldown." << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
                m_s.lastEvaluateSubintervalReturnCode = m_s.eventualEvaluateSubintervalReturnCode;

                if ( (m_s.number_of_IOs_running_at_end_of_subinterval > 0) && (cooldown_duration >= ivytime(60)) )
                {
                    std::ostringstream oo;
                    oo << "<Warning> over 60 seconds into cooldown at IOPS = 0 and some I/Os are still running.  Terminating without waiting any longer." << std::endl;
                    std::cout << o.str();
                    log(m_s.masterlogfile,o.str());
                }
            }
        }
        else // not m_s.in_cooldown_mode
        {
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: Not in cooldown mode.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif
            m_s.lastEvaluateSubintervalReturnCode = p_MeasureController->evaluateSubinterval();

            {
                std:: ostringstream o;

                o << "For subinterval " << m_s.rollups.current_index() << " evaluateSubinterval() returned ";

                if (EVALUATE_SUBINTERVAL_CONTINUE==m_s.lastEvaluateSubintervalReturnCode)
                    o << "CONTINUE";
                else if (EVALUATE_SUBINTERVAL_FAILURE==m_s.lastEvaluateSubintervalReturnCode)
                    o << "FAILURE";
                else if (EVALUATE_SUBINTERVAL_SUCCESS==m_s.lastEvaluateSubintervalReturnCode)
                    o << "SUCCESS";
                else
                    o << "unexpected return code " << m_s.lastEvaluateSubintervalReturnCode;

                o << std::endl;
                if (routine_logging || EVALUATE_SUBINTERVAL_CONTINUE != m_s.lastEvaluateSubintervalReturnCode) {std::cout << o.str();}
                log(m_s.masterlogfile,o.str());
            }


            if
            (
                m_s.lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_SUCCESS
                ||
                m_s.lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_FAILURE
            )
            {
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: SUCCESS or FAILURE.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

                m_s.current_measurement().last_subinterval = m_s.harvesting_subinterval;
                m_s.current_measurement().cooldown_end     = m_s.subintervalEnd;

                bool going_to_do_another_measurement = false;

                if (m_s.lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_SUCCESS)
                {
                    going_to_do_another_measurement = process_successful_measurement();

                    if (going_to_do_another_measurement)
                    {
                        // we have started the next measurement
                        m_s.lastEvaluateSubintervalReturnCode = EVALUATE_SUBINTERVAL_CONTINUE;
                    }
                }
                else
                {
                    process_failed_measurement();
                }


                if
                (
                    (!going_to_do_another_measurement)

                    &&

                    (
                         ( (m_s.cooldown_by_wp      != cooldown_mode::off) && ( m_s.suppress_subsystem_perf || m_s.some_cooldown_WP_not_empty() ) )
                      || ( (m_s.cooldown_by_MP_busy != cooldown_mode::off) && ( m_s.suppress_subsystem_perf || m_s.some_subsystem_still_busy()  ) )
                      || ( m_s.number_of_IOs_running_at_end_of_subinterval > 0 )
                      || ( m_s.cooldown_seconds > ivytime(0) )
                    )
                )
                {
                    m_s.eventualEvaluateSubintervalReturnCode = m_s.lastEvaluateSubintervalReturnCode;
                    m_s.lastEvaluateSubintervalReturnCode = EVALUATE_SUBINTERVAL_CONTINUE;

                    m_s.in_cooldown_mode = true;
                    m_s.start_of_cooldown.setToNow();

                    auto errc = m_s.edit_rollup("all=all","IOPS=pause",true);

                    if (!errc.first)
                    {
                        std::ostringstream o;
                        o << "<Error> Internal programming error.  When entering cooldown mode, edit_rollup(\"all=all\",\"IOPS=pause\") failed - " << errc.second << std::endl
                            << "Source code reference line " << __LINE__ << " of " << __FILE__ << "." << std::endl;
                        log(m_s.masterlogfile,o.str());
                        m_s.kill_subthreads_and_exit();
                    }

                    std::ostringstream o;
                    o << "DFC posted SUCCESS or FAILURE.  Entering coolcown at IOPS = 0." << std::endl;

                    std::cout << o.str();
                    log(m_s.masterlogfile,o.str());
                }
            }
        }


        ivytime before_continue_stop;
        before_continue_stop.setToNow();

#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: About to send out continue or stop.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif
        // Iterate over all hosts, posting command to run another subinterval, or to stop at the end of the current subinterval.
        for (auto& pear : m_s.host_subthread_pointers)
        {
            {
                std::unique_lock<std::mutex> u_lk(pear.second->master_slave_lk);
                if (EVALUATE_SUBINTERVAL_CONTINUE == m_s.lastEvaluateSubintervalReturnCode)
                {
                    pear.second->commandString = "continue";
                    m_s.last_command_was_stop = false;
                }
                else
                {
                    pear.second->commandString = "stop";
                    m_s.last_command_was_stop = true;
                }
                pear.second->command=true;
                pear.second->commandComplete=false;
                pear.second->commandSuccess=false;
                pear.second->commandErrorMessage.clear();
            }
            pear.second->master_slave_cv.notify_all();
        }

        // Wait for all ivydrivers to confirm receipt of continue or stop.
        for (auto& pear : m_s.host_subthread_pointers)
        {
            {
                std::unique_lock<std::mutex> s_lk(pear.second->master_slave_lk);

                if
                ( ! pear.second->master_slave_cv.wait_for
                    (
                        s_lk,
                        std::chrono::milliseconds( (unsigned int)(500 * m_s.subinterval_seconds)),
                        [&]() { return pear.second->commandComplete; }
                    )
                )
                {
                    // timed out.

                    std::ostringstream o;
                    o << "<Error> Timed out waiting more than 1/2 of a subinterval to get acknowledgement of "
                        << "\"" << pear.second->commandString << "\""
                        << " from host " << pear.first << std::endl
                        << "Source code reference line " << __LINE__ << " of " << __FILE__ << "." << std::endl;
                    log(m_s.masterlogfile,o.str());
                    m_s.kill_subthreads_and_exit();
                }

                if (!pear.second->commandSuccess)
                {
                    ostringstream o;
                    o << "<Error> \"" << pear.second->commandString << "\" failed - " << pear.second->commandErrorMessage;
                    log(m_s.masterlogfile,o.str());
                    m_s.kill_subthreads_and_exit();
                }
                pear.second->command=false;
                pear.second->commandComplete=false;
                pear.second->commandSuccess=false;
                pear.second->commandErrorMessage.clear();
            }
            pear.second->master_slave_cv.notify_all();
        }

#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: Have receipt of continue or stop.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif
        ivytime after_continue_stop;
        after_continue_stop.setToNow();

        m_s.continueCooldownStopAckSeconds.push( ivytime( after_continue_stop - before_continue_stop).getlongdoubleseconds() );


        ivytime first_gather_or_subinterval_end = m_s.nextSubintervalEnd;
          // The "current" subinterval is the one we are rolling up.
          // The "next" subinterval is the one that's running.
          // We now calculate the gather start time near the end of
          // the "next" subinterval, i.e. the one that is actually running.

        ivytime host_sendup_adjustment(0.0);

        if (m_s.hostSendupTimeSeconds.count() > 0)
        {
            host_sendup_adjustment = ivytime(m_s.hostSendupTimeSeconds.avg() - m_s.hostSendupTimeSeconds.standardDeviation());
        }

        ivytime gather_end;
        gather_end = m_s.nextSubintervalEnd + host_sendup_adjustment;

        for (auto& pear : m_s.command_device_subthread_pointers)
        {
            ivytime gather_adjustment = ivytime(pear.second->perSubsystemGatherTime.avg() + pear.second->perSubsystemGatherTime.standardDeviation());
                // This always works because there was an earlier t=0 gather.

            pear.second->gather_scheduled_start_time = m_s.nextSubintervalEnd + host_sendup_adjustment - gather_adjustment;

            ivytime now; now.setToNow();
            ivytime progress = now - m_s.subintervalStart;

            if (pear.second->gather_scheduled_start_time <= now)
            {
                std::ostringstream o;
                o << "<Error> Now " << progress.format_as_duration_HMMSSns()
                    << " into the " << m_s.subinterval_seconds
                    << " second subinterval not enough time to start the next gather in time on subsystem " << pear.first
                    << " gather history: ";
                pear.second->perSubsystemGatherTime.printme(o);
                o << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
                m_s.kill_subthreads_and_exit();
            }

            if ( pear.second->gather_scheduled_start_time < first_gather_or_subinterval_end )
            {
                first_gather_or_subinterval_end = pear.second->gather_scheduled_start_time;
                m_s.earliest_gather_before_subinterval_end = ivytime(m_s.nextSubintervalEnd - pear.second->gather_scheduled_start_time).getlongdoubleseconds();
            }
        }


        // Issue next gather

        for (auto& pear : m_s.command_device_subthread_pointers)
        {
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: About to issue next scheduled gather.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

            {
                pipe_driver_subthread*& p_pds = pear.second;

                if (p_pds->pCmdDevLUN == nullptr) continue;

                {
                    std::unique_lock<std::mutex> u_lk(p_pds->master_slave_lk);
                    p_pds->commandString = std::string("gather");
                    p_pds->command=true;
                    p_pds->commandComplete=false;
                    p_pds->commandSuccess=false;
                    p_pds->commandErrorMessage.clear();

                    if ( (!m_s.suppress_subsystem_perf) || (m_s.now_doing_suppress_perf_cooldown) )
                    {
                        std::ostringstream o;
                        o << "Gathering from subsystem serial " << pear.first << std::endl;
                        log(m_s.masterlogfile,o.str());
                        if (routine_logging) { std::cout << o.str(); }
                    }
                }
                p_pds->master_slave_cv.notify_all();
            }
        }
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: Barrier 2.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

        ivytime barrier_2_time;
        barrier_2_time.setToNow();

        long double CP_seconds = ivytime(barrier_2_time - barrier_1_time).getlongdoubleseconds();

        if (sendup_time < 0. )
        {
            std::ostringstream o;
            o << "<Error> internal programming error - sendup_time is negative -> " << sendup_time << std::endl
                << "Source code reference " << __FILE__ << " line " << __LINE__ << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
        }

        if (CP_seconds < 0. )
        {
            std::ostringstream o;
            o << "<Error> internal programming error - CP_seconds (central processing time) is negative -> " << CP_seconds << std::endl
                << "Source code reference " << __FILE__ << " line " << __LINE__ << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
        }


        if (m_s.earliest_gather_before_subinterval_end < 0. )
        {
            std::ostringstream o;
            o << "<Error> internal programming error - m_s.earliest_gather_before_subinterval_end is negative -> " << m_s.earliest_gather_before_subinterval_end << std::endl
                << "Source code reference " << __FILE__ << " line " << __LINE__ << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
        }

        ivy_float protocol = sendup_time + CP_seconds + m_s.earliest_gather_before_subinterval_end;

        ivy_float cruise_seconds = m_s.subinterval_seconds - protocol;

        m_s.protocolTimeSeconds              .push(protocol);
        m_s.hostSendupTimeSeconds            .push(host_sendup);
        m_s.sendupTimeSeconds                .push(sendup_time);
        m_s.centralProcessingTimeSeconds     .push(CP_seconds);
        m_s.gatherBeforeSubintervalEndSeconds.push(m_s.earliest_gather_before_subinterval_end);
        m_s.cruiseSeconds                    .push(cruise_seconds);

#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: Bottom of loop.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

    }

    // have broken out of subinterval loop.
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: Have broken out of loop.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

    // Check for failed subsystem components
    if (m_s.haveCmdDev & m_s.check_failed_component)
    {
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: About to check for failed components.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif
        m_s.have_failed_component = false;
        m_s.failed_component_message.clear();

        for (auto& pear : m_s.command_device_subthread_pointers)
        {
            {
                pipe_driver_subthread*& p_pds = pear.second;

                if (p_pds->pCmdDevLUN == nullptr) continue;

                std::unique_lock<std::mutex> u_lk(p_pds->master_slave_lk);
                p_pds->commandString = std::string("get failed_component");
                p_pds->command=true;
                p_pds->commandComplete=false;
                p_pds->commandSuccess=false;
                p_pds->commandErrorMessage.clear();
            }
            pear.second->master_slave_cv.notify_all();
        }

        std::chrono::system_clock::time_point leftnow {std::chrono::system_clock::now()};

        for (auto& pear : m_s.command_device_subthread_pointers)
        {
            {
                pipe_driver_subthread*& p_pds = pear.second;

                if (p_pds->pCmdDevLUN == nullptr) continue;

                {
                    std::unique_lock<std::mutex> u_lk(p_pds->master_slave_lk);

                    if (
                            p_pds->master_slave_cv.wait_until
                            (
                                u_lk,
                                leftnow + std::chrono::milliseconds(5000),
                                [&p_pds]() { return p_pds->commandComplete; }
                            )
                        )
                    {
                        if (!p_pds->commandSuccess)
                        {
                            m_s.have_failed_component = true;

                            std::ostringstream o;
                            o << "<Error> \"check_failed_component\" = \"on\" and found failed component on subsystem serial " << pear.first << " managed on host " << p_pds->ivyscript_hostname
                                << ", saying \"" << p_pds->commandErrorMessage << "\"." << std::endl;

                            m_s.failed_component_message += o.str();
                            log(m_s.masterlogfile,o.str());
                        }

                        if (routine_logging)
                        {
                            std::ostringstream o;
                            o << "\"check_failed_component\" = \"on\" and subsystem serial " << pear.first << " managed on host " << p_pds->ivyscript_hostname << " does not have a failed component." << std::endl;
                            log(m_s.masterlogfile,o.str());
                        }
                    }
                    else // timeout
                    {
                        std::ostringstream o;
                        o << "Aborting - timeout waiting for pipe_driver_subthread for subsystem " << pear.first << " to check for a failed component." <<  std::endl;
                        std::cout << o.str();
                        log(m_s.masterlogfile,o.str());
                        u_lk.unlock();
                        m_s.kill_subthreads_and_exit();
                        exit ( -1);
                    }
                }
                p_pds->master_slave_cv.notify_all();
            }
            // lock dropped
        }

        if (m_s.have_failed_component)
        {
            std::cout << m_s.failed_component_message;
            m_s.kill_subthreads_and_exit();
        }
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: Finished check for failed components.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

    }


    std::cout << "Printing csv files." << std::endl;

    // print csv files with real-time subsystem data

    // NOTE: this was done after the subinterval sequence is complete because I didn't want to avoid doing processing in real time if I could do it later.
    // But later if we want to see this data as we go along, it would not too hard to move this to "run_subinterval_sequence()" and do it in real time.

    // HOWEVER - need to think about column titles when a metric doesn't appear until after the first gather.

    if (m_s.stepcsv && m_s.storcsv) for (auto& pear : m_s.subsystems)
    {
        pear.second->print_subinterval_csv_line_set( pear.second->serial_number + std::string(".perf"));
    }
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: Past printing storcsv.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif


    for (unsigned int m = 0; m < m_s.measurements.size(); m++)
    {
        m_s.rollups.print_measurement_summary_csv_line(m);
    }
#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: Past print_measurement_summary_csv_line.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif

    if (m_s.stepcsv) m_s.print_latency_csvfiles();

#ifdef TRACE_RSS
{std::ostringstream trss; trss << "TRACE_RSS: Past print_latency_csvfiles.\n" << std::endl; log(m_s.masterlogfile, trss.str()); std::cout << trss.str();}
#endif
    if (m_s.stepcsv && m_s.have_pid)
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


    {
        std::ostringstream o;

        o << "Step Time Statistics: " << std::endl;

        {
            RunningStat<ivy_float,ivy_int>& rs = m_s.protocolTimeSeconds;

            o << "   Total interlock protocol time:                         "
//               "/---------------------------------------------------------"

            << ivytime(rs.avg()).format_with_ms() << " avg,"
            << ivytime(rs.min()).format_with_ms() << " min,"
            << ivytime(rs.max()).format_with_ms() << " max,"
            << rs.count() << " count." << std::endl;
        }

        {
            RunningStat<ivy_float,ivy_int>& rs = m_s.sendupTimeSeconds;

            o << "   Sendup time (Barrier 1 time):                          "
//               "/---------------------------------------------------------"

            << ivytime(rs.avg()).format_with_ms() << " avg,"
            << ivytime(rs.min()).format_with_ms() << " min,"
            << ivytime(rs.max()).format_with_ms() << " max,"
            << rs.count() << " count." << std::endl;
        }

        {
            RunningStat<ivy_float,ivy_int>& rs = m_s.centralProcessingTimeSeconds;

            o << "   Central processing time (Barrier 1 to Barrier 2 time): "
//               "/---------------------------------------------------------"

            << ivytime(rs.avg()).format_with_ms() << " avg,"
            << ivytime(rs.min()).format_with_ms() << " min,"
            << ivytime(rs.max()).format_with_ms() << " max,"
            << rs.count() << " count." << std::endl;
        }

        {
            RunningStat<ivy_float,ivy_int>& rs = m_s.gatherBeforeSubintervalEndSeconds;

            o << "   Earliest subsystem gather head start, seconds "
//               "/---------------------------------------------------------"

            << ivytime(rs.avg()).format_with_ms() << " avg,"
            << ivytime(rs.min()).format_with_ms() << " min,"
            << ivytime(rs.max()).format_with_ms() << " max,"
            << rs.count() << " count." << std::endl;
        }

        if (m_s.haveCmdDev)
        {
            RunningStat<ivy_float,ivy_int>& rs = m_s.overallGatherTimeSeconds;

            o << "   Gather time over all subsystems:                       "
//               "/---------------------------------------------------------"

            << ivytime(rs.avg()).format_with_ms() << " avg,"
            << ivytime(rs.min()).format_with_ms() << " min,"
            << ivytime(rs.max()).format_with_ms() << " max,"
            << rs.count() << " count." << std::endl;
        }

        {
            RunningStat<ivy_float,ivy_int>& rs = m_s.cruiseSeconds;

            o << "   Cruise time:                          "
//               "/---------------------------------------------------------"

            << ivytime(rs.avg()).format_as_duration_HMMSSns() << " avg,"
            << ivytime(rs.min()).format_as_duration_HMMSSns() << " min,"
            << ivytime(rs.max()).format_as_duration_HMMSSns() << " max,"
            << rs.count() << " count." << std::endl;
        }
        std::cout << std::endl;

        log(m_s.masterlogfile,o.str());

        log(m_s.masterlogfile,print_logfile_stats());

        // log gather time breakdown
        if (routine_logging) for (auto& pear : m_s.command_device_subthread_pointers)
        {
            std::ostringstream o;

            {
                auto& p_pds = pear.second;

                o << "Step Gather Statistics Breakdown for " << pear.first << ":" << std::endl;
                    o << "\tget CLPR detail stats: " << p_pds->getCLPRDetailTime.min()<< " seconds (Min) " << p_pds->getCLPRDetailTime.max() << " seconds (Max) " << p_pds->getCLPRDetailTime.avg() << " seconds (Avg) " << std::endl;
                    o << "\tget MP_busy_detail stats: " << p_pds->getMPbusyTime.min()<< " seconds (Min) " << p_pds->getMPbusyTime.max() << " seconds (Max) " << p_pds->getMPbusyTime.avg() << " seconds (Avg) " << std::endl;
                    o << "\tget LDEVIO detail stats: " << p_pds->getLDEVIOTime.min()<< " seconds (Min) " << p_pds->getLDEVIOTime.max() << " seconds (Max) " << p_pds->getLDEVIOTime.avg() << " seconds (Avg) " << std::endl;
                    o << "\tget PORTIO detail stats: " << p_pds->getPORTIOTime.min()<< " seconds (Min) " << p_pds->getPORTIOTime.max() << " seconds (Max) " << p_pds->getPORTIOTime.avg() << " seconds (Avg) " << std::endl;
                    o << "\tget UR_Jnl detail stats: " << p_pds->getUR_JnlTime.min()<< " seconds (Min) " << p_pds->getUR_JnlTime.max() << " seconds (Max) " << p_pds->getUR_JnlTime.avg() << " seconds (Avg) " << std::endl;

                    log(p_pds->logfilename,o.str());
            }
        }
    }

    return;
}



