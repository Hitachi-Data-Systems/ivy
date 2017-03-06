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
#include <sys/types.h>
#include <signal.h>

#include "ParameterValueLookupTable.h"
#include "MeasureDFC.h"
#include "run_subinterval_sequence.h"

#include "ivytime.h"
#include "ivydefines.h"
#include "ivyhelpers.h"
#include "LUN.h"
#include "AttributeNameCombo.h"
#include "IosequencerInputRollup.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "IosequencerInput.h"
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
    if (normalized_identifier_equality(s,"bytes_transferred"))  return accumulator_type_enum::bytes_transferred;
    if (normalized_identifier_equality(s,"response_time"))      return accumulator_type_enum::response_time;
    if (normalized_identifier_equality(s,"service_time"))       return accumulator_type_enum::service_time;
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

void master_stuff::kill_subthreads_and_exit()
{
    std::pair<bool,std::string> rc = shutdown_subthreads();
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
        o << "Failed trying to build " << scriptname << std::endl;
        std::cout << o.str();
        log(masterlogfile, o.str());
        oaf.close();
        std::remove(scriptname.c_str());
        return;
    }

    oaf << "#!/bin/bash" << std::endl;

    int rc = gethostname(my_hostname,sizeof(my_hostname)-1);
    if (0 != rc)
    {
        std::ostringstream o;
        o << method << "Unable to get my own hostname - gethostname() return code = " << rc << " - " << std::endl;
        o << "Failed trying to build " << scriptname << std::endl;
        std::cout << o.str();
        log(masterlogfile, o.str());
        oaf.close();
        std::remove(scriptname.c_str());
        return;
    }

    hostent* p_hostent = gethostbyname(my_hostname);
    if (p_hostent == NULL)
    {
        std::ostringstream o;
        o << method << "Unable to get my own IP address - gethostbyname(\"" << my_hostname << "\" failed." << std::endl;
        o << "Failed trying to build " << scriptname << std::endl;
        std::cout << o.str();
        log(masterlogfile, o.str());
        oaf.close();
        std::remove(scriptname.c_str());
        return;
    }

    oaf << std::endl << "# my canonical name is " << p_hostent->h_name << std::endl;

    if (p_hostent->h_addrtype != AF_INET)
    {
        std::ostringstream o;
        o << method << "My own address is not AF_INET, not an internet IP address." << std::endl;
        o << "Failed trying to build " << scriptname << std::endl;
        std::cout << o.str();
        log(masterlogfile, o.str());
        oaf.close();
        std::remove(scriptname.c_str());
        return;
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
            o << "Failed trying to build " << scriptname << std::endl;
            std::cout << o.str();
            log(masterlogfile, o.str());
            oaf.close();
            std::remove(scriptname.c_str());
            return;
        }

        if (p_hostent->h_addrtype != AF_INET)
        {
            std::ostringstream o;
            o << method << "For ivyslave host \"" << slavehost << "\", address is not AF_INET, not an internet IP address." << std::endl;
            o << "Failed trying to build " << scriptname << std::endl;
            std::cout << o.str();
            log(masterlogfile, o.str());
            oaf.close();
            std::remove(scriptname.c_str());
            return;
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

std::pair<bool,std::string> master_stuff::make_measurement_rollup_CPU(unsigned int firstMeasurementIndex, unsigned int lastMeasurementIndex)
{
		if (cpu_by_subinterval.size() < 3)
		{
			std::ostringstream o;
			o << "master_stuff::make_measurement_rollup_CPU() - The total number of subintervals in the cpu_by_subinterval sequence is " << cpu_by_subinterval.size()
			<< std::endl << "and there must be at least one warmup subinterval, one measurement subinterval, and one cooldown subinterval, "
			<< std::endl << "due to TCP/IP network time jitter when each test host hears the \"Go\" command.  This means we don't depend on NTP or clock synchronization.";
			return std::make_pair(false,o.str());
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
			return std::make_pair(false,o.str());
		}

		measurement_rollup_CPU.clear();

		for (unsigned int i=firstMeasurementIndex; i <= lastMeasurementIndex; i ++)
		{
			measurement_rollup_CPU.rollup(cpu_by_subinterval[i]);
		}

		return std::make_pair(true,"");
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
        if (normalized_identifier_equality(category_parameter, Accumulators_by_io_type::getRunningStatTitleByCategory(i)))
        {
            RunningStat<ivy_float, ivy_int> rs = p_acc->getRunningStatByCategory(i);

            if (stringCaseInsensitiveEquality(metric_parameter,"count"))             return (ivy_float) rs.count();
            if (stringCaseInsensitiveEquality(metric_parameter,"min"))               return rs.min();
            if (stringCaseInsensitiveEquality(metric_parameter,"max"))               return rs.max();
            if (stringCaseInsensitiveEquality(metric_parameter,"sum"))               return rs.sum();
            if (stringCaseInsensitiveEquality(metric_parameter,"avg"))               return rs.avg();
            if (stringCaseInsensitiveEquality(metric_parameter,"variance"))          return rs.variance();
            if (normalized_identifier_equality(metric_parameter,"standardDeviation")) return rs.standardDeviation();

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
        o << ", accuracy_plus_minus=" << std::fixed << std::setprecision(2) << (100.*accuracy_plus_minus_fraction) << "%";
        o << ", confidence="          << std::fixed << std::setprecision(2) << (100.*confidence                  ) << "%";
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

std::string master_stuff::last_result()
{
    if (lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_SUCCESS) return "success";
    if (lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_FAILURE) return "failure";
    if (lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_CONTINUE) return "continue <internal programming error if you see this>";
    return "<Error> internal programming error - last_result() - unknown lastEvaluateSubintervalReturnCode";
}

std::string last_result() {return m_s.last_result();}  // used by a builtin function

int exit_statement() { m_s.kill_subthreads_and_exit(); return 0; }

std::string master_stuff::show_rollup_structure()
{
    std::ostringstream o;

    o << "{";

    bool first_type_pass {true};

    for (auto& pear : rollups.rollups)
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

std::string show_rollup_structure(){ return m_s.show_rollup_structure();}

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

// =============  ivy engine API calls:

std::pair<bool /*success*/, std::string /* message */>
master_stuff::set_iosequencer_template(
    const std::string& template_name,
    const std::string& parameters)
{
    {
        std::ostringstream o;

        o << "ivy engine API set_iosequencer_template(";
        o << "template_name = \"" << template_name << "\"";
        o << ", parameters = \"" << parameters << "\"";
        o << ")" << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        log(ivy_engine_logfile,o.str());
    }

    std::unordered_map<std::string,IosequencerInput*>::iterator iogen_it;
    iogen_it=iosequencer_templates.find(toLower(template_name));
    if (iosequencer_templates.end()==iogen_it)
    {
        std::ostringstream o;
        o << "<Error> [SetIosequencerTemplate] - invalid iosequencer name \"" << template_name << "\". "  << std::endl;
        o << "Valid iosequencer names are:";
        for (auto& pear : iosequencer_templates) o << " \"" << pear.first << "\" ";
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }
    IosequencerInput* p=(*iogen_it).second;

    auto rv = p->setMultipleParameters(parameters);
    if (!rv.first)
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - set iosequencer template - for iosequencer named \"" << template_name
            << "\" - error setting parameter values \"" << parameters
            << "\" - error message was - " << rv.second << std::endl;
        return std::make_pair(false,o.str());
    }
    return std::make_pair(true,std::string(""));
}

std::pair<bool /*success*/, std::string /* message */>
master_stuff::createWorkload(
    const std::string& workloadName,
    const std::string& select_string,
    const std::string& iosequencerName,
    const std::string& parameters)
{
    {
        std::ostringstream o;
        o << "ivy engine API create_workload("
            << "workload_name = \"" << workloadName << "\""
            << ", select = \"" << select_string << "\""
            << ", iosequencer = \"" << iosequencerName << "\""
            << ", parameters = \"" << parameters << "\""
            << ")" << std::endl;
        std::cout << o.str();
        log (m_s.masterlogfile,o.str());
        log(ivy_engine_logfile,o.str());
    }

	// Look at proposed workload name

	if (!looksLikeHostname(workloadName))
	{
                ostringstream o;
		o << "Invalid workload name \"" << workloadName << "\".  Workload names start with an alphabetic [a-zA-Z] character and the rest is all alphanumerics and underscores [a-zA-Z0-9_].";
        return std::make_pair(false,o.str());
	}

    JSON_select select;

    if (!select.compile_JSON_select(select_string))
    {
        std::ostringstream o;
        o << "<Error> create workload] failed - invalid \"select\" expression \"" << select_string << "\" - error message follows: " << select.error_message << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }

    { std::ostringstream o; o << "      select " << select << std::endl; std::cout << o.str(); log(m_s.masterlogfile,o.str()); }

	// Look at [select] clause and filter against all discovered test LUNs to define "createWorkloadLUNs".

	LUNpointerList createWorkloadLUNs;

	if (select.is_null())
	{
        createWorkloadLUNs.clone(m_s.availableTestLUNs);
	}
	else
	{
        createWorkloadLUNs.clear_and_set_filtered_version_of(m_s.availableTestLUNs,select);
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

	// Look at iosequencer name

	std::unordered_map<std::string,IosequencerInput*>::iterator template_it;
	template_it=iosequencer_templates.find(iosequencerName);
	if (iosequencer_templates.end()==template_it)
	{
        ostringstream o;
        o << "Create workload - invalid [iosequencer] specification - no such I/O generator name found - \"" << iosequencerName << "\".";
        return std::make_pair(false,o.str());
	}

	// Make a copy of the selected iosequencer's default template as a starting point before
	// we apply the user's parameters to the default template to create a "stamp", an IosequencerInput object
	// that we will then clone for each WorkloadTracker that we create to track a particular test host workload thread.

	// Later on, the DynamicFeedbackController can decide to update these parameters.

	// Because we keep a local copy in ivymaster in the WorkloadTracker object of the most recent update to a workload's
	// IosequencerInput object, we can also do things like "increase IOPS by 10%".

	IosequencerInput i_i;  // the "stamp"
	i_i.fromString((*template_it).second->toString(),masterlogfile);

	// now apply the user-provided parameter settings on top of the template


	if (0<parameters.length())
	{
		auto rv = i_i.setMultipleParameters(parameters);
		if (!rv.first)
		{
            ostringstream o;
            o << "Create workload - failure setting initial parameters - \"" << parameters << "\"." <<std::endl
                << rv.second << std::endl;
            return std::make_pair(false,o.str());
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
            return std::make_pair(false,o.str());
		}
		std::map<std::string, WorkloadTracker*>::iterator it = workloadTrackers.workloadTrackerPointers.find(workloadID.workloadID);
		if (it != workloadTrackers.workloadTrackerPointers.end())
		{
            ostringstream o;
            o << "Create workload - failed - workload ID = \"" << workloadID.workloadID <<   "\" already exists.";
            return std::make_pair(false,o.str());
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
            return std::make_pair(false,o.str());
		}

		pipe_driver_subthread* p_host = (*it).second;

		// Next we make a new WorkloadTracker object for the remote thread we are going to create on a test host.

		WorkloadTracker* pWorkloadTracker = new WorkloadTracker(workloadID.workloadID, iosequencerName, p_LUN);

		pWorkloadTracker->wT_IosequencerInput.copy(i_i);

		workloadTrackers.workloadTrackerPointers[workloadID.workloadID] = pWorkloadTracker;

		// We time how long it takes to make the thread
		{
			ostringstream o;
			o << "Creating LUN workload thread on host \"" << p_host->ivyscript_hostname << "\", LUN \"" << p_LUN->attribute_value(std::string("LUN_name")) << "\", workload name \""
			  << workloadName << "\", iosequencer \"" << iosequencerName << "\", parameters \"" << i_i.toString() << "\".";
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
			p_host->commandIosequencerName = iosequencerName;
			p_host->commandIosequencerParameters = i_i.toString();

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
				  << workloadName << "\", iosequencer \"" << iosequencerName << "\", parameters \"" << i_i.toString() << "\".";
                return std::make_pair(false,o.str());
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


	return std::make_pair(true,std::string(""));
}

std::pair<bool /*success*/, std::string /* message */>
master_stuff::deleteWorkload(
    const std::string& workloadName,
    const std::string& select_string)
{
    {
        std::ostringstream o;
        o << "ivy engine API delete_workload("
            << "workload name = \"" << workloadName << "\""
            << ", select = \"" << select_string << "\""
            << ")" << std::endl;
        std::cout << o.str();
        log (m_s.masterlogfile,o.str());
        log(ivy_engine_logfile,o.str());
   }

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
            return std::make_pair(false,o.str());
        }
    }

    JSON_select select;

    if (!select.compile_JSON_select(select_string))
    {
        std::ostringstream o;
        o << "<Error> delete workload] failed - invalid \"select\" expression \"" << select_string << "\" - error message follows: " << select.error_message << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }
    { std::ostringstream o; o << "delete workload select " << select << std::endl; std::cout << o.str(); log(m_s.masterlogfile,o.str()); }

    for (auto& pear : workloadTrackers.workloadTrackerPointers)
    {
        const std::string& key = pear.first;
        WorkloadTracker* p_WorkloadTracker = pear.second;

        WorkloadID w;

        if (!w.set(key))
        {
		    ostringstream o;
		    o << "<Error> delete workload - internal programming error.  The key of an entry in workloadTrackers.workloadTrackerPointers did not parse as a valid WorkloadID.";
			std::cout << "Sorry if you are going to see this twice, but just in case: " << o.str() << std::endl;
            return std::make_pair(false,o.str());
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
                if (select.is_null())
                {
                    delete_list.push_back(w);
                }
                else
                {
                    if (select.matches(&(p_WorkloadTracker->workloadLUN)))
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
        std::cout << "Sorry if you are going to see this twice, but just in case: " << o.str() << std::endl;
        return std::make_pair(false,o.str());
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
            return std::make_pair(false,o.str());
		}

		pipe_driver_subthread* p_host = it->second;

        auto wit = workloadTrackers.workloadTrackerPointers.find(wid.workloadID);

        if (wit == workloadTrackers.workloadTrackerPointers.end())
        {
		    ostringstream o;
		    o << "<Error> [DeleteWorkload] - internal programming error.  workloadTrackers.workloadTrackerPointers.find(\"" << wid.workloadID << "\") failed when about to delete it.";
            return std::make_pair(false,o.str());
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
                return std::make_pair(false,o.str());
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


	return std::make_pair(true,std::string(""));
}

std::pair<bool /*success*/, std::string /* message */>
master_stuff::create_rollup(
      std::string attributeNameComboText
    , bool nocsvSection
    , bool quantitySection
    , bool maxDroopMaxtoMinIOPSSection
    , ivy_int quantity
    , ivy_float maxDroop
)
{
    {
        std::ostringstream o;
        o << "ivy engine API create_rollup(";
        o << "rollup_name = \"" << attributeNameComboText << "\"";
        o << ", nocsv = ";                          if (nocsvSection)                o << true; else o << "false";
        o << ", have_quantity_validation = ";       if (quantitySection)             o << true; else o << "false";
        o << ", have_max_IOPS_droop_validation = "; if (maxDroopMaxtoMinIOPSSection) o << true; else o << "false";
        o << ", quantity = " << quantity;
        o << ", max_droop = " << maxDroop;
        o << ")" << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        log(ivy_engine_logfile,o.str());
    }

    if ( !haveHosts )
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - attempt to create a rollup with no preceeding [hosts] statement to start up the ivy engine and discover the test configuration." << std::endl;
        return std::make_pair(false, o.str());
    }

    std::pair<bool,std::string> rc = rollups.addRollupType(attributeNameComboText, nocsvSection, quantitySection, maxDroopMaxtoMinIOPSSection, quantity, maxDroop);

    if (!rc.first) return rc;

    auto it = rollups.rollups.find(toLower(attributeNameComboText));
    if (rollups.rollups.end() == it)
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - create rollup failed - internal programming error.  Unable to find the new RollupType we supposedly just made."<< std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }

    //m_s.need_to_rebuild_rollups=true; - removed so scripting language builtins see up to date data structures
    rollups.rebuild();

    return rc;
}

std::pair<bool /*success*/, std::string /* message */>
master_stuff::edit_rollup(const std::string& rollupText, const std::string& original_parametersText)
// returns true on success
{
    {
        std::ostringstream o;
        o << "ivy engine API edit_rollup("
            << "rollup_name = \"" << rollupText << "\""
            << ", parameters = \"" << original_parametersText << "\""
            << ")" << std::endl;
        std::cout << o.str();
        log (m_s.masterlogfile,o.str());
        log(ivy_engine_logfile,o.str());
    }

//variables
	ListOfNameEqualsValueList listOfNameEqualsValueList(rollupText);
	WorkloadTrackers editRollupWorkloadTrackers;
	int fieldsInAttributeNameCombo,fieldsInAttributeValueCombo;
	ivytime beforeEditWorkload;  // it's called editRollup at the ivymaster level, but once we build the list of edits by host, at the host level it's editWorkload

    std::string parametersText = original_parametersText;
//code

	beforeEditWorkload.setToNow();
	editWorkloadExecutionTimeSeconds.clear();

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
            return std::make_pair(false,o.str());
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

		// We make a ListOfWorkloadIDs for each test host, as this has the toString() and fromString() methods to send over the pipe to ivyslave at the other end;
		// Then on the other end these are also the keys for looking up the iosequencer threads to post the parameter updates to.

//   XXXXXXX  And we are going to need to add "shorthand" for ranges of LDEVs, or hostnames, or PGs, ...

		// Then once we have the map ListOfWorkloadIDs[ivyscript_hostname] all built successfully
		// before we start sending any updates to ivyslave hosts we first apply the parameter updates
		// on the local copy we keep of the "next" subinterval IosequencerInput objects.

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
                return std::make_pair(false,o.str());
			}

			ListOfWorkloadIDs& listofWIDs = (*rollupInstance_it).second->workloadIDs;

			for (auto& wID : listofWIDs.workloadIDs)
			{
				host_subthread_pointers[wID.getHostPart()]->commandListOfWorkloadIDs.workloadIDs.push_back(wID);
				command_workload_IDs++;
			}
		}

		if (0 == command_workload_IDs) { return std::make_pair(false,string("edit_rollup() failed - zero test host workload threads selected\n")); }

		// OK now we have built the individual lists of affected workload IDs for each test host for the upcoming command


		trim(parametersText);

		if (0 == parametersText.length())
		{
			return std::make_pair(false,std::string("edit_rollup() failed - called with an empty string as parameter updates.\n"));
		}

		rewrite_total_IOPS(parametersText, command_workload_IDs);
			// rewrites the first instance of "total_IOPS = 5000" to "IOPS=1000.000000000000" by dividing total_IOPS by instances.
			// total_IOPS number forms recognized:  1 .2 3. 4.4 1.0e3 1e+3 1.e-3 3E4, which is perhaps more flexible than with IOPS, which may only take fixed point numbers
			// returns true if it re-wrote, false if it didn't see "total_IOPS".

		// Now we first verify that user parameters apply successfully to the "next subinterval" IosequencerInput objects in the affected WorkloadTrackers

		for (auto& pear : host_subthread_pointers)
		{
			for (auto& wID : pear.second->commandListOfWorkloadIDs.workloadIDs)
			{
				auto wit = workloadTrackers.workloadTrackerPointers.find(wID.workloadID);
				if (workloadTrackers.workloadTrackerPointers.end() == wit)
				{
					std::ostringstream o;
					o << "master_stuff::editRollup() - dreaded internal programming error - at the last moment the WorkloadTracker pointer lookup failed for workloadID = \"" << wID.workloadID << "\"" << std::endl;
                    return std::make_pair(false,o.str());
				}

				WorkloadTracker* pWT = (*wit).second;
				auto rv = pWT->wT_IosequencerInput.setMultipleParameters(parametersText);
				if (!rv.first)
				{
					std::ostringstream o;
					o << "<Error> Failed setting parameters \"" << parametersText
                        << "\" into local WorkloadTracker object for WorkloadID = \"" << wID.workloadID << "\"" << std::endl << rv.second;
                    return std::make_pair(false,o.str());
				}
			}
		}


		// Finally, we have a list of WorkloadIDs for each test host, and we have confirmed that the parameter updates applied successfully to the local copies of the IosequencerInput object.

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
				p_host->commandIosequencerParameters = parametersText;

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
                    return std::make_pair(false,o.str());
				}
			}
//*debug*/{std::ostringstream o; o << "p_host->commandFinish = " << p_host->commandFinish.format_as_datetime_with_ns() << ", beforeEditWorkload = " << beforeEditWorkload.format_as_datetime_with_ns() << std::endl; log(masterlogfile,o.str());}
			ivytime editWorkloadExecutionTime = p_host->commandFinish - beforeEditWorkload;

			editWorkloadExecutionTimeSeconds.push(editWorkloadExecutionTime.getlongdoubleseconds());
		}

		{
			std::ostringstream o;
			o << "     - execution time by host thread: - number of hosts = " <<   editWorkloadExecutionTimeSeconds.count()
				<< " - average " << std::fixed << std::setprecision(1) << (100.*editWorkloadExecutionTimeSeconds.avg()) << " ms"
				<< " - min " << std::fixed << std::setprecision(1) << (100.*editWorkloadExecutionTimeSeconds.min()) << " ms"
				<< " - max " << std::fixed << std::setprecision(1) << (100.*editWorkloadExecutionTimeSeconds.max()) << " ms"
				<< std::endl;

			log(masterlogfile,o.str());
			std::cout<< o.str();
            return std::make_pair(true,o.str());
		}
	}
	return std::make_pair(false,std::string("let Ian know if this happens."));
}

