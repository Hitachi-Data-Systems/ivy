//Copyright (c) 2016, 2017, 2018, 2019 Hitachi Vantara Corporation
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
#include <sys/stat.h>

#include "ivy_engine.h"

extern bool routine_logging;

ivy_engine m_s;

std::string outputFolderRoot() {return m_s.outputFolderRoot;}
std::string masterlogfile()    {return m_s.masterlogfile.logfilename;}
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
        ivy_float wp {0.0};
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
                o << "With cooldown_by_wp = on/last, a CLPR Write Pending % is " << std::fixed << std::setprecision(2) << (100.0 * wp) << "%"
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

//    std::string my_IP;
//
//    std::map<std::string,std::string> other_ivydriver_hosts; // <IP address, ivyscript hostname>
//
//    const unsigned int max_hostnamesize = 100;
//    char my_hostname[max_hostnamesize];
//    my_hostname[max_hostnamesize-1] = 0;
//
//    const std::string method = "ivy_engine::write_clear_script() - ";

    const std::string scriptname = "clear_hung_ivy_threads.sh";
    const std::string executable = "clear_hung_ivy_threads";

    std::ofstream oaf(scriptname,std::ofstream::out);
    if ( (!oaf.is_open()) || oaf.fail() )
    {
        std::ostringstream o;
        o << "<Error> ivy_engine::write_clear_script() - Unable to open output file \"" << scriptname << "\"." << std::endl;
        o << "Failed trying to build " << scriptname << std::endl;
        std::cout << o.str();
        log(masterlogfile, o.str());
        oaf.close();
        std::remove(scriptname.c_str());
        return;
    }

//    oaf << "#!/bin/bash" << std::endl;
//
//    int rc = gethostname(my_hostname,sizeof(my_hostname)-1);
//    if (0 != rc)
//    {
//        std::ostringstream o;
//        o << method << "Unable to get my own hostname - gethostname() return code = " << rc << " - " << std::endl;
//        o << "Failed trying to build " << scriptname << std::endl;
//        std::cout << o.str();
//        log(masterlogfile, o.str());
//        oaf.close();
//        std::remove(scriptname.c_str());
//        return;
//    }
//
//    hostent* p_hostent = gethostbyname(my_hostname);
//    if (p_hostent == NULL)
//    {
//        std::ostringstream o;
//        o << method << "Unable to get my own IP address - gethostbyname(\"" << my_hostname << "\" failed." << std::endl;
//        o << "Failed trying to build " << scriptname << std::endl;
//        std::cout << o.str();
//        log(masterlogfile, o.str());
//        oaf.close();
//        std::remove(scriptname.c_str());
//        return;
//    }
//
//    oaf << std::endl << "# my canonical name is " << p_hostent->h_name << std::endl;
//
//    if (p_hostent->h_addrtype != AF_INET)
//    {
//        std::ostringstream o;
//        o << method << "My own address is not AF_INET, not an internet IP address." << std::endl;
//        o << "Failed trying to build " << scriptname << std::endl;
//        std::cout << o.str();
//        log(masterlogfile, o.str());
//        oaf.close();
//        std::remove(scriptname.c_str());
//        return;
//    }
//
//    in_addr * p_in_addr = (in_addr * )p_hostent->h_addr;
//    {
//        std::ostringstream o;
//        o << inet_ntoa( *p_in_addr);
//        my_IP = o.str();
//    }
//
//    oaf << "# my IP address is " << my_IP << std::endl;
//
//    oaf << std::endl
//        << "echo" << std::endl
//        << "echo -----------------------------" << std::endl
//        << "echo" << std::endl
//        << "echo "
//            << executable << std::endl
//        << "echo" << std::endl
//            << executable << std::endl;
//
//    for (auto& slavehost : hosts)
//    {
//        p_hostent = gethostbyname(slavehost.c_str());
//        if (p_hostent == NULL)
//        {
//            std::ostringstream o;
//            o << method << "Unable to get IP address for ivydriver host (\"" << slavehost << "\"." << std::endl;
//            o << "Failed trying to build " << scriptname << std::endl;
//            std::cout << o.str();
//            log(masterlogfile, o.str());
//            oaf.close();
//            std::remove(scriptname.c_str());
//            return;
//        }
//
//        if (p_hostent->h_addrtype != AF_INET)
//        {
//            std::ostringstream o;
//            o << method << "For ivydriver host \"" << slavehost << "\", address is not AF_INET, not an internet IP address." << std::endl;
//            o << "Failed trying to build " << scriptname << std::endl;
//            std::cout << o.str();
//            log(masterlogfile, o.str());
//            oaf.close();
//            std::remove(scriptname.c_str());
//            return;
//        }
//
//        in_addr * p_in_addr = (in_addr * )p_hostent->h_addr;
//        {
//            std::ostringstream o;
//            o << inet_ntoa( *p_in_addr);
//            std::string ivydriver_IP = o.str();
//            auto it = other_ivydriver_hosts.find(ivydriver_IP);
//            if ( (0 != my_IP.compare(ivydriver_IP)) && (it == other_ivydriver_hosts.end()) )
//            {
//                other_ivydriver_hosts[ivydriver_IP] = slavehost;
//                oaf << std::endl
//                    << "echo" << std::endl
//                    << "echo -----------------------------" << std::endl
//                    << "echo" << std::endl
//                    << "echo "
//                        << "ssh " << slavehost  << " " << executable << std::endl
//                    << "echo" << std::endl
//                        << "ssh " << slavehost  << " " << executable << std::endl;
//            }
//            else
//            {
//                oaf << "# duplicate ivydriver host " << slavehost << " ( " << ivydriver_IP << " )" << std::endl;
//            }
//        }
//    }

    for (const std::string& h : unique_ivyscript_hosts)
    {
        std::string cmd = "ssh "s + h + " "s + executable;

        oaf
            << "echo -----------------------------" << std::endl
            << "echo " << cmd << std::endl
            <<            cmd << std::endl;
    }

    oaf << "echo -----------------------------" << std::endl;

    oaf.close();

    {
        std::ostringstream o;
        o << "chmod +x " << scriptname;
        system (o.str().c_str());
    }

    return;
}



