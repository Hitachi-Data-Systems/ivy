//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
using namespace std;

#include <string>
#include <list>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <set>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "ivytime.h"
#include "ivydefines.h"
#include "ivyhelpers.h"
#include "LUN.h"
#include "AttributeNameCombo.h"
#include "IogeneratorInputRollup.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "IogeneratorInput.h"
#include "SubintervalOutput.h"
#include "SubintervalRollup.h"
#include "SequenceOfSubintervalRollup.h"
#include "WorkloadID.h"
#include "ListOfWorkloadIDs.h"
#include "RollupInstance.h"
#include "AttributeNameCombo.h"
#include "RollupType.h"
#include "ivylinuxcpubusy.h"
#include "Subinterval_CPU.h"
#include "LUN.h"
#include "Select.h"
#include "LUNpointerList.h"
#include "ivylinuxcpubusy.h"
#include "GatherData.h"
#include "Subsystem.h"
#include "pipe_driver_subthread.h"
#include "WorkloadTracker.h"
#include "WorkloadTrackers.h"
#include "Subinterval_CPU.h"
#include "RollupSet.h"
#include "master_stuff.h"
#include "NameEqualsValueList.h"
#include "ListOfNameEqualsValueList.h"
#include "Ivy_pgm.h"

master_stuff m_s;

std::string outputFolderRoot() {return m_s.outputFolderRoot;}
std::string masterlogfile()    {return m_s.masterlogfile;}
std::string testName()         {return m_s.testName;}
std::string testFolder()       {return m_s.testFolder;}
std::string stepNNNN()         {return m_s.stepNNNN;}
std::string stepName()         {return m_s.stepName;}
std::string stepFolder()       {return m_s.stepFolder;}


accumulator_type_enum string_to_accumulator_type_enum (const std::string& s)
{
    if (stringCaseInsensitiveEquality(s,"bytes_transferred"))  return accumulator_type_enum::bytes_transferred;
    if (stringCaseInsensitiveEquality(s,"response_time"))      return accumulator_type_enum::response_time;
    if (stringCaseInsensitiveEquality(s,"service_time"))       return accumulator_type_enum::service_time;
    return accumulator_type_enum::error;
}

std::string accumulator_types() { return "{ \"bytes_transferred\", \"service_time\", \"response_time\" }"; }

std::string source_enum_to_string(source_enum e)
{
    switch (e)
    {
        case source_enum::error:          return "error";
        case source_enum::workload:       return "workload";
        case source_enum::RAID_subsystem: return "RAID_subsystem";
        default:                          return "<Error> internal programming error - source_enum_to_string() - invalid source_enum value.";
    }
}

std::string category_enum_to_string(category_enum e)
{
    switch (e)
    {
        case category_enum::error:            return "error";
        case category_enum::overall:          return "overall";
        case category_enum::read:             return "read";
        case category_enum::write:            return "write";
        case category_enum::random:           return "random";
        case category_enum::sequential:       return "sequential";
        case category_enum::random_read:      return "random_read";
        case category_enum::random_write:     return "random_write";
        case category_enum::sequential_read:  return "sequential_read";
        case category_enum::sequential_write: return "sequential_write";
        default:                              return "<Error> internal programming error - category_enum_to_string() - invalid accumulator_type_enum value.";
    }
}

std::string accumulator_type_to_string(accumulator_type_enum e)
{
    switch (e)
    {
        case accumulator_type_enum::error:             return "error";
        case accumulator_type_enum::bytes_transferred: return "bytes_transferred";
        case accumulator_type_enum::response_time:     return "response_time";
        case accumulator_type_enum::service_time:      return "service_time";
        default:                                       return "<Error> internal programming error - accumulator_type_to_string() - invalid accumulator_type_enum value.";
    }
}

std::string accessor_enum_to_string(accessor_enum e)
{
    switch (e)
    {
        case accessor_enum::error:             return "error";
        case accessor_enum::avg:               return "avg";
        case accessor_enum::count:             return "count";
        case accessor_enum::min:               return "min";
        case accessor_enum::max:               return "max";
        case accessor_enum::sum:               return "sum";
        case accessor_enum::variance:          return "variance";
        case accessor_enum::standardDeviation: return "standardDeviation";
        default:                               return "<Error> internal programming error - accessor_enum_to_string() - invalid accessor_enum value.";
    }
}