std::pair<bool /*success*/, std::string /* message */>
master_stuff::delete_rollup(const std::string& attributeNameComboText)
{
    {
        std::ostringstream o;
        o << "ivy engine API delete_rollup("
            << "rollup_name = \"" << attributeNameComboText << "\""
            << ")" << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        log(ivy_engine_logfile,o.str());
    }
    bool delete_all_not_all {false};

    if (attributeNameComboText.size()==0 || attributeNameComboText == "*") { delete_all_not_all = true;}

    if (delete_all_not_all)
    {
        // delete all rollups except "all"
        for (auto& pear : rollups.rollups)
        {
            if (stringCaseInsensitiveEquality("all", pear.first)) continue;

            auto rc = rollups.deleteRollup(pear.first);
            if ( !rc.first )
            {
                std::ostringstream o;
                o << "<Error> ivy engine API - for \"delete all rollups except the \'all\' rollup\" - failed trying to delete \""
                    << pear.first << "\" - " << rc.second << std::endl;
                log(m_s.masterlogfile,o.str());
                std::cout << o.str();
                return std::make_pair(false,o.str());
            }
        }
    }
    else
    {
        if (stringCaseInsensitiveEquality(std::string("all"),attributeNameComboText))
        {
            std::string msg = "\nI\'m so sorry, Dave, I cannot delete the \"all\" rollup.\n\n";
            log(m_s.masterlogfile,msg);
            std::cout << msg;
            return std::make_pair(false,msg);
        }

        auto rc = m_s.rollups.deleteRollup(attributeNameComboText);
        if ( !rc.first )
        {
            std::ostringstream o;
            o << "<Error> ivy engine API - delete rollup for \"" << attributeNameComboText << "\" failed - " << rc.second << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            return std::make_pair(false,o.str());
        }
    }

    //m_s.need_to_rebuild_rollups=true; - removed so scripting language builtins see up to date data structures
    rollups.rebuild();

    return std::make_pair(true,std::string(""));
}

