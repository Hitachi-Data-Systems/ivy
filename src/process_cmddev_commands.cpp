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

extern bool routine_logging;

void pipe_driver_subthread::process_cmddev_commands(std::unique_lock<std::mutex>& s_lk)
{
    // process commands to interact with a remote ivy_cmddev CLI
    // indent level processing command device commands
    ivytime start;
    start.setToNow();

    if (0==std::string("get config").compare(commandString))
    {
//        std::cout << "Getting configuration from " << p_Hitachi_RAID_subsystem->command_device_description << "\n";
        ivytime gatherStart;
        gatherStart.setToNow();

        try
        {
            send_and_get_OK("get config");

            try
            {
                process_ivy_cmddev_response(p_Hitachi_RAID_subsystem->configGatherData);
            }
            catch (std::invalid_argument& iaex)
            {
                std::ostringstream o;
                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get config\"), std::invalid_argument saying \"" << iaex.what() << "\"." << std::endl;
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
                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get config\"), std::runtime_error saying \"" << reex.what() << "\"." << std::endl;
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
            o << "\"get config\" command sent to ivy_cmddev failed saying \"" << reex.what() << "\"." << std::endl;
            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
            kill_ssh_and_harvest();
            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
            master_slave_cv.notify_all();
            return;
        }
        commandComplete=true;
        commandSuccess=true;
        ivytime end;
        end.setToNow();
        /* ivytime */ duration = end-start;

//        std::cout << "Configuration gather from " << p_Hitachi_RAID_subsystem->command_device_description << " completed in " << duration.format_as_duration_HMMSSns()<< ".\n";
//        std::cout << "Post-processing configuration information from " << p_Hitachi_RAID_subsystem->command_device_description << ".\n";


        // Post process config gather

        auto element_it = p_Hitachi_RAID_subsystem->configGatherData.data.find("LDEV");
        if (element_it == p_Hitachi_RAID_subsystem->configGatherData.data.end())
        {
            std::ostringstream o;
            o << "pipe_driver_subthread for remote ivy_cmddev CLI - post-processing configuration gather data, gathered configuration did not have the LDEV element type" << std::endl;
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
        for (auto& LDEV_pear : element_it->second)
        {
            // for every LDEV in the subsystem config, if it's a Pool Vol, insert it in the list of pool vols

            const std::string& subsystem_LDEV = LDEV_pear.first;
//                            std::map
//                            <
//                                std::string, // Metric, e.g. "drive type"
//                                metric_value
//                            >& subsystem_LDEV_metrics = LDEV_pear.second;

            //{std::ostringstream o; o << "Looking for pool vols, examining " << subsystem_LDEV << std::endl;log(logfilename,o.str());}

            auto metric_it = LDEV_pear.second.find("Pool_Vol");

            if (metric_it != LDEV_pear.second.end())
            {
                //{std::ostringstream o; o << "it's a pool vol. " << std::endl; log(logfilename,o.str());}
                const std::string& pool_ID = LDEV_pear.second["Pool ID"].string_value();
                std::pair
                <
                    const std::string, // Configuration element instance, e.g. 00:FF
                    std::map
                    <
                        std::string, // Metric, e.g. "drive type"
                        metric_value
                    >
                >*p;

                p = & LDEV_pear;

                if (routine_logging)
                {
                    std::ostringstream o;
                    o << "Inserting Pool Vol " << subsystem_LDEV << " into pool_vols[pool ID \"" << pool_ID << "\"]." << std::endl;
                    log(logfilename,o.str());
                }

                p_Hitachi_RAID_subsystem->pool_vols[pool_ID].insert(p);
            }
            //else
            //{
            //    {std::ostringstream o; o << "it's not a pool vol. " << std::endl; log(logfilename,o.str());}
            //}
        }

        if (routine_logging)
        {
            std::ostringstream o;
            o << "pool_vols.size() = " << p_Hitachi_RAID_subsystem->pool_vols.size() << std::endl;
            for (auto& pear : p_Hitachi_RAID_subsystem->pool_vols)
            {
                const std::string& poolid = pear.first;
                std::set     // Set of pointers to pool volume instances in configGatherData
                <
                    std::pair
                    <
                        const std::string, // Configuration element instance, e.g. 00:FF
                        std::map
                        <
                            std::string, // Metric, e.g. "total I/O count"
                            metric_value
                        >
                    >*
                >& pointers = pear.second;

                o << "Pool ID \"" << poolid << "\", Pool Vols = " << std::endl;

                for (auto& p : pointers)
                {
                    o << "Pool ID \"" << poolid << "\", Pool Vol = " << p->first << ", Drive type = " << p->second["Drive type"].string_value() << std::endl;
                }
            }
            log(logfilename,o.str());
        }

        // Until such time as we print the GatherData csv files, we log to confirm at least the gather is working

        if (routine_logging)
        {
            std::ostringstream o;
            o << "Completed config gather for subsystem serial_number=" << p_Hitachi_RAID_subsystem->serial_number << ", gather time = " << duration.format_as_duration_HMMSSns() << std::endl;
            //o << p_Hitachi_RAID_subsystem->configGatherData;
            log(logfilename, o.str());
        }

        ivytime gatherEnd; gatherEnd.setToNow();
        getConfigTime.push(ivytime(gatherEnd - gatherStart).getlongdoubleseconds());

        s_lk.unlock();
        master_slave_cv.notify_all();

        // "get config" - end
    }
    else if (0==std::string("get failed_component").compare(commandString))
    {
        send_and_get_OK("get failed_component");

        std::string component_failure_line = get_line_from_pipe(ivytime(5),"get failed_component");

        if      (startsWith(component_failure_line,"<Good>"   )) { commandSuccess = true;}
        else if (startsWith(component_failure_line,"<Failure>")) { commandSuccess = false; commandErrorMessage = component_failure_line; }
        else
        {
            std::ostringstream o;
            o << "pipe_driver_subthread for remote ivy_cmddev CLI - issued \"get failed_component\", and did not get <Good> or <Failure>, instead saw \"" << component_failure_line << "\"." << std::endl;
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

        commandComplete=true;
        ivytime end;
        end.setToNow();
        /* ivytime */ duration = end-start;
        s_lk.unlock();
        master_slave_cv.notify_all();
    }
    else if (0==std::string("t=0 gather").compare(commandString))
    {
        pipe_driver_gather(s_lk,true);
    }
    else if (0==std::string("gather").compare(commandString))
    {
        pipe_driver_gather(s_lk,false);
    }
    else
    {
        // if we do not recognize the command
        std::ostringstream o;
        o << "ivy_cmddev pipe_driver_subthread failed to recognize ivymaster command \"" << commandString << "\"." << std::endl;
        std::cout << o.str();
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