std::string ivy_engine::getCLPRthumbnail(int subinterval_index) // use subinterval_index = -1 to get the t=0 gather thumbnail
// throws std::invalid_argument.    Shows WP for each CLPR on the watch list as briefly as possible
{
	std::ostringstream result;

	//result << "Available test CLPRs:";

	for (auto& pear : cooldown_WP_watch_set) // std::set<std::pair<std::string /*CLPR*/, Subsystem*>>
	{
		const std::string& CLPR {pear.first};
		const std::string& cereal {pear.second->serial_number};
		Hitachi_RAID_subsystem* p_RAID = (Hitachi_RAID_subsystem*) pear.second;

		ivy_float WP, sidefile;

		try
		{
			WP = p_RAID->get_wp(CLPR, subinterval_index);
		}
		catch (std::invalid_argument& iae)
		{
			std::ostringstream o;
			o << "ivy_engine::getCLPRthumbnail(int subinterval_index="
				<< subinterval_index << ") - WP = Subsystem.get_wp(CLPR=\"" << CLPR << "\", subinterval_index=" << subinterval_index
				<< ") failed saying - " << iae.what();
			log(masterlogfile,o.str());
			kill_subthreads_and_exit();
			throw std::invalid_argument(o.str());
		}

		try
		{
			sidefile = p_RAID->get_sidefile(CLPR, subinterval_index);
		}
		catch (std::invalid_argument& iae)
		{
			std::ostringstream o;
			o << "ivy_engine::getCLPRthumbnail(int subinterval_index="
				<< subinterval_index << ") - WP = Subsystem.get_sidefile(CLPR=\"" << CLPR << "\", subinterval_index=" << subinterval_index
				<< ") failed saying - " << iae.what();
			log(masterlogfile,o.str());
			kill_subthreads_and_exit();
			throw std::invalid_argument(o.str());
		}

		result << "<" << cereal << " " << CLPR << ": WP = " << std::fixed << std::setprecision(2) << (100.*WP) << '%';

		if (subinterval_index >-1)
		{
			ivy_float delta, slew_rate;
			delta = p_RAID->get_wp_change_from_last_subinterval( CLPR,subinterval_index );
			slew_rate = delta/subinterval_seconds;
			result << " ";
			if (slew_rate >= 0.0) result << '+';
			result << std::fixed << std::setprecision(2) << (100.*slew_rate) << "%/sec";
		}

		result << "; sidefile = " << std::fixed << std::setprecision(2) << (100.*sidefile) << '%';

		if (subinterval_index >-1)
		{
			ivy_float delta, slew_rate;
			delta = p_RAID->get_sidefile_change_from_last_subinterval( CLPR,subinterval_index );
			slew_rate = delta/subinterval_seconds;
			result << " ";
			if (slew_rate >= 0.0) result << '+';
			result << std::fixed << std::setprecision(2) << (100.*slew_rate) << "%/sec";
		}

		result << "> ";
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
        && element_metric == "Busy %")
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
        && element_metric == "WP %")
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
        o << ", warmup_seconds=" << seconds_to_hhmmss((unsigned int) warmup_seconds);
        o << ", measure_seconds=" << seconds_to_hhmmss((unsigned int) measure_seconds);
        o << ", timeout_seconds=" << seconds_to_hhmmss((unsigned int) timeout_seconds);

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

    if (lastEvaluateSubintervalReturnCode == EVALUATE_SUBINTERVAL_FAILURE)
    {
        if (have_IOPS_staircase && (!have_staircase_ending_IOPS) && measurements.size() > 1) return "success";

        return "failure";
    }
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
        log(ivy_engine_calls_filename,o.str());
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


