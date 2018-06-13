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

#include "ivy_engine.h"
#include "pipe_driver_subthread.h"
#include "ivyhelpers.h"

extern bool routine_logging;

void pipe_driver_subthread::pipe_driver_gather(std::unique_lock<std::mutex>& s_lk)
{
    ivytime tn_gather_start, tn_gather_complete;
    ivytime tn_sub_gather_end;
    ivytime gatherStart;

    if (routine_logging)
    {
        std::ostringstream o;
        o << "\"gather\" command received.  There are data from " << p_Hitachi_RAID_subsystem->gathers.size() << " previous gathers." << std::endl;
        log(logfilename,o.str());
    }

    // add a new GatherData on the end of the Subsystem's chain.
    GatherData gd;
    p_Hitachi_RAID_subsystem->gathers.push_back(gd);


    // run whatever ivy_cmddev commands we need to, and parse the output pointing at the current GatherData object.
    GatherData& currentGD = p_Hitachi_RAID_subsystem->gathers.back();

    gather_scheduled_start_time.waitUntilThisTime();

    tn_gather_start.setToNow();
    gatherStart = tn_gather_start;

    try
    {
        send_and_get_OK("get CLPRdetail");

        try
        {
            process_ivy_cmddev_response(currentGD, gatherStart);
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
            process_ivy_cmddev_response(currentGD, gatherStart);
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
    try
    {
        send_and_get_OK("get LDEVIO");
        try
        {
            process_ivy_cmddev_response(currentGD, gatherStart);
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
            process_ivy_cmddev_response(currentGD, gatherStart);
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
            process_ivy_cmddev_response(currentGD, gatherStart);
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
    // Post-processing after gather to fill in derived metrics.


    if (p_Hitachi_RAID_subsystem->gathers.size() > 1)
    {
        // post processing for the end of a subinterval (not for the t=0 gather)

        GatherData& lastGD = p_Hitachi_RAID_subsystem->gathers[p_Hitachi_RAID_subsystem->gathers.size() - 2 ];

        // compute & store MP % busy from MP counter values

        auto this_element_it = currentGD.data.find(std::string("MP_core"));
        auto last_element_it =    lastGD.data.find(std::string("MP_core"));

        if (this_element_it != currentGD.data.end() && last_element_it != lastGD.data.end()) // processing MP_core from 2nd gather on
        {
            std::map<std::string, RunningStat<ivy_float, ivy_int>> mpu_busy;

            for (auto this_instance_it = this_element_it->second.begin(); this_instance_it != this_element_it->second.end(); this_instance_it++)
            {
                // for each MP_core instance

                std::string MP_core_instance_name = this_instance_it->first;

                auto last_instance_it = last_element_it->second.find(MP_core_instance_name);

                if (last_instance_it == last_element_it->second.end() )
                {
                    throw std::runtime_error(std::string("pipe_driver_subthread.cpp: Computation of subsystem MP % busy as delta busy counter divided by delta elapsed counter - corresponding MP_core not found in previous gather for current gather location =\"") + MP_core_instance_name + std::string("\"."));
                }

                auto this_elapsed_it = this_instance_it->second.find(std::string("elapsed_counter"));
                auto last_elapsed_it = last_instance_it->second.find(std::string("elapsed_counter"));
                auto this_busy_it    = this_instance_it->second.find(std::string("busy_counter"));
                auto last_busy_it    = last_instance_it->second.find(std::string("busy_counter"));

                if (this_elapsed_it == this_instance_it->second.end())
                {
                    std::ostringstream o;
                    o 	<< "pipe_driver_subthread.cpp: Computation of subsystem MP % busy as delta busy counter divided by delta elapsed counter -"
                        << "this gather instance " << this_instance_it->first << " does not have has metric_value elapsed_counter." << std::endl;
                    log(logfilename,o.str());
                    throw std::runtime_error(o.str());
                }

                if (last_elapsed_it == last_instance_it->second.end())
                {
                    std::ostringstream o;
                    o 	<< "pipe_driver_subthread.cpp: Computation of subsystem MP % busy as delta busy counter divided by delta elapsed counter -"
                        << "last gather instance " << last_instance_it->first << " does not have has metric_value elapsed_counter." << std::endl;
                    log(logfilename,o.str());
                    throw std::runtime_error(o.str());
                }

                if (this_busy_it == this_instance_it->second.end())
                {
                    std::ostringstream o;
                    o 	<< "pipe_driver_subthread.cpp: Computation of subsystem MP % busy as delta busy counter divided by delta elapsed counter -"
                        << "this gather instance " << this_instance_it->first << " does not have has metric_value busy_counter." << std::endl;
                    log(logfilename,o.str());
                    throw std::runtime_error(o.str());
                }

                if (last_busy_it == last_instance_it->second.end())
                {
                    std::ostringstream o;
                    o 	<< "pipe_driver_subthread.cpp: Computation of subsystem MP % busy as delta busy counter divided by delta elapsed counter -"
                        << "last gather instance " << last_instance_it->first << " does not have has metric_value busy_counter." << std::endl;
                    log(logfilename,o.str());
                    throw std::runtime_error(o.str());
                }

                uint64_t this_elapsed = this_elapsed_it->second.uint64_t_value("this_elapsed_it->second"); // throws std::invalid_argument
                uint64_t last_elapsed = last_elapsed_it->second.uint64_t_value("last_elapsed_it->second"); // throws std::invalid_argument
                uint64_t this_busy    = this_busy_it   ->second.uint64_t_value("this_busy_it   ->second"); // throws std::invalid_argument
                uint64_t last_busy    = last_busy_it   ->second.uint64_t_value("last_busy_it   ->second"); // throws std::invalid_argument

                std::ostringstream os;
                if (this_elapsed<= last_elapsed || this_busy < last_busy)
                {
                    std::ostringstream o;
                    o 	<< "pipe_driver_subthread.cpp: Computation of subsystem MP % busy as delta busy counter divided by delta elapsed counter - "
                        << "elapsed_counter this gather = " << this_elapsed << ", elapsed_counter last gather = " << last_elapsed
                        << ", busy_counter this gather = " << this_busy << ", busy_counter last gather = " << last_busy << std::endl
                        << "Elapsed counter did not increase, or busy counter decreased";
                    log(logfilename,o.str());
                    throw std::runtime_error(o.str());
                }
                std::ostringstream o;
                ivy_float busy_percent = 100. * ( ((ivy_float) this_busy) - ((ivy_float) last_busy) ) / ( ((ivy_float) this_elapsed) - ((ivy_float) last_elapsed) );
                o << std::fixed <<  std::setprecision(3) << busy_percent  << "%";
                {
                    auto& metric_value = this_instance_it->second["busy_percent"];
                    metric_value.start = this_elapsed_it->second.start;
                    metric_value.complete = this_elapsed_it->second.complete;
                    metric_value.value = o.str();
                    currentGD.data["MP_busy"]["all"][MP_core_instance_name] = metric_value;
                }
                auto this_MPU_it = this_instance_it->second.find(std::string("MPU"));
                if (this_MPU_it != this_instance_it->second.end())
                {
                    mpu_busy[this_MPU_it->second.value/* MPU as decimal digits HM800: 0-3, RAID800 0-15 */].push(busy_percent);
                }
            }

            for (auto& pear : mpu_busy)
            {
                std::ostringstream m;
                m << pear.first; // MPU number
                auto& mv = currentGD.data["MPU"][m.str()];
                {
                    std::ostringstream v;
                    v <<                                       pear.second.count();
                    mv["count"].value = v.str();
                }
                {
                    std::ostringstream v;
                    v << std::fixed << std::setprecision(3) << pear.second.avg() << "%";
                    mv["avg_busy"].value = v.str();
                }
                {
                    std::ostringstream v;
                    v << std::fixed << std::setprecision(3) << pear.second.min() << "%";
                    mv["min_busy"].value = v.str();
                }
                {
                    std::ostringstream v;
                    v << std::fixed << std::setprecision(3) << pear.second.max() << "%";
                    mv["max_busy"].value = v.str();
                }
            }
        } //  processing MP_core from 2nd gather on


        // post-process LDEV I/O data only starts from third subinterval, subinterval #1
        // because the t=0 gather is asynchronous from "Go!", so at the end of subinterval #0
        // there is no valid delta-DKC time yet.  The calculations work starting the 3rd subinterval.


        // Note that the suffix "_count" at the end of a metric name is significant.

        // Metric names ending in _count are [usually cumulative] counters that require some
        // further processing before they are "consumeable".

        // So in the code below, we create whatever derived metrics we want from the raw collected data, including the counts.

        // Metrics whose name ends in _count are ignored when printing csv file titles and data lines.

        // Even though the _count metrics don't print in by-step subsystem performance csv files, they are retained in the data structures
        // so that you can easily interpret the raw counter values over any time period.

        // This makes it easy for a MeasureController to get an average value over a subinterval sequence by looking at the values
        // of raw cumulative counters at the beginning and end of the sequence and dividing the delta by the elapsed time.

        // Note that the way ivy_cmddevice operates, cumulative counter values are monotonically increasing
        // from the time that the ivy_cmddev executable starts.  This is because ivy_cmddev extends every
        // RMLIB unsigned 32 bit cumulative counter value that is subject to rollovers to a 64-bit unsigned value
        // using as the upper 32 bits the number of rollovers that ivy_cmddev has seen for this particular counter.

        // For 64-bit unsigned RMLIB raw counters, ivy_cmddev reports a 64-bit unsigned int representing the
        // amount that the raw counter has incremented since the ivy_cmddev run started.

        m_s.rollups.not_participating.push_back(subsystem_summary_data());

        void rollup_Hitachi_RAID_data(const std::string&, Hitachi_RAID_subsystem*, GatherData& currentGD, GatherData& lastGD);

        if (p_Hitachi_RAID_subsystem->gathers.size() > 2) rollup_Hitachi_RAID_data(logfilename, p_Hitachi_RAID_subsystem, currentGD, lastGD);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


        // Filter & post subsystem performance data by RollupInstance

        //- As each gather is post-processed for a Subsystem, after we have posted derived metrics like MPU busy:
        //	- if gather is not for t=0
        //		- for each RollupType, for each RollupInstance
        //			- add a subsystem_summary_data object for this subinterval
        //			- for each summary element type ("MP_core", "CLPR", ...) ivy_engine::subsystem_summary_metrics
        //				- for each instance of the element, e.g. MP_core="MP10-00"
        //					- if the subsystem serial number and attribute name match the RollupInstance
        //						- for each metric
        //							- post values -.




    } // if (p_Hitachi_RAID_subsystem->gathers.size() > 1)
    else
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

    m_s.overallGatherTimeSeconds.push(tn_gather_time.getlongdoubleseconds());
    perSubsystemGatherTime.push(tn_gather_time.getlongdoubleseconds());
}
