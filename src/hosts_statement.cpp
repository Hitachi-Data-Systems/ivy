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
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <list>

#include "master_stuff.h"
#include "host_range.h"
#include "Stmt.h"

extern bool routine_logging;

void invokeThread(pipe_driver_subthread* T)
{
    T->threadRun();
}

bool Stmt_hosts::execute()
{
    if (trace_evaluate) { trace_ostream << "[hosts] statement at " << bookmark << ":" << std::endl; }

    if (m_s.haveHosts)
    {
        std::ostringstream o;

        o << "[Hosts] statement at " << bookmark << " - error - There may be only one [hosts] statement." << std::endl;
        p_Ivy_pgm->error(o.str());
    }

    if (phspl == nullptr)
    {
        std::ostringstream o;

        o << "[Hosts] statement at " << bookmark << " - internal programming error - null pointer to host_spec_pointer_list." << std::endl;
        p_Ivy_pgm->error(o.str());
    }
    else
    {
        get_host_list(phspl,&(m_s.hosts)); // this evaluates the string expressions and builds a std::list<std::string> with the results
    }

    std::ostringstream console_msg;

    extern std::string inter_statement_divider;

    console_msg << inter_statement_divider << std::endl << "[Hosts]";
    for (auto& s: m_s.hosts) console_msg << " " << s;
    console_msg << std::endl;
    std::cout << console_msg.str();
    log(m_s.masterlogfile, console_msg.str());

    m_s.write_clear_script();

    if (trace_evaluate)
    {
        trace_ostream << "[Hosts] statement - generated host list contains " << m_s.hosts.size() << " entries:";
        for (auto& s: m_s.hosts) trace_ostream << " " << s;
        trace_ostream << std::endl;
    }

    if (m_s.hosts.size() == 0)
    {
        std::ostringstream o;

        o << "[Hosts] statement at " << bookmark << " - error - No hosts specified." << std::endl;
        p_Ivy_pgm->error(o.str());
    }

    if (p_Select == nullptr)
    {
        std::ostringstream o;

        o << "[Hosts] statement at " << bookmark << " - error - missing [Select] clause." << std::endl;
        p_Ivy_pgm->error(o.str());
    }


    if (trace_evaluate)
    {
        if (p_Select == nullptr) trace_ostream << "Stmt_hosts has null pointer to [Select] clause.";
        else
        {
            trace_ostream << "[Hosts] statement at " << bookmark << " [Select] clause list contains ";

            if ( p_Select->p_SelectClause_pointer_list == nullptr)
            {
                trace_ostream << "null pointer to SelectClause pointer list." << std::endl;
            }
            else
            {
                trace_ostream << p_Select->p_SelectClause_pointer_list->size() << " entries:" << std::endl;

                for (auto& p : (*p_Select->p_SelectClause_pointer_list))
                {
                    trace_ostream << "\"" << p->attribute_name_token() << "\" is ";
                    auto l = p->attribute_value_tokens();
                    if (l.size() == 1)
                    {
                        trace_ostream << l.front();
                    }
                    else
                    {
                        trace_ostream << "{";
                        for (auto& s : l) { trace_ostream << " \"" << s << "\"";}
                        trace_ostream << "}";
                    }
                    trace_ostream << std::endl;
                }
            }
        }
    }

    if
    (
        (!p_Select->contains_attribute("serial_number"))
        &&
        (!p_Select->contains_attribute("vendor"))
    )
    {
        std::ostringstream o;

        o << "[Hosts] statement at " << bookmark << " - [Select] clause must specify either \"serial_number\" or \"vendor\"." << std::endl;
        p_Ivy_pgm->error(o.str());
    }

    for ( auto& host : m_s.hosts )
    {
        log( m_s.masterlogfile, std::string("Starting thread for ") + host + std::string("\n") );
        std::cout << "Starting thread for " << host << std::endl;
        pipe_driver_subthread* p_pipe_driver_subthread = new pipe_driver_subthread
            (
                host,
                m_s.outputFolderRoot,
                m_s.testName,
                m_s.testFolder+std::string("/logs")
            );
        m_s.host_subthread_pointers[host] = p_pipe_driver_subthread;
        m_s.threads.push_back(std::thread(invokeThread,p_pipe_driver_subthread));
    }
    ofstream ahl(m_s.testFolder+std::string("/all_discovered_LUNs.txt"));
    ofstream ahl_csv(m_s.testFolder+std::string("/all_discovered_LUNs.csv"));

    bool first_host {true};

    for (auto&  pear : m_s.host_subthread_pointers)
    {
        {
            std::unique_lock<std::mutex> u_lk(pear.second->master_slave_lk);

            std::chrono::system_clock::time_point leftnow {std::chrono::system_clock::now()};
            int timeout_seconds {10};
            if
            (
                pear.second->master_slave_cv.wait_until
                (
                    u_lk,
                    leftnow + std::chrono::seconds(timeout_seconds),
                    [&pear]() { return pear.second->startupComplete; }
                )
            )
            {
                if (pear.second->startupSuccessful)
                {
                    std::ostringstream o;
                    o << "pipe_driver_subthread successful fireup for host " << pear.first << std::endl;
                    std::cout << o.str();
                    log(m_s.masterlogfile,o.str());
                }
                else
                {
                    std::ostringstream o;
                    o << "Aborting - unsuccessful startup for ivyslave on host " << pear.first << ".  Reason: \"" << pear.second->commandErrorMessage << "\"." << std::endl;
                    o << std::endl << "Usually if try to run ivy again, it will work the next time.  Don\'t know why this happens." << std::endl;
                    o << std::endl << "Run the \"clear_hung_ivy_threads.sh\" script to clear any hung ivy / ivyslave / ivy_cmddev threads on the ivy master host and all test hosts." << std::endl;
                    std::cout << o.str();
                    log(m_s.masterlogfile,o.str());
                    u_lk.unlock();
                    m_s.kill_subthreads_and_exit(); // doesn't return
                }

            }
            else
            {
                std::ostringstream o;
                o << "Aborting - timeout waiting " << timeout_seconds << " seconds for pipe_driver_subthread fireup for host " << pear.first << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
                u_lk.unlock();
                m_s.kill_subthreads_and_exit(); // doesn't return
            }
        }


        {ostringstream o; o << "Subthread for \"" << pear.first << "\" posted startupComplete." << std::endl; std::cout  << o.str(); log(m_s.masterlogfile,o.str());}

        for (auto& pLUN : pear.second->thisHostLUNs.LUNpointers)
        {
            m_s.allDiscoveredLUNs.LUNpointers.push_back(pLUN);
            ahl << pLUN->toString() << std::endl;
        }
//*debug*/{ostringstream o; o << "Loop for \"" << pear.first << "\" about to copy on sample LUN." << std::endl; std::cout  << o.str(); log(masterlogfile,o.str());}

//*debug*/{ostringstream o; o << "Loop for \"" << pear.first << "\" HostSampleLUN=" << pear.second->HostSampleLUN.toString() << std::endl; std::cout  << o.str(); log(m_s.masterlogfile,o.str());}

        m_s.TheSampleLUN.copyOntoMe(&pear.second->HostSampleLUN);

        if (first_host) {ahl_csv << pear.second->lun_csv_header;}
        ahl_csv << pear.second->lun_csv_body.str();

        first_host = false;
    }
    ahl.close();
    ahl_csv.close();
    m_s.allThreadsSentTheLUNsTheySee=true;
    /*debug*/
    if (routine_logging){
        ostringstream o;
        o <<"allDiscoveredLUNs contains:"<<std::endl<< m_s.allDiscoveredLUNs.toString() <<std::endl;
        //std::cout<<o.str();
        log(m_s.masterlogfile,o.str());
    }
    else
    {
        ostringstream o;
        o <<"Discovered " << m_s.allDiscoveredLUNs.LUNpointers.size() << " LUNs on the test hosts." << std::endl;
        //std::cout<<o.str();
        log(m_s.masterlogfile,o.str());
    }

    if (0 == m_s.allDiscoveredLUNs.LUNpointers.size())
    {
        std::ostringstream o;
        o << m_s.ivyscript_line_number << ": \"" << m_s.ivyscript_line << "\"" << std::endl;
        o << "No LUNs were found on any of the hosts." << std::endl;
        std::cout << o.str();
        log (m_s.masterlogfile,o.str());
        m_s.ivyscriptIfstream.close();
        m_s.kill_subthreads_and_exit();
    }


    m_s.allDiscoveredLUNs.split_out_command_devices_into(m_s.commandDeviceLUNs);

    /*debug*/
    {
        ostringstream o;
        o <<"commandDeviceLUNs contains:"<<std::endl<< m_s.commandDeviceLUNs.toString() <<std::endl;
        //std::cout<<o.str();
        log(m_s.masterlogfile,o.str());
    }

    for (LUN* pLUN : m_s.allDiscoveredLUNs.LUNpointers)
    {
        if (p_Select->matches(pLUN))
        {
            m_s.availableTestLUNs.LUNpointers.push_back(pLUN);
        }
    }


    if ( 0 == m_s.availableTestLUNs.LUNpointers.size() )
    {
        std::ostringstream o;
        o << "No LUNs matched [hosts] statement [select] clause." << std::endl;
        //p_Select->display("",o);

        std::cout << o.str();
        log(m_s.masterlogfile,o.str());
        m_s.kill_subthreads_and_exit();
    }

    for (auto& pLUN : m_s.availableTestLUNs.LUNpointers) pLUN -> createNicknames();

    // Now we create the Subsystem objects

    for (auto& pLUN : m_s.availableTestLUNs.LUNpointers)
    {
        std::string serial_number = pLUN->attribute_value(std::string("serial_number"));
        trim(serial_number);

        Subsystem* pSubsystem;

        auto subsystemIt = m_s.subsystems.find(serial_number);
        if (m_s.subsystems.end() == subsystemIt)
        {
            std::string product = pLUN->attribute_value("product");
            trim(product);
            if (0 == product.compare(std::string("OPEN-V")))
            {
                pSubsystem = new Hitachi_RAID_subsystem(serial_number, pLUN);
            }
            else
            {
                pSubsystem = new Subsystem(serial_number);
            }
            m_s.subsystems[serial_number] = pSubsystem;
        }
        else
        {
            pSubsystem = (*subsystemIt).second;
        }
        pSubsystem->add(pLUN);
    }

    // For each subsystem that the available test LUNs map to,
    // for the first available real-time interface,
    // start up the appropriate subthread to drive that interface.

    // Which means - for each Hitachi RAID subsystem, use the first command device
    // in commandDeviceLUNs that leads to that subsystem and start an
    // pipe_driver_subthread, which fires up the ivy_cmddev executable remotely via ssh
    // communicating via stdin/stdout pipes the same way we communicate with remote ivyslave instances.

    for (auto& pear : m_s.subsystems)
    {
        Subsystem* pSubsystem = pear.second;

        if (0 == std::string("Hitachi RAID").compare(pSubsystem->type()))
        {
            Hitachi_RAID_subsystem* pRAIDsubsystem {(Hitachi_RAID_subsystem*) pSubsystem};
            bool have_cmd_dev {false};

            for (auto& pL : m_s.commandDeviceLUNs.LUNpointers)
                // for each command device LUN as candidate to match as first command device for this subsystem.
            {
                if (0 == pSubsystem->serial_number.compare(pL->attribute_value("serial_number")))
                    // we have found the first command device for this subsystem
                {
                    // fire up command device thread

                    std::string cmd_dev_description;

                    {
                        std::ostringstream o;
                        o << "command device for " << pL->attribute_value("Hitachi_product") << " " << pL->attribute_value("HDS_product")
                            <<  " serial number " << pSubsystem->serial_number
                            << " on host = " << pL->attribute_value("ivyscript_hostname")
                            << ", subsystem port = " << pL->attribute_value("port")
                            << ", LDEV = " << pL->attribute_value("LDEV")
                            << std::endl;
                        cmd_dev_description = o.str();
                    }

                    std::string cmddevLDEV = pL->attribute_value("LDEV");
                    if (cmddevLDEV.length() >= 5 && ':' == cmddevLDEV[2]) cmddevLDEV.erase(2,1);

                    std::ostringstream o;
                    o << "Connecting to " << cmd_dev_description << std::endl;
                    std::cout << o.str();
                    log(m_s.masterlogfile,o.str());

                    pipe_driver_subthread* p_pipe_driver_subthread = new pipe_driver_subthread(

                        pL->attribute_value("ivyscript_hostname")
                        , m_s.outputFolderRoot
                        , m_s.testName,
                        m_s.testFolder+std::string("/logs")
                    );

                    p_pipe_driver_subthread->pCmdDevLUN = pL;  // Seeing this set tells the subthread it's running a command device.

                    p_pipe_driver_subthread->p_Hitachi_RAID_subsystem = pRAIDsubsystem;

                    pRAIDsubsystem->command_device_description = cmd_dev_description;

                    pRAIDsubsystem->pRMLIBthread=p_pipe_driver_subthread;

                    m_s.ivymaster_RMLIB_threads.push_back(std::thread(invokeThread,p_pipe_driver_subthread));
                    // Note: I wasn't sure, since std::thread is not copyable but is moveable, how to handle
                    //       the case when the command device doesn't start up correctly.  So in the interest of
                    //       time, I left any unsuccessful-startup command device threads in moribund state
                    //       in m_s.ivymaster_RMLIB_threads to be joined when ivy shuts down.

                    // wait with timeout for thread status to show "startupComplete", i.e. waiting for command;

                    {
                        std::unique_lock<std::mutex> u_lk(p_pipe_driver_subthread->master_slave_lk);
                        std::chrono::system_clock::time_point leftnow {std::chrono::system_clock::now()};
                        int timeout_seconds {5};

                        if
                        (
                            p_pipe_driver_subthread->master_slave_cv.wait_until
                            (
                                u_lk,
                                leftnow + std::chrono::seconds(timeout_seconds),
                                [&p_pipe_driver_subthread](){return p_pipe_driver_subthread->startupComplete;}
                        )
                    )
                        {
                            if (p_pipe_driver_subthread->startupSuccessful)
                            {
                                std::ostringstream o;
                                o << "ivy_cmddev pipe_driver_subthread successful fireup for subsystem " << pSubsystem->serial_number << std::endl;
                                std::cout << o.str();
                                log(m_s.masterlogfile,o.str());
                                m_s.haveCmdDev = true;
                                have_cmd_dev = true;
                            }
                            else
                            {
                                std::ostringstream o;
                                o   << "Unsuccessful startup for command device = " << cmd_dev_description << std::endl
                                    << "Reason - " <<  p_pipe_driver_subthread->commandErrorMessage << std::endl;
                                std::cout << o.str();
                                log(m_s.masterlogfile,o.str());
                                pRAIDsubsystem->command_device_description.clear();
                                pRAIDsubsystem->pRMLIBthread=nullptr;
                            }
                        }
                        else
                        {
                            std::ostringstream o;
                            o   << "Timeout waiting " << timeout_seconds << " seconds for for startup of command device = " << cmd_dev_description << std::endl;
                            std::cout << o.str();
                            log(m_s.masterlogfile,o.str());
                            pRAIDsubsystem->command_device_description.clear();
                            pRAIDsubsystem->pRMLIBthread=nullptr;
                        }
                    }

                    if (!have_cmd_dev) continue;  // This command device did not start up OK - try the next one if we have another to the same subsystem.
                                                  // Maybe there is a command device perhaps on a different host that does have RMLIB installed,
                                                  // does have a valid license key installed, and does have the ivy_cmddev executable.


                    // Command device reports startup complete

                    // gather config

                    ivytime config_gather_start;
                    config_gather_start.setToNow();
                    std::chrono::system_clock::time_point
                    start_getconfig_time_point { std::chrono::system_clock::now() };

                    {
                        std::unique_lock<std::mutex> u_lk(p_pipe_driver_subthread->master_slave_lk);
                        p_pipe_driver_subthread->commandString = std::string("get config");
                        p_pipe_driver_subthread->command=true;
                        p_pipe_driver_subthread->commandComplete=false;
                        p_pipe_driver_subthread->commandSuccess=false;
                        p_pipe_driver_subthread->commandErrorMessage.clear();

                        {
                            std::ostringstream o;
                            o << "Posted \"get config\" to thread for subsystem serial " << pRAIDsubsystem->serial_number << " managed on host " << p_pipe_driver_subthread->ivyscript_hostname << '.' << std::endl;
                            log(m_s.masterlogfile,o.str());
                        }
                    }
                    p_pipe_driver_subthread->master_slave_cv.notify_all();

                    {
                        std::unique_lock<std::mutex> u_lk(p_pipe_driver_subthread->master_slave_lk);

                        if
                        (
                            p_pipe_driver_subthread->master_slave_cv.wait_until
                            (
                                u_lk,
                                start_getconfig_time_point + std::chrono::seconds(get_config_timeout_seconds /* see ivydefines.h */),
                                [&p_pipe_driver_subthread](){return p_pipe_driver_subthread->commandComplete;}
                            )
                        )
                        {
                            if (!p_pipe_driver_subthread->commandSuccess)
                            {
                                std::ostringstream o;
                                o << "\"get config\" to thread for subsystem serial " << pRAIDsubsystem->serial_number
                                  << " managed on host " << p_pipe_driver_subthread->ivyscript_hostname << " failed.  Aborting." << std::endl;
                                log(m_s.masterlogfile,o.str());
                                m_s.kill_subthreads_and_exit();
                            }

                            ivytime config_gather_complete;
                            config_gather_complete.setToNow();

                            pRAIDsubsystem->config_gather_time  = config_gather_complete - config_gather_start;

                            {
                                std::ostringstream o;
                                o << "\"get config\" reported complete with duration " << pRAIDsubsystem->config_gather_time.format_as_duration_HMMSSns()
                                  << " by thread for subsystem serial " << pRAIDsubsystem->serial_number << " managed on host " << p_pipe_driver_subthread->ivyscript_hostname << '.' << std::endl;
                                log(m_s.masterlogfile,o.str());
                            }
                        }
                        else // timeout
                        {
                            std::ostringstream o;
                            o << "Aborting - timeout waiting " << get_config_timeout_seconds
                              << " seconds for pipe_driver_subthread for subsystem " << pRAIDsubsystem->serial_number
                              << " managed on host " << p_pipe_driver_subthread->ivyscript_hostname
                              << " to complete a \"get config\"." <<  std::endl;
                            std::cout << o.str();
                            log(m_s.masterlogfile,o.str());
                            u_lk.unlock();
                            m_s.kill_subthreads_and_exit();
                        }
                    }

                    // post process after gather to augment available test LUN attributes with LDEV attributes from the config gather

                    for (auto pLUN : m_s.availableTestLUNs.LUNpointers)
                    {
                        if ( 0 == std::string("Hitachi RAID").compare(pSubsystem->type())  &&  0 == pSubsystem->serial_number.compare(pLUN->attribute_value("serial_number")))
                        {

                            // for each test LUN, if we are processing a Hitachi RAID subsystem with a given serial number and the test LUN serial_number matches the subsystem
                            // 1) if CLPR is non-empty add the <CLPR,serial_number> pair to cooldown_WP_watch_set
                            // 2) copy the LDEV's RMLIB attributes to the test LUN.
                            // put a suffix "_RMLIB" if the test LUN already has the attribute name (from the SCSI Inquiry LUN lister tool" showluns.sh").

                            if (0 < pLUN->attribute_value("CLPR").length())
                            {
                                m_s.cooldown_WP_watch_set.insert(std::make_pair(pLUN->attribute_value("CLPR"),pSubsystem));
                            }


                            // Copy the LDEV attributes obtained from a command device to the corresponding available test LUN,
                            // appending "_RMLIB" upon attribute name collisions.

                            // NOTE - in the LUN object all map keys are first translated to lower case upon insertion.
                            //        It is advised to use the LUN's own functions to do lookups, attribute value matching, etc.
                            //        as then you won't need to worry about case sensitivity of attribute names.

                            // NOTE2 - The data returned by ivy_cmddev for a "get config" command has attribute names like LDEV
                            //         in upper case and the LDEV attribute values like 00:FF are provided in upper case.


                            if (!pLUN->contains_attribute_name(std::string("ldev")))
                            {
                                std::ostringstream o;
                                o << "Aborting - available test LUN matched on Hitachi RAID subsystem serial number " << pSubsystem->serial_number
                                  << ", but the LUN did have the \"ldev\" attribute." <<  std::endl
                                  << "  LUN =" << pLUN->toString() <<  std::endl;
                                std::cout << o.str();
                                log(m_s.masterlogfile,o.str());
                                m_s.kill_subthreads_and_exit();
                            }

                            std::string LUN_ldev = pLUN->attribute_value(std::string("ldev"));

                            if (0==LUN_ldev.size())
                            {
                                std::ostringstream o;
                                o << "Aborting - available test LUN matched on Hitachi RAID subsystem serial number " << pSubsystem->serial_number
                                  << ", but the LUN's \"ldev\" attribute value was the null string." <<  std::endl
                                  << "  LUN =" << pLUN->toString() <<  std::endl;
                                std::cout << o.str();
                                log(m_s.masterlogfile,o.str());
                                m_s.kill_subthreads_and_exit();
                            }

                            auto subsystemLDEVit = pRAIDsubsystem->configGatherData.data.find(std::string("LDEV"));
                            if (pRAIDsubsystem->configGatherData.data.end() == subsystemLDEVit)
                            {
                                std::ostringstream o;
                                o << "Aborting - available test LUN matched on Hitachi RAID subsystem serial number " << pSubsystem->serial_number
                                  << ", but the subsystem configuration data from a command device didn\'t have \"LDEV\" element type." <<  std::endl
                                  << "  LUN =" << pLUN->toString() <<  std::endl;
                                std::cout << o.str();
                                log(m_s.masterlogfile,o.str());
                                m_s.kill_subthreads_and_exit();
                            }

                            std::string cmddev_LDEV = toUpper(LUN_ldev);

                            auto subsystemLDEVinstanceIt = (*subsystemLDEVit).second.find(cmddev_LDEV);
                            if ((*subsystemLDEVit).second.end() == subsystemLDEVinstanceIt)
                            {
                                std::ostringstream o;
                                o << "Aborting - available test LUN matched on Hitachi RAID subsystem serial number " << pSubsystem->serial_number
                                  << ", but the subsystem didn\'t have the \"LDEV\" instance \"" << cmddev_LDEV << "\"." <<  std::endl
                                  << "  LUN =" << pLUN->toString() <<  std::endl;
                                std::cout << o.str();
                                log(m_s.masterlogfile,o.str());
                                m_s.kill_subthreads_and_exit();
                            }

                            for (auto& pear : (*subsystemLDEVinstanceIt).second)
                            {

                                // To reduce clutter, upon an attribute name collision, meaning that both the SCSI Inquiry-based LUN Lister tool like showluns.sh
                                // and the RMLIB API-based command device data returned an attribute name that when translated to lower case was the same,
                                // then if the attribute values from either side are the same when translated to lower case and with leading/trailing
                                // blanks removed, we don't alter the existing attribute value, and the RMLIB API value is discarded.

                                // If RMLIB returned a substantially different value for the same metric, then we create a new attribute name
                                // by appending "_rmlib" and store the RMLIB value there.

                                // pear is std::map<std::string,metric_value>

                                std::string attribute_name = toLower(pear.first);
                                std::string attribute_value = pear.second.string_value();

                                std::string trimmed_attribute_value = toLower(attribute_value);
                                trim(trimmed_attribute_value);

                                if (pLUN->contains_attribute_name(attribute_name))
                                {
                                    std::string trimmed_LUN_value = toLower(pLUN->attribute_value(attribute_name));
                                    trim(trimmed_LUN_value);
                                    if (0 != trimmed_attribute_value.compare(trimmed_LUN_value))
                                    {
                                        attribute_name += std::string("_rmlib");
                                        pLUN->set_attribute(attribute_name, attribute_value);
                                    }
                                }
                                else
                                {
                                    pLUN->set_attribute(attribute_name, attribute_value);
                                }

                            }
                        }
                    }

                    // end of augmenting available test LUN attributes with config info from the command device

                    break;  // out of loop over command device LUNs as candidates to be first command device for this subsystem


                } // if we found the first command device for this subsystem


            } // for each command device LUN as candidate to match as first command device for this subsystem.

            // back to Hitachi RAID subsystem type

            if (!have_cmd_dev)
            {
                std::ostringstream o;
                o << "No command device found for subsystem " << pSubsystem->serial_number << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
            }

            pRAIDsubsystem->configGatherData.print_csv_file_set
            (
                m_s.testFolder,
                pRAIDsubsystem->serial_number+std::string(".config")
            );

        } // end for Hitachi RAID subsystem

    } // end for each subsystem

    for (auto& pLUN : m_s.availableTestLUNs.LUNpointers)
    {
        m_s.TheSampleLUN.copyOntoMe(pLUN);  // add nicknames, and command device LDEV attributes to TheSampleLUN.
    }


    for (auto& pLUN : m_s.availableTestLUNs.LUNpointers) // populate test config thumbnail data structures
    {
        m_s.available_LUNs_thumbnail.add(pLUN);
    }

    {
        std::ostringstream o;
        o << m_s.available_LUNs_thumbnail;

        bool saw_command_device {false};

        for (auto& pear : m_s.subsystems)
        {
            if (0 == std::string("Hitachi RAID").compare(pear.second->type()))
            {
                Hitachi_RAID_subsystem* pRAID = (Hitachi_RAID_subsystem*) pear.second;
                if (pRAID->command_device_description.size() > 0)
                {
                    saw_command_device = true;
                    o << pRAID->command_device_description;
                }
            }
        }

        if (saw_command_device) o << std::endl;

        log(m_s.masterlogfile,o.str());
        std::cout << o.str();

        ofstream ofs(m_s.testFolder+std::string("/available_test_config.txt"));
        ofs << o.str();
        ofs.close();

    }

    if (routine_logging)
    {
        ostringstream o;
        o << "After adding subsystem config attributes, availableTestLUNs contains:" << std::endl << m_s.availableTestLUNs.toString() << std::endl;
        // std::cout << o.str();
        log(m_s.masterlogfile,o.str());
    }
    else
    {
        ostringstream o;
        o <<"availableTestLUNs contains " << m_s.availableTestLUNs.LUNpointers.size() << " LUNs." << std::endl;
        //std::cout<<o.str();
        log(m_s.masterlogfile,o.str());
    }


    ofstream atlcsv(m_s.testFolder+std::string("/available_test_LUNs.csv"));
    m_s.availableTestLUNs.print_csv_file(atlcsv);
    atlcsv.close();

    if (m_s.cooldown_WP_watch_set.size() == 0)

    {
        std::ostringstream o;
        o << "No command devices for RAID_subsystem LDEVs, so cooldown_by_wp settings will not have any effect." << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
    }
    else
    {
        std::ostringstream o;
        o << "Cooldown_WP_watch_set contains:";
        for (auto& pear : m_s.cooldown_WP_watch_set) o << " < " << pear.first << ", " << pear.second->serial_number << " >";
        o << std::endl << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
    }

    m_s.haveHosts = true;

    return false; // false means "no return statement was executed"
}


