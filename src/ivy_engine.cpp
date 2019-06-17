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
using namespace std;
#include <string>
using namespace std::string_literals;  // for std::string literals like "bork"s
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
#include <sys/stat.h>
#include <cmath>

#include "ParameterValueLookupTable.h"
#include "MeasureCtlr.h"
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
#include "ivy_engine.h"
#include "NameEqualsValueList.h"
#include "ListOfNameEqualsValueList.h"
#include "Ivy_pgm.h"
#include "skew_weight.h"

extern bool routine_logging;

ivy_engine m_s;

std::string outputFolderRoot() {return m_s.outputFolderRoot;}
std::string masterlogfile()    {return m_s.masterlogfile;}
std::string testName()         {return m_s.testName;}
std::string testFolder()       {return m_s.testFolder;}
std::string stepNNNN()         {return m_s.stepNNNN;}
std::string stepName()         {return m_s.stepName;}
std::string stepFolder()       {return m_s.stepFolder;}

void say_and_log(const std::string& s)
{
    std::cout << s;
    log(m_s.masterlogfile,s);
}

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

bool ivy_engine::some_cooldown_WP_not_empty()
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
            std::ostringstream o; o << "MeasureCtlr::cooldown_return_code() - for subsystem " << pear.second->serial_number << ", get_WP(CLPR=\"" << CLPR
                << "\", subinterval=" << (pear.second->gathers.size()-2) <<") failed saying - " << iae.what() << std::endl;
            log (m_s.masterlogfile,o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }

        if (wp > m_s.subsystem_WP_threshold)
        {
            {
                std::ostringstream o;
                o << "With cooldown_by_wp = on, a CLPR Write Pending % is " << std::fixed << std::setprecision(2) << (100.0 * wp) << "%"
                  << ", which is above subsystem_WP_threshold = " << std::fixed << std::setprecision(1) << (100.0 * m_s.subsystem_WP_threshold) << "%" << std::endl;
                std::cout << o.str();
                if (routine_logging) { log(masterlogfile,o.str()); }
            }
            return true;
        }
    }

    return false;
}


bool ivy_engine::some_subsystem_still_busy()
{
    RollupInstance* p_allall = m_s.rollups.get_all_equals_all_instance();

    if (p_allall->subsystem_data_by_subinterval.size() == 0) return false;

    ivy_float avg_busy = p_allall->subsystem_data_by_subinterval.back().avg_MP_core_busy_fraction();

    if (avg_busy < subsystem_busy_threshold)
    {
        cooldown_by_MP_busy_stay_down_count ++;

        if (cooldown_by_MP_busy_stay_down_count >= cooldown_by_MP_busy_stay_down_count_limit)
        {
            return false;
        }
        else
        {
            std::ostringstream o;
            o << "With cooldown_by_MP_busy=on, average MP core % busy is " << std::fixed << std::setprecision(2) << (100.0 * avg_busy) << "%"
              << ", below the subsystem_busy_threshold " << std::fixed << std::setprecision(1) << (100.0 * m_s.subsystem_busy_threshold) << "% for " << cooldown_by_MP_busy_stay_down_count
              << " of the required " << cooldown_by_MP_busy_stay_down_count_limit << " subintervals." << std::endl;
            std::cout << o.str();
            if (routine_logging) { log(masterlogfile,o.str()); }
            return true;
        }
    }
    else // avg_busy > m_s.subsystem_busy_threshold
    {
        cooldown_by_MP_busy_stay_down_count = 0;
        std::ostringstream o;
        o << "With cooldown_by_MP_busy=on, subsystem average MP core % busy is " << std::fixed << std::setprecision(2) << (100.0 * avg_busy) << "%"
          << ", which is above subsystem_busy_threshold = " << std::fixed << std::setprecision(1) << (100.0 * m_s.subsystem_busy_threshold) << "%" << std::endl;
        std::cout << o.str();
        if (routine_logging) { log(masterlogfile,o.str()); }

        return true;
    }
    return false;
}