std::pair<bool /*success*/, std::string /* message */>
master_stuff::go(const std::string& parameters)
{
    {
        std::ostringstream o;
        o << "ivy engine API go("
            << "parameters = \"" << parameters << "\""
            << " )" << std::endl;
        log (masterlogfile,o.str());
        std::cout << o.str();
        log(ivy_engine_logfile,o.str());
    }
    std::pair<bool,std::string> rc {};

    if ( !haveHosts )
    {
        std::ostringstream o;
        o << "<Error> ivy engine API \"go\" invoked with no preceeding ivy engine startup and no workloads created." << std::endl;
        rc.first=false;
        rc.second=o.str();
        return rc;
    }

    // This code starts with the get go.  I just had to say that.

    have_pid = false;
    have_measure = false;
    have_workload = false;
    have_RAID_subsystem = false;
    have_within = false;
    have_max_above_or_below = false;

    goStatementsSeen++;

    get_go.setToNow();

    {
        std:: ostringstream o;
        o << "step" << std::setw(4) << std::setfill('0') << (goStatementsSeen-1);
        stepNNNN=o.str();
    }
    trim(goParameters);

    std::pair<bool,std::string> r = go_parameters.fromString(parameters);
    if (!r.first)
    {
        std::ostringstream o;
        o << std::endl << "<Error> ivy engine API - Failed parsing [Go!] parameters \"" << parameters << "\"." << std::endl << std::endl
            << "[Go] parameters should look like:" << std::endl << "     parameter1=value1, parameter2=\"value 2\", ..." << std:: endl
            << "where parameter names are identifiers starting with an alphabetic and continuing with alphanumerics and underscores" << std::endl
            << "and where parameter values need to be enclosed in quotes if they contain any characters other than"
            << "English alphanumerics (\'a\'-\'z\', \'A\'-\'Z\', \'0\'-\'9\'), underscores (\'_\'), periods (\'.\'), or percent signs (\'%\')." << std::endl
            << "Example of OK unquoted value:  accuracy_plus_minus = 2.5%" << std::endl << std::endl
            << r.second << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }

    std::string valid_parameter_names = "stepname, subinterval_seconds, warmup_seconds, measure_seconds, cooldown_by_wp, catnap_time_seconds, post_time_limit_seconds";

    std::string valid_parameters_message =
        "The following parameter names are always valid:    stepname, subinterval_seconds, warmup_seconds, measure_seconds, cooldown_by_wp,\n"
        "                                                   catnap_time_seconds, post_time_limit_seconds.\n\n"
        "For dfc = pid, additional valid parameters are:    p, i, d, target_value, starting_total_IOPS, min_IOPS.\n\n"
        "For measure = on, additional valid parameters are: accuracy_plus_minus, confidence, max_wp, min_wp, max_wp_change, timeout_seconds\n\n."
        "For dfc=pid or measure=on, must have either source = workload, or source = RAID_subsystem.\n\n"
        "For source = workload, additional valid parameters are: category, accumulator_type, accessor.\n\n"
        "For source = RAID_subsystem, additional valid parameters are: subsystem_element, element_metric.\n\n";

    if (go_parameters.contains("stepname")) stepName = go_parameters.retrieve("stepname");
    else stepName = stepNNNN;

    if (!go_parameters.contains(std::string("subinterval_seconds"))) go_parameters.contents[normalize_identifier(std::string("subinterval_seconds"))] = subinterval_seconds_default;
    if (!go_parameters.contains(std::string("warmup_seconds")))      go_parameters.contents[normalize_identifier(std::string("warmup_seconds"))]      = warmup_seconds_default;
    if (!go_parameters.contains(std::string("measure_seconds")))     go_parameters.contents[normalize_identifier(std::string("measure_seconds"))]     = measure_seconds_default;
    if (!go_parameters.contains(std::string("cooldown_by_wp")))      go_parameters.contents[normalize_identifier(std::string("cooldown_by_wp"))]      = cooldown_by_wp_default;

    if (!go_parameters.contains(std::string("catnap_time_seconds")))
    {
        std::ostringstream o;
        o << std::fixed << catnap_time_seconds_default;
        go_parameters.contents[normalize_identifier(std::string("catnap_time_seconds"))] = o.str();
    }

    if (!go_parameters.contains(std::string("post_time_limit_seconds")))
    {
        std::ostringstream o;
        o << std::fixed << post_time_limit_seconds_default;
        go_parameters.contents[normalize_identifier(std::string("post_time_limit_seconds"))] = o.str();
    }

    if (go_parameters.contains(std::string("dfc")))
    {

        std::string controllerName = go_parameters.retrieve("dfc");

        if (stringCaseInsensitiveEquality("pid", controllerName))
        {
            have_pid = true;
            valid_parameter_names += ", dfc, p, i, d, target_value, starting_total_IOPS, min_IOPS";

            if (!go_parameters.contains(std::string("p")))                        go_parameters.contents[toLower(std::string("p"                  ))] = p_default;
            if (!go_parameters.contains(std::string("i")))                        go_parameters.contents[toLower(std::string("i"                  ))] = i_default;
            if (!go_parameters.contains(std::string("d")))                        go_parameters.contents[toLower(std::string("d"                  ))] = d_default;
            if (!go_parameters.contains(std::string("target_value")))             go_parameters.contents[normalize_identifier(std::string("target_value"       ))] = target_value_default;
            if (!go_parameters.contains(std::string("starting_total_IOPS")))      go_parameters.contents[normalize_identifier(std::string("starting_total_IOPS"))] = starting_total_IOPS_default;
            if (!go_parameters.contains(std::string("min_IOPS")))                 go_parameters.contents[normalize_identifier(std::string("min_IOPS"           ))] = min_IOPS_default;
        }
        else
        {
            std::ostringstream o;
            o << std::endl << "<Error> ivy engine API go() - \"dfc\", if specified, may only be set to \"pid\"." << std::endl << std::endl;
            std::cout << o.str();
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }

//        std::map<std::string, DynamicFeedbackController*>::iterator controller_it = availableControllers.find(toLower(controllerName));
//
//        if (availableControllers.end() == controller_it)
//        {
//            std::ostringstream o;
//            o << std::endl << "Failed - Dynamic Feedback Controller (dfc) \"" << controllerName << "\" not found. Valid values are:" << std::endl;
//            for (auto& pear : availableControllers) o << pear.first << std::endl;
//            std::cout << o.str();
//            log(masterlogfile,o.str());
//            kill_subthreads_and_exit();
//        }

        the_dfc.reset();
    }

    if (go_parameters.contains(std::string("measure")))
    {
        std::string measure_parameter_value = go_parameters.retrieve("measure");

        if (normalized_identifier_equality(measure_parameter_value,std::string("MB_per_second")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "workload";
            go_parameters.contents[toLower("category")] = "overall";
            go_parameters.contents[normalize_identifier("accumulator_type")] = "bytes_transferred";
            go_parameters.contents[normalize_identifier("accessor")] = "sum";
        }
        else if (stringCaseInsensitiveEquality(measure_parameter_value,std::string("IOPS")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[normalize_identifier("source")] = "workload";
            go_parameters.contents[toLower("category")] = "overall";
            go_parameters.contents[normalize_identifier("accumulator_type")] = "bytes_transferred";
            go_parameters.contents[toLower("accessor")] = "count";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("service_time_seconds")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "workload";
            go_parameters.contents[toLower("category")] = "overall";
            go_parameters.contents[normalize_identifier("accumulator_type")] = "service_time";
            go_parameters.contents[toLower("accessor")] = "avg";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("response_time_seconds")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "workload";
            go_parameters.contents[toLower("category")] = "overall";
            go_parameters.contents[normalize_identifier("accumulator_type")] = "response_time";
            go_parameters.contents[toLower("accessor")] = "avg";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("MP_core_busy_percent")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "RAID_subsystem";
            go_parameters.contents[normalize_identifier("subsystem_element")] = "MP_core";
            go_parameters.contents[normalize_identifier("element_metric")] = "busy_percent";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("PG_busy_percent")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "RAID_subsystem";
            go_parameters.contents[normalize_identifier("subsystem_element")] = "PG";
            go_parameters.contents[normalize_identifier("element_metric")] = "busy_percent";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("CLPR_WP_percent")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "RAID_subsystem";
            go_parameters.contents[normalize_identifier("subsystem_element")] = "CLPR";
            go_parameters.contents[normalize_identifier("element_metric")] = "WP_percent";
        }

        // maybe we have a measure_focus_rollup and a pid_focus_rollup and they default to focus_rollup which defaults to "all"?

        // How hard would it be to have measure and pid each on their own focus_rollup?

        measure_parameter_value = go_parameters.retrieve("measure");

        if (stringCaseInsensitiveEquality(std::string("on"),measure_parameter_value))
        {
            have_measure = true;
            valid_parameter_names += ", measure, accuracy_plus_minus, confidence, max_wp, min_wp, max_wp_change, timeout_seconds";
            if (!go_parameters.contains(std::string("accuracy_plus_minus"))) go_parameters.contents[normalize_identifier(std::string("accuracy_plus_minus"))] = accuracy_plus_minus_default;
            if (!go_parameters.contains(std::string("confidence")))          go_parameters.contents[normalize_identifier(std::string("confidence"))]          = confidence_default;
            if (!go_parameters.contains(std::string("min_wp")))              go_parameters.contents[normalize_identifier(std::string("min_wp"))]              = min_wp_default;
            if (!go_parameters.contains(std::string("max_wp")))              go_parameters.contents[normalize_identifier(std::string("max_wp"))]              = max_wp_default;
            if (!go_parameters.contains(std::string("max_wp_change")))       go_parameters.contents[normalize_identifier(std::string("max_wp_change"))]       = max_wp_change_default;
            if (!go_parameters.contains(std::string("timeout_seconds")))     go_parameters.contents[normalize_identifier(std::string("timeout_seconds"))]     = timeout_seconds_default;
        }
        else
        {
            if (!stringCaseInsensitiveEquality(std::string("off"),measure_parameter_value))
            {
                std::ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - invalid \"measure\" setting value \"" << measure_parameter_value << "\"." << std::endl
                    << "\"measure\" may only be set to \"on\" or \"off\", or to one of the following shorthand settings:" << std::endl
                    << R"(
    measure = MB_per_second           is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=bytes_transferred, accessor=sum
    measure = IOPS                    is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=bytes_transferred, accessor=count
    measure = service_time_seconds    is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=service_time,      accessor=avg
    measure = response_time_seconds   is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=response_time,     accessor=avg
    measure = MP_core_busy_percent    is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=MP_core, element_metric=busy_percent
    measure = PG_busy_percent         is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=PG,      element_metric=busy_percent
    measure = CLPR_WP_percent         is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=CLPR,    element_metric=WP_percent
)"
                << std::endl
                                   << "Go statement specifies parameter values " << go_parameters.toString() << std::endl;
                std::cout << o.str();
                log(masterlogfile,o.str());
                kill_subthreads_and_exit();
            }
        }

    }

    if (have_measure || have_pid)
    {
        valid_parameter_names += ", focus_rollup, source";

        if (!go_parameters.contains(std::string("focus_rollup"))) go_parameters.contents[normalize_identifier(std::string("focus_rollup"))] = focus_rollup_default;
        if (!go_parameters.contains(std::string("source")))       go_parameters.contents[normalize_identifier(std::string("source"))]       = source_default;

        source_parameter = go_parameters.retrieve("source");

        if (stringCaseInsensitiveEquality(source_parameter,std::string("workload")))
        {
            have_workload = true;
            valid_parameter_names += ", category, accumulator_type, accessor";

            if (!go_parameters.contains(std::string("category")))         go_parameters.contents[normalize_identifier(std::string("category"))]         = category_default;
            if (!go_parameters.contains(std::string("accumulator_type"))) go_parameters.contents[normalize_identifier(std::string("accumulator_type"))] = accumulator_type_default;
            if (!go_parameters.contains(std::string("accessor")))         go_parameters.contents[normalize_identifier(std::string("accessor"))]         = accessor_default;
        }
        else if (stringCaseInsensitiveEquality(source_parameter,std::string("RAID_subsystem")))
        {
            have_RAID_subsystem = true;
            valid_parameter_names += ", subsystem_element, element_metric";

            if (!go_parameters.contains(std::string("subsystem_element")))  go_parameters.contents[normalize_identifier(std::string("subsystem_element"))] = subsystem_element_default;
            if (!go_parameters.contains(std::string("element_metric")))     go_parameters.contents[normalize_identifier(std::string("element_metric"))]    = element_metric_default;
        }
        else
        {
            std::ostringstream o;
            o << std::endl << "<Error>  ivy engine API - go() - The only valid values for the \"source\" parameter are \"workload\" and \"RAID_subsystem\"." << std::endl << std::endl
                               << "[Go] statement parameter values " << go_parameters.toString() << std::endl;
            std::cout << o.str();
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }
    }

    if (have_pid && have_measure)
    {
        valid_parameter_names += ", within";
        if (go_parameters.contains(std::string("within")))
        {
            // within="1%"
            // must be greater than zero, less than or equal to one.

            have_within = true;

            try
            {
                within = number_optional_trailing_percent(go_parameters.retrieve("within"));
            }
            catch(std::invalid_argument& iaex)
            {
                std::ostringstream o;
                o << "<Error> ivy engine API - go() - Invalid \"within\" parameter value \"" << go_parameters.retrieve("within") << "\"." << std::endl;
                o << "Must be a number greater than zero and less than or equal to 1." << std::endl;
                std::cout << o.str();
                log(masterlogfile,o.str());
                kill_subthreads_and_exit();
            }
            if (within <= 0.0 || within >1)
            {
                std::ostringstream o;
                o << "<Error> ivy engine API - go() - Invalid \"within\" parameter value \"" << go_parameters.retrieve("within") << "\".";
                o << "  Must be a number greater than zero and less than or equal to 1." << std::endl;
                std::cout << o.str();
                log(masterlogfile,o.str());
                kill_subthreads_and_exit();
            }
        }

        valid_parameter_names += ", max_above_or_below";
        if (go_parameters.contains(std::string("max_above_or_below")))
        {
            // must be a positive integer

            // max number of consecutive intervals where the error signal stays the same sign

            // "within" may be what you want to use most of the time, but I'm leaving this here for now.

            have_max_above_or_below = true;

            std::string maob_parm = go_parameters.retrieve("max_above_or_below");

            trim(maob_parm);  // by reference

            if ( (!isalldigits(maob_parm))  || (0==( max_above_or_below = (unsigned int) atoi(maob_parm.c_str()) )) )
            {
                std::ostringstream o;
                o << "<Error> ivy engine API - go() - Invalid \"max_above_or_below\" parameter value \"" << go_parameters.retrieve("max_above_or_below") << "\"." << std::endl;
                o << "Must be all digits representing a positive integer." << std::endl;
                std::cout << o.str();
                log(masterlogfile,o.str());
                kill_subthreads_and_exit();
            }
        }
    }

    if (!go_parameters.containsOnlyValidParameterNames(valid_parameter_names))
    {
        std::ostringstream o;
        o << std::endl << "<Error> ivy engine API - go() - Invalid [Go] statement parameter." << std::endl << std::endl
                           << "Error found in [Go] statement parameter values: " << go_parameters.toString() << std::endl << std::endl
                           << valid_parameters_message << std::endl << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }


    {
        std::ostringstream o;
        o << std::endl << "Effective [Go!] statement parameters including defaulted parameters:" << go_parameters.toString() << std::endl << std::endl;
        std::cout << o.str();
        log(masterlogfile, o.str());
    }


//----------------------------------- catnap_time_seconds
    try
    {
        catnap_time_seconds = number_optional_trailing_percent(go_parameters.retrieve("catnap_time_seconds"));
    }
    catch(std::invalid_argument& iaex)
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - go() - Invalid catnap_time_seconds parameter value \"" << go_parameters.retrieve("catnap_time_seconds") << "\"." << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }

//----------------------------------- post_time_limit_seconds
    try
    {
        post_time_limit_seconds = number_optional_trailing_percent(go_parameters.retrieve("post_time_limit_seconds"));
    }
    catch(std::invalid_argument& iaex)
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - go() - Invalid post_time_limit_seconds parameter value \"" << go_parameters.retrieve("post_time_limit_seconds") << "\"." << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }

//----------------------------------- subinterval_seconds
    try
    {
        subinterval_seconds = number_optional_trailing_percent(go_parameters.retrieve("subinterval_seconds"));
    }
    catch(std::invalid_argument& iaex)
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - go() - Invalid subinterval_seconds parameter value \"" << go_parameters.retrieve("subinterval_seconds") << "\"." << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }

    if (subinterval_seconds < min_subinterval_seconds || subinterval_seconds > max_subinterval_seconds )
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - go() - Invalid subinterval_seconds parameter value \"" << go_parameters.retrieve("subinterval_seconds") << "\".  Permitted range is from " << min_subinterval_seconds << " to " << max_subinterval_seconds << "." << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }
    subintervalLength = ivytime(subinterval_seconds);
    if (subinterval_seconds < 3.)
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - go() - Invalid subinterval_seconds parameter value \"" << go_parameters.retrieve("subinterval_seconds") << "\".  Permitted range is from " << min_subinterval_seconds << " to " << max_subinterval_seconds << "." << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }

//----------------------------------- warmup_seconds
    try
    {
        warmup_seconds = number_optional_trailing_percent(go_parameters.retrieve("warmup_seconds"));
    }
    catch(std::invalid_argument& iae)
    {
        ostringstream o;
        o << "<Error> ivy engine API - go() - [Go] statement - invalid warmup_seconds parameter \"" << go_parameters.retrieve("warmup_seconds") << "\".  Must be a number." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }
    if (warmup_seconds < subinterval_seconds)
    {
        ostringstream o;
        o << "<Error> ivy engine API - go() - [Go] statement - invalid warmup_seconds parameter \"" << go_parameters.retrieve("warmup_seconds") << "\".  Must be at least as long as subinterval_seconds = ." << subinterval_seconds << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }
    min_warmup_count = (int) ceil( warmup_seconds / subinterval_seconds);


