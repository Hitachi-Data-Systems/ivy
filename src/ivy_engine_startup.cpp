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


#include <sys/stat.h>
#include <cstring>

#include "ivybuilddate.h"
#include "ivy_engine.h"
#include "hosts_list.h"

extern bool routine_logging;
void initialize_io_time_clip_levels();

void invokeThread(pipe_driver_subthread* T)
{
    T->threadRun();
}


std::pair<bool /*success*/, std::string /* message */>
    ivy_engine::startup(
            const std::string& output_folder_root,
            const std::string& test_name,
            const std::string& ivyscript_filename,
            const std::string& hosts_string,
            const std::string& select_available_test_LUNs)
{
    std::string api_log_entry;
    {
        std::ostringstream o;
        o << "ivy engine API startup("
            << "output_folder_root = "   << output_folder_root
            << ", test_name = "          << test_name
            << ", ivyscript_filename = " << ivyscript_filename
            << ", test_hosts = "         << hosts_string
            << ", select = "             << select_available_test_LUNs
            << ")" << std::endl;
        std::cout << o.str();
        api_log_entry = o.str();
    }

    outputFolderRoot = output_folder_root;
    testName = test_name;

    struct stat struct_stat;

    if( stat(outputFolderRoot.c_str(),&struct_stat))
    {
        std::ostringstream o;
        o << "<Error> directory \"" << outputFolderRoot << "\" does not exist." << std::endl;
        std::cout << o.str();
        return std::make_pair(false,o.str());
    }

    if(!S_ISDIR(struct_stat.st_mode))
    {
        std::ostringstream o;
        o << "<Error> \"" << outputFolderRoot << "\" is not a directory." << std::endl;
        std::cout << o.str();
        return std::make_pair(false,o.str());
    }

    testFolder=outputFolderRoot + std::string("/") + testName;

    if(0==stat(testFolder.c_str(),&struct_stat))  // output folder already exists
    {
        if(!S_ISDIR(struct_stat.st_mode))
        {
            std::ostringstream o;
            o << "<Error> Output folder for this test run \"" << testFolder << "\" already exists but is not a directory." << std::endl;
            std::cout << o.str();
            return std::make_pair(false,o.str());
        }
        // output folder already exists and is a directory, so we delete it to make a fresh one.
        if (0 == system((std::string("rm -rf ")+testFolder).c_str()))   // ugly but easy.
        {
            std::ostringstream o;
            o << "      Deleted pre-existing folder \"" << testFolder << "\"." << std::endl;
            std::cout << o.str();

        }
        else
        {
            std::ostringstream o;
            o << "<Error> Failed trying to delete previously existing version of test run output folder \"" << testFolder << "\"." << std::endl;
            std::cout << o.str();
            return std::make_pair(false,o.str());
        }

    }

    if (mkdir(testFolder.c_str(),S_IRWXU | S_IRWXG | S_IRWXO))
    {
        std::ostringstream o;
        o << "<Error> Failed trying to create output folder \"" << testFolder << "\" << errno = " << errno << " " << strerror(errno) << std::endl;
        std::cout << o.str();
        return std::make_pair(false,o.str());
    }

    std::cout << "      Created test run output folder \"" << testFolder << "\"." << std::endl;

    if (mkdir((testFolder+std::string("/logs")).c_str(),S_IRWXU | S_IRWXG | S_IRWXO))
    {
        std::ostringstream o;
        o << "<Error> - Failed trying to create logs subfolder in output folder \"" << testFolder << "\" << errno = " << errno << " " << strerror(errno) << std::endl;
        std::cout << o.str();
        return std::make_pair(false,o.str());
    }

    masterlogfile = testFolder + std::string("/logs/log.ivymaster.txt");
    ivy_engine_logfile = testFolder + std::string("/logs/ivy_engine_API_calls.txt");


    log(masterlogfile,      api_log_entry);
    log(ivy_engine_logfile, api_log_entry);

    {
        std::ostringstream o;
        o << "ivy version " << ivy_version << " build date " << IVYBUILDDATE << " starting." << std::endl << std::endl;
        log(masterlogfile,o.str());
    }

    if (!routine_logging)
    {
        std::ostringstream o;
        o << "For logging of routine (non-error) events, and to record the conversation with each ivydriver/ivy_cmddev, use the ivy -log command line option, like \"ivy -log a.ivyscript\".\n\n";
        log(masterlogfile,o.str());
    }

    if (ivyscript_filename.size() > 0)
    {
        std::string copyivyscriptcmd = std::string("cp -p ") + ivyscript_filename + std::string(" ") +
                                       testFolder + std::string("/") + testName + std::string(".ivyscript");
        if (0!=system(copyivyscriptcmd.c_str()))   // now getting lazy, but purist maintainers could write C++ code to do this.
        {
            std::ostringstream o;
            o << "<Error> Failed trying to copy input ivyscript to output folder: \"" << copyivyscriptcmd << "\"." << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            return std::make_pair(false,o.str());
        }
    }

    std::pair<bool,std::string> retval = rollups.initialize();
    if (!retval.first)
    {
        std::ostringstream o;
        o << "<Error> Internal programming error - failed initializing rollups in ivymaster.cpp saying: " << retval.second << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        return std::make_pair(false,o.str());
    }

    test_start_time.setToNow();

    initialize_io_time_clip_levels();

    availableControllers[toLower(std::string("measure"))] = &the_dfc;

    auto rv = random_steady_template.setParameter("iosequencer=random_steady");
    if ( ! rv.first)
    {
        std::ostringstream o;
        o << "<Error> dreaded internal programming error - ivymaster startup - failed trying to set the default random steady I/O generator template - " << rv.second << std::endl;
        std::cout << o.str();
        return std::make_pair(false,o.str());
    }

    rv = random_independent_template.setParameter("iosequencer=random_independent");
    if ( !rv.first )
    {
        std::ostringstream o;
        o << "<Error> dreaded internal programming error - ivymaster startup - failed trying to set the default random independent I/O generator template - " << rv.second << std::endl;
        std::cout << o.str();
        return std::make_pair(false,o.str());
    }

    rv = sequential_template.setParameter("iosequencer=sequential");
    if ( !rv.first )
    {
        std::ostringstream o;
        o << "<Error> dreaded internal programming error - ivymaster startup - failed trying to set the default sequential I/O generator template - " << rv.second << std::endl;
        std::cout << o.str();
        return std::make_pair(false,o.str());
    }

    hosts_list test_hosts;
    if (!test_hosts.compile(hosts_string))
    {
        std::ostringstream o;

        o << "<Error> failed parsing list of test hosts \"" << hosts_string << "\" - " << test_hosts.message;
        return std::make_pair(false,o.str());
    }

    for (auto& s: test_hosts.hosts) { hosts.push_back(s);}

    write_clear_script();  // This writes a shell script to run the clear_hung_ivy_threads executable on all involved hosts - handy if you are a developer and you mess things up

    if (hosts.size() == 0)
    {
        std::ostringstream o;

        o << "<Error> ivy engine startup failed - No hosts specified." << std::endl;
        return std::make_pair(false,o.str());
    }

    JSON_select select;

    if (!select.compile_JSON_select(select_available_test_LUNs))
    {
        std::ostringstream o;
        o << "<Error> ivy engine startup failure - invalid select expression for available test LUNs \"" << select_available_test_LUNs << "\" - error message follows: " << select.error_message << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }

    { std::ostringstream o; o << "select " << select << std::endl; std::cout << o.str(); log(masterlogfile,o.str()); }

    if
    (
        (!select.contains_attribute("serial_number"))
        &&
        (!select.contains_attribute("vendor"))
    )
    {
        std::ostringstream o;

        o << "<Error> ivy engine startup failure -  select for available test LUNs \"" << select_available_test_LUNs << "\" - must specify either \"serial_number\" or \"vendor\"." << std::endl;
        return std::make_pair(false,o.str());
    }

    for ( auto& host : hosts )
    {
        if (routine_logging) { std::cout << "Starting thread for " << host << std::endl; }
        log( masterlogfile, std::string("Starting thread for ") + host + std::string("\n") );

        pipe_driver_subthread* p_pipe_driver_subthread = new pipe_driver_subthread
            (
                host,
                outputFolderRoot,
                testName,
                testFolder+std::string("/logs")
            );
        host_subthread_pointers[host] = p_pipe_driver_subthread;
        threads.push_back(std::thread(invokeThread,p_pipe_driver_subthread));
    }
    ofstream ahl(testFolder+std::string("/all_discovered_LUNs.txt"));
    ofstream ahl_csv(testFolder+std::string("/all_discovered_LUNs.csv"));

    {
        std::ostringstream o;
        o << std::endl << "Note:" << std::endl
            << "Sometimes the ssh command to start up ivydriver on a test host can take a long time when waiting for DNS timeouts.  "
            << "This can be speeded up by editing /etc/nsswitch.conf / resolv.conf to use /etc/hosts first, or options for the sshd daemon can be edited; search for \"ssh login timeout\"." << std::endl << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
    }

    bool first_host {true};

    for (auto&  pear : host_subthread_pointers)
    {
        {
            std::unique_lock<std::mutex> u_lk(pear.second->master_slave_lk);

            std::chrono::system_clock::time_point leftnow {std::chrono::system_clock::now()};
            if
            (
                pear.second->master_slave_cv.wait_until
                (
                    u_lk,
                    leftnow + std::chrono::seconds(ivy_ssh_timeout),
                    [&pear]() { return pear.second->startupComplete || pear.second->dead; }
                )
            )
            {
                if (pear.second->startupSuccessful)
                {
                    std::ostringstream o;
                    o << "pipe_driver_subthread successful fireup for host " << pear.first << std::endl;
                    std::cout << o.str();
                    log(masterlogfile,o.str());
                }
                else
                {
                    std::ostringstream o;
                    o << "Aborting - unsuccessful startup for ivydriver on host " << pear.first << ".  Reason: \"" << pear.second->commandErrorMessage << "\"." << std::endl;
                    o << std::endl << "Usually if try to run ivy again, it will work the next time.  Don\'t know why this happens." << std::endl;
                    o << std::endl << "Run the \"clear_hung_ivy_threads.sh\" script to clear any hung ivy / ivydriver / ivy_cmddev threads on the ivy master host and all test hosts." << std::endl;
                    std::cout << o.str();
                    log(masterlogfile,o.str());
                    u_lk.unlock();
                    kill_subthreads_and_exit(); // doesn't return
                }

            }
            else
            {
                std::ostringstream o;
                o << "Aborting - timeout waiting " << timeout_seconds << " seconds for pipe_driver_subthread fireup for host " << pear.first << std::endl;
                std::cout << o.str();
                log(masterlogfile,o.str());
                u_lk.unlock();
                kill_subthreads_and_exit(); // doesn't return
            }
        }


        {ostringstream o; o << "Subthread for \"" << pear.first << "\" posted startupComplete." << std::endl; std::cout  << o.str(); log(masterlogfile,o.str());}

        for (auto& pLUN : pear.second->thisHostLUNs.LUNpointers)
        {
            allDiscoveredLUNs.LUNpointers.push_back(pLUN);
            ahl << pLUN->toString() << std::endl;
        }
//*debug*/{ostringstream o; o << "Loop for \"" << pear.first << "\" about to copy on sample LUN." << std::endl; std::cout  << o.str(); log(masterlogfile,o.str());}

//*debug*/{ostringstream o; o << "Loop for \"" << pear.first << "\" HostSampleLUN=" << pear.second->HostSampleLUN.toString() << std::endl; std::cout  << o.str(); log(masterlogfile,o.str());}

        TheSampleLUN.copyOntoMe(&pear.second->HostSampleLUN);

        if (first_host) {ahl_csv << pear.second->lun_csv_header;}
        ahl_csv << pear.second->lun_csv_body.str();

        first_host = false;
    }
    ahl.close();
    ahl_csv.close();
    allThreadsSentTheLUNsTheySee=true;
    /*debug*/
    if (routine_logging){
        ostringstream o;
        o <<"allDiscoveredLUNs contains:"<<std::endl<< allDiscoveredLUNs.toString() <<std::endl;
        //std::cout<<o.str();
        log(masterlogfile,o.str());
    }
    else
    {
        ostringstream o;
        o <<"Discovered " << allDiscoveredLUNs.LUNpointers.size() << " LUNs on the test hosts." << std::endl;
        //std::cout<<o.str();
        log(masterlogfile,o.str());
    }

    if (0 == allDiscoveredLUNs.LUNpointers.size())
    {
        std::ostringstream o;
        o << ivyscript_line_number << ": \"" << ivyscript_line << "\"" << std::endl;
        o << "No LUNs were found on any of the hosts." << std::endl;
        std::cout << o.str();
        log (masterlogfile,o.str());
        ivyscriptIfstream.close();
        kill_subthreads_and_exit();
    }


    allDiscoveredLUNs.split_out_command_devices_into(commandDeviceLUNs);

    /*debug*/
    {
        ostringstream o;
        o <<"commandDeviceLUNs contains:"<<std::endl<< commandDeviceLUNs.toString() <<std::endl;
        //std::cout<<o.str();
        log(masterlogfile,o.str());
    }

    for (LUN* pLUN : allDiscoveredLUNs.LUNpointers)
    {
        if (select.matches(pLUN))
        {
            if (     pLUN->attribute_value_matches("ldev",      "FF:FF"         )
                &&   pLUN->attribute_value_matches("ldev_type", ""              )
                && ( pLUN->attribute_value_matches("product",   "DISK-SUBSYSTEM") || pLUN->attribute_value_matches("product","OPEN-V") ) )
            {
                ; // Ignore "phantom LUNs" that have been deleted on the subsystem but for which a /dev/sdxxx entry still exists.
            }
            else if ( pLUN->attribute_value_matches("ldev",      "FF:FF")
                &&    pLUN->attribute_value_matches("ldev_type", "phantom LUN")  )
            {
                ; // Ignore "phantom LUNs" that have been deleted on the subsystem but for which a /dev/sdxxx entry still exists.
            }
            else
            {
                availableTestLUNs.LUNpointers.push_back(pLUN);
            }
        }
    }


    if ( 0 == availableTestLUNs.LUNpointers.size() )
    {
        std::ostringstream o;
        o << "No LUNs matched [hosts] statement [select] clause." << std::endl;
        //p_Select->display("",o);

        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }

    for (auto& pLUN : availableTestLUNs.LUNpointers) pLUN -> createNicknames();

    // Now we create the Subsystem objects

    for (auto& pLUN : availableTestLUNs.LUNpointers)
    {
        std::string serial_number = pLUN->attribute_value(std::string("serial_number"));
        trim(serial_number);

        Subsystem* pSubsystem;

        auto subsystemIt = subsystems.find(serial_number);
        if (subsystems.end() == subsystemIt)
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
            subsystems[serial_number] = pSubsystem;
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
    // communicating via stdin/stdout pipes the same way we communicate with remote ivydriver instances.

    for (auto& pear : subsystems)
    {
        Subsystem* pSubsystem = pear.second;

        if (0 == std::string("Hitachi RAID").compare(pSubsystem->type()))
        {
            Hitachi_RAID_subsystem* pRAIDsubsystem {(Hitachi_RAID_subsystem*) pSubsystem};
            bool have_cmd_dev_this_subsystem = false;

            if (m_s.use_command_device) for (auto& pL : commandDeviceLUNs.LUNpointers)
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
                    log(masterlogfile,o.str());

                    pipe_driver_subthread* p_pipe_driver_subthread = new pipe_driver_subthread(

                        pL->attribute_value("ivyscript_hostname")
                        , outputFolderRoot
                        , testName,
                        testFolder+std::string("/logs")
                    );

                    p_pipe_driver_subthread->pCmdDevLUN = pL;  // Seeing this set tells the subthread it's running a command device.

                    p_pipe_driver_subthread->p_Hitachi_RAID_subsystem = pRAIDsubsystem;

                    pRAIDsubsystem->command_device_description = cmd_dev_description;

                    pRAIDsubsystem->pRMLIBthread=p_pipe_driver_subthread;

                    command_device_subthread_pointers[pSubsystem->serial_number] = p_pipe_driver_subthread;

                    ivymaster_RMLIB_threads.push_back(std::thread(invokeThread,p_pipe_driver_subthread));
                    // Note: I wasn't sure, since std::thread is not copyable but is moveable, how to handle
                    //       the case when the command device doesn't start up correctly.  So in the interest of
                    //       time, I left any unsuccessful-startup command device threads in moribund state
                    //       in ivymaster_RMLIB_threads to be joined when ivy shuts down.

                    // wait with timeout for thread status to show "startupComplete", i.e. waiting for command;

                    {
                        std::unique_lock<std::mutex> u_lk(p_pipe_driver_subthread->master_slave_lk);
                        std::chrono::system_clock::time_point leftnow {std::chrono::system_clock::now()};

                        if
                        (
                            p_pipe_driver_subthread->master_slave_cv.wait_until
                            (
                                u_lk,
                                leftnow + std::chrono::seconds(ivy_ssh_timeout),
                                [&p_pipe_driver_subthread](){return p_pipe_driver_subthread->startupComplete || p_pipe_driver_subthread->dead;}
                        )
                    )
                        {
                            if (p_pipe_driver_subthread->startupSuccessful)
                            {
                                std::ostringstream o;
                                o << "ivy_cmddev pipe_driver_subthread successful fireup for subsystem " << pSubsystem->serial_number << std::endl;
                                std::cout << o.str();
                                log(masterlogfile,o.str());
                                haveCmdDev = true;
                                have_cmd_dev_this_subsystem = true;
                            }
                            else
                            {
                                std::ostringstream o;
                                o   << "Unsuccessful startup for command device = " << cmd_dev_description << std::endl
                                    << "Reason - " <<  p_pipe_driver_subthread->commandErrorMessage << std::endl;
                                std::cout << o.str();
                                log(masterlogfile,o.str());
                                pRAIDsubsystem->command_device_description.clear();
                                pRAIDsubsystem->pRMLIBthread=nullptr;
                            }
                        }
                        else
                        {
                            std::ostringstream o;
                            o   << "Timeout waiting " << timeout_seconds << " seconds for for startup of command device = " << cmd_dev_description << std::endl;
                            std::cout << o.str();
                            log(masterlogfile,o.str());
                            pRAIDsubsystem->command_device_description.clear();
                            pRAIDsubsystem->pRMLIBthread=nullptr;
                        }
                    }

                    if (!have_cmd_dev_this_subsystem) continue;  // This command device did not start up OK - try the next one if we have another to the same subsystem.
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
                            log(masterlogfile,o.str());
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
                                [&p_pipe_driver_subthread](){ return p_pipe_driver_subthread->commandComplete || p_pipe_driver_subthread->dead; }
                            )
                        )
                        {
                            if (!p_pipe_driver_subthread->commandSuccess)
                            {
                                std::ostringstream o;
                                o << "\"get config\" to thread for subsystem serial " << pRAIDsubsystem->serial_number
                                  << " managed on host " << p_pipe_driver_subthread->ivyscript_hostname << " failed.  Aborting." << std::endl;
                                log(masterlogfile,o.str());
                                kill_subthreads_and_exit();
                            }

                            ivytime config_gather_complete;
                            config_gather_complete.setToNow();

                            pRAIDsubsystem->config_gather_time  = config_gather_complete - config_gather_start;

                            {
                                std::ostringstream o;
                                o << "\"get config\" reported complete with duration " << pRAIDsubsystem->config_gather_time.format_as_duration_HMMSSns()
                                  << " by thread for subsystem serial " << pRAIDsubsystem->serial_number << " managed on host " << p_pipe_driver_subthread->ivyscript_hostname << '.' << std::endl;
                                log(masterlogfile,o.str());
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
                            log(masterlogfile,o.str());
                            u_lk.unlock();
                            kill_subthreads_and_exit();
                        }
                    }

                    // post process after gather to augment available test LUN attributes with LDEV attributes from the config gather

                    for (auto pLUN : availableTestLUNs.LUNpointers)
                    {
                        if ( 0 == std::string("Hitachi RAID").compare(pSubsystem->type())  &&  0 == pSubsystem->serial_number.compare(pLUN->attribute_value("serial_number")))
                        {

                            // for each test LUN, if we are processing a Hitachi RAID subsystem with a given serial number and the test LUN serial_number matches the subsystem
                            // 1) if CLPR is non-empty add the <CLPR,serial_number> pair to cooldown_WP_watch_set
                            // 2) copy the LDEV's RMLIB attributes to the test LUN.
                            // put a suffix "_RMLIB" if the test LUN already has the attribute name (from the SCSI Inquiry LUN lister tool" showluns.sh").

                            if (0 < pLUN->attribute_value("CLPR").length())
                            {
                                cooldown_WP_watch_set.insert(std::make_pair(pLUN->attribute_value("CLPR"),pSubsystem));
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
                                log(masterlogfile,o.str());
                                kill_subthreads_and_exit();
                            }

                            std::string LUN_ldev = pLUN->attribute_value(std::string("ldev"));

                            if (0==LUN_ldev.size())
                            {
                                std::ostringstream o;
                                o << "Aborting - available test LUN matched on Hitachi RAID subsystem serial number " << pSubsystem->serial_number
                                  << ", but the LUN's \"ldev\" attribute value was the null string." <<  std::endl
                                  << "  LUN =" << pLUN->toString() <<  std::endl;
                                std::cout << o.str();
                                log(masterlogfile,o.str());
                                kill_subthreads_and_exit();
                            }

                            auto subsystemLDEVit = pRAIDsubsystem->configGatherData.data.find(std::string("LDEV"));
                            if (pRAIDsubsystem->configGatherData.data.end() == subsystemLDEVit)
                            {
                                std::ostringstream o;
                                o << "Aborting - available test LUN matched on Hitachi RAID subsystem serial number " << pSubsystem->serial_number
                                  << ", but the subsystem configuration data from a command device didn\'t have \"LDEV\" element type." <<  std::endl
                                  << "  LUN =" << pLUN->toString() <<  std::endl;
                                std::cout << o.str();
                                log(masterlogfile,o.str());
                                kill_subthreads_and_exit();
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
                                log(masterlogfile,o.str());
                                kill_subthreads_and_exit();
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

            if (!have_cmd_dev_this_subsystem)
            {
                std::ostringstream o;
                o << "No command device found for subsystem " << pSubsystem->serial_number << std::endl;
                std::cout << o.str();
                log(masterlogfile,o.str());
            }
            else
            {
                pRAIDsubsystem->configGatherData.print_csv_file_set
                (
                    testFolder,
                    pRAIDsubsystem->serial_number+std::string(".config")
                );
            }

        } // end for Hitachi RAID subsystem

    } // end for each subsystem

    for (auto& pLUN : availableTestLUNs.LUNpointers)
    {
        TheSampleLUN.copyOntoMe(pLUN);  // add nicknames, and command device LDEV attributes to TheSampleLUN.
    }


    for (auto& pLUN : availableTestLUNs.LUNpointers) // populate test config thumbnail data structures
    {
        available_LUNs_thumbnail.add(pLUN);
    }

    {
        std::ostringstream o;
        o << available_LUNs_thumbnail;

        bool saw_command_device {false};

        for (auto& pear : subsystems)
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

        log(masterlogfile,o.str());
        std::cout << o.str();

        ofstream ofs(testFolder+std::string("/available_test_config.txt"));
        ofs << o.str();
        ofs.close();

    }

    if (routine_logging)
    {
        ostringstream o;
        o << "After adding subsystem config attributes, availableTestLUNs contains:" << std::endl << availableTestLUNs.toString() << std::endl;
        // std::cout << o.str();
        log(masterlogfile,o.str());
    }
    else
    {
        ostringstream o;
        o <<"availableTestLUNs contains " << availableTestLUNs.LUNpointers.size() << " LUNs." << std::endl;
        //std::cout<<o.str();
        log(masterlogfile,o.str());
    }


    ofstream atlcsv(testFolder+std::string("/available_test_LUNs.csv"));
    availableTestLUNs.print_csv_file(atlcsv);
    atlcsv.close();

    if (cooldown_WP_watch_set.size() == 0)

    {
        std::ostringstream o;
        o << "No command devices for RAID_subsystem LDEVs, so cooldown_by_wp settings will not have any effect." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
    }
    else
    {
        std::ostringstream o;
        o << "Cooldown_WP_watch_set contains:";
        for (auto& pear : cooldown_WP_watch_set) o << " < " << pear.first << ", " << pear.second->serial_number << " >";
        o << std::endl << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
    }

    haveHosts = true;



    return std::make_pair(true,std::string("hello, whirled!"));
}