bool rewrite_total_IOPS(std::string& parametersText, ivy_float fraction_of_total_IOPS)
{
	// rewrites the first instance of "total_IOPS = 5000" to "IOPS=1000.000000000000" by dividing total_IOPS by instances.

	// total_IOPS number forms recognized:  1 .2 3. 4.4 1.0e3 1e+3 1.e-3 3E4

	// returns true if it re-wrote, false if it didn't see "total_IOPS"

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
                {   // number is in quotes
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
                    v *= fraction_of_total_IOPS;
                    std::ostringstream newv;
                    newv << "IOPS=" << std::fixed << std::setprecision(12) << v;

                    parametersText.replace(start_point,last_plus_one_point-start_point,newv.str());

                    return true;
                }
                else
                {   // number is not in quotes
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

                        value *= fraction_of_total_IOPS;

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

void ivy_engine::error(std::string m)
{
    std::ostringstream o;
    o << "Error: " << m << std::endl;
    std::cout << o.str();
    log(masterlogfile,o.str());
    kill_subthreads_and_exit();
}

void ivy_engine::kill_subthreads_and_exit()
{
    std::pair<bool,std::string> rc = shutdown_subthreads();
	exit(0);
}

void ivy_engine::write_clear_script()
{
    // This writes a script to invoke the "clear_hung_ivy_threads" executable on this ivy master host,
    // as well as once for each distinct IP address for ivydriver hosts.

    std::string my_IP;

    std::map<std::string,std::string> other_ivydriver_hosts; // <IP address, ivyscript hostname>

    const unsigned int max_hostnamesize = 100;
    char my_hostname[max_hostnamesize];
    my_hostname[max_hostnamesize-1] = 0;

    const std::string method = "ivy_engine::write_clear_script() - ";

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
            o << method << "Unable to get IP address for ivydriver host (\"" << slavehost << "\"." << std::endl;
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
            o << method << "For ivydriver host \"" << slavehost << "\", address is not AF_INET, not an internet IP address." << std::endl;
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
            std::string ivydriver_IP = o.str();
            auto it = other_ivydriver_hosts.find(ivydriver_IP);
            if ( (0 != my_IP.compare(ivydriver_IP)) && (it == other_ivydriver_hosts.end()) )
            {
                other_ivydriver_hosts[ivydriver_IP] = slavehost;
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
                oaf << "# duplicate ivydriver host " << slavehost << " ( " << ivydriver_IP << " )" << std::endl;
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

std::pair<bool,std::string> ivy_engine::make_measurement_rollup_CPU(unsigned int firstMeasurementIndex, unsigned int lastMeasurementIndex)
{
		if (cpu_by_subinterval.size() < 3)
		{
			std::ostringstream o;
			o << "ivy_engine::make_measurement_rollup_CPU() - The total number of subintervals in the cpu_by_subinterval sequence is " << cpu_by_subinterval.size()
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
			o << "ivy_engine::make_measurement_rollup_CPU() - Invalid first (" << firstMeasurementIndex << ") and last (" << lastMeasurementIndex << ") measurement period indices."
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

std::string ivy_engine::getWPthumbnail(int subinterval_index) // use subinterval_index = -1 to get the t=0 gather thumbnail
// throws std::invalid_argument.    Shows WP for each CLPR on the watch list as briefly as possible
{
	std::ostringstream result;

	result << "Available test CLPRs:";

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
			o << "ivy_engine::getWPthumbnail(int subinterval_index="
				<< subinterval_index << ") - WP = Subsystem.get_wp(CLPR=\"" << CLPR << "\", subinterval_index=" << subinterval_index
				<< ") failed saying - " << iae.what();
			log(masterlogfile,o.str());
			kill_subthreads_and_exit();
			throw std::invalid_argument(o.str());
		}



		result << " <" << cereal << " " << CLPR << ": WP = " << std::fixed << std::setprecision(2) << (100.*WP) << '%';

		if (subinterval_index >-1)
		{
			ivy_float delta, slew_rate;
			delta = p_RAID->get_wp_change_from_last_subinterval( CLPR,subinterval_index );
			slew_rate = delta/subinterval_seconds;
			result << " ";
			if (slew_rate >= 0.0) result << '+';
			result << std::fixed << std::setprecision(2) << (100.*slew_rate) << "%/sec";
		}

		result << ">";
	}

	return result.str();
}


ivy_float ivy_engine::get_rollup_metric
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
        o << "In ivy_engine::get_rollup_metric(rollup_type=\"" << rollup_type_parameter << "\", rollup_instance=\"" << rollup_instance_parameter
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
        o << "In ivy_engine::get_rollup_metric(rollup_type=\"" << rollup_type_parameter << "\", rollup_instance=\"" << rollup_instance_parameter
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
        o << "In ivy_engine::get_rollup_metric(rollup_type=\"" << rollup_type_parameter << "\", rollup_instance=\"" << rollup_instance_parameter
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
        o << "In ivy_engine::get_rollup_metric(rollup_type=\"" << rollup_type_parameter << "\", rollup_instance=\"" << rollup_instance_parameter
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
            throw std::runtime_error("ivy_engine::get_rollup_metric - this cannot happen.  impossible accumulator_type.");
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
                o << "In ivy_engine::get_rollup_metric(rollup_type=\"" << rollup_type_parameter << "\", rollup_instance=\"" << rollup_instance_parameter
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
        o << "In ivy_engine::get_rollup_metric(rollup_type=\"" << rollup_type_parameter << "\", rollup_instance=\"" << rollup_instance_parameter
            << "\", subinterval_number=" << subinterval_number << ", accumulator_type=\"" << accumulator_type_parameter
            << "\", category=\"" << category_parameter << "\", metric=\"" << metric_parameter << "\"," << std::endl;
        o << "<Error> invalid category \"" << category_parameter << "\".  Valid categories are " << Accumulators_by_io_type::getCategories() << "." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }
    return -1.0; // unreachable, but to keep compiler happy.

}

std::string ivy_engine::focus_caption()
{
    if (! (have_pid || have_measure) ) return "";

    std::ostringstream o;

    if (p_focus_rollup == nullptr)
    {
        std::string m {"<Error> internal programming error. in ivy_engine::focus_rollup_metric_caption(), p_focus_rollup == nullptr.\n"};
        std::cout << m;
        log(masterlogfile,m);
        kill_subthreads_and_exit();
    }

    o << "focus_rollup=" << p_focus_rollup->attributeNameCombo.attributeNameComboID;

    if (have_measure
        && source == source_enum::workload
        && category == category_enum::overall
        && accumulator_type == accumulator_type_enum::bytes_transferred
        && accessor == accessor_enum::sum)
    {
        o << ", "; // measure = MB_per_second
    }
    else if (have_measure
        && source == source_enum::workload
        && category == category_enum::overall
        && accumulator_type == accumulator_type_enum::bytes_transferred
        && accessor == accessor_enum::count)
    {
        o << ", "; // measure = IOPS
    }
    else if (have_measure
        && source == source_enum::workload
        && category == category_enum::overall
        && accumulator_type == accumulator_type_enum::service_time
        && accessor == accessor_enum::avg)
    {
        o << ", "; // measure = service_time_seconds
    }
    else if (have_measure
        && source == source_enum::workload
        && category == category_enum::overall
        && accumulator_type == accumulator_type_enum::response_time
        && accessor == accessor_enum::avg)
    {
        o << ", "; // measure = response_time_seconds
    }
    else if (have_measure
        && source == source_enum::RAID_subsystem
        && subsystem_element == "MP core"
        && element_metric == "busy %")
    {
        o << ", "; // measure = MP_core_busy_percent
    }
    else if (have_measure
        && source == source_enum::RAID_subsystem
        && subsystem_element == "PG"
        && element_metric == "busy %")
    {
        o << ", "; // measure = PG_busy_percent
    }
    else if (have_measure
        && source == source_enum::RAID_subsystem
        && subsystem_element == "CLPR"
        && element_metric == "WP_percent")
    {
        o << ", "; // measure = CLPR_WP_percent
    }
    else
    {
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
                    std::string m {"<Error> internal programming error. in ivy_engine::focus_rollup_metric_caption(), unknown source_enum value.\n"};
                    std::cout << m;
                    log(masterlogfile,m);
                    kill_subthreads_and_exit();
                }
        }

        o << std::endl << std::endl;
    }

    if (have_pid)
    {
        o << "dfc = pid, ";

        if (have_measure
            && source == source_enum::workload
            && category == category_enum::overall
            && accumulator_type == accumulator_type_enum::service_time
            && accessor == accessor_enum::avg)
        {
            // measure = service_time_seconds
            o << "target service time = " << std::fixed << std::setprecision(2) << (1000.0 * target_value) << " ms";
            o << ", operating range = { (" << low_IOPS << " IOPS, "
                << std::fixed << std::setprecision(2) << (1000.0*low_target)
                << " ms), (" << high_IOPS << " IOPS, "
                << std::fixed << std::setprecision(2) << (1000.0*high_target) << " ms) }";
        }
        else if (have_measure
            && source == source_enum::workload
            && category == category_enum::overall
            && accumulator_type == accumulator_type_enum::response_time
            && accessor == accessor_enum::avg)
        {
            // measure = response_time_seconds
            o << "target response time = " << std::fixed << std::setprecision(2) << (1000.0 * target_value) << " ms";
            o << ", operating range = { (" << low_IOPS << " IOPS, "
                << std::fixed << std::setprecision(2) << (1000.0*low_target)
                << " ms), (" << high_IOPS << " IOPS, "
                << std::fixed << std::setprecision(2) << (1000.0*high_target) << " ms) }";
        }
        else if (have_measure
            && source == source_enum::RAID_subsystem
            && subsystem_element == "MP core"
            && element_metric == "busy %")
        {
            // measure = MP_core_busy_percent
            o << "target MP core busy = " << std::fixed << std::setprecision(2) << (100.0 * target_value) << "%";
            o << ", operating range = { (" << low_IOPS << " IOPS, "
                << std::fixed << std::setprecision(2) << (100.0*low_target)
                << "%), (" << high_IOPS << " IOPS, "
                << std::fixed << std::setprecision(2) << (100.0*high_target) << "%) }";
        }
        else if (have_measure
            && source == source_enum::RAID_subsystem
            && subsystem_element == "PG"
            && element_metric == "busy %")
        {
            // measure = PG_busy_percent
            o << "target PG busy = " << std::fixed << std::setprecision(2) << (100.0 * target_value) << "%";
            o << ", operating range = { (" << low_IOPS << " IOPS, "
                << std::fixed << std::setprecision(2) << (100.0*low_target)
                << "%), (" << high_IOPS << " IOPS, "
                << std::fixed << std::setprecision(2) << (100.0*high_target) << "%) }";
        }
        else if (have_measure
            && source == source_enum::RAID_subsystem
            && subsystem_element == "CLPR"
            && element_metric == "WP_percent")
        {
            // measure = CLPR_WP_percent
            o << "target CLPR Write Pending = " << std::fixed << std::setprecision(2) << (100.0 * target_value) << "%";
            o << ", operating range = { (" << low_IOPS << " IOPS, "
                << std::fixed << std::setprecision(2) << (100.0*low_target)
                << "%), (" << high_IOPS << " IOPS, "
                << std::fixed << std::setprecision(2) << (100.0*high_target) << "%) }";
        }
        else
        {
            o << "target = " << target_value;
            o << ", operating range = { (" << low_IOPS << " IOPS, " << low_target << "), (" << high_IOPS << " IOPS, " << high_target << ") }";
        }


//        o << ", p = " << std::fixed << std::setprecision(2) << p_multiplier;
//        o << ", i = " << std::fixed << std::setprecision(2) << i_multiplier;
//        o << ", d = " << std::fixed << std::setprecision(2) << d_multiplier;

//        if (m_s.have_within) { o << ", within = " << (100*m_s.within) << "%"; }

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
//        o << ", over " << avg_error.count() << " instances of the focus rollup, for only the most recent subinterval the average error was ";
//        if (m_s.target_value != 0)
//        {
//            if (avg_error.avg() >= 0.0)
//            {
//                o << (100.*( avg_error.avg() / m_s.target_value )) << "% above target_value";
//            }
//            else
//            {
//                o << (-100.*( avg_error.avg() / m_s.target_value )) << "% below target_value";
//            }
//        }
//        o << " err. = " << std::fixed << std::setprecision(6) << avg_error.avg();
//        o << ", err. int. = "   << std::fixed << std::setprecision(6) << avg_error_integral.avg();
//        o << ", err. deriv. = " << std::fixed << std::setprecision(6) << avg_error_derivative.avg();

        o << std::endl << std::endl;
    }

    if (have_measure)
    {
        if (source == source_enum::workload
            && category == category_enum::overall
            && accumulator_type == accumulator_type_enum::bytes_transferred
            && accessor == accessor_enum::sum)
        {
            o << "measure = MB_per_second";
        }
        else if (source == source_enum::workload
            && category == category_enum::overall
            && accumulator_type == accumulator_type_enum::bytes_transferred
            && accessor == accessor_enum::count)
        {
            o << "measure = IOPS";
        }
        else if (source == source_enum::workload
            && category == category_enum::overall
            && accumulator_type == accumulator_type_enum::service_time
            && accessor == accessor_enum::avg)
        {
            o << "measure = service_time_seconds";
        }
        else if (source == source_enum::workload
            && category == category_enum::overall
            && accumulator_type == accumulator_type_enum::response_time
            && accessor == accessor_enum::avg)
        {
            o << "measure = response_time_seconds";
        }
        else if (source == source_enum::RAID_subsystem
            && subsystem_element == "MP core"
            && element_metric == "busy %")
        {
            o << "measure = MP_core_busy_percent";
        }
        else if (source == source_enum::RAID_subsystem
            && subsystem_element == "PG"
            && element_metric == "busy %")
        {
            o << "measure = PG_busy_percent";
        }
        else if (source == source_enum::RAID_subsystem
            && subsystem_element == "CLPR"
            && element_metric == "WP_percent")
        {
            o << "measure = CLPR_WP_percent";
        }
        else
        {
            o << "measure = on";
        }

        o << ", accuracy_plus_minus=" << std::fixed << std::setprecision(2) << (100.*accuracy_plus_minus_fraction) << "%";
        o << ", confidence="          << std::fixed << std::setprecision(2) << (100.*confidence                  ) << "%";
        o << ", warmup_seconds=" << std::fixed << std::setprecision(0) << warmup_seconds;
        o << ", measure_seconds=" << std::fixed << std::setprecision(0)<< measure_seconds;

        {
            int seconds = (int) timeout_seconds;
            int minutes = seconds / 60;
                seconds = seconds % 60;
            int hours   = minutes / 60;
                minutes = minutes % 60;
            int days  = hours / 24;
                hours = hours % 24;

            o << ", timeout_seconds = ";
            if (days > 0) o << days << " days and ";
            o          << std::setw(2) << std::setfill('0') << hours
                << ":" << std::setw(2) << std::setfill('0') << minutes
                << ":" << std::setw(2) << std::setfill('0') << seconds;
        }

//        if (m_s.have_max_above_or_below)
//        {
//            o << ", max_above_or_below=" << m_s.max_above_or_below;
//        }

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

std::string ivy_engine::last_result()
{
    if (lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_SUCCESS) return "success";
    if (lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_FAILURE) return "failure";
    if (lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_CONTINUE) return "continue <internal programming error if you see this>";
    return "<Error> internal programming error - last_result() - unknown lastEvaluateSubintervalReturnCode";
}

std::string last_result() {return m_s.last_result();}  // used by a builtin function

int exit_statement() { m_s.kill_subthreads_and_exit(); return 0; }

std::string ivy_engine::show_rollup_structure()
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

std::string ivy_engine::focus_metric_ID()
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

std::string ivy_engine_get(std::string thing)   //
{
    auto r = m_s.get(thing);
    if (!r.first)
    {
        std::ostringstream o;
        o << "For ivyscript builtin function ivy_engine_get(thing=\"" << thing << "\") "
            << "the corresponding ivy engine API call failed saying:" << std::endl
            << r.second;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
    }
    return r.second;
}

int ivy_engine_set(std::string thing, std::string value)
{
    auto r = m_s.set(thing,value);
    if (!r.first)
    {
        std::ostringstream o;
        o << "For ivyscript builtin function ivy_engine_set(thing=\"" << thing << "\", value=\"" << value << "\") "
            << "the corresponding ivy engine API call failed saying:" << std::endl
            << r.second;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        return 0;
    }
    return 1;
}

// =============  ivy engine API calls:


std::pair<bool /*success*/, std::string /* message */>
ivy_engine::set_iosequencer_template(
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
ivy_engine::createWorkload(
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

	// Later on, the MeasureController can decide to update these parameters.

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
	//	- in ivydriver to lookup an I/O driver subthread

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
				  p_LUN->attribute_value(std::string("ivyscript hostname"))
				+ std::string("+")
				+ p_LUN->attribute_value(std::string("LUN_name"))
				+ std::string("+")
				+ workloadName
			)
		)
		{
		        ostringstream o;
			o << "Create workload - internal programming error - invalid WorkloadID = \"" << workloadID <<   "\" generated that is not well formed as \"ivyscript_hostname+LUN_name+workload_name\".";
            return std::make_pair(false,o.str());
		}
		std::map<std::string, WorkloadTracker*>::iterator it = workloadTrackers.workloadTrackerPointers.find(workloadID.workloadID);
		if (it != workloadTrackers.workloadTrackerPointers.end())
		{
            ostringstream o;
            o << "Create workload - failed - workload ID = \"" << workloadID <<   "\" already exists.";
            return std::make_pair(false,o.str());
		}
	}

	createWorkloadExecutionTimeSeconds.clear();

	for (auto& p_LUN : createWorkloadLUNs.LUNpointers)  // maybe come back later and do this for a set of LUNs in parallel, like we do for editWorkload().
	{
		std::string ivyscript_hostname = p_LUN->attribute_value(std::string("ivyscript hostname"));

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
			o << "Creating workload on host \"" << p_host->ivyscript_hostname << "\", LUN \"" << p_LUN->attribute_value(std::string("LUN_name")) << "\", workload name \""
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

			p_host->commandHost = p_LUN->attribute_value(std::string("ivyscript hostname"));
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
		o << "      remote create workload execution time summary: total time " << createWorkloadExecutionTimeSeconds.sum()
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
ivy_engine::deleteWorkload(
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
			<< " seconds - minimum " << deleteWorkloadExecutionTimeSeconds.min()
			<< " seconds - maximum " << deleteWorkloadExecutionTimeSeconds.max() << " seconds." << std::endl;

/*debug*/ std::cout<< o.str();

		log(masterlogfile,o.str());
	}


	return std::make_pair(true,std::string(""));
}

std::pair<bool /*success*/, std::string /* message */>
ivy_engine::create_rollup(
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

    rollups.rebuild();

    return rc;
}

std::pair<bool /*success*/, std::string /* message */>
ivy_engine::edit_rollup(const std::string& rollupText, const std::string& original_parametersText, bool do_not_log_API_call)
// returns true on success
{
    {
        std::ostringstream o;
        o << "ivy engine API edit_rollup("
            << "rollup_name = \"" << rollupText << "\""
            << ", parameters = \"" << original_parametersText << "\""
            << ")" << std::endl;
        std::cout << o.str()
            << std::endl;  // The extra new line groups fine-grained DFC lines showing first what happened in English, and then here show setting the resulting new IOPS.
        log (m_s.masterlogfile,o.str());
        if (!do_not_log_API_call) { log(ivy_engine_logfile,o.str()); }
    }

//variables
	ListOfNameEqualsValueList listOfNameEqualsValueList(rollupText);
	int fieldsInAttributeNameCombo,fieldsInAttributeValueCombo;
	ivytime beforeEditRollup;  // it's called editRollup at the ivymaster level, but once we build the list of edits by host, at the host level it's editWorkload

    std::string parametersText = original_parametersText;
//code

	beforeEditRollup.setToNow();

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

	int command_workload_IDs {0};

	if (0 == listOfNameEqualsValueList.clauses.size())
	{
	    for (auto& pear: host_subthread_pointers)
	    {
	        pear.second->commandListOfWorkloadIDs.clear();
	    }

	    // The default is to apply to all workloads

	    for (auto& pear : workloadTrackers.workloadTrackerPointers)
	    {
	        WorkloadID wID;
	        wID.set(pear.first);
	        if (!wID.isWellFormed)
            {
                std::ostringstream o;
                o << "Internal programming error at line " << __LINE__ << " of " << __FILE__ << "bad WorkloadID \"" << pear.first << "\"" << std::endl;
                throw std::runtime_error(o.str());
            }

            auto host_iter = host_subthread_pointers.find(wID.ivyscript_hostname);
            if (host_iter == host_subthread_pointers.end())
            {
                std::ostringstream o;
                o << "Internal programming error at line " << __LINE__ << " of " << __FILE__ << "bad hostname part \"" << wID.ivyscript_hostname << "\" in WorkloadID \"" << pear.first << "\"" << std::endl;
                throw std::runtime_error(o.str());
            }
            host_iter->second->commandListOfWorkloadIDs.workloadIDs.push_back(wID);
            command_workload_IDs++;
	    }
	}
	else
	{
		// validate the "serial_number+Port" part of "serial_number+Port = { 410321+1A, 410321+2A }"

		std::string rollupsKey;
		std::string my_error_message;

		AttributeNameCombo anc;
		auto rc = anc.set(listOfNameEqualsValueList.clauses.front()->name_token, &m_s.TheSampleLUN);
		if (!rc.first)
		{
		    std::ostringstream o;
		    o << "Edit rollup - invalid atrribute name combo \"" << listOfNameEqualsValueList.clauses.front()->name_token << "\" - " << rc.second;
		    return std::make_pair(false,o.str());
		}

		rollupsKey = anc.attributeNameComboID;

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
/*debug*/		o << " rollupsKey=\"" << rollupsKey << "\" ";
			o << "No such rollup \"" << listOfNameEqualsValueList.clauses.front()->name_token << "\".  Valid choices are:";
			for (auto& pear : rollups.rollups)
			{
/*debug*/			o << " key=\"" << pear.first << "\"";
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

		// We make a ListOfWorkloadIDs for each test host, as this has the toString() and fromString() methods to send over the pipe to ivydriver at the other end;
		// Then on the other end these are also the keys for looking up the iosequencer threads to post the parameter updates to.

//   XXXXXXX  And we are going to need to add "shorthand" for ranges of LDEVs, or hostnames, or PGs, ...

		// Then once we have the map ListOfWorkloadIDs[ivyscript_hostname] all built successfully
		// before we start sending any updates to ivydriver hosts we first apply the parameter updates
		// on the local copy we keep of the "next" subinterval IosequencerInput objects.

		// We fail out with an error message if there is any problem applying the parameters locally.

		// Then when we know that these particular parameters apply successfully to these particular WorkloadIDs,
		// and we do local applys in parallel, of course, only then do we send the command to ivydriver to
		// update parameters.

		// We send out exactly the same command whether ivydriver is running or whether it's stopped waiting for commands.
		// - if it's running, the parameters will be applied to both the running IosequencerInput and the non-running one.
		// - if it's stopped, the parameters will be applied to both subinterval objects which will be subintervals 0 and 1.

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

    }

    if (0 == command_workload_IDs) { return std::make_pair(false,string("edit_rollup() failed - zero test host workload threads selected\n")); }

    // OK now we have built the individual lists of affected workload IDs for each test host for the upcoming command


    // iterate over all the affected workload IDs to populate the "skew" data.

    // This was added later, and to make it easier to code & maintain, it is kept apart.

    skew_data s_d {};
    s_d.clear();

    for (auto& pear : host_subthread_pointers)
    {
        for (auto& wID : pear.second->commandListOfWorkloadIDs.workloadIDs)
        {
            auto wit = workloadTrackers.workloadTrackerPointers.find(wID.workloadID);
            if (workloadTrackers.workloadTrackerPointers.end() == wit)
            {
                std::ostringstream o;
                o << "ivy_engine::editRollup() - dreaded internal programming error - at the last moment the WorkloadTracker pointer lookup failed for workloadID = \"" << wID.workloadID << "\"" << std::endl;
                return std::make_pair(false,o.str());
            }

            WorkloadTracker* pWT = (*wit).second;

            ivy_float skew_w = pWT->wT_IosequencerInput.skew_weight;

            s_d.push(wID, skew_w);
        }
    }

    trim(parametersText);

    if (0 == parametersText.length())
    {
        return std::make_pair(false,std::string("edit_rollup() failed - called with an empty string as parameter updates.\n"));
    }

    for (auto& pear : host_subthread_pointers)
    {
        pipe_driver_subthread* p_host = pear.second;
        p_host->workloads_by_text.clear();
    }

    // Now we first verify that user parameters apply successfully to the "next subinterval" IosequencerInput objects in the affected WorkloadTrackers

    for (auto& pear : host_subthread_pointers)
    {
        pipe_driver_subthread* p_host = pear.second;

        for (auto& wID : pear.second->commandListOfWorkloadIDs.workloadIDs)
        {
            auto wit = workloadTrackers.workloadTrackerPointers.find(wID.workloadID);
            if (workloadTrackers.workloadTrackerPointers.end() == wit)
            {
                std::ostringstream o;
                o << "ivy_engine::editRollup() - dreaded internal programming error - at the last moment the WorkloadTracker pointer lookup failed for workloadID = \"" << wID.workloadID << "\"" << std::endl;
                return std::make_pair(false,o.str());
            }

            WorkloadTracker* pWT = (*wit).second;

            std::string edited_text = parametersText;

            rewrite_total_IOPS(edited_text, s_d.fraction_of_total_IOPS(wID.workloadID));

//*debug*/ {std::ostringstream o; o << "for wID = " << wID.workloadID << "parametersText = \"" << parametersText << "\", edited_text = \"" << edited_text << "\"." << std::endl; std::cout << o.str(); log(masterlogfile,o.str());}

            // rewrites the first instance of "total_IOPS = 5000" to "IOPS= xxxx where the total_IOPS
            // is first divided equally across all represented LUNs on all hosts,
            // and then within a LUN, the IOPS is allocated according to the "skew" parameter value (default 1.0).
            // The skew parameter is treated as a weight.  Using weights adding to 1.0 or 100 work the way you expect.

            // total_IOPS number forms recognized:  1 .2 3. 4.4 1.0e3 1e+3 1.e-3 3E4, which is perhaps more flexible than with IOPS, which may only take fixed point numbers
            // returns true if it re-wrote, false if it didn't see "total_IOPS".

            auto rv = pWT->wT_IosequencerInput.setMultipleParameters(edited_text);
            if (!rv.first)
            {
                std::ostringstream o;
                o << "<Error> Failed setting parameters \"" << edited_text
                    << "\" into local WorkloadTracker object for WorkloadID = \"" << wID.workloadID << "\"" << std::endl << rv.second;
                return std::make_pair(false,o.str());
            }

            p_host->workloads_by_text[edited_text].workloadIDs.push_back(wID);

        }
    }

//    // print out pipe_driver_subthread's workloads_by_text
//    {
//        std::ostringstream o;
//        o << "debug: printing out workloads_by_text" << std::endl;
//        for (auto& pear : host_subthread_pointers)
//        {
//            const std::string& host = pear.first;
//            o << "debug:    host  \"" << host << "\"" << std::endl;
//
//            pipe_driver_subthread* p_host = pear.second;
//
//            bool need_comma = false;
//
//            o << "debug:      ";
//
//            for (auto& peach : p_host->workloads_by_text)
//            {
//                if (need_comma) o << "," << std::endl << "debug:      ";
//                need_comma = true;
//                o << "< \"" << peach.first << "\" { ";
//
//                bool need_comma2 = false;
//                for (auto& plum : peach.second.workloadIDs)
//                {
//                    if (need_comma2) o << ", ";
//                    need_comma2 = true;
//                    o << plum.workloadID;
//                }
//
//                o << " } >";
//            }
//        }
//        o << std::endl << "debug: end." << std::endl;
//        std::cout << o.str();
//        log(masterlogfile,o.str());
//    }

    // Originally, when there was only one command for each host, they were launched
    // in parallel.  Now with multiple versions of IOPS for total_IOPS + skew,
    // serializing all commands one host at a time, one command at a time.

    // If this takes too long, come back here and figure out how to launch in parallel.

    editWorkloadInterlockTimeSeconds.clear();
    ivytime before_edit_workload_interlock;
    ivytime after_edit_workoad_interlock;

    for (auto& pear : host_subthread_pointers)
    {
        pipe_driver_subthread* p_host = pear.second;

        for (auto& peach : p_host->workloads_by_text)
        {
            before_edit_workload_interlock.setToNow();
            {
                std::unique_lock<std::mutex> u_lk(p_host->master_slave_lk);

                // Already set earlier: ListOfWorkloadIDs p_host->commandListOfWorkloadIDs;
                const std::string& parameters = peach.first;
                p_host->commandIosequencerParameters = parameters;
                p_host->p_edit_workload_IDs = &peach.second;
                p_host->commandStart.setToNow();
                p_host->commandString=std::string("[EditWorkload]");
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
                    o << "ivymaster main thread notified by host driver subthread of failure to set parameters \""
                      << p_host->commandIosequencerParameters << "\" in WorkloadIDs " << p_host->p_edit_workload_IDs->toString() << std::endl
                      << p_host->commandErrorMessage;  // don't forget to clear this when we get there
                    return std::make_pair(false,o.str());
                }
            }

            after_edit_workoad_interlock.setToNow();

            ivytime edit_worklock_interlock = after_edit_workoad_interlock - before_edit_workload_interlock;

            editWorkloadInterlockTimeSeconds.push(edit_worklock_interlock.getlongdoubleseconds());
        }
    }

    ivytime editRollupExecutionTime = after_edit_workoad_interlock - beforeEditRollup;

    if (routine_logging)
    {
        std::ostringstream o;
        o << "edit rollup - overall execution time = " << std::fixed << std::setprecision(1) << (100.*editRollupExecutionTime.getlongdoubleseconds()) << " ms" << std::endl
            << "edit workload interlock time "
            << " - average " << std::fixed << std::setprecision(1) << (100.*editWorkloadInterlockTimeSeconds.avg()) << " ms"
            << " - min " << std::fixed << std::setprecision(1) << (100.*editWorkloadInterlockTimeSeconds.min()) << " ms"
            << " - max " << std::fixed << std::setprecision(1) << (100.*editWorkloadInterlockTimeSeconds.max()) << " ms"
            << std::endl << std::endl;
        log(masterlogfile,o.str());
        std::cout<< o.str();
    }

    return std::make_pair(true,"");

}

std::pair<bool /*success*/, std::string /* message */>
ivy_engine::delete_rollup(const std::string& attributeNameComboText)
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

        AttributeNameCombo anc {};

        std::pair<bool,std::string>
            rc = anc.set(attributeNameComboText, &m_s.TheSampleLUN);
            // This is to handle spaces around attribute tokens & attribute names in quotes.

        if ( !rc.first )
        {
            std::ostringstream o;
            o << "<Error> ivy engine API - delete rollup for \"" << attributeNameComboText << "\" failed - " << rc.second << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            return std::make_pair(false,o.str());
        }

        rc = m_s.rollups.deleteRollup(anc.attributeNameComboID);
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
ivy_engine::go(const std::string& parameters)
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

//*debug*/std::string debug_console_input; static bool have_already_waited {false}; if (!have_already_waited) { std::cout << "debug: pausing while you could attach a debugger - hit return to resume." << std::endl; std::getline(std::cin,debug_console_input); } have_already_waited = true;

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

    std::string parameter_value;

    std::string valid_parameter_names = "suppress_perf, skip_LDEV, stepname, subinterval_seconds, warmup_seconds, measure_seconds, cooldown_by_wp"s
        + ",cooldown_by_MP_busy, cooldown_by_MP_busy_stay_down_time_seconds, subsystem_WP_threshold, subsystem_busy_threshold, sequential_fill"s
        + ",stepcsv, storcsv, check_failed_component"s;

    std::string valid_parameters_message =
        "The following parameter names are always valid:\n"
        "    stepname, subinterval_seconds, warmup_seconds, measure_seconds, cooldown_by_wp,\n"
        "    suppress_perf, skip_LDEV, subsystem_WP_threshold, cooldown_by_MP_busy, cooldown_by_MP_busy_stay_down_time_seconds,\n"
        "    subsystem_busy_threshold, sequential_fill, stepcsv, storcsv, check_failed_component.\n\n"
        "For dfc = PID, additional valid parameters are:\n"
        "    target_value, low_IOPS, low_target, high_IOPS, high_target, max_IOPS, max_ripple, gain_step,\n"
        "    ballpark_seconds, max_monotone, balanced_step_direction_by.\n\n"
        "For measure = on, additional valid parameters are:\n"
        "    accuracy_plus_minus, confidence, max_wp, min_wp, max_wp_change, timeout_seconds\n\n."
        "For dfc=pid or measure=on, must have either source = workload, or source = RAID_subsystem.\n"
        "    These are usually set using \"shorthand\" such as measure=service_time_seconds or measure=PG_percent_busy.\n\n"
        "For source = workload, additional valid parameters are: category, accumulator_type, accessor.\n\n"
        "    These are usually set using \"shorthand\" such as measure=service_time_seconds or measure=PG_percent_busy.\n\n"
        "For source = RAID_subsystem, additional valid parameters are: subsystem_element, element_metric.\n\n"
        "    These are usually set using \"shorthand\" such as measure=service_time_seconds or measure=PG_percent_busy.\n\n"

R"("measure" may be set to "on" or "off", or to one of the following shorthand settings:

    measure = MB_per_second           is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=bytes_transferred, accessor=sum
    measure = IOPS                    is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=bytes_transferred, accessor=count
    measure = service_time_seconds    is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=service_time,      accessor=avg
    measure = response_time_seconds   is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=response_time,     accessor=avg
    measure = MP_core_busy_percent    is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=MP_core, element_metric=busy_percent
    measure = PG_busy_percent         is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=PG,      element_metric=busy_percent
    measure = CLPR_WP_percent         is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=CLPR,    element_metric=WP_percent

    Note: In the above shorthand settings, the "focus_rollup" is only set to "all" if the user did not specify "focus_rollup".
          That is, you can override focus_rollup even if you use one of the shorthand settings.
)"
    ;

    if (go_parameters.contains("stepname")) stepName = go_parameters.retrieve("stepname");
    else stepName = stepNNNN;

    if (!go_parameters.contains("suppress_perf"s))          go_parameters.contents[normalize_identifier(std::string("suppress_perf"))]            = suppress_subsystem_perf_default ? "on" : "off";
    if (!go_parameters.contains("skip_LDEV"s))              go_parameters.contents[normalize_identifier(std::string("skip_LDEV"))]                = skip_ldev_data_default          ? "on" : "off";
    if (!go_parameters.contains("stepcsv"s))                go_parameters.contents[normalize_identifier(std::string("stepcsv"))]                  = stepcsv_default                 ? "on" : "off";
    if (!go_parameters.contains("storcsv"s))                go_parameters.contents[normalize_identifier(std::string("storcsv"))]                  = storcsv_default                 ? "on" : "off";
    if (!go_parameters.contains("check_failed_component"s)) go_parameters.contents[normalize_identifier(std::string("check_failed_component"))]   = check_failed_component_default  ? "on" : "off";

    if (!go_parameters.contains(std::string("subinterval_seconds")))      go_parameters.contents[normalize_identifier(std::string("subinterval_seconds"))]      = subinterval_seconds_default;
    if (!go_parameters.contains(std::string("warmup_seconds")))           go_parameters.contents[normalize_identifier(std::string("warmup_seconds"))]           = warmup_seconds_default;
    if (!go_parameters.contains(std::string("measure_seconds")))          go_parameters.contents[normalize_identifier(std::string("measure_seconds"))]          = measure_seconds_default;
    if (!go_parameters.contains(std::string("cooldown_by_wp")))           go_parameters.contents[normalize_identifier(std::string("cooldown_by_wp"))]           = cooldown_by_wp_default;
    if (!go_parameters.contains(std::string("cooldown_by_MP_busy")))      go_parameters.contents[normalize_identifier(std::string("cooldown_by_MP_busy"))]      = cooldown_by_MP_busy_default;
    if (!go_parameters.contains(std::string("cooldown_by_MP_busy_stay_down_time_seconds")))      go_parameters.contents[normalize_identifier(std::string("cooldown_by_MP_busy_stay_down_time_seconds"))] = go_parameters.retrieve("subinterval_seconds");
    if (!go_parameters.contains(std::string("subsystem_WP_threshold")))   go_parameters.contents[normalize_identifier(std::string("subsystem_WP_threshold"))]   = subsystem_WP_threshold_default; // used with cooldown_by_wp
    if (!go_parameters.contains(std::string("subsystem_busy_threshold"))) go_parameters.contents[normalize_identifier(std::string("subsystem_busy_threshold"))] = subsystem_busy_threshold_default; // used with cooldown_by_MP_busy

    if (go_parameters.contains(std::string("dfc")))
    {

        std::string controllerName = go_parameters.retrieve("dfc");

        if (stringCaseInsensitiveEquality("pid", controllerName))
        {
            have_pid = true;
            valid_parameter_names += ", dfc, target_value, low_IOPS, low_target, high_IOPS, high_target, max_IOPS, max_ripple, gain_step, ballpark_seconds, max_monotone, balanced_step_direction_by";
            if (!go_parameters.contains(std::string("max_ripple")))                 go_parameters.contents[normalize_identifier(std::string("max_ripple"))]       = std::string("1%");
            if (!go_parameters.contains(std::string("gain_step")))                  go_parameters.contents[normalize_identifier(std::string("gain_step"))]        = std::string("2");
            if (!go_parameters.contains(std::string("ballpark_seconds")))           go_parameters.contents[normalize_identifier(std::string("ballpark_seconds"))] = std::string("60");
            if (!go_parameters.contains(std::string("balanced_step_direction_by"))) go_parameters.contents[normalize_identifier(std::string("balanced_step_direction_by"))] = std::string("12");
            if (!go_parameters.contains(std::string("max_monotone")))               go_parameters.contents[normalize_identifier(std::string("max_monotone"))]     = std::string("5");
        }
        else
        {
            std::ostringstream o;
            o << std::endl << "<Error> ivy engine API go() - \"dfc\", if specified, may only be set to \"pid\"." << std::endl << std::endl;
            std::cout << o.str();
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }

//        std::map<std::string, MeasureController*>::iterator controller_it = availableControllers.find(toLower(controllerName));
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
            if (!go_parameters.contains("focus_rollup")) go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "workload";
            go_parameters.contents[toLower("category")] = "overall";
            go_parameters.contents[normalize_identifier("accumulator_type")] = "bytes_transferred";
            go_parameters.contents[normalize_identifier("accessor")] = "sum";
        }
        else if (stringCaseInsensitiveEquality(measure_parameter_value,std::string("IOPS")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            if (!go_parameters.contains("focus_rollup")) go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[normalize_identifier("source")] = "workload";
            go_parameters.contents[toLower("category")] = "overall";
            go_parameters.contents[normalize_identifier("accumulator_type")] = "bytes_transferred";
            go_parameters.contents[toLower("accessor")] = "count";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("service_time_seconds")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            if (!go_parameters.contains("focus_rollup")) go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "workload";
            go_parameters.contents[toLower("category")] = "overall";
            go_parameters.contents[normalize_identifier("accumulator_type")] = "service_time";
            go_parameters.contents[toLower("accessor")] = "avg";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("response_time_seconds")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            if (!go_parameters.contains("focus_rollup")) go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "workload";
            go_parameters.contents[toLower("category")] = "overall";
            go_parameters.contents[normalize_identifier("accumulator_type")] = "response_time";
            go_parameters.contents[toLower("accessor")] = "avg";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("MP_core_busy_percent")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            if (!go_parameters.contains("focus_rollup")) go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "RAID_subsystem";
            go_parameters.contents[normalize_identifier("subsystem_element")] = "MP core";
            go_parameters.contents[normalize_identifier("element_metric")] = "busy %";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("PG_busy_percent")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            if (!go_parameters.contains("focus_rollup")) go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "RAID_subsystem";
            go_parameters.contents[normalize_identifier("subsystem_element")] = "PG";
            go_parameters.contents[normalize_identifier("element_metric")] = "busy %";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("CLPR_WP_percent")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            if (!go_parameters.contains("focus_rollup")) go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
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

    Note: In the above shorthand settings, the "focus_rollup" is only set to "all" if the user did not specify "focus_rollup".
          That is, you can override focus_rollup even if you use one of the shorthand settings.
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

    auto names_are_valid = go_parameters.containsOnlyValidParameterNames(valid_parameter_names);
    if (!names_are_valid.first)
    {
        std::ostringstream o;
        o << std::endl << "<Error> ivy engine API - invalid [Go] parameter(s) ==> " << names_are_valid.second << std::endl << std::endl
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



//----------------------------------- sequential_fill = on
    if (go_parameters.contains("sequential_fill"))
    {
        std::string w = go_parameters.retrieve("sequential_fill");

        if ( stringCaseInsensitiveEquality(w,std::string("on")) || stringCaseInsensitiveEquality(w,std::string("true")) )
        {
            sequential_fill = true;
        }
        else
        {
            std::ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid \"sequential_fill\" parameter value \""
            << w << "\".  \"sequential_fill\", if specified, may only be set to \"on\"."  << std::endl;
            std::cout << o.str();
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }
    }
    else
    {
        sequential_fill = false;
    }



//----------------------------------- subinterval_seconds
    parameter_value = go_parameters.retrieve("subinterval_seconds");
    try
    {
        subinterval_seconds = number_optional_trailing_percent(parameter_value);
    }
    catch(std::invalid_argument& iaex)
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - go() - Invalid subinterval_seconds parameter value \"" << parameter_value << "\"." << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }

    if (subinterval_seconds < min_subinterval_seconds || subinterval_seconds > max_subinterval_seconds )
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - go() - Invalid subinterval_seconds parameter value \"" << parameter_value << "\".  Permitted range is from " << min_subinterval_seconds << " to " << max_subinterval_seconds << "." << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }
    subintervalLength = ivytime(subinterval_seconds);
    if (subinterval_seconds < 3.)
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - go() - Invalid subinterval_seconds parameter value \"" << parameter_value << "\".  Permitted range is from " << min_subinterval_seconds << " to " << max_subinterval_seconds << "." << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }

//----------------------------------- warmup_seconds
    try
    {
        std::string ws = go_parameters.retrieve("warmup_seconds");
        trim(ws);
        warmup_seconds = number_optional_trailing_percent(rewrite_HHMMSS_to_seconds(ws));
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
        std::string ms = go_parameters.retrieve("measure_seconds");
        trim(ms);
        measure_seconds = number_optional_trailing_percent(rewrite_HHMMSS_to_seconds(ms));

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

//----------------------------------- suppress_perf
    parameter_value = go_parameters.retrieve("suppress_perf");
    if      ( stringCaseInsensitiveEquality(std::string("on"),    parameter_value)
           || stringCaseInsensitiveEquality(std::string("true"),  parameter_value)
           || stringCaseInsensitiveEquality(std::string("yes"),   parameter_value) ) suppress_subsystem_perf = true;
    else if ( stringCaseInsensitiveEquality(std::string("off"),   parameter_value)
           || stringCaseInsensitiveEquality(std::string("false"), parameter_value)
           || stringCaseInsensitiveEquality(std::string("no"),    parameter_value) ) suppress_subsystem_perf = false;
    else
    {
        ostringstream o;
        o << "<Error> ivy engine API - go() - [Go] statement - invalid suppress_perf parameter \"" << parameter_value << "\".  Must be \"on\" or \"off\"." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }

//----------------------------------- stepcsv
    parameter_value = go_parameters.retrieve("stepcsv");
    if      ( stringCaseInsensitiveEquality(std::string("on"),    parameter_value)
           || stringCaseInsensitiveEquality(std::string("true"),  parameter_value)
           || stringCaseInsensitiveEquality(std::string("yes"),   parameter_value) ) stepcsv = true;
    else if ( stringCaseInsensitiveEquality(std::string("off"),   parameter_value)
           || stringCaseInsensitiveEquality(std::string("false"), parameter_value)
           || stringCaseInsensitiveEquality(std::string("no"),    parameter_value) ) stepcsv = false;
    else
    {
        ostringstream o;
        o << "<Error> ivy engine API - go() - [Go] statement - invalid stepcsv parameter \"" << parameter_value << "\".  Must be \"on\" or \"off\"." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }

//----------------------------------- storcsv
    parameter_value = go_parameters.retrieve("storcsv");
    if      ( stringCaseInsensitiveEquality(std::string("on"),    parameter_value)
           || stringCaseInsensitiveEquality(std::string("true"),  parameter_value)
           || stringCaseInsensitiveEquality(std::string("yes"),   parameter_value) ) storcsv = true;
    else if ( stringCaseInsensitiveEquality(std::string("off"),   parameter_value)
           || stringCaseInsensitiveEquality(std::string("false"), parameter_value)
           || stringCaseInsensitiveEquality(std::string("no"),    parameter_value) ) storcsv = false;
    else
    {
        ostringstream o;
        o << "<Error> ivy engine API - go() - [Go] statement - invalid storcsv parameter \"" << parameter_value << "\".  Must be \"on\" or \"off\"." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }
//----------------------------------- check_failed_component
    parameter_value = go_parameters.retrieve("check_failed_component");
    if      ( stringCaseInsensitiveEquality(std::string("on"),    parameter_value)
           || stringCaseInsensitiveEquality(std::string("true"),  parameter_value)
           || stringCaseInsensitiveEquality(std::string("yes"),   parameter_value) ) check_failed_component = true;
    else if ( stringCaseInsensitiveEquality(std::string("off"),   parameter_value)
           || stringCaseInsensitiveEquality(std::string("false"), parameter_value)
           || stringCaseInsensitiveEquality(std::string("no"),    parameter_value) ) check_failed_component = false;
    else
    {
        ostringstream o;
        o << "<Error> ivy engine API - go() - [Go] statement - invalid check_failed_component parameter \"" << parameter_value << "\".  Must be \"on\" or \"off\"." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }


//----------------------------------- skip_LDEV
    parameter_value = go_parameters.retrieve("skip_LDEV");
    if      ( stringCaseInsensitiveEquality(std::string("on"),    parameter_value)
           || stringCaseInsensitiveEquality(std::string("true"),  parameter_value)
           || stringCaseInsensitiveEquality(std::string("yes"),   parameter_value) ) skip_ldev_data = true;
    else if ( stringCaseInsensitiveEquality(std::string("off"),   parameter_value)
           || stringCaseInsensitiveEquality(std::string("false"), parameter_value)
           || stringCaseInsensitiveEquality(std::string("no"),    parameter_value) ) skip_ldev_data = false;
    else
    {
        ostringstream o;
        o << "<Error> ivy engine API - go() - [Go] statement - invalid skip_LDEV parameter \"" << parameter_value << "\".  Must be \"on\" or \"off\"." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }


//----------------------------------- cooldown_by_wp
    parameter_value = go_parameters.retrieve("cooldown_by_wp");
    if      ( stringCaseInsensitiveEquality(std::string("on"),    parameter_value)
           || stringCaseInsensitiveEquality(std::string("true"),  parameter_value)
           || stringCaseInsensitiveEquality(std::string("yes"),   parameter_value) ) cooldown_by_wp = true;
    else if ( stringCaseInsensitiveEquality(std::string("off"),   parameter_value)
           || stringCaseInsensitiveEquality(std::string("false"), parameter_value)
           || stringCaseInsensitiveEquality(std::string("no"),    parameter_value) ) cooldown_by_wp = false;
    else
    {
        ostringstream o;
        o << "<Error> ivy engine API - go() - [Go] statement - invalid cooldown_by_wp parameter \"" << parameter_value << "\".  Must be \"on\" or \"off\"." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }


//----------------------------------- subsystem_WP_threshold  - goes with cooldown_by_wp
    std::string subsystem_WP_threshold_parameter = go_parameters.retrieve("subsystem_WP_threshold");
    {
        std::ostringstream where_the;
        where_the << "go_statement() - when setting \"subsystem_WP_threshold\" parameter to value \""
            << subsystem_WP_threshold_parameter << "\": ";

        subsystem_WP_threshold = number_optional_trailing_percent(subsystem_WP_threshold_parameter,where_the.str());
    }


//----------------------------------- cooldown_by_MP_busy
    parameter_value = go_parameters.retrieve("cooldown_by_MP_busy");
    if      ( stringCaseInsensitiveEquality(std::string("on"),    parameter_value)
           || stringCaseInsensitiveEquality(std::string("true"),  parameter_value)
           || stringCaseInsensitiveEquality(std::string("yes"),   parameter_value) ) cooldown_by_MP_busy = true;
    else if ( stringCaseInsensitiveEquality(std::string("off"),   parameter_value)
           || stringCaseInsensitiveEquality(std::string("false"), parameter_value)
           || stringCaseInsensitiveEquality(std::string("no"),    parameter_value) ) cooldown_by_MP_busy = false;
    else
    {
        ostringstream o;
        o << "<Error> ivy engine API - go() - [Go] statement - invalid cooldown_by_MP_busy parameter \"" << go_parameters.retrieve("cooldown_by_wp") << "\".  Must be \"on\" or \"off\"." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }

    //----------------------------------- cooldown_by_MP_busy_stay_down_time_seconds
    try
    {
        cooldown_by_MP_busy_stay_down_time_seconds = number_optional_trailing_percent(rewrite_HHMMSS_to_seconds(go_parameters.retrieve("cooldown_by_MP_busy_stay_down_time_seconds")));
    }
    catch(std::invalid_argument& iaex)
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - go() - Invalid cooldown_by_MP_busy_stay_down_time_seconds parameter value \"" << go_parameters.retrieve("cooldown_by_MP_busy_stay_down_time_seconds") << "\"." << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }

    cooldown_by_MP_busy_stay_down_count_limit = (unsigned int) std::ceil(cooldown_by_MP_busy_stay_down_time_seconds / subinterval_seconds);