std::pair<bool,std::string>
ivy_engine::shutdown_subthreads()
{
    {
        std::string m = "ivy engine API shutdown_subthreads()\n";
        log(masterlogfile,m);
        log(ivy_engine_calls_filename,m);
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

    if (m_s.copy_back_ivy_logs_sh_filename.size() > 0)
    {
        int rc = system(copy_back_ivy_logs_sh_filename.c_str());
        if (WIFEXITED(rc) && (0 == WEXITSTATUS(rc)))
        {
            std::string cmd = "rm -f "s + m_s.copy_back_ivy_logs_sh_filename;
            system(cmd.c_str());
        }
        else
        {
            something_bad_happened = true;
        }
    }
    else
    {
        for (auto& host : hosts)
        {
            {
                std::ostringstream o;
                o << "scp -p " << SLAVEUSERID << '@' << host << ":" << "/var/ivydriver_logs/log.ivydriver." << host << ".* " << testFolder << "/logs";
                log(masterlogfile,o.str());
                int rc = system(o.str().c_str());
                if (WIFEXITED(rc) && (0 == WEXITSTATUS(rc)))
                {
                    log(masterlogfile,std::string("success: ")+o.str()+std::string("\n"));
                    std::ostringstream rm;
                    rm << "ssh " << SLAVEUSERID << '@' << host << " rm -f " << "/var/ivydriver_logs/log.ivydriver." << host << ".*";
                    log(masterlogfile,rm.str());
                    int rc = system(rm.str().c_str());
                    if (WIFEXITED(rc) && (0 == WEXITSTATUS(rc)))
                    {
                        log(masterlogfile,std::string("success: ")+rm.str()+std::string("\n"));
                    }
                    else
                    {
                        log(masterlogfile,std::string("failure: ")+rm.str()+std::string("\n"));
                        something_bad_happened = true;
                    }
                }
                else
                {
                    something_bad_happened = true;
                    log(masterlogfile,std::string("failure: ")+o.str()+std::string("\n"));
                }
            }
        }

        std::string s = print_logfile_stats();  // separated out from next statement so no issues with mutex locks.
        log(masterlogfile,s);

        {
            std::ostringstream o;
            o << "cp -p " << m_s.var_ivymaster_logs_testName << "/* " << m_s.testFolder << "/logs";
            int rc = system(o.str().c_str());
            if (!WIFEXITED(rc) || (0 != WEXITSTATUS(rc)))
            {
                std::cout << "error copying ivy master host logs to the output folder using the command \"" << o.str() << "\"." << std::endl;
            }
            else
            {
                std::string cmd = "rm -rf " + var_ivymaster_logs_testName;

                system(cmd.c_str());
            }
        }

        std::string logtailcmd = "logtail "s + testFolder + "/logs"s;

        system(logtailcmd.c_str());
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

                                  "continue/stop ack seconds",
                                  "overall_gather_seconds",
                                  "cruise seconds",

                                  "dispatcher latency seconds",
                                  "lock aquisition latency seconds",
                                  "switchover completion latency seconds",

                                  "distribution over_workloads of avg dispatching latency",
                                  "distribution over_workloads of avg lock acquisition",
                                  "distribution over_workloads of avg switchover",

                                  "make measurement rollup seconds"                })
    {
        h << "," << s << " avg";
    }
        h << ",details:";


    for (const std::string& s : { "protocol time seconds",
                                  "host sendup seconds",
                                  "sendup time seconds",
                                  "central processing seconds",
                                  "earliest subsystem gather head start seconds",

                                  "continue/stop ack seconds",
                                  "overall gather seconds",
                                  "cruise seconds",

                                  "dispatcher latency seconds",
                                  "lock aquisition latency seconds",
                                  "switchover completion latency seconds",

                                  "distribution over_workloads of avg dispatching latency",
                                  "distribution over_workloads of avg lock acquisition",
                                  "distribution over_workloads of avg switchover",

                                  "make measurement rollup seconds"                })
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

        latency_line << ',' << m_s.makeMeasurementRollup_seconds                         .avg();

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

        latency_line << ',' << m_s.makeMeasurementRollup_seconds                         .avg() << ',' << m_s.makeMeasurementRollup_seconds                         .min() << ',' << m_s.makeMeasurementRollup_seconds                         .max() << ',' << m_s.makeMeasurementRollup_seconds                         .count();

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
                                  "get UR Jnl seconds",
                                  "get MP busy detail seconds" })
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
                latency_line << ',' << pear.second->getMP_busy_detail_Time.avg() << ',' << pear.second->getMP_busy_detail_Time.min() << ',' << pear.second->getMP_busy_detail_Time.max() << ',' << pear.second->getMP_busy_detail_Time.count();
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