bool master_stuff::some_cooldown_WP_not_empty()
{
    for (auto& pear : cooldown_WP_watch_set)
    {
        Hitachi_RAID_subsystem* pR = (Hitachi_RAID_subsystem*) pear.second;
        ivy_float wp;
        std::string CLPR = pear.first;
        try
        {
            wp = pR->get_wp(CLPR, pear.second->gathers.size()-2);  // there is gather at t=0
        }
        catch (std::invalid_argument& iae)
        {
            // theoretically this isn't supposed to happen, as if there is a command device to the subsystem behind an available test LUN,
            // that command device always collects by-CLPR data for all CLPRs.
            std::ostringstream o; o << "MeasureDFC::cooldown_return_code() - for subsystem " << pear.second->serial_number << ", get_WP(CLPR=\"" << CLPR
                << "\", subinterval=" << (pear.second->gathers.size()-2) <<") failed saying - " << iae.what() << std::endl;
            log (m_s.masterlogfile,o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }

        if (wp > .015)
        {
            return true;
        }
    }

    return false;
}


bool rewrite_total_IOPS(std::string& parametersText, int instances)
{
	// rewrites the first instance of "total_IOPS = 5000" to "IOPS=1000.000000000000" by dividing total_IOPS by instances.

	// total_IOPS number forms recognized:  1 .2 3. 4.4 1.0e3 1e+3 1.e-3 3E4

	// returns true if it re-wrote, false if it didn't see "total_IOPS"

	if (instances <= 0) throw (std::invalid_argument("rewrite_total_IOPS() command_workload_IDs must be greater than zero."));

	std::string total_IOPS {"total_IOPS"};

	if (total_IOPS.length() > parametersText.length()) return false;

	int start_point, number_start_point, last_plus_one_point;

	for (unsigned int i=0; i < (parametersText.length()-(total_IOPS.length()-1)); i++)
	{
		if (stringCaseInsensitiveEquality(total_IOPS,parametersText.substr(i,total_IOPS.length())))
		{
			start_point=i;
			i += total_IOPS.length();

			while (i < parametersText.length()  && isspace(parametersText[i])) i++;

			if (i < parametersText.length()  && '=' == parametersText[i])
			{
				i++;
				while (i < parametersText.length()  && isspace(parametersText[i])) i++;

                char starting_quote = parametersText[i];

                if ( (i < (parametersText.length()-1)) && ( '\'' == starting_quote || '\"' == starting_quote))
                {
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
                    v /= (ivy_float) instances;
                    std::ostringstream newv;
                    newv << "IOPS=" << std::fixed << std::setprecision(12) << v;

                    parametersText.replace(start_point,last_plus_one_point-start_point,newv.str());

                    return true;
                }
                else
                {
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

                        value /= (ivy_float) instances;

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

void master_stuff::error(std::string m)
{
    std::ostringstream o;
    o << "Error: " << m << std::endl;
    std::cout << o.str();
    log(masterlogfile,o.str());
    kill_subthreads_and_exit();
}

void	master_stuff::kill_subthreads_and_exit()
{
	int notified=0, joined=0;
	for (auto&  pear : host_subthread_pointers)
	{
		{
			std::unique_lock<std::mutex> u_lk(pear.second->master_slave_lk);
			pear.second->commandString=std::string("die");
			pear.second->command=true;

			std::ostringstream o;
			o << "posting \"die\" command to subthread for host " << pear.first << " whose pipe_driver_subthread pid = " << pear.second->pipe_driver_pid << std::endl;
			log (masterlogfile, o.str());
		}
		pear.second->master_slave_cv.notify_all();
		notified++;
	}


	int notified_subsystems {0};

	for (auto& pear : subsystems)
	{
		if (0==pear.second->type().compare(std::string("Hitachi RAID")))
		{
			Hitachi_RAID_subsystem* pRAID = (Hitachi_RAID_subsystem*) pear.second;
			if (pRAID->pRMLIBthread)
			{
				pipe_driver_subthread* p_pds = pRAID->pRMLIBthread;
				{
					std::unique_lock<std::mutex> u_lk(p_pds->master_slave_lk);
					p_pds->commandString=std::string("die");
					p_pds->command=true;

					std::ostringstream o;
					o << "posting \"die\" command to subthread for subsystem " << pear.first << " whose pipe_driver_subthread pid = " << p_pds->pipe_driver_pid << std::endl;
					log (masterlogfile, o.str());
				}
				p_pds->master_slave_cv.notify_all();
				notified_subsystems++;
			}
		}
	}


	// NEED TO COME BACK AND TIMEOUT and KILL

	// Not waiting for subsystem threads to finish - join will wait anyway ...

	for (auto&  pear : host_subthread_pointers)
	{
		{
			std::unique_lock<std::mutex> u_lk(pear.second->master_slave_lk);
			while (!pear.second->dead) pear.second->master_slave_cv.wait(u_lk);
			std::ostringstream o;
			o << "waiting for subthread for host " << pear.first << " whose pipe_driver_subthread pid = " << pear.second->pipe_driver_pid << " to show as dead." << std::endl;
			log (masterlogfile, o.str());
		}
	}

	for (auto& thread : threads)
	{  // come back later and maybe make this time out
        thread.join();
		joined++;
	}

	for (auto& thread : ivymaster_RMLIB_threads)
	{  // come back later and maybe make this time out
        thread.join();
		joined++;
	}


	std::ostringstream o;
	o << "master_stuff::kill_subthreads_and_exit() told " << notified << " host driver and " << notified_subsystems << " command device subthreads to die, and then harvested " << joined << " moribund pids." << std::endl;
	log(masterlogfile,o.str());

	for (auto& host : hosts)
	{
		{
			std::ostringstream o;
			o << "scp " << SLAVEUSERID << '@' << host << ":" << IVYSLAVELOGFOLDER << "/log.ivyslave." << host << "* " << testFolder << "/logs";
			if (0 == system(o.str().c_str()))
			{
				log(masterlogfile,std::string("success: ")+o.str()+std::string("\n"));
				std::ostringstream rm;
				rm << "ssh " << SLAVEUSERID << '@' << host << " rm -f " << IVYSLAVELOGFOLDER << "/log.ivyslave." << host << "*";
				if (0 == system(rm.str().c_str()))
					log(masterlogfile,std::string("success: ")+rm.str()+std::string("\n"));
				else
					log(masterlogfile,std::string("failure: ")+rm.str()+std::string("\n"));
			}
			else log(masterlogfile,std::string("failure: ")+o.str()+std::string("\n"));
		}
	}

	exit(0);
}

void master_stuff::write_clear_script()
{
    // This writes a script to invoke the "clear_hung_ivy_threads" executable on this ivy master host,
    // as well as once for each distinct IP address for ivyslave hosts.

    std::string my_IP;

    std::map<std::string,std::string> other_ivyslave_hosts; // <IP address, ivyscript hostname>

    const unsigned int max_hostnamesize = 100;
    char my_hostname[max_hostnamesize];
    my_hostname[max_hostnamesize-1] = 0;

    const std::string method = "master_stuff::write_clear_script() - ";

    const std::string scriptname = "clear_hung_ivy_threads.sh";
    const std::string executable = "clear_hung_ivy_threads";

    std::ofstream oaf(scriptname,std::ofstream::out);
    if ( (!oaf.is_open()) || oaf.fail() )
    {
        std::ostringstream o;
        o << method << "Unable to open output file \"" << scriptname << "\"." << std::endl;
        std::cout << o.str();
        log(masterlogfile, o.str());
        exit(-1);
    }

    oaf << "#!/bin/bash" << std::endl;

    int rc = gethostname(my_hostname,sizeof(my_hostname)-1);
    if (0 != rc)
    {
        std::ostringstream o;
        o << method << "Unable to get my own hostname - gethostname() return code = " << rc << " - " << std::endl;
        std::cout << o.str();
        log(masterlogfile, o.str());
        exit(-1);
    }

    hostent* p_hostent = gethostbyname(my_hostname);
    if (p_hostent == NULL)
    {
        std::ostringstream o;
        o << method << "Unable to get my own IP address - gethostbyname(\"" << my_hostname << "\" failed." << std::endl;
        std::cout << o.str();
        log(masterlogfile, o.str());
        exit(-1);
    }

    oaf << std::endl << "# my canonical name is " << p_hostent->h_name << std::endl;

    if (p_hostent->h_addrtype != AF_INET)
    {
        std::ostringstream o;
        o << method << "My own address is not AF_INET, not an internet IP address." << std::endl;
        std::cout << o.str();
        log(masterlogfile, o.str());
        exit(-1);
    }

    in_addr * p_in_addr = (in_addr * )p_hostent->h_addr;
    {
        std::ostringstream o;
        o << inet_ntoa( *p_in_addr);
        my_IP = o.str();
    }

    oaf << "# my IP address is " << my_IP << std::endl;

    oaf << std::endl
        << "echo" << std::endl
        << "echo -----------------------------" << std::endl
        << "echo" << std::endl
        << "echo "
            << executable << std::endl
        << "echo" << std::endl
            << executable << std::endl;

    for (auto& slavehost : hosts)
    {
        p_hostent = gethostbyname(slavehost.c_str());
        if (p_hostent == NULL)
        {
            std::ostringstream o;
            o << method << "Unable to get IP address for ivyslave host (\"" << slavehost << "\"." << std::endl;
            std::cout << o.str();
            log(masterlogfile, o.str());
            exit(-1);
        }

        if (p_hostent->h_addrtype != AF_INET)
        {
            std::ostringstream o;
            o << method << "For ivyslave host \"" << slavehost << "\", address is not AF_INET, not an internet IP address." << std::endl;
            std::cout << o.str();
            log(masterlogfile, o.str());
            exit(-1);
        }

        in_addr * p_in_addr = (in_addr * )p_hostent->h_addr;
        {
            std::ostringstream o;
            o << inet_ntoa( *p_in_addr);
            std::string ivyslave_IP = o.str();
            auto it = other_ivyslave_hosts.find(ivyslave_IP);
            if ( (0 != my_IP.compare(ivyslave_IP)) && (it == other_ivyslave_hosts.end()) )
            {
                other_ivyslave_hosts[ivyslave_IP] = slavehost;
                oaf << std::endl
                    << "echo" << std::endl
                    << "echo -----------------------------" << std::endl
                    << "echo" << std::endl
                    << "echo "
                        << "ssh " << slavehost  << " " << executable << std::endl
                    << "echo" << std::endl
                        << "ssh " << slavehost  << " " << executable << std::endl;
            }
            else
            {
                oaf << "# duplicate ivyslave host " << slavehost << " ( " << ivyslave_IP << " )" << std::endl;
            }
        }
    }

    oaf.close();

    {
        std::ostringstream o;
        o << "chmod +x " << scriptname;
        system (o.str().c_str());
    }

    return;
}

bool master_stuff::make_measurement_rollup_CPU(std::string callers_error_message, unsigned int firstMeasurementIndex, unsigned int lastMeasurementIndex)
{
		if (cpu_by_subinterval.size() < 3)
		{
			std::ostringstream o;
			o << "master_stuff::make_measurement_rollup_CPU() - The total number of subintervals in the cpu_by_subinterval sequence is " << cpu_by_subinterval.size()
			<< std::endl << "and there must be at least one warmup subinterval, one measurement subinterval, and one cooldown subinterval, "
			<< std::endl << "due to TCP/IP network time jitter when each test host hears the \"Go\" command.  This means we don't depend on NTP or clock synchronization.";
			callers_error_message = o.str();
			return false;
		}

		if
		(!(	// not:
			   firstMeasurementIndex >0 && firstMeasurementIndex < (cpu_by_subinterval.size()-1)
			&& lastMeasurementIndex >0 && lastMeasurementIndex < (cpu_by_subinterval.size()-1)
			&& firstMeasurementIndex <= lastMeasurementIndex
		))
		{
			std::ostringstream o;
			o << "master_stuff::make_measurement_rollup_CPU() - Invalid first (" << firstMeasurementIndex << ") and last (" << lastMeasurementIndex << ") measurement period indices."
			<< std::endl << " There must be at least one warmup subinterval, one measurement subinterval, and one cooldown subinterval, "
			<< std::endl << "due to TCP/IP network time jitter when each test host hears the \"Go\" command.  This means we don't depend on NTP or clock synchronization.";
			callers_error_message = o.str();
			return false;
		}

		measurement_rollup_CPU.clear();

		for (unsigned int i=firstMeasurementIndex; i <= lastMeasurementIndex; i ++)
		{
			measurement_rollup_CPU.rollup(cpu_by_subinterval[i]);
		}

		return true;
}


bool master_stuff::createWorkload(std::string& callers_error_message, std::string workloadName, Select* pSelect, std::string iogeneratorName, std::string parameters)
{
	// Look at proposed workload name

	if (!looksLikeHostname(workloadName))
	{
                ostringstream o;
		o << "Invalid workload name \"" << workloadName << "\".  Workload names start with an alphabetic [a-zA-Z] character and the rest is all alphanumerics and underscores [a-zA-Z0-9_].";
		callers_error_message = o.str();
		return false;
	}


	// Look at [select] clause and filter against all discovered test LUNs to define "createWorkloadLUNs".

	LUNpointerList createWorkloadLUNs;

	if (pSelect == nullptr)
	{
        createWorkloadLUNs.clone(m_s.availableTestLUNs);
	}
	else
	{
        createWorkloadLUNs.clear_and_set_filtered_version_of(m_s.availableTestLUNs,pSelect);
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

	// Look at iogenerator name

	std::unordered_map<std::string,IogeneratorInput*>::iterator template_it;
	template_it=iogenerator_templates.find(iogeneratorName);
	if (iogenerator_templates.end()==template_it)
	{
                ostringstream o;
                o << "Create workload - invalid [iogenerator] specification - no such I/O generator name found - \"" << iogeneratorName << "\".";
		callers_error_message = o.str();
		return false;
	}

	// Make a copy of the selected iogenerator's default template as a starting point before
	// we apply the user's parameters to the default template to create a "stamp", an IogeneratorInput object
	// that we will then clone for each WorkloadTracker that we create to track a particular test host workload thread.

	// Later on, the DynamicFeedbackController can decide to update these parameters.

	// Because we keep a local copy in ivymaster in the WorkloadTracker object of the most recent update to a workload's
	// IogeneratorInput object, we can also do things like "increase IOPS by 10%".

	IogeneratorInput i_i;  // the "stamp"
	i_i.fromString((*template_it).second->toString(),masterlogfile);

	// now apply the user-provided parameter settings on top of the template


	if (0<parameters.length())
	{
		std::string my_error_message;
		if (!i_i.setMultipleParameters(my_error_message, parameters))
		{
		        ostringstream o;
		        o << "Create workload - failure setting initial parameters - \"" << parameters << "\"." <<std::endl
			  << my_error_message << std::endl;
		callers_error_message = o.str();
		return false;
		}
	}


	// The same WorkloadID in text form is used as the lookup key
	//	- in ivymaster to lookup a WorkloadTracker object
	//	- in ivyslave to lookup an I/O driver subthread

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
				  p_LUN->attribute_value(std::string("ivyscript_hostname"))
				+ std::string("+")
				+ p_LUN->attribute_value(std::string("LUN_name"))
				+ std::string("+")
				+ workloadName
			)
		)
		{
		        ostringstream o;
			o << "Create workload - internal programming error - invalid WorkloadID = \"" << workloadID.workloadID <<   "\" generated that is not well formed as \"ivyscript_hostname+LUN_name+workload_name\".";
			callers_error_message = o.str();
			return false;
		}
		std::map<std::string, WorkloadTracker*>::iterator it = workloadTrackers.workloadTrackerPointers.find(workloadID.workloadID);
		if (it != workloadTrackers.workloadTrackerPointers.end())
		{
		        ostringstream o;
		        o << "Create workload - failed - workload ID = \"" << workloadID.workloadID <<   "\" already exists.";
			callers_error_message = o.str();
			return false;
		}
	}

	createWorkloadExecutionTimeSeconds.clear();

	for (auto& p_LUN : createWorkloadLUNs.LUNpointers)  // maybe come back later and do this for a set of LUNs in parallel, like we do for editWorkload().
	{
		std::string ivyscript_hostname = p_LUN->attribute_value(std::string("ivyscript_hostname"));

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
			callers_error_message = o.str();
			return false;
		}

		pipe_driver_subthread* p_host = (*it).second;

		// Next we make a new WorkloadTracker object for the remote thread we are going to create on a test host.

		WorkloadTracker* pWorkloadTracker = new WorkloadTracker(workloadID.workloadID, iogeneratorName, p_LUN);

		pWorkloadTracker->wT_IogeneratorInput.copy(i_i);

		workloadTrackers.workloadTrackerPointers[workloadID.workloadID] = pWorkloadTracker;

		// We time how long it takes to make the thread
		{
			ostringstream o;
			o << "Creating LUN workload thread on host \"" << p_host->ivyscript_hostname << "\", LUN \"" << p_LUN->attribute_value(std::string("LUN_name")) << "\", workload name \""
			  << workloadName << "\", iogenerator \"" << iogeneratorName << "\", parameters \"" << i_i.toString() << "\".";
			log(masterlogfile,o.str());
		}

		ivytime beforeMakingThread;
		beforeMakingThread.setToNow();

		// get lock on talking to pipe_driver_subthread for this remote host
		{
			std::unique_lock<std::mutex> u_lk(p_host->master_slave_lk);
			// tell slave driver thread to talk to the other end and make the thread

			p_host->commandString=std::string("[CreateWorkload]");

			p_host->commandHost = p_LUN->attribute_value(std::string("ivyscript_hostname"));
			p_host->commandLUN = p_LUN->attribute_value(std::string("LUN_name"));
			p_host->commandWorkloadID = workloadID.workloadID;
			p_host->commandIogeneratorName = iogeneratorName;
			p_host->commandIogeneratorParameters = i_i.toString();

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
				  << workloadName << "\", iogenerator \"" << iogeneratorName << "\", parameters \"" << i_i.toString() << "\".";
				callers_error_message = o.str();
				return false;
			}
		}
		ivytime afterMakingThread;
		afterMakingThread.setToNow();
		ivytime createThreadExecutionTime = afterMakingThread - beforeMakingThread;
		createWorkloadExecutionTimeSeconds.push(createThreadExecutionTime.getlongdoubleseconds());
	}


	{
		std::ostringstream o;
		o << "      remote create thread execution time summary: total time " << createWorkloadExecutionTimeSeconds.sum()
			<<" seconds - count " <<   createWorkloadExecutionTimeSeconds.count()
			<< " - average " << createWorkloadExecutionTimeSeconds.avg()
			<< " seconds - minimum " << createWorkloadExecutionTimeSeconds.min()
			<< " seconds - maximum " << createWorkloadExecutionTimeSeconds.max()
			<< " seconds." << std::endl;

/*debug*/ std::cout<< o.str();

		log(masterlogfile,o.str());
	}


	return true;
}

bool master_stuff::deleteWorkload(std::string& callers_error_message, std::string workloadName, Select* pSelect)
{
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
            callers_error_message = o.str();
            return false;
        }
    }

    for (auto& pear : workloadTrackers.workloadTrackerPointers)
    {
        const std::string& key = pear.first;
        WorkloadTracker* p_WorkloadTracker = pear.second;

        WorkloadID w;

        if (!w.set(key))
        {
		    ostringstream o;
		    o << "<Error> delete workload - internal programming error.  The key of an entry in workloadTrackers.workloadTrackerPointers did not parse as a valid WorkloadID.";
			callers_error_message = o.str();
			std::cout << "Sorry if you are going to see this twice, but just in case: " << o.str() << std::endl;
			return false;
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
                if (pSelect == nullptr)
                {
                    delete_list.push_back(w);
                }
                else
                {
                    if (pSelect->matches(&(p_WorkloadTracker->workloadLUN)))
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
        callers_error_message = o.str();
        std::cout << "Sorry if you are going to see this twice, but just in case: " << o.str() << std::endl;
        return false;
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
			callers_error_message = o.str();
			return false;
		}

		pipe_driver_subthread* p_host = it->second;

        auto wit = workloadTrackers.workloadTrackerPointers.find(wid.workloadID);

        if (wit == workloadTrackers.workloadTrackerPointers.end())
        {
		    ostringstream o;
		    o << "<Error> [DeleteWorkload] - internal programming error.  workloadTrackers.workloadTrackerPointers.find(\"" << wid.workloadID << "\") failed when about to delete it.";
			callers_error_message = o.str();
			return false;
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
				callers_error_message = o.str();
				return false;
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
			<< "seconds - minimum " << deleteWorkloadExecutionTimeSeconds.min()
			<< "seconds - maximum " << deleteWorkloadExecutionTimeSeconds.max() << "seconds." << std::endl;

/*debug*/ std::cout<< o.str();

		log(masterlogfile,o.str());
	}


	return true;
}

bool master_stuff::editRollup(std::string& callers_error_message, std::string rollupText, std::string original_parametersText)
// returns true on success
{
//variables
	ListOfNameEqualsValueList listOfNameEqualsValueList(rollupText);
	WorkloadTrackers editRollupWorkloadTrackers;
	int fieldsInAttributeNameCombo,fieldsInAttributeValueCombo;
	ivytime beforeEditWorkload;  // it's called editRollup at the ivymaster level, but once we build the list of edits by host, at the host level it's editWorkload

    std::string parametersText = original_parametersText;
//code

//*debug*/{std::ostringstream o; o << "master_stuff::editRollup(, rollupText=\"" << rollupText << "\", parametersText=\"" << parametersText << "\")" << std::endl; std::cout << o.str(); log(masterlogfile,o.str());}

	beforeEditWorkload.setToNow();
	editWorkloadExecutionTimeSeconds.clear();



	callers_error_message.clear();

	if (!listOfNameEqualsValueList.parsedOK())
	{
		std::ostringstream o;

		o << "Parse error.  Rollup specification \"" << rollupText << "\" should look like \"host=sun159\" or \"serial_number+port = { 410321+1A, 410321+2A }\" - "
		  << listOfNameEqualsValueList.getParseErrorMessage();

		callers_error_message = o.str();
		return false;
	}

	if (1 < listOfNameEqualsValueList.clauses.size())
	{
		callers_error_message = "Rollup specifications with more than one attribute = value list clause are not implemented at this time.\n"
			"Should multiple clauses do a union or an intersection of the WorkloadIDs selected in each clause?\n";
		return false;
	}

	if (0 == listOfNameEqualsValueList.clauses.size())
	{
		editRollupWorkloadTrackers.clone(workloadTrackers);
	}
	else
	{
		// validate the "serial_number+Port" part of "serial_number+Port = { 410321+1A, 410321+2A }"

		std::string rollupsKey;
		std::string my_error_message;

		rollupsKey = toLower(listOfNameEqualsValueList.clauses.front()->name_token);

		if (!isPlusSignCombo(my_error_message, fieldsInAttributeNameCombo, rollupsKey ))
		{
			std::ostringstream o;
			o << "rollup type \"" << listOfNameEqualsValueList.clauses.front()->name_token << "\" is malformed - " << my_error_message
			  << " - The rollup type is the \"serial_number+Port\" part of \"serial_number+Port = { 410321+1A, 410321+2A }\".";
			callers_error_message = o.str();
			return false;
		}

		auto it = rollups.rollups.find(rollupsKey);  // thank you, C++11.

		if (rollups.rollups.end() == it)
		{
			std::ostringstream o;
//*debug*/		o << " rollupsKey=\"" << rollupsKey << "\" ";
			o << "No such rollup \"" << listOfNameEqualsValueList.clauses.front()->name_token << "\".  Valid choices are:";
			for (auto& pear : rollups.rollups)
			{
//*debug*/			o << " key=\"" << pear.first << "\"";
				o << " " << pear.second->attributeNameCombo.attributeNameComboID;
			}
			callers_error_message = o.str();
			return false;
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
				callers_error_message = o.str();
				return false;
			}
			if (fieldsInAttributeNameCombo != fieldsInAttributeValueCombo)
			{
				std::ostringstream o;
				o << "rollup instance \"" << matchToken << "\" does not have the same number of \'+\'-delimited fields as the rollup type \"" << listOfNameEqualsValueList.clauses.front()->name_token << "\".";
				callers_error_message = o.str();
				return false;
			}
		}

		// We make a ListOfWorkloadIDs for each test host, as this has the toString() and fromString() methods to send over the pipe to ivyslave at the other end;
		// Then on the other end these are also the keys for looking up the iogenerator threads to post the parameter updates to.

//   XXXXXXX  And we are going to need to add "shorthand" for ranges of LDEVs, or hostnames, or PGs, ...

		// Then once we have the map ListOfWorkloadIDs[ivyscript_hostname] all built successfully
		// before we start sending any updates to ivyslave hosts we first apply the parameter updates
		// on the local copy we keep of the "next" subinterval IogeneratorInput objects.

		// We fail out with an error message if there is any problem applying the parameters locally.

		// Then when we know that these particular parameters apply successfully to these particular WorkloadIDs,
		// and we do local applys in parallel, of course, only then do we send the command to ivyslave to
		// update parameters.

		// We send out exactly the same command whether ivyslave is running or whether it's stopped waiting for commands.
		// - if it's running, the parameters will be applied to the "next subinterval".
		// - if it's stopped, the parameters will be applied to both subinterval objects which will be subintervals 0 and 1.


		int command_workload_IDs {0};

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
				callers_error_message = o.str();
				return false;
			}

			ListOfWorkloadIDs& listofWIDs = (*rollupInstance_it).second->workloadIDs;

			for (auto& wID : listofWIDs.workloadIDs)
			{
				host_subthread_pointers[wID.getHostPart()]->commandListOfWorkloadIDs.workloadIDs.push_back(wID);
				command_workload_IDs++;
			}
		}

		if (0 == command_workload_IDs)
		{
			callers_error_message = "editRollup() failed as no target workloads were selected.";
			return false;
		}

		// OK now we have built the individual lists of affected workload IDs for each test host for the upcoming command


		trim(parametersText);

		if (0 == parametersText.length())
		{
			callers_error_message = "editRollup() failed - called with an empty string as parameter updates.";
			return false;
		}

		rewrite_total_IOPS(parametersText, command_workload_IDs);
			// rewrites the first instance of "total_IOPS = 5000" to "IOPS=1000.000000000000" by dividing total_IOPS by instances.
			// total_IOPS number forms recognized:  1 .2 3. 4.4 1.0e3 1e+3 1.e-3 3E4, which is perhaps more flexible than with IOPS, which may only take fixed point numbers
			// returns true if it re-wrote, false if it didn't see "total_IOPS".

		// Now we first verify that user parameters apply successfully to the "next subinterval" IogeneratorInput objects in the affected WorkloadTrackers

		for (auto& pear : host_subthread_pointers)
		{
			for (auto& wID : pear.second->commandListOfWorkloadIDs.workloadIDs)
			{
				auto wit = workloadTrackers.workloadTrackerPointers.find(wID.workloadID);
				if (workloadTrackers.workloadTrackerPointers.end() == wit)
				{
					std::ostringstream o;
					o << "master_stuff::editRollup() - dreaded internal programming error - at the last moment the WorkloadTracker pointer lookup failed for workloadID = \"" << wID.workloadID << "\"";
					callers_error_message += o.str();
					std::cout << o.str() << std::endl;
				}

				WorkloadTracker* pWT = (*wit).second;
				std::string error_message;
				if (!pWT->wT_IogeneratorInput.setMultipleParameters(error_message, parametersText))
				{
					std::ostringstream o;
					o << "Failed setting parameters \"" << parametersText << "\" into local WorkloadTracker object for WorkloadID = \"" << wID.workloadID << std::endl << error_message;
					callers_error_message = o.str();
				}
			}
		}


		// Finally, we have a list of WorkloadIDs for each test host, and we have confirmed that the parameter updates applied successfully to the local copies of the IogeneratorInput object.

		for (auto& pear : host_subthread_pointers)
		{
			pipe_driver_subthread* p_host = pear.second;
			if (0 == p_host->commandListOfWorkloadIDs.workloadIDs.size()) continue;

			// get lock on talking to pipe_driver_subthread for this remote host
			{
				std::unique_lock<std::mutex> u_lk(p_host->master_slave_lk);

				// Already set earlier: ListOfWorkloadIDs p_host->commandListOfWorkloadIDs;
				p_host->commandStart.setToNow();
				p_host->commandString=std::string("[EditWorkload]");
				p_host->commandIogeneratorParameters = parametersText;

				p_host->command=true;
				p_host->commandSuccess=false;
				p_host->commandComplete=false;
			}
			p_host->master_slave_cv.notify_all();
		}

		// Now we wait for the hosts in no particular order to have finished executing the command.
		// The slave driver thread does commandFinish.setToNow() when it is done, so we can still see the min/max/avg by-host in parallel editRollup execution time

		for (auto& pear : host_subthread_pointers)
		{
			pipe_driver_subthread* p_host = pear.second;

			if (0 != std::string("[EditWorkload]").compare(p_host->commandString)) continue;

			// wait for the slave driver thread to say commandComplete
			{
				std::unique_lock<std::mutex> u_lk(p_host->master_slave_lk);
				while (!p_host->commandComplete) p_host->master_slave_cv.wait(u_lk);

				if (!p_host->commandSuccess)
				{
					std::ostringstream o;
					o << "ivymaster main thread notified by host driver subthread of failure to set parameters \""
					  << parametersText << "\" in WorkloadIDs " << p_host-> commandListOfWorkloadIDs.toString() << std::endl
					  << p_host->commandErrorMessage;  // don't forget to clear this when we get there
					callers_error_message = o.str();
					return false;
				}
			}
//*debug*/{std::ostringstream o; o << "p_host->commandFinish = " << p_host->commandFinish.format_as_datetime_with_ns() << ", beforeEditWorkload = " << beforeEditWorkload.format_as_datetime_with_ns() << std::endl; log(masterlogfile,o.str());}
			ivytime editWorkloadExecutionTime = p_host->commandFinish - beforeEditWorkload;

			editWorkloadExecutionTimeSeconds.push(editWorkloadExecutionTime.getlongdoubleseconds());
		}

		{
			std::ostringstream o;
			o << "[EditRollup] \"" << rollupText << "\" [parameters] \"" << original_parametersText << "\";   -    execution time by host thread: - number of hosts = " <<   editWorkloadExecutionTimeSeconds.count()
				<< " - average " << std::fixed << std::setprecision(1) << (100.*editWorkloadExecutionTimeSeconds.avg()) << " ms"
				<< " - min " << std::fixed << std::setprecision(1) << (100.*editWorkloadExecutionTimeSeconds.min()) << " ms"
				<< " - max " << std::fixed << std::setprecision(1) << (100.*editWorkloadExecutionTimeSeconds.max()) << " ms"
				<< std::endl;

			std::cout<< o.str();
			log(masterlogfile,o.str());
		}
	}

	return true;
}