//----------------------------------- subsystem_busy_threshold  - goes with cooldown_by_MP_busy
    std::string subsystem_busy_threshold_parameter = go_parameters.retrieve("subsystem_busy_threshold");
    {
        std::ostringstream where_the;
        where_the << "go_statement() - when setting \"subsystem_busy_threshold\" parameter to value \""
            << subsystem_busy_threshold_parameter << "\": ";

        subsystem_busy_threshold = number_optional_trailing_percent(subsystem_busy_threshold_parameter,where_the.str());
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

                        o << std::endl << "The definition of the these subsystems metrics which are rolled up for each rollup instance is in subsystem_summary_metrics in ivy_engine.h." << std::endl << std::endl;

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

                o << std::endl << "The definition of these subsystem element metrics which are rolled up for each rollup instance is in subsystem_summary_metrics in ivy_engine.h." << std::endl << std::endl;

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
        // valid_parameter_names += ", dfc, target_value, low_IOPS, low_target, high_IOPS, high_target, max_IOPS, max_ripple, gain_step, ballpark_seconds, max_monotone, balanced_step_direction_by";

        if ( (!go_parameters.contains("target_value"))
          || (!go_parameters.contains("low_IOPS"))
          || (!go_parameters.contains("low_target"))
          || (!go_parameters.contains("high_IOPS"))
          || (!go_parameters.contains("high_target"))
        )
        {
            ostringstream o;
            o << std::endl << "<Error> ivy engine API - go() - [Go] statement - when DFC = PID is used, the user must specify positive values for \n"
                              "the parameters target_value, low_IOPS, low_target, high_IOPS, and high_target.  Furthermore, high_IOPS must be\n"
                              "greater than low_IOPS and high_target must be greater than low_larget, and the target_value must be greater than\n"
                              "low_target and less than high_target." << std::endl << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }

//----------------------------------- target_value
        target_value_parameter = go_parameters.retrieve("target_value");
        {
            std::ostringstream where_the;
            where_the << "go_statement(), DFCcategory::PID - when setting \"target_value\" parameter to value \""
                << target_value_parameter << "\": ";

            target_value = number_optional_trailing_percent(target_value_parameter,where_the.str());
        }

//----------------------------------- low_IOPS
        low_IOPS_parameter = go_parameters.retrieve("low_IOPS");
        {
            std::ostringstream where_the;
            where_the << "go_statement(), DFCcategory::PID - when setting \"low_IOPS\" parameter to value \""
                << low_IOPS_parameter << "\": ";

            low_IOPS = number_optional_trailing_percent(low_IOPS_parameter,where_the.str());
        }

//----------------------------------- low_target
        low_target_parameter = go_parameters.retrieve("low_target");
        {
            std::ostringstream where_the;
            where_the << "go_statement(), DFCcategory::PID - when setting \"low_target\" parameter to value \""
                << low_target_parameter << "\": ";

            low_target = number_optional_trailing_percent(low_target_parameter,where_the.str());
        }
//----------------------------------- high_IOPS
        high_IOPS_parameter = go_parameters.retrieve("high_IOPS");
        {
            std::ostringstream where_the;
            where_the << "go_statement(), DFCcategory::PID - when setting \"high_IOPS\" parameter to value \""
                << high_IOPS_parameter << "\": ";

            high_IOPS = number_optional_trailing_percent(high_IOPS_parameter,where_the.str());
        }

//----------------------------------- high_target
        high_target_parameter = go_parameters.retrieve("high_target");
        {
            std::ostringstream where_the;
            where_the << "go_statement(), DFCcategory::PID - when setting \"high_target\" parameter to value \""
                << high_target_parameter << "\": ";

            high_target = number_optional_trailing_percent(high_target_parameter,where_the.str());
        }


        if ( low_IOPS     <= 0.0        || low_target   <= 0.0          || high_IOPS <= 0.0 || high_target <= 0.0
          || low_IOPS     >= high_IOPS  || low_target   >= high_target
          || target_value <= low_target || target_value >= high_target
        )
        {
            ostringstream o;
            o << std::endl << "<Error> ivy engine API - go() - [Go] statement - when DFC = PID is used, the user must specify positive values for \n"
                              "the parameters target_value, low_IOPS, low_target, high_IOPS, and high_target.  Furthermore, high_IOPS must be\n"
                              "greater than low_IOPS and high_target must be greater than low_larget, and the target_value must be greater than\n"
                              "low_target and less than high_target." << std::endl << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }


//----------------------------------- max_IOPS
        if (go_parameters.contains("max_IOPS"))
        {
            max_IOPS_parameter = go_parameters.retrieve("max_IOPS");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFCcategory::PID - when setting \"max_IOPS\" parameter to value \""
                    << max_IOPS_parameter << "\": ";

                max_IOPS = number_optional_trailing_percent(max_IOPS_parameter,where_the.str());
            }

            if (max_IOPS < high_IOPS)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid max_IOPS parameter \"" << max_IOPS_parameter << "\".  "
                    << "max_IOPS, where optionally specified, may not be below the specified high_IOPS value " << high_IOPS_parameter << "."
                    << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }
        else
        {
            max_IOPS = high_IOPS;
        }