void ivy_engine::clear_measurements()
{
    measurements.clear();

    rollups.not_participating_measurement.clear();

    for (auto& pear : rollups.rollups)
    {
        for (auto& peach : pear.second->instances)
        {
            peach.second->things_by_measurement.clear();
        }
    }

    return;
}

#include "multi_measure.h"

bool ivy_engine::start_new_measurement()
// returns false if we have completed all mesurements and it's time to print the csv files.
{
    m_s.in_cooldown_mode = false;

    if (measurements.size() == 0)
    {
        measurements.emplace_back();

        multi_measure_initialize_first();

        current_measurement().first_subinterval = 0;
        current_measurement().warmup_start = m_s.subintervalStart;
    }
    else
    {
        if (!have_IOPS_staircase) return false;

        if (!multi_measure_proceed_to_next()) return false;

        measurements.emplace_back();

        multi_measure_edit_rollup_total_IOPS();

        current_measurement().first_subinterval = 1 + measurements[measurements.size()-2].last_subinterval;
        current_measurement().warmup_start      =     measurements[measurements.size()-2].cooldown_end;
    }

    return true;
}

std::string ivy_engine::print_logfile_stats()
{
    RunningStat<long double, long> overall_durations {};

    std::ostringstream o;

    o << "log file time to write a log entry:" << std::endl;

    {
        std::unique_lock<std::mutex> m_lk(masterlogfile.now_speaks);

        const auto& rs = masterlogfile.durations;

        if (rs.count() > 0)
        {
            o << "count " << rs.count()
                << ", avg " << (1000.0 * rs.avg()) << " ms"
                << ", min " << (1000.0 * rs.min()) << " ms"
                << ", max " << (1000.0 * rs.max()) << " ms"
                << " - " << masterlogfile.logfilename << std::endl;

            overall_durations += masterlogfile.durations;
        }
    }

    {
        std::unique_lock<std::mutex> m_lk(ivy_engine_calls_filename.now_speaks);

        const auto& rs = ivy_engine_calls_filename.durations;

        if (rs.count() > 0)
        {
            o << "count " << rs.count()
                << ", avg " << (1000.0 * rs.avg()) << " ms"
                << ", min " << (1000.0 * rs.min()) << " ms"
                << ", max " << (1000.0 * rs.max()) << " ms"
                << " - " << ivy_engine_calls_filename.logfilename << std::endl;

            overall_durations += ivy_engine_calls_filename.durations;
        }
    }

    for (auto& pear : host_subthread_pointers)
    {
        {
            logger& bunyan = pear.second->logfilename;

            std::unique_lock<std::mutex> m_lk(bunyan.now_speaks);

            const auto& rs = bunyan.durations;

            if (rs.count() > 0)
            {
                o << "count " << rs.count()
                << ", avg " << (1000.0 * rs.avg()) << " ms"
                << ", min " << (1000.0 * rs.min()) << " ms"
                << ", max " << (1000.0 * rs.max()) << " ms"
                << " - " << bunyan.logfilename << std::endl;

                overall_durations += bunyan.durations;
            }
        }
    }

    for (auto& pear : command_device_subthread_pointers)
    {
        {
            logger& bunyan = pear.second->logfilename;

            std::unique_lock<std::mutex> m_lk(bunyan.now_speaks);

            const auto& rs = bunyan.durations;

            if (rs.count() > 0)
            {
                o << "count " << rs.count()
                << ", avg " << (1000.0 * rs.avg()) << " ms"
                << ", min " << (1000.0 * rs.min()) << " ms"
                << ", max " << (1000.0 * rs.max()) << " ms"
                << " - " << bunyan.logfilename << std::endl;

                overall_durations += bunyan.durations;
            }
        }
    }

    {
        const auto& rs = overall_durations;

        o
            << "count " << rs.count()
            << ", avg " << (1000.0 * rs.avg()) << " ms"
            << ", min " << (1000.0 * rs.min()) << " ms"
            << ", max " << (1000.0 * rs.max()) << " ms"
            << " - " << "< Overall >" << std::endl;
    }
    o << std::endl;

    return o.str();
}