std::string master_stuff::getWPthumbnail(int subinterval_index) // use subinterval_index = -1 to get the t=0 gather thumbnail
// throws std::invalid_argument.    Shows WP for each CLPR on the watch list as briefly as possible
{
	std::ostringstream result;
	for (auto& pear : cooldown_WP_watch_set) // std::set<std::pair<std::string /*CLPR*/, Subsystem*>>
	{
		const std::string& CLPR {pear.first};
		const std::string& cereal {pear.second->serial_number};
		Hitachi_RAID_subsystem* p_RAID = (Hitachi_RAID_subsystem*) pear.second;
		ivy_float WP;

		try
		{
			WP = p_RAID->get_wp(CLPR, subinterval_index);
		}
		catch (std::invalid_argument& iae)
		{
			std::ostringstream o;
			o << "master_stuff::getWPthumbnail(int subinterval_index="
				<< subinterval_index << ") - WP = Subsystem.get_wp(CLPR=\"" << CLPR << "\", subinterval_index=" << subinterval_index
				<< ") failed saying - " << iae.what();
			log(masterlogfile,o.str());
			kill_subthreads_and_exit();
			throw std::invalid_argument(o.str());
		}


		if (result.str().size() > 0) result << " ";

		result << cereal << ':' << CLPR << ":WP=" << std::fixed << std::setprecision(3) << (100.*WP) << '%';

		if (subinterval_index >-1)
		{
			ivy_float delta, slew_rate;
			delta = p_RAID->get_wp_change_from_last_subinterval( CLPR,subinterval_index );
			slew_rate = delta/subinterval_seconds;
			result << ' ';
			if (slew_rate >= 0.0) result << '+';
			result << std::fixed << std::setprecision(3) << (100.*slew_rate) << "%/sec";
		}
	}

	return result.str();
}