//----------------------------------- gain_step
        {
            std::string parameter = go_parameters.retrieve("gain_step");

            std::ostringstream where_the; where_the << "go_statement(), DFC = PID - when setting \"gain_step\" parameter to value \"" << parameter << "\": ";

            m_s.gain_step = number_optional_trailing_percent(parameter,where_the.str());

            if (m_s.gain_step <= 0.0)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid gain_step parameter \"" << parameter << "\".  Must be greater than zero." << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }

//----------------------------------- max_ripple
        {
            std::string parameter = go_parameters.retrieve("max_ripple");

            std::ostringstream where_the; where_the << "go_statement(), DFC = PID - when setting \"max_ripple\" parameter to value \"" << parameter << "\": ";

            m_s.max_ripple = number_optional_trailing_percent(parameter,where_the.str());

            if (m_s.max_ripple <= 0.0)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid max_ripple parameter \"" << parameter << "\".  Must be greater than zero." << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }

//----------------------------------- max_monotone
        {
            std::string parameter = go_parameters.retrieve("max_monotone");

            std::ostringstream where_the; where_the << "go_statement(), DFC = PID - when setting \"max_monotone\" parameter to value \"" << parameter << "\": ";

            m_s.max_monotone = unsigned_int(parameter,where_the.str());

            if (m_s.max_monotone < 4)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid max_monotone parameter \"" << parameter << "\".  Must be greater than or equal to 4." << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }

//----------------------------------- balanced_step_direction_by
        {
            std::string parameter = go_parameters.retrieve("balanced_step_direction_by");

            std::ostringstream where_the; where_the << "go_statement(), DFC = PID - when setting \"balanced_step_direction_by\" parameter to value \"" << parameter << "\": ";

            m_s.balanced_step_direction_by = unsigned_int(parameter,where_the.str());

            if (m_s.balanced_step_direction_by < 6)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid balanced_step_direction_by parameter \"" << parameter << "\".  Must be greater than or equal to 6." << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }

//----------------------------------- ballpark_seconds
        {
            std::string parameter = go_parameters.retrieve("ballpark_seconds");

            std::ostringstream where_the; where_the << "go_statement(), DFC = PID - when setting \"ballpark_seconds\" parameter to value \"" << parameter << "\": ";

            m_s.ballpark_seconds = number_optional_trailing_percent(parameter,where_the.str());

            if (m_s.ballpark_seconds <= 0.0)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid ballpark_seconds parameter \"" << parameter << "\".  Must be greater than zero." << std::endl << std::endl;
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
            accuracy_plus_minus_fraction = number_optional_trailing_percent(accuracy_plus_minus_parameter,"accuracy_plus_minus");
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
            confidence = number_optional_trailing_percent(confidence_parameter,"confidence");
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
            std::string ts = go_parameters.retrieve("timeout_seconds");
            trim(ts);
            timeout_seconds = number_optional_trailing_percent(rewrite_HHMMSS_to_seconds(ts),"timeout_seconds");
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
            min_wp = number_optional_trailing_percent(min_wp_parameter,"min_wp");

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
            max_wp = number_optional_trailing_percent(go_parameters.retrieve("max_wp"), "max_wp");

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
            max_wp_change = number_optional_trailing_percent(max_wp_change_parameter,"max_wp_change");

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
        o << "********* " << stepNNNN << " duration " << flight_duration.format_as_duration_HMMSS()
            << " = warmup " << m_s.warmup_duration.format_as_duration_HMMSS()
            << " + measurement " << m_s.measurement_duration.format_as_duration_HMMSS()
            << " + cooldown " << m_s.cooldown_duration.format_as_duration_HMMSS()
            << " for step name \"" << stepName << "\"" << std::endl;

        step_times << o.str();
        log(masterlogfile, o.str());
        std::cout << o.str();
        rc.second = o.str();
    }
    rc.first=true;
    return rc;
}