void ivy_log(std::string s) {log(m_s.masterlogfile,s);} // crutch for Builtin.cpp


void ivy_engine::write_copy_back_ivy_logs_dot_sh()
{
    std::ofstream o;

    o.open(copy_back_ivy_logs_sh_filename);

    if (o.fail())
    {
        std::ostringstream e; e << "<Error> Failed trying to open \"" << copy_back_ivy_logs_sh_filename << "\" for output to write script." << std::endl;
        throw std::runtime_error(e.str());
    }

    o << "#!/bin/bash" << std::endl << std::endl;

    o << "encountered_error=0" << std::endl << std::endl;

    std::string cmd, cmd2;

	for (auto& host : hosts)
	{
		{ std::ostringstream c; c << "scp -p " << SLAVEUSERID << '@' << host << ":" << "/var/ivydriver_logs/log.ivydriver." << host << ".* " << testFolder << "/logs"; cmd = c.str(); }

        o << "if " << cmd << ";" << std::endl;

        o << "then" << std::endl;

        o << "\t" << "echo " << "Success: " << cmd << std::endl << std::endl;

        { std::ostringstream c; c << "ssh " << SLAVEUSERID << '@' << host << " rm -f " << "/var/ivydriver_logs/log.ivydriver." << host << ".*"; cmd2 = c.str(); }

        o << "\t" << "if " << cmd2 << ";" << std::endl;

        o << "\t" << "then" << std::endl;

        o << "\t\t" << "echo " << "Success: " << cmd2 << std::endl;

        o << "\t" << "else" << std::endl;

        o << "\t\t" << "echo " << "Failure: " << cmd2 << std::endl;

        o << "\t\t" << "encountered_error=1" << std::endl;

        o << "\t" << "fi" << std::endl;

        o << "else" << std::endl;

        o << "\t" << "echo " << "Failure: " << cmd << std::endl;

        o << "\t" << "encountered_error=1" << std::endl;

        o << "fi" << std::endl << std::endl;
    }

    { std::ostringstream c; c << "cp -p " << m_s.var_ivymaster_logs_testName << "/* " << m_s.testFolder << "/logs"; cmd = c.str(); }

    o << "if " << cmd << ";" << std::endl;

    o << "then" << std::endl;

    o << "\t" << "echo " << "Success: " << cmd << std::endl << std::endl;

    cmd2 = "rm -rf "s + var_ivymaster_logs_testName;

    o << "\t" << "if " << cmd2 << ";" << std::endl;

    o << "\t" << "then" << std::endl;

    o << "\t\t" << "echo " << "Success: " << cmd2 << std::endl;

    o << "\t" << "else" << std::endl;

    o << "\t\t" << "echo " << "Failure: " << cmd2 << std::endl;

    o << "\t\t" << "encountered_error=1" << std::endl;

    o << "\t" << "fi" << std::endl;

    o << "else" << std::endl;

    o << "\t" << "echo " << "Failure: " << cmd << std::endl;

    o << "\t" << "encountered_error=1" << std::endl;

    o << "fi" << std::endl;

    o << std::endl;

    o << "if [ 0 -eq $encountered_error ] ;" << std::endl;

    o << "then" << std::endl;

    o << "\t" << "logtail " << testFolder << "/logs" << std::endl;

    o << "fi" << std::endl;

    o << std::endl << "exit $encountered_error" << std::endl;

    o.close();

    chmod(copy_back_ivy_logs_sh_filename.c_str(),S_IRWXU | S_IRWXG | S_IRWXO);

    return;
}