//----------------------------------- measure_seconds
    try
    {
        measure_seconds = number_optional_trailing_percent(go_parameters.retrieve("measure_seconds"));
    }
    catch(std::invalid_argument& iae)
    {
        ostringstream o;
        o << "<Error> ivy engine API - go() - [Go] statement - invalid measure_seconds parameter \"" << go_parameters.retrieve("measure_seconds") << "\".  Must be a number." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }
    if (measure_seconds < subinterval_seconds)
    {
        ostringstream o;
        o << "<Error> ivy engine API - go() - [Go] statement - invalid measure_seconds parameter \"" << go_parameters.retrieve("measure_seconds") << "\".  Must be at least as long as subinterval_seconds = ." << subinterval_seconds << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }
    min_measure_count = (int) ceil( measure_seconds / subinterval_seconds);

//----------------------------------- cooldown_by_wp
    if      ( stringCaseInsensitiveEquality(std::string("on"),  go_parameters.retrieve("cooldown_by_wp")) ) cooldown_by_wp = true;
    else if ( stringCaseInsensitiveEquality(std::string("off"), go_parameters.retrieve("cooldown_by_wp")) ) cooldown_by_wp = false;
    else
    {
        ostringstream o;
        o << "<Error> ivy engine API - go() - [Go] statement - invalid cooldown_by_wp parameter \"" << go_parameters.retrieve("cooldown_by_wp") << "\".  Must be \"on\" or \"off\"." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }


    if (have_pid || have_measure)
    {
//----------------------------------- focus_rollup
        focus_rollup_parameter = go_parameters.retrieve("focus_rollup");

        auto it = rollups.rollups.find(toLower(focus_rollup_parameter));

        if (it == rollups.rollups.end() )
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - [Go] statement - invalid focus_rollup parameter \"" << focus_rollup_parameter << "\"." << std::endl;
            o << "The rollups are"; for (auto& pear : rollups.rollups) o << "  \"" << pear.first << "\""; o << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }

        p_focus_rollup = it->second;

        if (have_workload)
        {
            source = source_enum::workload;

//----------------------------------- category
            category_parameter = go_parameters.retrieve("category");
            if (stringCaseInsensitiveEquality(category_parameter,"overall")) category = category_enum::overall;
            else if (stringCaseInsensitiveEquality(category_parameter,"read")) category = category_enum::read;
            else if (stringCaseInsensitiveEquality(category_parameter,"write")) category = category_enum::write;
            else if (stringCaseInsensitiveEquality(category_parameter,"random")) category = category_enum::random;
            else if (stringCaseInsensitiveEquality(category_parameter,"sequential")) category = category_enum::sequential;
            else if (normalized_identifier_equality(category_parameter,"random_read")) category = category_enum::random_read;
            else if (normalized_identifier_equality(category_parameter,"sequential_read")) category = category_enum::sequential_read;
            else if (normalized_identifier_equality(category_parameter,"sequential_write")) category = category_enum::sequential_write;
            else
            {
                ostringstream o;
                o << "<Error> ivy engine API - go() - [Go] statement - invalid category parameter \"" << category_parameter << "\"." << std::endl;
                o << "The categories are overall, read, write, random, sequential, random_read, random_write, sequential_read, and sequential_write."; o << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
//----------------------------------- accumulator_type
            accumulator_type_parameter = go_parameters.retrieve("accumulator_type");
            if (normalized_identifier_equality(accumulator_type_parameter,"bytes_transferred")) accumulator_type = accumulator_type_enum::bytes_transferred;
            else if (normalized_identifier_equality(accumulator_type_parameter,"service_time")) accumulator_type = accumulator_type_enum::service_time;
            else if (normalized_identifier_equality(accumulator_type_parameter,"response_time")) accumulator_type = accumulator_type_enum::response_time;
            else
            {
                ostringstream o;
                o << "<Error> ivy engine API - go() - [Go] statement - invalid accumulator_type parameter \"" << accumulator_type_parameter << "\"." << std::endl;
                o << "The accumulator_types are bytes_transferred, service_time, and response_timee."; o << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
//----------------------------------- accessor
            accessor_parameter = go_parameters.retrieve("accessor");
            if (stringCaseInsensitiveEquality(accessor_parameter,"avg")) accessor = accessor_enum::avg;
            else if (stringCaseInsensitiveEquality(accessor_parameter,"count")) accessor = accessor_enum::count;
            else if (stringCaseInsensitiveEquality(accessor_parameter,"min")) accessor = accessor_enum::min;
            else if (stringCaseInsensitiveEquality(accessor_parameter,"max")) accessor = accessor_enum::max;
            else if (stringCaseInsensitiveEquality(accessor_parameter,"sum")) accessor = accessor_enum::sum;
            else if (stringCaseInsensitiveEquality(accessor_parameter,"variance")) accessor = accessor_enum::variance;
            else if (normalized_identifier_equality(accessor_parameter,"standardDeviation")) accessor = accessor_enum::standardDeviation;
            else
            {
                ostringstream o;
                o << "<Error> ivy engine API - go() - invalid accessor parameter \"" << accessor_parameter << "\"." << std::endl;
                o << "The accessors are avg, count, min, max, sum, variance, and standardDeviation."; o << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }

        }
        else if (have_RAID_subsystem)
        {
            source = source_enum::RAID_subsystem;

            subsystem_element = go_parameters.retrieve("subsystem_element");
            element_metric = go_parameters.retrieve("element_metric");

            bool found_element {false};
            bool found_metric {false};

            for (auto& e_pear : subsystem_summary_metrics)
            {
                std::string& element = e_pear.first;

                if (stringCaseInsensitiveEquality(element,subsystem_element))
                {
//----------------------------------- subsystem_element
                    found_element = true;

                    for (const std::pair<std::string,unsigned char>& metric_pair : e_pear.second)
                    {
                        const std::string metric = metric_pair.first;

                        if (stringCaseInsensitiveEquality(metric,element_metric))
                        {
//----------------------------------- element_metric
                            found_metric = true;

                            break;
                        }
                    }

                    if (!found_metric)
                    {
                        ostringstream o;

                        o << "<Error> ivy engine API - go() - [Go] statement - invalid element_metric \"" << element_metric << "\" for subsystem_element \"" << subsystem_element << "\"." << std::endl;

                        o << "The subsystem_elements and their element_metrics are:" << std::endl;

                        for (auto& e_pear : subsystem_summary_metrics)
                        {
                            std::string& element = e_pear.first;

                            o << "   subsystem_element = " << element << std::endl;

                            for (const std::pair<std::string,unsigned char>& metric_pair : e_pear.second)
                            {
                                const std::string metric = metric_pair.first;

                                o << "       element_metric = " << element;
                                o << std::endl;

                            }
                        }

                        o << std::endl << "The definition of the these subsystems metrics which are rolled up for each rollup instance is in subsystem_summary_metrics in master_stuff.h." << std::endl << std::endl;

                        log(masterlogfile,o.str());
                        std::cout << o.str();
                        kill_subthreads_and_exit();
                    }
                    break;
                }

            }
            if (!found_element)
            {
                ostringstream o;

                o << "<Error> ivy engine API - go() - [Go] statement - invalid subsystem_element parameter \"" << subsystem_element << "\"." << std::endl;

                o << "The subsystem_elements and their element_metrics are:" << std::endl;

                for (auto& e_pear : subsystem_summary_metrics)
                {
                    std::string& element = e_pear.first;

                    o << "   subsystem_element = " << element << std::endl;

                    for (const std::pair<std::string,unsigned char>& metric_pair : e_pear.second)
                    {
                        const std::string metric = metric_pair.first;

                        o << "       element_metric = " << element << std::endl;

                    }
                }

                o << std::endl << "The definition of these subsystem element metrics which are rolled up for each rollup instance is in subsystem_summary_metrics in master_stuff.h." << std::endl << std::endl;

                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }
        else
        {
            ostringstream o;
            o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid source parameter \"" << source_parameter << "\".  Must be \"workload\" or \"RAID_subsystem\"." << std::endl << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }
    }

    if (have_pid)
    {
        // valid_parameter_names += ", dfc, p, i, d, target_value, starting_total_IOPS, min_IOPS";

            // If a parameter is not present, "retrieve" returns a null string.

//----------------------------------- element_metric
            target_value_parameter = go_parameters.retrieve("target_value");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFCcategory::PID - when setting \"target_value\" parameter to value \""
                    << target_value_parameter << "\": ";

                target_value = number_optional_trailing_percent(target_value_parameter,where_the.str());
            }

//----------------------------------- p
            p_parameter               = go_parameters.retrieve("p");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFCcategory::PID - when setting \"p\" parameter to value \""
                    << p_parameter << "\": ";

                p_multiplier = number_optional_trailing_percent(p_parameter,where_the.str());
            }

//----------------------------------- i
            i_parameter               = go_parameters.retrieve("i");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFCcategory::PID - when setting \"i\" parameter  to value \""
                    << i_parameter << "\": ";

                i_multiplier = number_optional_trailing_percent(i_parameter,where_the.str());
            }