std::pair<bool,std::string>
ivy_engine::shutdown_subthreads()
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
			o << "ivy_engine::shutdown_subthreads() - posting \"die\" command to subthread for host " << pear.first
                << " pid = " << pear.second->pipe_driver_pid << ", gettid() thread ID = " << pear.second->linux_gettid_thread_id<< std::endl;
			log (masterlogfile, o.str());
		}
		pear.second->master_slave_cv.notify_all();
		notified++;
	}

	int notified_subsystems {0};

	for (auto& pear : command_device_subthread_pointers)
	{
        {
            pear.second->commandString=std::string("die");
            pear.second->command=true;

            std::ostringstream o;
            o << "ivy_engine::shutdown_subthreads() - posting \"die\" command to subthread for subsystem " << pear.first
                << " pid = " << pear.second->pipe_driver_pid << ", gettid() thread ID = " << pear.second->linux_gettid_thread_id<< std::endl;
            log (masterlogfile, o.str());
        }
        pear.second->master_slave_cv.notify_all();
        notified_subsystems++;
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

	for (auto& pear : command_device_subthread_pointers)
    {
        bool died_on_its_own;
        died_on_its_own = false;

        std::chrono::milliseconds nap(100);

        for (unsigned int i = 0; i < 30; i++)
        {
            if (pear.second->dead)
            {
                died_on_its_own = true;
                break;
            }
            std::this_thread::sleep_for(nap);
        }

        if (!died_on_its_own)
        {
            std::ostringstream o;
            o << "********** Subthread for command device on host " << pear.second->ivyscript_hostname
                << " subsystem serial number " << pear.first
                << " did not exit normally." << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();

            need_harakiri = true;
        }
    }

	std::ostringstream o;
	o << "ivy_engine::shutdown_subthreads() told " << notified << " host driver and "
        << notified_subsystems << " command device subthreads to die." << std::endl;
	log(masterlogfile,o.str());

    bool something_bad_happened {false};

	for (auto& host : hosts)
	{
		{
			std::ostringstream o;
			o << "scp " << SLAVEUSERID << '@' << host << ":" << IVYDRIVERLOGFOLDERROOT IVYDRIVERLOGFOLDER << "/log.ivydriver." << host << "* " << testFolder << "/logs";
			if (0 == system(o.str().c_str()))
			{
				log(masterlogfile,std::string("success: ")+o.str()+std::string("\n"));
				std::ostringstream rm;
				rm << "ssh " << SLAVEUSERID << '@' << host << " rm -f " << IVYDRIVERLOGFOLDERROOT IVYDRIVERLOGFOLDER << "/log.ivydriver." << host << "*";
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

std::string valid_get_parameters()
{
    std::ostringstream o;
    o << "Valid ivy engine get parameters are:"
            << " output_folder_root"
            << ", test_name"
            << ", last_result"
            << ", step_NNNN"
            << ", step_name"
            << ", and step_folder."
                << std::endl
            << "(Parameter names are not case sensitive and underscores are ignored in parameter names, and thus OutputFolderRoot is equivalent to output_folder_root.)"
                << std::endl << std::endl;
    return o.str();
}

    std::pair<bool, std::string>  // <true,value> or <false,error message>
ivy_engine::get(const std::string& thingee)
{
    std::string t {};

    try
    {
        t = normalize_identifier(thingee);
    }
    catch (const std::invalid_argument& ia)
    {
        std::ostringstream o;
        o << "<Error> ivy engine get(thing=\"" << thingee << "\") - thing must be an identifier, that is, starts with an alphabetic character and continues with alphabetics, numerics, and underscores."
            << std::endl << std::endl
            << valid_get_parameters();
        return std::make_pair(false,o.str());
    }

    if (0 == t.compare(normalize_identifier("summary_csv")))
    {
        return std::make_pair(true,testFolder + "/all/"s + testName + ".all=all.summary.csv"s);
    }


    if (0 == t.compare(normalize_identifier("output_folder_root")))
    {
        return std::make_pair(true,outputFolderRoot);
    }


    if (0 == t.compare(normalize_identifier("test_name")))
    {
        return std::make_pair(true,testName);
    }


    if (0 == t.compare(normalize_identifier("last_result")))
    {
        return std::make_pair(true,last_result());
    }


    if (0 == t.compare(normalize_identifier("step_NNNN")))
    {
        return std::make_pair(true,stepNNNN);
    }


    if (0 == t.compare(normalize_identifier("step_name")))
    {
        return std::make_pair(true,stepName);
    }


    if (0 == t.compare(normalize_identifier("step_folder")))
    {
        return std::make_pair(true,stepFolder);
    }


    if (0 == t.compare(normalize_identifier("test_folder")))
    {
        return std::make_pair(true,testFolder);
    }



    if (0 == t.compare(normalize_identifier("masterlogfile")))
    {
        return std::make_pair(true,masterlogfile);
    }


    if (0 == t.compare(normalize_identifier("rollup_structure")))
    {
        return std::make_pair(true,show_rollup_structure());
    }


    if (0 == t.compare(normalize_identifier("thing")))
    {
        return std::make_pair(true,thing);
    }


    {
        std::ostringstream o;
        o << "<Error> Unknown ivy engine get parameter \"" << thingee << "\"." << std::endl << std::endl
            << valid_get_parameters();
        return std::make_pair(false,o.str());
    }
}

    std::pair<bool, std::string>  // <true,possibly in future some additional info if needed> or <false,error message>
ivy_engine::set(const std::string& thingee,
    const std::string& value)
{
    std::string t {};

    try
    {
        t = normalize_identifier(thingee);
    }
    catch (const std::invalid_argument& ia)
    {
        std::ostringstream o;
        o << "<Error> ivy engine set(thing=\"" << thingee << "\", value = \"" << value
            << "\") - thing must be an identifier, that is, starts with an alphabetic character and continues with alphabetics, numerics, and underscores."
            << std::endl << std::endl;
        return std::make_pair(false,o.str());
    }


    if (0 == t.compare(normalize_identifier("thing")))
    {
        thing = value;
        return std::make_pair(true,"");
    }


    {
        std::ostringstream o;
        o << "<Error> Unknown ivy engine set parameter \"" << thingee << "\"." << std::endl << std::endl;
        return std::make_pair(false,o.str());
    }
}

void ivy_engine::print_latency_csvfiles()
{
    std::string latency_subfolder = testFolder + std::string("/interlock_latencies");

    struct stat struct_stat;

	if ( 0 == stat(latency_subfolder.c_str(),&struct_stat))  // latencies folder already exists
    {
        if(!S_ISDIR(struct_stat.st_mode))
        {
            std::ostringstream o;
            o << "<Error> internal_programming_error.  " << latency_subfolder << " already exists but is not a directory." << std::endl
                << "  Occurred at line " << __LINE__ << "of " << __FILE__ << std::endl;
            std::cout << o.str();
            log(masterlogfile, o.str());
            kill_subthreads_and_exit();
        }
    } else {
        if (mkdir(latency_subfolder.c_str(),S_IRWXU | S_IRWXG | S_IRWXO))
        {
            std::ostringstream o;
            o << "<Error> internal_programming_error.  Failed trying to create " << latency_subfolder << "." << std::endl
                << "  Occurred at line " << __LINE__ << "of " << __FILE__ << std::endl;
            std::cout << o.str();
            log(masterlogfile, o.str());
            kill_subthreads_and_exit();
        }
    }

    m_s.dispatching_latency_seconds_accumulator.clear();
    m_s.lock_aquisition_latency_seconds_accumulator.clear();
    m_s.switchover_completion_latency_seconds_accumulator.clear();

    m_s.distribution_over_workloads_of_avg_dispatching_latency.clear();
    m_s.distribution_over_workloads_of_avg_lock_acquisition.clear();
    m_s.distribution_over_workloads_of_avg_switchover.clear();

    for (auto& pear : host_subthread_pointers)
    {
        m_s.dispatching_latency_seconds_accumulator           += pear.second->dispatching_latency_seconds_accumulator;
        m_s.lock_aquisition_latency_seconds_accumulator       += pear.second->lock_aquisition_latency_seconds_accumulator;
        m_s.switchover_completion_latency_seconds_accumulator += pear.second->switchover_completion_latency_seconds_accumulator;

        m_s.distribution_over_workloads_of_avg_dispatching_latency += pear.second->distribution_over_workloads_of_avg_dispatching_latency;
        m_s.distribution_over_workloads_of_avg_lock_acquisition    += pear.second->distribution_over_workloads_of_avg_lock_acquisition;
        m_s.distribution_over_workloads_of_avg_switchover          += pear.second->distribution_over_workloads_of_avg_switchover;
    }

    std::ostringstream h;

    h << "test name,stepNNNN,step name,hostname";

    for (const std::string& s : { "protocol time seconds",
                                  "host sendup seconds",
                                  "sendup time seconds",
                                  "central processing seconds",
                                  "earliest subsystem gather head start seconds",

                                  "continue/cooldown/stop ack seconds",
                                  "overall_gather_seconds",
                                  "cruise seconds",

                                  "dispatcher latency seconds",
                                  "lock aquisition latency seconds",
                                  "switchover completion latency seconds",

                                  "distribution over_workloads of avg dispatching latency",
                                  "distribution over_workloads of avg lock acquisition",
                                  "distribution over_workloads of avg switchover"            })
    {
        h << "," << s << " avg";
    }
        h << ",details:";


    for (const std::string& s : { "protocol time seconds",
                                  "host sendup seconds",
                                  "sendup time seconds",
                                  "central processing seconds",
                                  "earliest subsystem gather head start seconds",

                                  "continue/cooldown/stop ack seconds",
                                  "overall gather seconds",
                                  "cruise seconds",

                                  "dispatcher latency seconds",
                                  "lock aquisition latency seconds",
                                  "switchover completion latency seconds",

                                  "distribution over_workloads of avg dispatching latency",
                                  "distribution over_workloads of avg lock acquisition",
                                  "distribution over_workloads of avg switchover"            })
    {
        h << "," << s << " avg";
        h << "," << s << " min";
        h << "," << s << " max";
        h << "," << s << " count";
    }
    h << std::endl;

    std::string header_line = h.str();

    std::string overall_filename  = latency_subfolder + std::string("/") + testName + std::string(".latencies.overall.csv");

    if ( 0 != stat(overall_filename.c_str(),&struct_stat))
    {
        // doesn't exist yet.  write header line
        fileappend(overall_filename,header_line);
    }

    {
        std::ostringstream latency_line;
        latency_line << testName << ',' << stepNNNN << ',' << stepName << ",overall";

// first we print a set of columns with just the avg() for the sake of humans
        latency_line << ',' << m_s.protocolTimeSeconds                                   .avg();
        latency_line << ',' << m_s.hostSendupTimeSeconds                                 .avg();
        latency_line << ',' << m_s.sendupTimeSeconds                                     .avg();
        latency_line << ',' << m_s.centralProcessingTimeSeconds                          .avg();
        latency_line << ',' << m_s.gatherBeforeSubintervalEndSeconds                     .avg();

        latency_line << ',' << m_s.continueCooldownStopAckSeconds                        .avg();
        latency_line << ',' << m_s.overallGatherTimeSeconds                              .avg();
        latency_line << ',' << m_s.cruiseSeconds                                 .avg();

        latency_line << ',' << m_s.dispatching_latency_seconds_accumulator               .avg();
        latency_line << ',' << m_s.lock_aquisition_latency_seconds_accumulator           .avg();
        latency_line << ',' << m_s.switchover_completion_latency_seconds_accumulator     .avg();

        latency_line << ',' << m_s.distribution_over_workloads_of_avg_dispatching_latency.avg();
        latency_line << ',' << m_s.distribution_over_workloads_of_avg_lock_acquisition   .avg();
        latency_line << ',' << m_s.distribution_over_workloads_of_avg_switchover         .avg();

        latency_line << ",details:";
// The avg columsn repeat in the full set so that they can be used standalone

        latency_line << ',' << m_s.protocolTimeSeconds                                   .avg() << ',' << m_s.protocolTimeSeconds                                   .min() << ',' << m_s.protocolTimeSeconds                                   .max() << ',' << m_s.protocolTimeSeconds                                   .count();
        latency_line << ',' << m_s.hostSendupTimeSeconds                                 .avg() << ',' << m_s.hostSendupTimeSeconds                                 .min() << ',' << m_s.hostSendupTimeSeconds                                 .max() << ',' << m_s.hostSendupTimeSeconds                                 .count();
        latency_line << ',' << m_s.sendupTimeSeconds                                     .avg() << ',' << m_s.sendupTimeSeconds                                     .min() << ',' << m_s.sendupTimeSeconds                                     .max() << ',' << m_s.sendupTimeSeconds                                     .count();
        latency_line << ',' << m_s.centralProcessingTimeSeconds                          .avg() << ',' << m_s.centralProcessingTimeSeconds                          .min() << ',' << m_s.centralProcessingTimeSeconds                          .max() << ',' << m_s.centralProcessingTimeSeconds                          .count();
        latency_line << ',' << m_s.gatherBeforeSubintervalEndSeconds                     .avg() << ',' << m_s.gatherBeforeSubintervalEndSeconds                     .min() << ',' << m_s.gatherBeforeSubintervalEndSeconds                     .max() << ',' << m_s.gatherBeforeSubintervalEndSeconds                     .count();

        latency_line << ',' << m_s.continueCooldownStopAckSeconds                        .avg() << ',' << m_s.continueCooldownStopAckSeconds                        .min() << ',' << m_s.continueCooldownStopAckSeconds                        .max() << ',' << m_s.continueCooldownStopAckSeconds                        .count();
        latency_line << ',' << m_s.overallGatherTimeSeconds                              .avg() << ',' << m_s.overallGatherTimeSeconds                              .min() << ',' << m_s.overallGatherTimeSeconds                              .max() << ',' << m_s.overallGatherTimeSeconds                              .count();
        latency_line << ',' << m_s.cruiseSeconds                                         .avg() << ',' << m_s.cruiseSeconds                                         .min() << ',' << m_s.cruiseSeconds                                         .max() << ',' << m_s.cruiseSeconds                                         .count();

        latency_line << ',' << m_s.dispatching_latency_seconds_accumulator               .avg() << ',' << m_s.dispatching_latency_seconds_accumulator               .min() << ',' << m_s.dispatching_latency_seconds_accumulator               .max() << ',' << m_s.dispatching_latency_seconds_accumulator               .count();
        latency_line << ',' << m_s.lock_aquisition_latency_seconds_accumulator           .avg() << ',' << m_s.lock_aquisition_latency_seconds_accumulator           .min() << ',' << m_s.lock_aquisition_latency_seconds_accumulator           .max() << ',' << m_s.lock_aquisition_latency_seconds_accumulator           .count();
        latency_line << ',' << m_s.switchover_completion_latency_seconds_accumulator     .avg() << ',' << m_s.switchover_completion_latency_seconds_accumulator     .min() << ',' << m_s.switchover_completion_latency_seconds_accumulator     .max() << ',' << m_s.switchover_completion_latency_seconds_accumulator     .count();

        latency_line << ',' << m_s.distribution_over_workloads_of_avg_dispatching_latency.avg() << ',' << m_s.distribution_over_workloads_of_avg_dispatching_latency.min() << ',' << m_s.distribution_over_workloads_of_avg_dispatching_latency.max() << ',' << m_s.distribution_over_workloads_of_avg_dispatching_latency.count();
        latency_line << ',' << m_s.distribution_over_workloads_of_avg_lock_acquisition   .avg() << ',' << m_s.distribution_over_workloads_of_avg_lock_acquisition   .min() << ',' << m_s.distribution_over_workloads_of_avg_lock_acquisition   .max() << ',' << m_s.distribution_over_workloads_of_avg_lock_acquisition   .count();
        latency_line << ',' << m_s.distribution_over_workloads_of_avg_switchover         .avg() << ',' << m_s.distribution_over_workloads_of_avg_switchover         .min() << ',' << m_s.distribution_over_workloads_of_avg_switchover         .max() << ',' << m_s.distribution_over_workloads_of_avg_switchover         .count();

        latency_line << std::endl;

        fileappend(overall_filename,latency_line.str());
    }

    for (auto& pear : host_subthread_pointers)
    {
        std::string by_host_filename = latency_subfolder + std::string("/")
            + testName + std::string(".latencies.") + pear.first + std::string(".csv");

        if ( 0 != stat(by_host_filename.c_str(),&struct_stat))
        {
            // doesn't exist yet.  write header line
            fileappend(by_host_filename,header_line);
        }

        {
            std::ostringstream latency_line;
            latency_line << testName << ',' << stepNNNN << ',' << stepName << ',' << pear.first;

// first we print a set of columns with just the avg() for the sake of humans
            latency_line << ',' << m_s.protocolTimeSeconds                                   .avg();
            latency_line << ',' << m_s.hostSendupTimeSeconds                                 .avg();
            latency_line << ',' << m_s.sendupTimeSeconds                                     .avg();
            latency_line << ',' << m_s.centralProcessingTimeSeconds                          .avg();
            latency_line << ',' << m_s.gatherBeforeSubintervalEndSeconds                     .avg();

            latency_line << ',' << m_s.continueCooldownStopAckSeconds                        .avg();
            latency_line << ',' << m_s.overallGatherTimeSeconds                              .avg();
            latency_line << ',' << m_s.cruiseSeconds                                 .avg();

            latency_line << ',' << pear.second->dispatching_latency_seconds_accumulator               .avg();
            latency_line << ',' << pear.second->lock_aquisition_latency_seconds_accumulator           .avg();
            latency_line << ',' << pear.second->switchover_completion_latency_seconds_accumulator     .avg();

            latency_line << ',' << pear.second->distribution_over_workloads_of_avg_dispatching_latency.avg();
            latency_line << ',' << pear.second->distribution_over_workloads_of_avg_lock_acquisition   .avg();
            latency_line << ',' << pear.second->distribution_over_workloads_of_avg_switchover         .avg();

            latency_line << ",details:";


// The avg columsn repeat in the full set so that they can be used standalone

            latency_line << ',' << m_s.protocolTimeSeconds                                   .avg() << ',' << m_s.protocolTimeSeconds                                   .min() << ',' << m_s.protocolTimeSeconds                                   .max() << ',' << m_s.protocolTimeSeconds                                   .count();
            latency_line << ',' << m_s.hostSendupTimeSeconds                                 .avg() << ',' << m_s.hostSendupTimeSeconds                                 .min() << ',' << m_s.hostSendupTimeSeconds                                 .max() << ',' << m_s.hostSendupTimeSeconds                                 .count();
            latency_line << ',' << m_s.sendupTimeSeconds                                     .avg() << ',' << m_s.sendupTimeSeconds                                     .min() << ',' << m_s.sendupTimeSeconds                                     .max() << ',' << m_s.sendupTimeSeconds                                     .count();
            latency_line << ',' << m_s.centralProcessingTimeSeconds                          .avg() << ',' << m_s.centralProcessingTimeSeconds                          .min() << ',' << m_s.centralProcessingTimeSeconds                          .max() << ',' << m_s.centralProcessingTimeSeconds                          .count();
            latency_line << ',' << m_s.gatherBeforeSubintervalEndSeconds                     .avg() << ',' << m_s.gatherBeforeSubintervalEndSeconds                     .min() << ',' << m_s.gatherBeforeSubintervalEndSeconds                     .max() << ',' << m_s.gatherBeforeSubintervalEndSeconds                     .count();

            latency_line << ',' << m_s.continueCooldownStopAckSeconds                        .avg() << ',' << m_s.continueCooldownStopAckSeconds                        .min() << ',' << m_s.continueCooldownStopAckSeconds                        .max() << ',' << m_s.continueCooldownStopAckSeconds                        .count();
            latency_line << ',' << m_s.overallGatherTimeSeconds                              .avg() << ',' << m_s.overallGatherTimeSeconds                              .min() << ',' << m_s.overallGatherTimeSeconds                              .max() << ',' << m_s.overallGatherTimeSeconds                              .count();
            latency_line << ',' << m_s.cruiseSeconds                                         .avg() << ',' << m_s.cruiseSeconds                                         .min() << ',' << m_s.cruiseSeconds                                         .max() << ',' << m_s.cruiseSeconds                                         .count();

            latency_line << ',' << pear.second->dispatching_latency_seconds_accumulator               .avg() << ',' << pear.second->dispatching_latency_seconds_accumulator               .min() << ',' << pear.second->dispatching_latency_seconds_accumulator               .max() << ',' << pear.second->dispatching_latency_seconds_accumulator               .count();
            latency_line << ',' << pear.second->lock_aquisition_latency_seconds_accumulator           .avg() << ',' << pear.second->lock_aquisition_latency_seconds_accumulator           .min() << ',' << pear.second->lock_aquisition_latency_seconds_accumulator           .max() << ',' << pear.second->lock_aquisition_latency_seconds_accumulator           .count();
            latency_line << ',' << pear.second->switchover_completion_latency_seconds_accumulator     .avg() << ',' << pear.second->switchover_completion_latency_seconds_accumulator     .min() << ',' << pear.second->switchover_completion_latency_seconds_accumulator     .max() << ',' << pear.second->switchover_completion_latency_seconds_accumulator     .count();

            latency_line << ',' << pear.second->distribution_over_workloads_of_avg_dispatching_latency.avg() << ',' << pear.second->distribution_over_workloads_of_avg_dispatching_latency.min() << ',' << pear.second->distribution_over_workloads_of_avg_dispatching_latency.max() << ',' << pear.second->distribution_over_workloads_of_avg_dispatching_latency.count();
            latency_line << ',' << pear.second->distribution_over_workloads_of_avg_lock_acquisition   .avg() << ',' << pear.second->distribution_over_workloads_of_avg_lock_acquisition   .min() << ',' << pear.second->distribution_over_workloads_of_avg_lock_acquisition   .max() << ',' << pear.second->distribution_over_workloads_of_avg_lock_acquisition   .count();
            latency_line << ',' << pear.second->distribution_over_workloads_of_avg_switchover         .avg() << ',' << pear.second->distribution_over_workloads_of_avg_switchover         .min() << ',' << pear.second->distribution_over_workloads_of_avg_switchover         .max() << ',' << pear.second->distribution_over_workloads_of_avg_switchover         .count();
            latency_line << std::endl;

            fileappend(by_host_filename,latency_line.str());
        }
    }


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


    // print subsystem gather latency csv files

    std::ostringstream gather_latency_header;

    gather_latency_header << "test name,stepNNNN,step name,serial_number";
    gather_latency_header << ",get config seconds";

    for (const std::string& s : { "gather seconds",
                                  "get CLPR seconds",
                                  "get MP seconds",
                                  "get LDEV seconds",
                                  "get PORT seconds",
                                  "get UR Jnl seconds"            })
    {
        gather_latency_header << "," << s << " avg";
        gather_latency_header << "," << s << " min";
        gather_latency_header << "," << s << " max";
        gather_latency_header << "," << s << " count";
    }
    gather_latency_header << std::endl;

    for (auto& pear : command_device_subthread_pointers)
    {
        { // nested block to get fresh references

            const std::string& cereal_number = pear.first;

            std::string subsystem_latency_filename = latency_subfolder + std::string("/") + testName + std::string(".latencies.") + cereal_number + std::string(".csv");

            if ( 0 != stat(subsystem_latency_filename.c_str(),&struct_stat))
            {
                // doesn't exist yet.  write header line
                fileappend(subsystem_latency_filename,gather_latency_header.str());
            }

            {
                std::ostringstream latency_line;
                latency_line << testName << ',' << stepNNNN << ',' << stepName << ',' << cereal_number;

                latency_line << ',' << pear.second->getConfigTime         .avg();

                latency_line << ',' << pear.second->perSubsystemGatherTime.avg() << ',' << pear.second->perSubsystemGatherTime.min() << ',' << pear.second->perSubsystemGatherTime.max() << ',' << pear.second->perSubsystemGatherTime.count();
                latency_line << ',' << pear.second->getCLPRDetailTime     .avg() << ',' << pear.second->getCLPRDetailTime     .min() << ',' << pear.second->getCLPRDetailTime     .max() << ',' << pear.second->getCLPRDetailTime     .count();
                latency_line << ',' << pear.second->getMPbusyTime         .avg() << ',' << pear.second->getMPbusyTime         .min() << ',' << pear.second->getMPbusyTime         .max() << ',' << pear.second->getMPbusyTime         .count();
                latency_line << ',' << pear.second->getLDEVIOTime         .avg() << ',' << pear.second->getLDEVIOTime         .min() << ',' << pear.second->getLDEVIOTime         .max() << ',' << pear.second->getLDEVIOTime         .count();
                latency_line << ',' << pear.second->getPORTIOTime         .avg() << ',' << pear.second->getPORTIOTime         .min() << ',' << pear.second->getPORTIOTime         .max() << ',' << pear.second->getPORTIOTime         .count();
                latency_line << ',' << pear.second->getUR_JnlTime         .avg() << ',' << pear.second->getUR_JnlTime         .min() << ',' << pear.second->getUR_JnlTime         .max() << ',' << pear.second->getUR_JnlTime         .count();
                latency_line << std::endl;

                fileappend(subsystem_latency_filename,latency_line.str());
            }
        }
    }
}

void ivy_engine::print_iosequencer_template_deprecated_msg()
{
    std::string m { R"(

*********************************************************************************************
*                                                                                           *
*   NOTE: I/O sequencer templates were used.  (In ivyscript "[SetIosequencerTemplate]".)    *
*                                                                                           *
*         The use of I/O sequencer templates is deprecated meaning that it still works,     *
*         but users should switch instead to using "edit rollup" to set parameters across   *
*         a group of workloads after they have been created.                                *
*                                                                                           *
*         At some point in the future, I/O sequencer templates will be removed from ivy.    *
*                                                                                           *
*********************************************************************************************

)"};

    std::cout << m;
    log (masterlogfile,m);

    return;
}

















