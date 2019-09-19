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

#include "ivy_engine.h"
#include "pipe_driver_subthread.h"
#include "ivyhelpers.h"

extern bool routine_logging;

void pipe_driver_subthread::pipe_driver_gather(std::unique_lock<std::mutex>& s_lk, bool t_0_gather)
{
    ivytime entry_time; entry_time.setToNow();

    gather_scheduled_start_time.waitUntilThisTime();

    ivytime after_waiting; after_waiting.setToNow();
    ivytime wait_time = after_waiting-entry_time;

    bool fake_gather;

    if      (  m_s.now_doing_suppress_perf_cooldown)          { fake_gather = false; }
    else if ( !m_s.suppress_subsystem_perf )                  { fake_gather = false; }
    else if (  p_Hitachi_RAID_subsystem->gathers.size() > 0 ) { fake_gather = true;  }
    else                                                      { fake_gather = false; } // t=0 gather

    ivytime tn_gather_start, tn_gather_complete;

    tn_gather_start.setToNow();

    ivytime tn_sub_gather_end;
    ivytime gatherStart;

    if (routine_logging)
    {
        std::ostringstream o;

        o << "\"gather\" command received, and waited for " << wait_time.format_as_duration_HMMSSns() << " until scheduled gather start time.";

        if (fake_gather)
        {
            o << "  With command line option -suppress_perf, and with I/O being driven, doing a fake gather.";
        }

        o << "  There are data from " << p_Hitachi_RAID_subsystem->gathers.size() << " previous gathers." << std::endl;

        log(logfilename,o.str());
    }

    // add a new GatherData on the end of the Subsystem's chain.
    GatherData gd;
    p_Hitachi_RAID_subsystem->gathers.push_back(gd);

    // run whatever ivy_cmddev commands we need to, and parse the output pointing at the current GatherData object.
    GatherData& currentGD = p_Hitachi_RAID_subsystem->gathers.back();

    gatherStart = tn_gather_start;

    if (fake_gather)
    {
        ivytime delay(0.25);
        delay.wait_for_this_long();
    }
    else
    {
        if
        (
            t_0_gather
         || ( m_s.now_doing_suppress_perf_cooldown && m_s.suppress_perf_cooldown_subinterval_count == 1 )
        )
        {
            try
            {
                send_and_get_OK("reset rollover counters");
            }
            catch (std::runtime_error& reex)
            {
                std::ostringstream o;
                o << "\"reset rollover counters\" command sent to ivy_cmddev failed saying \"" << reex.what() << "\"." << std::endl;
                log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                kill_ssh_and_harvest();
                commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                master_slave_cv.notify_all();
                return;
            }
        }
        else
        {
            try
            {
                send_and_get_OK("start normal gather");
            }
            catch (std::runtime_error& reex)
            {
                std::ostringstream o;
                o << "\"start normal gather\" command sent to ivy_cmddev failed saying \"" << reex.what() << "\"." << std::endl;
                log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                kill_ssh_and_harvest();
                commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                master_slave_cv.notify_all();
                return;
            }

        }

        try
        {
            send_and_get_OK("get CLPRdetail");

            try
            {
                process_ivy_cmddev_response(currentGD);
            }
            catch (std::invalid_argument& iaex)
            {
                std::ostringstream o;
                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get CLPRdetail\"), std::invalid_argument saying \"" << iaex.what() << "\"." << std::endl;
                log(logfilename,o.str());
                kill_ssh_and_harvest();
                commandComplete=true;
                commandSuccess=false;
                commandErrorMessage = o.str();
                dead=true;
                s_lk.unlock();
                master_slave_cv.notify_all();
                return;
            }
            catch (std::runtime_error& reex)
            {
                std::ostringstream o;
                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get CLPRdetail\"), std::runtime_error saying \"" << reex.what() << "\"." << std::endl;
                log(logfilename,o.str());
                kill_ssh_and_harvest();
                commandComplete=true;
                commandSuccess=false;
                commandErrorMessage = o.str();
                dead=true;
                s_lk.unlock();
                master_slave_cv.notify_all();
                return;
            }
        }
        catch (std::runtime_error& reex)
        {
            std::ostringstream o;
            o << "\"get CLPRdetail\" command sent to ivy_cmddev failed saying \"" << reex.what() << "\"." << std::endl;
            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
            kill_ssh_and_harvest();
            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
            master_slave_cv.notify_all();
            return;
        }

        tn_sub_gather_end.setToNow();
        getCLPRDetailTime.push(ivytime(tn_sub_gather_end - gatherStart));

        gatherStart.setToNow();
        try
        {
            send_and_get_OK("get MP_busy");
            try
            {
                process_ivy_cmddev_response(currentGD);
            }
            catch (std::invalid_argument& iaex)
            {
                std::ostringstream o;
                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get MP_busy\"), std::invalid_argument saying \"" << iaex.what() << "\"." << std::endl;
                log(logfilename,o.str());
                kill_ssh_and_harvest();
                commandComplete=true;
                commandSuccess=false;
                commandErrorMessage = o.str();
                dead=true;
                s_lk.unlock();
                master_slave_cv.notify_all();
                return;
            }
            catch (std::runtime_error& reex)
            {
                std::ostringstream o;
                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get MP_busy\"), std::runtime_error saying \"" << reex.what() << "\"." << std::endl;
                log(logfilename,o.str());
                kill_ssh_and_harvest();
                commandComplete=true;
                commandSuccess=false;
                commandErrorMessage = o.str();
                dead=true;
                s_lk.unlock();
                master_slave_cv.notify_all();
                return;
            }
        }
        catch (std::runtime_error& reex)
        {
            std::ostringstream o;
            o << "\"get MP_busy\" command sent to ivy_cmddev failed saying \"" << reex.what() << "\"." << std::endl;
            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
            kill_ssh_and_harvest();
            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
            master_slave_cv.notify_all();
            return;
        }

        tn_sub_gather_end.setToNow();
        getMPbusyTime.push(ivytime(tn_sub_gather_end - gatherStart));

        gatherStart.setToNow();
        if (!m_s.skip_ldev_data)
        {
            try
            {
                send_and_get_OK("get LDEVIO");
                try
                {
                    process_ivy_cmddev_response(currentGD);
                }
                catch (std::invalid_argument& iaex)
                {
                    std::ostringstream o;
                    o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get LDEVIO\"), std::invalid_argument saying \"" << iaex.what() << "\"." << std::endl;
                    log(logfilename,o.str());
                    kill_ssh_and_harvest();
                    commandComplete=true;
                    commandSuccess=false;
                    commandErrorMessage = o.str();
                    dead=true;
                    s_lk.unlock();
                    master_slave_cv.notify_all();
                    return;
                }
                catch (std::runtime_error& reex)
                {
                    std::ostringstream o;
                    o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get LDEVIO\"), std::runtime_error saying \"" << reex.what() << "\"." << std::endl;
                    log(logfilename,o.str());
                    kill_ssh_and_harvest();
                    commandComplete=true;
                    commandSuccess=false;
                    commandErrorMessage = o.str();
                    dead=true;
                    s_lk.unlock();
                    master_slave_cv.notify_all();
                    return;
                }
            }
            catch (std::runtime_error& reex)
            {
                std::ostringstream o;
                o << "\"get LDEVIO\" command sent to ivy_cmddev failed saying \"" << reex.what() << "\"." << std::endl;
                log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                kill_ssh_and_harvest();
                commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                master_slave_cv.notify_all();
                return;
            }
        }

        tn_sub_gather_end.setToNow();
        getLDEVIOTime.push(ivytime(tn_sub_gather_end - gatherStart));

        // element port
            // instance 1a
                // mainframe:
                    // IO_count (rollover)
                    // block_count (rollover)
                // open
                    // max_IOPS 32-bit (not rollover)
                    // min_IOPS 32-bit (not rollover)
                    // avg_IOPS 32-bit (not rollover)
                    // max_KBPS 32-bit (not rollover)
                    // min_KBPS 32-bit (not rollover)
                    // avg_KBPS 32-bit (not rollover)

                    // IO_count 64-bit (rollover)
                    // block_count 64-bit(rollover)

                    // initiator_max_IOPS 32-bit (not rollover)
                    // initiator_min_IOPS 32-bit (not rollover)
                    // initiator_avg_IOPS 32-bit (not rollover)
                    // initiator_max_KBPS 32-bit (not rollover)
                    // initiator_min_KBPS 32-bit (not rollover)
                    // initiator_avg_KBPS 32-bit (not rollover)

                    // initiator_IO_count 64-bit (rollover)
                    // initiator_block_count 64-bit (rollover)
        gatherStart.setToNow();
        try
        {
            send_and_get_OK("get PORTIO");
            try
            {
                process_ivy_cmddev_response(currentGD);
            }
            catch (std::invalid_argument& iaex)
            {
                std::ostringstream o;
                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get PORTIO\"), std::invalid_argument saying \"" << iaex.what() << "\"." << std::endl;
                log(logfilename,o.str());
                kill_ssh_and_harvest();
                commandComplete=true;
                commandSuccess=false;
                commandErrorMessage = o.str();
                dead=true;
                s_lk.unlock();
                master_slave_cv.notify_all();
                return;
            }
            catch (std::runtime_error& reex)
            {
                std::ostringstream o;
                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get PORIO\"), std::runtime_error saying \"" << reex.what() << "\"." << std::endl;
                log(logfilename,o.str());
                kill_ssh_and_harvest();
                commandComplete=true;
                commandSuccess=false;
                commandErrorMessage = o.str();
                dead=true;
                s_lk.unlock();
                master_slave_cv.notify_all();
                return;
            }
        }
        catch (std::runtime_error& reex)
        {
            std::ostringstream o;
            o << "\"get PORTIO\" command sent to ivy_cmddev failed saying \"" << reex.what() << "\"." << std::endl;
            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
            kill_ssh_and_harvest();
            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
            master_slave_cv.notify_all();
            return;
        }

        tn_sub_gather_end.setToNow();
        getPORTIOTime.push(ivytime(tn_sub_gather_end - gatherStart));

        gatherStart.setToNow();
        try
        {
            send_and_get_OK("get UR_Jnl");
            try
            {
                process_ivy_cmddev_response(currentGD);
            }
            catch (std::invalid_argument& iaex)
            {
                std::ostringstream o;
                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get UR_Jnl\"), std::invalid_argument saying \"" << iaex.what() << "\"." << std::endl;
                log(logfilename,o.str());
                kill_ssh_and_harvest();
                commandComplete=true;
                commandSuccess=false;
                commandErrorMessage = o.str();
                dead=true;
                s_lk.unlock();
                master_slave_cv.notify_all();
                return;
            }
            catch (std::runtime_error& reex)
            {
                std::ostringstream o;
                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get UR_Jnl\"), std::runtime_error saying \"" << reex.what() << "\"." << std::endl;
                log(logfilename,o.str());
                kill_ssh_and_harvest();
                commandComplete=true;
                commandSuccess=false;
                commandErrorMessage = o.str();
                dead=true;
                s_lk.unlock();
                master_slave_cv.notify_all();
                return;
            }
        }
        catch (std::runtime_error& reex)
        {
            std::ostringstream o;
            o << "\"get UR_Jnl\" command sent to ivy_cmddev failed saying \"" << reex.what() << "\"." << std::endl;
            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
            kill_ssh_and_harvest();
            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
            master_slave_cv.notify_all();
            return;
        }

        tn_sub_gather_end.setToNow();
        getUR_JnlTime.push(ivytime(tn_sub_gather_end - gatherStart));

        gatherStart.setToNow();
        try
        {
            send_and_get_OK("get MP_busy_detail");
            try
            {
                process_ivy_cmddev_response(currentGD);
            }
            catch (std::invalid_argument& iaex)
            {
                std::ostringstream o;
                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get MP_busy_detail\"), std::invalid_argument saying \"" << iaex.what() << "\"." << std::endl;
                log(logfilename,o.str());
                kill_ssh_and_harvest();
                commandComplete=true;
                commandSuccess=false;
                commandErrorMessage = o.str();
                dead=true;
                s_lk.unlock();
                master_slave_cv.notify_all();
                return;
            }
            catch (std::runtime_error& reex)
            {
                std::ostringstream o;
                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"getMP_busy_detailUR_Jnl\"), std::runtime_error saying \"" << reex.what() << "\"." << std::endl;
                log(logfilename,o.str());
                kill_ssh_and_harvest();
                commandComplete=true;
                commandSuccess=false;
                commandErrorMessage = o.str();
                dead=true;
                s_lk.unlock();
                master_slave_cv.notify_all();
                return;
            }
        }
        catch (std::runtime_error& reex)
        {
            std::ostringstream o;
            o << "\"get MP_busy_detail\" command sent to ivy_cmddev failed saying \"" << reex.what() << "\"." << std::endl;
            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
            kill_ssh_and_harvest();
            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
            master_slave_cv.notify_all();
            return;
        }

        tn_sub_gather_end.setToNow();
        getMP_busy_detail_Time.push(ivytime(tn_sub_gather_end - gatherStart));
    }


    if (
         ( (!m_s.suppress_subsystem_perf) && (p_Hitachi_RAID_subsystem->gathers.size() > 1) )
      || ( m_s.suppress_perf_cooldown_subinterval_count >= 2 )
    )
    {
        // post processing for the end of a subinterval (not for the t=0 gather)

        if (!m_s.suppress_subsystem_perf) { m_s.rollups.not_participating.push_back(subsystem_summary_data()); }

        void rollup_Hitachi_RAID_data(logger&, Hitachi_RAID_subsystem*, GatherData& currentGD);

        if
        (
            ( (!m_s.suppress_subsystem_perf) && (p_Hitachi_RAID_subsystem->gathers.size() > 1) )
         ||
            ( m_s.suppress_subsystem_perf && (m_s.suppress_perf_cooldown_subinterval_count > 1) )
        )
        {
            rollup_Hitachi_RAID_data(logfilename, p_Hitachi_RAID_subsystem, currentGD);
        }


        // Filter & post subsystem performance data by RollupInstance

        //- As each gather is post-processed for a Subsystem, after we have posted derived metrics like MPU busy:
        //	- if gather is not for t=0
        //		- for each RollupType, for each RollupInstance
        //			- add a subsystem_summary_data object for this subinterval
        //			- for each summary element type ("MP core", "CLPR", ...) ivy_engine::subsystem_summary_metrics
        //				- for each instance of the element, e.g. MP_core="MP10-00"
        //					- if the subsystem serial number and attribute name match the RollupInstance
        //						- for each metric
        //							- post values -.

    }

    if (m_s.doing_t0_gather)
    {
        // post-processing for the t=0 gather

        // Weirdness here is that some configuration information is collected with performance data so we copy these over to the config gather
        // so that from subinterval 1 on the DFC will see these metrics in the config data.

        GatherData& gd = p_Hitachi_RAID_subsystem->gathers[0];
        auto element = gd.data.find("subsystem");
        if (element != gd.data.end())
        {
            auto instance = element->second.find("subsystem");
            if (instance != element->second.end())
            {
                // It's possible that we might record "subsystem=subsystem" performance data (like the old cache path busy metric that was average over whole subsystem).

                // Therefore each subsystem=subsystem metric that is a constant config metric is individually copied over.

                auto metric = instance->second.find("max_MP_io_buffers");
                if (metric != instance->second.end())
                {
                    p_Hitachi_RAID_subsystem->configGatherData.data[element->first][instance->first][metric->first] = metric->second;
                }
                metric = instance->second.find("total_LDEV_capacity_512_sectors");
                if (metric != instance->second.end())
                {
                    p_Hitachi_RAID_subsystem->configGatherData.data[element->first][instance->first][metric->first] = metric->second;
                }
            }
        }
    }

    commandComplete=true;
    commandSuccess=true;
    commandErrorMessage.clear();
    ivytime end;
    end.setToNow();
    /* ivytime */ duration = end-tn_gather_start;
    gather_times_seconds.push_back(duration.getlongdoubleseconds());

    // Until such time as we print the GatherData csv files, we log to confirm at least the gather is working

    if (routine_logging)
    {
        std::ostringstream o;
        o << "Completed gather for subsystem serial_number=" << p_Hitachi_RAID_subsystem->serial_number << ", gather time = " << duration.format_as_duration_HMMSSns() << std::endl;
        //o << currentGD;
        log(logfilename, o.str());
    }

    s_lk.unlock();
    master_slave_cv.notify_all();

    // record gather time completion
    tn_gather_complete.setToNow();

    ivytime tn_gather_time {tn_gather_complete - tn_gather_start};

    if (!fake_gather)
    {
        m_s.overallGatherTimeSeconds.push(tn_gather_time.getlongdoubleseconds());
        perSubsystemGatherTime.push(tn_gather_time.getlongdoubleseconds());
    }
}