//----------------------------------- d
            d_parameter               = go_parameters.retrieve("d");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFCcategory::PID - when setting \"d\" parameter  to value \""
                    << d_parameter << "\": ";

                d_multiplier = number_optional_trailing_percent(d_parameter,where_the.str());
            }

//----------------------------------- starting_total_IOPS
            starting_total_IOPS_parameter = go_parameters.retrieve("starting_total_IOPS");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFCcategory::PID - when setting \"starting_total_IOPS\" parameter to value \""
                    << starting_total_IOPS << "\": ";

                starting_total_IOPS = number_optional_trailing_percent(starting_total_IOPS_parameter,where_the.str());
                if (starting_total_IOPS < 0.0)
                {
                    ostringstream o;
                    o << std::endl << "<Error> [Go] statement - invalid starting_total_IOPS parameter \"" << starting_total_IOPS_parameter << "\".  May not be negative." << std::endl << std::endl;
                    log(masterlogfile,o.str());
                    std::cout << o.str();
                    kill_subthreads_and_exit();
                }
            }

//----------------------------------- min_IOPS
            min_IOPS_parameter = go_parameters.retrieve("min_IOPS");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFCcategory::PID - when setting \"min_IOPS\" parameter to value \""
                    << min_IOPS_parameter << "\": ";

                min_IOPS = number_optional_trailing_percent(min_IOPS_parameter,where_the.str());
                if (min_IOPS < 0.0)
                {
                    ostringstream o;
                    o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid min_IOPS parameter \"" << min_IOPS_parameter << "\".  May not be negative." << std::endl << std::endl;
                    log(masterlogfile,o.str());
                    std::cout << o.str();
                    kill_subthreads_and_exit();
                }
            }

    }


    if (have_measure)

    {
//----------------------------------- accuracy_plus_minus
        accuracy_plus_minus_parameter = go_parameters.retrieve("accuracy_plus_minus");

        try
        {
            accuracy_plus_minus_fraction = number_optional_trailing_percent(accuracy_plus_minus_parameter);
        }
        catch (std::invalid_argument& iaex)
        {
            std::ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid accuracy_plus_minus parameter \"" << accuracy_plus_minus_parameter << "\".  Must be a number optionally with a trailing percent sign," << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }


//----------------------------------- confidence
        confidence_parameter = go_parameters.retrieve("confidence");
        try
        {
            confidence = number_optional_trailing_percent(confidence_parameter);
        }
        catch(std::invalid_argument& iae)
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid confidence parameter \"" << confidence_parameter << "\".  Must be a number." << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }


//----------------------------------- timeout_seconds
        timeout_seconds_parameter = go_parameters.retrieve("timeout_seconds");
        try
        {
            timeout_seconds = number_optional_trailing_percent(timeout_seconds_parameter);
        }
        catch(std::invalid_argument& iae)
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid timeout_seconds parameter \"" << timeout_seconds_parameter << "\".  Must be a number." << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }

//----------------------------------- min_wp
        min_wp_parameter = go_parameters.retrieve("min_wp");
        try
        {
            min_wp = number_optional_trailing_percent(min_wp_parameter);

        }
        catch (std::invalid_argument& iaex)
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid min_wp parameter \"" << min_wp_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0." << std::endl;
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }
        if ( min_wp < 0.0 || min_wp > 1.0 )
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid min_wp_range parameter \"" << min_wp_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0." << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }

//----------------------------------- max_wp
        max_wp_parameter = go_parameters.retrieve("max_wp");
        try
        {
            max_wp = number_optional_trailing_percent(go_parameters.retrieve("max_wp"));

        }
        catch (std::invalid_argument& iaex)
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid max_wp parameter \"" << max_wp_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0." << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }
        if ( max_wp < 0.0 || max_wp > 1.0 || max_wp < min_wp)
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid max_wp_change parameter \"" << max_wp_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0 and must not be less than min_wp." << std::endl;
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }

//----------------------------------- max_wp_change
        max_wp_change_parameter = go_parameters.retrieve("max_wp_change");
        try
        {
            max_wp_change = number_optional_trailing_percent(max_wp_change_parameter);

        }
        catch (std::invalid_argument& iaex)
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid max_wp_change parameter \"" << max_wp_change_parameter << "\"." << std::endl;
            std::cout << o.str();
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }
        if ( max_wp_change < 0.0 || max_wp_change > 1.0 )
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid max_wp_change parameter \"" << max_wp_change_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0." << std::endl;
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }
    }

    the_dfc.reset();

    void prepare_dedupe();

    prepare_dedupe();

    run_subinterval_sequence(&the_dfc);

    ivytime went; went.setToNow();
    ivytime flight_duration = went - get_go;

    {
        std::ostringstream o;
        o << "********* " << stepNNNN << " duration " << flight_duration.format_as_duration_HMMSS() << " \"" << stepName << "\"" << std::endl;

        step_times << o.str();
        log(masterlogfile, o.str());
        std::cout << o.str();
        rc.second = o.str();
    }
    rc.first=true;
    return rc;
}


std::pair<bool,std::string>
master_stuff::shutdown_subthreads()
{
    {
        std::string m = "ivy engine API shutdown_subthreads()\n";
        log(masterlogfile,m);
        log(ivy_engine_logfile,m);
        std::cout << m;
    }

	int notified=0;

	for (auto&  pear : host_subthread_pointers)
	{
		{
			//std::unique_lock<std::mutex> u_lk(pear.second->master_slave_lk);
			pear.second->commandString=std::string("die");
			pear.second->command=true;

			std::ostringstream o;
			o << "master_stuff::shutdown_subthreads() - posting \"die\" command to subthread for host " << pear.first
                << " pid = " << pear.second->pipe_driver_pid << ", gettid() thread ID = " << pear.second->linux_gettid_thread_id<< std::endl;
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
					//std::unique_lock<std::mutex> u_lk(p_pds->master_slave_lk);
					p_pds->commandString=std::string("die");
					p_pds->command=true;

					std::ostringstream o;
					o << "master_stuff::shutdown_subthreads() - posting \"die\" command to subthread for subsystem " << pear.first
                        << " pid = " << p_pds->pipe_driver_pid << ", gettid() thread ID = " << p_pds->linux_gettid_thread_id<< std::endl;
					log (masterlogfile, o.str());
				}
				p_pds->master_slave_cv.notify_all();
				notified_subsystems++;
			}
		}
	}

    bool need_harakiri {false};

	for (auto&  pear : host_subthread_pointers)
	{
        bool died_on_its_own;
        died_on_its_own = false;

        std::chrono::milliseconds nap(100);

        for (unsigned int i = 0; i < 70; i++)
        {
//            if (pear.second->master_slave_lk.try_lock())
//            {
                if (pear.second->dead)
                {
                    died_on_its_own = true;
                    break;
                }
//            }
            std::this_thread::sleep_for(nap);
        }

        if (!died_on_its_own)
        {
            std::ostringstream o;
            o << "********** Subthread for test host " << pear.first << " did not exit normally. " << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();

            need_harakiri = true;
        }
	}

	for (auto& pear : subsystems)
	{
		if (0==pear.second->type().compare(std::string("Hitachi RAID")))
		{
			Hitachi_RAID_subsystem* pRAID = (Hitachi_RAID_subsystem*) pear.second;
			if (pRAID->pRMLIBthread)
			{
				pipe_driver_subthread* p_pds = pRAID->pRMLIBthread;
				{
                    bool died_on_its_own;
                    died_on_its_own = false;

                    std::chrono::milliseconds nap(100);

                    for (unsigned int i = 0; i < 30; i++)
                    {
  //                      if (p_pds->master_slave_lk.try_lock())
  //                      {
                            if (p_pds->dead)
                            {
                                died_on_its_own = true;
                                break;
                            }
  //                      }
                        std::this_thread::sleep_for(nap);
                    }

                    if (!died_on_its_own)
                    {
                        std::ostringstream o;
                        o << "********** Subthread for command device on host " << p_pds->ivyscript_hostname
                            << " subsystem serial number " << pRAID->serial_number
                            << " did not exit normally." << std::endl;
                        log(m_s.masterlogfile,o.str());
                        std::cout << o.str();

                        need_harakiri = true;
                    }
				}
			}
		}
	}

	std::ostringstream o;
	o << "master_stuff::shutdown_subthreads() told " << notified << " host driver and "
        << notified_subsystems << " command device subthreads to die." << std::endl;
	log(masterlogfile,o.str());

    bool something_bad_happened {false};

	for (auto& host : hosts)
	{
		{
			std::ostringstream o;
			o << "scp " << SLAVEUSERID << '@' << host << ":" << IVYSLAVELOGFOLDERROOT IVYSLAVELOGFOLDER << "/log.ivyslave." << host << "* " << testFolder << "/logs";
			if (0 == system(o.str().c_str()))
			{
				log(masterlogfile,std::string("success: ")+o.str()+std::string("\n"));
				std::ostringstream rm;
				rm << "ssh " << SLAVEUSERID << '@' << host << " rm -f " << IVYSLAVELOGFOLDERROOT IVYSLAVELOGFOLDER << "/log.ivyslave." << host << "*";
				if (0 == system(rm.str().c_str()))
					log(masterlogfile,std::string("success: ")+rm.str()+std::string("\n"));
				else
				{
					log(masterlogfile,std::string("failure: ")+rm.str()+std::string("\n"));
					something_bad_happened = true;
				}
			}
			else {something_bad_happened = true; log(masterlogfile,std::string("failure: ")+o.str()+std::string("\n"));}
		}
	}

    if (need_harakiri)
    {
        std::ostringstream o;
        o << "One or more subthreads did not exit normally, abnormal termination." << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        kill(getpid(),SIGKILL);
    }
    else
    {
        for (auto& thread : threads)
        {
            thread.join();
        }

        for (auto& thread : ivymaster_RMLIB_threads)
        {
            thread.join();
        }
        {
            std::ostringstream o;
            if (m_s.overall_success)
            {
                o << "Successful run." << std::endl;
            }
            else
            {
                o << "All threads exited on their own, but there was an error." << std::endl;

            }
            std::cout << o.str();
            log(m_s.masterlogfile, o.str());
        }
    }

    return std::make_pair((!something_bad_happened) && (!need_harakiri),std::string(""));

}