ivy_float master_stuff::get_rollup_metric
(
    const std::string& rollup_type_parameter,
    const std::string& rollup_instance_parameter,
    unsigned int       subinterval_number,
    const std::string& accumulator_type_parameter,
    const std::string& category_parameter,
    const std::string& metric_parameter
)
{
    auto rt_it = m_s.rollups.rollups.find(toLower(rollup_type_parameter));
    if (rt_it == m_s.rollups.rollups.end())
    {
        std::ostringstream o;
        o << "In master_stuff::get_rollup_metric(rollup_type=\"" << rollup_type_parameter << "\", rollup_instance=\"" << rollup_instance_parameter
            << "\", subinterval_number=" << subinterval_number << ", accumulator_type=\"" << accumulator_type_parameter
            << "\", category=\"" << category_parameter << "\", metric=\"" << metric_parameter << "\"," << std::endl;
        o << "<Error> did not find rollup type \"" << rollup_type_parameter << "\"." << std::endl;
        o << "Valid rollup types are:"; for (auto& pear : m_s.rollups.rollups) o << "  \"" << pear.first << "\""; o << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }
    RollupType* pRT = rt_it->second;

    auto ri_it = pRT->instances.find(toLower(rollup_instance_parameter));
    if (ri_it == pRT->instances.end())
    {
        std::ostringstream o;
        o << "In master_stuff::get_rollup_metric(rollup_type=\"" << rollup_type_parameter << "\", rollup_instance=\"" << rollup_instance_parameter
            << "\", subinterval_number=" << subinterval_number << ", accumulator_type=\"" << accumulator_type_parameter
            << "\", category=\"" << category_parameter << "\", metric=\"" << metric_parameter << "\"," << std::endl;
        o << "<Error> did not find rollup instance \"" << rollup_instance_parameter << "\"." << std::endl;
        o << "Valid rollup instances of this rollup type are:"; for (auto& pear : pRT->instances) o << "  \"" << pear.first << "\""; o << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }
    RollupInstance* pRI = ri_it->second;
    SequenceOfSubintervalRollup& sosr = pRI->subintervals;
    if (subinterval_number > sosr.sequence.size())
    {
        std::ostringstream o;
        o << "In master_stuff::get_rollup_metric(rollup_type=\"" << rollup_type_parameter << "\", rollup_instance=\"" << rollup_instance_parameter
            << "\", subinterval_number=" << subinterval_number << ", accumulator_type=\"" << accumulator_type_parameter
            << "\", category=\"" << category_parameter << "\", metric=\"" << metric_parameter << "\"," << std::endl;
        o << "<Error> requested subinterval number is greater than the number of subintervals in the rollup, which is " << sosr.sequence.size() << "." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }
    SubintervalRollup& subinterval_rollup = sosr.sequence[subinterval_number];

    SubintervalOutput& so = subinterval_rollup.outputRollup;

    accumulator_type_enum acc_type = string_to_accumulator_type_enum(accumulator_type_parameter);

    if (acc_type == accumulator_type_enum::error)
    {
        std::ostringstream o;
        o << "In master_stuff::get_rollup_metric(rollup_type=\"" << rollup_type_parameter << "\", rollup_instance=\"" << rollup_instance_parameter
            << "\", subinterval_number=" << subinterval_number << ", accumulator_type=\"" << accumulator_type_parameter
            << "\", category=\"" << category_parameter << "\", metric=\"" << metric_parameter << "\"," << std::endl;
        o << "<Error> Invalid accumulator_type.  Valid values are "  << accumulator_types() << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }

    Accumulators_by_io_type* p_acc;
    switch (acc_type)
    {
        case accumulator_type_enum::bytes_transferred:
            p_acc = & so.u.a.bytes_transferred;
            break;
        case accumulator_type_enum::service_time:
            p_acc = & so.u.a.service_time;
            break;
        case accumulator_type_enum::response_time:
            p_acc = & so.u.a.response_time;
            break;
        default:
            throw std::runtime_error("master_stuff::get_rollup_metric - this cannot happen.  impossible accumulator_type.");
    }

    for (int i = 0; i <= Accumulators_by_io_type::max_category_index(); i++)
    {
        if (stringCaseInsensitiveEquality(category_parameter, Accumulators_by_io_type::getRunningStatTitleByCategory(i)))
        {
            RunningStat<ivy_float, ivy_int> rs = p_acc->getRunningStatByCategory(i);

            if (stringCaseInsensitiveEquality(metric_parameter,"count"))             return (ivy_float) rs.count();
            if (stringCaseInsensitiveEquality(metric_parameter,"min"))               return rs.min();
            if (stringCaseInsensitiveEquality(metric_parameter,"max"))               return rs.max();
            if (stringCaseInsensitiveEquality(metric_parameter,"sum"))               return rs.sum();
            if (stringCaseInsensitiveEquality(metric_parameter,"avg"))               return rs.avg();
            if (stringCaseInsensitiveEquality(metric_parameter,"variance"))          return rs.variance();
            if (stringCaseInsensitiveEquality(metric_parameter,"standardDeviation")) return rs.standardDeviation();

            {
                std::ostringstream o;
                o << "In master_stuff::get_rollup_metric(rollup_type=\"" << rollup_type_parameter << "\", rollup_instance=\"" << rollup_instance_parameter
                    << "\", subinterval_number=" << subinterval_number << ", accumulator_type=\"" << accumulator_type_parameter
                    << "\", category=\"" << category_parameter << "\", metric=\"" << metric_parameter << "\"," << std::endl;
                o << "<Error> invalid metric \"" << metric_parameter << "\".  Valid metrics (accumulator access functions) are \"count\", \"min\", \"max\", \"sum\", \"avg\", \"variance\", and \"standardDeviation\"." << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }
    }
    {
        std::ostringstream o;
        o << "In master_stuff::get_rollup_metric(rollup_type=\"" << rollup_type_parameter << "\", rollup_instance=\"" << rollup_instance_parameter
            << "\", subinterval_number=" << subinterval_number << ", accumulator_type=\"" << accumulator_type_parameter
            << "\", category=\"" << category_parameter << "\", metric=\"" << metric_parameter << "\"," << std::endl;
        o << "<Error> invalid category \"" << category_parameter << "\".  Valid categories are " << Accumulators_by_io_type::getCategories() << "." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }
    return -1.0; // unreachable, but to keep compiler happy.

}

std::string master_stuff::focus_caption()
{
    if (! (have_pid || have_measure) ) return "";

    std::ostringstream o;

    if (p_focus_rollup == nullptr)
    {
        std::string m {"<Error> internal programming error. in master_stuff::focus_rollup_metric_caption(), p_focus_rollup == nullptr.\n"};
        std::cout << m;
        log(masterlogfile,m);
        kill_subthreads_and_exit();
    }

    o << "focus_rollup=" << p_focus_rollup->attributeNameCombo.attributeNameComboID;

    o << ", source=" << source_enum_to_string(source);

    switch (source)
    {
        case source_enum::workload:
            {
                o << ", category="         << category_enum_to_string   (category);
                o << ", accumulator_type=" << accumulator_type_to_string(accumulator_type);
                o << ", accessor="         << accessor_enum_to_string   (accessor);
            }
            break;

        case source_enum::RAID_subsystem:
            {
                 o << ", subsystem_element=" << subsystem_element;
                 o << ", element_metric="    << element_metric;
            }
            break;

        default:
            {
                std::string m {"<Error> internal programming error. in master_stuff::focus_rollup_metric_caption(), unknown source_enum value.\n"};
                std::cout << m;
                log(masterlogfile,m);
                kill_subthreads_and_exit();
            }
    }

    o << std::endl << std::endl;

    if (have_pid)
    {
        o << "dfc=pid, target_value=" << target_value;
        o << ", p=" << std::fixed << std::setprecision(2) << p_multiplier;
        o << ", i=" << std::fixed << std::setprecision(2) << i_multiplier;
        o << ", d=" << std::fixed << std::setprecision(2) << d_multiplier;

        if (m_s.have_within) { o << ", within = " << (100*m_s.within) << "%"; }

        RunningStat<ivy_float,ivy_int> avg_error;
        RunningStat<ivy_float,ivy_int> avg_error_integral;
        RunningStat<ivy_float,ivy_int> avg_error_derivative;

        for (auto& pear:m_s.p_focus_rollup->instances)
        {
            const RollupInstance* pRollupInstance = pear.second;
            avg_error           .push(pRollupInstance->error_signal);
            avg_error_integral  .push(pRollupInstance->error_integral);
            avg_error_derivative.push(pRollupInstance->error_derivative);
        }
        o << ", avg over " << avg_error.count() << " focus rollup instances:";
        if (m_s.target_value != 0)
        {
            if (avg_error.avg() >= 0.0)
            {
                o << (100.*( avg_error.avg() / m_s.target_value )) << "% above target_value";
            }
            else
            {
                o << (-100.*( avg_error.avg() / m_s.target_value )) << "% below target_value";
            }
        }
        o << " err. = " << std::fixed << std::setprecision(6) << avg_error.avg();
        o << ", err. int. = "   << std::fixed << std::setprecision(6) << avg_error_integral.avg();
        o << ", err. deriv. = " << std::fixed << std::setprecision(6) << avg_error_derivative.avg();

        o << std::endl << std::endl;
    }

    if (have_measure)
    {
        o << "measure=on";
        o << ", accuracy_plus_minus=\"" << std::fixed << std::setprecision(2) << (100.*accuracy_plus_minus_fraction) << "%\"";
        o << ", confidence=\""          << std::fixed << std::setprecision(2) << (100.*confidence                  ) << "%\"";
        o << ", warmup_seconds=" << std::fixed << std::setprecision(0) << warmup_seconds;
        o << ", measure_seconds=" << std::fixed << std::setprecision(0)<< measure_seconds;
        o << ", timeout_seconds=" << std::fixed << std::setprecision(0)<< timeout_seconds;

        if (m_s.have_max_above_or_below)
        {
            o << ", max_above_or_below=" << m_s.max_above_or_below;
        }

        if (m_s.haveCmdDev)
        {
            o << ", min_wp=\""              << std::fixed << std::setprecision(2) << (100.*min_wp                      ) << "%\"";
            o << ", max_wp=\""              << std::fixed << std::setprecision(2) << (100.*max_wp                      ) << "%\"";
            o << ", max_wp_change=\""       << std::fixed << std::setprecision(2) << (100.*max_wp_change               ) << "%\"";
        }
        o << std::endl << std::endl;
    }

    return o.str();
}

std::string last_result()
{
    if (m_s.lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_SUCCESS) return "success";
    if (m_s.lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_FAILURE) return "failure";
    if (m_s.lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_CONTINUE) return "continue <internal programming error if you see this>";
    return "<Error> internal programming error - last_result() - unknown m_s.lastEvaluateSubintervalReturnCode";
}

int exit_statement() { m_s.kill_subthreads_and_exit(); return 0; }

std::string show_rollup_structure()
{
    std::ostringstream o;

    o << "{";

    bool first_type_pass {true};

    for (auto& pear : m_s.rollups.rollups)
    {
        if (first_type_pass) first_type_pass = false;
        else                 o << ",";

        o << std::endl;

        const std::string& type = pear.first;

        o << "   { \"" << type << "\"(" << pear.second->instances.size() << ")," << std::endl;

        o << "      {";
        bool first_instance_pass {true};

        for (auto& kumquat : pear.second->instances)
        {
            if (first_instance_pass) first_instance_pass = false;
            else                     o << ",";

            o << std::endl;

            o << "         { \"" << kumquat.first << "\"(" << kumquat.second->workloadIDs.workloadIDs.size() << ")," << std::endl;
            o << "            {";
            bool need_comma = false;
            for (auto& w : kumquat.second->workloadIDs.workloadIDs)
            {
                if (need_comma) o << ",";
                else            need_comma=true;
                o << std::endl << "               \"" << w.workloadID << "\"";
            }
            o << std::endl << "            }";
            o << std::endl << "         }";

        }

        o << std::endl;
        o << "      }" << std::endl;
        o << "   }";

    }

    o << std::endl << "}" << std::endl;

    return o.str();
}


std::string master_stuff::focus_metric_ID()
{
    std::ostringstream o;

    if (have_pid || have_measure)
    {
        o << source_parameter;

        if (have_workload)
        {
            o
                << ":"
                << category_parameter
                << ":"
                << accumulator_type_parameter
                << ":"
                << accessor_parameter
                ;
        }
        if (have_RAID_subsystem)
        {
            o
                << ":"
                << subsystem_element
                << ":"
                << element_metric
                ;
        }
    }

    return o.str();
}


