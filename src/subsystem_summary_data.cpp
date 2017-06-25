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

#include "subsystem_summary_data.h"

void subsystem_summary_data::addIn(const subsystem_summary_data& other)
{
    for (auto& other_pear : other.data)
    {
        const std::string& element = other_pear.first;

        for (auto& metric_pair : other_pear.second)
        {
            const std::string& metric = metric_pair.first;
            const RunningStat<ivy_float,ivy_int>& other_rs = metric_pair.second;

            data[element][metric] += other_rs;
        }
    }

    repetition_factor += other.repetition_factor;
}

/* static */  std::string subsystem_summary_data::csvHeadersPartOne(std::string np)
{
    std::ostringstream headers;

    for (auto& e_pear : m_s.subsystem_summary_metrics)
    {
        std::string& element = e_pear.first;

        for (const std::pair<std::string,unsigned char>& metric_pair : e_pear.second)
        {
            const std::string metric = metric_pair.first;

            if (metric_pair.second & print_count_part_1       ) headers << ",subsystem " << np << element << " count";
            if (metric_pair.second & print_avg_part_1         ) headers << ",subsystem " << np << "avg " << element << ' ' << metric;
            if (metric_pair.second & print_min_max_stddev_1   )
            {
                headers << ",subsystem " << np << "min " << element << ' ' << metric;
                headers << ",subsystem " << np << "max " << element << ' ' << metric;
                headers << ",subsystem " << np << "std dev " << element << ' ' << metric;
            }
        }
    }

    return headers.str();
}

/* static */  std::string subsystem_summary_data::csvHeadersPartTwo(std::string np)
{
    std::ostringstream headers;

    for (auto& e_pear : m_s.subsystem_summary_metrics)
    {
        std::string& element = e_pear.first;

        for (const std::pair<std::string,unsigned char>& metric_pair : e_pear.second)
        {
            const std::string metric = metric_pair.first;

            if (metric_pair.second & print_count_part_2       ) headers << ",subsystem " << np << element << " count";
            if (metric_pair.second & print_avg_part_2         ) headers << ",subsystem " << np << "avg " << element << ' ' << metric;
            if (metric_pair.second & print_min_max_stddev_2   )
            {
                headers << ",subsystem " << np << "min " << element << ' ' << metric;
                headers << ",subsystem " << np << "max " << element << ' ' << metric;
                headers << ",subsystem " << np << "std dev " << element << ' ' << metric;
            }
        }
    }

    return headers.str();

}

std::string subsystem_summary_data::csvValuesPartOne(unsigned int divide_count_by)
{
    std::ostringstream values;

    for (auto& e_pear : m_s.subsystem_summary_metrics)
    {
        std::string& element = e_pear.first;

        for (const std::pair<std::string,unsigned char>& metric_pair : e_pear.second)
        {
            const std::string& metric = metric_pair.first;
            unsigned char flags       = metric_pair.second;

            auto element_pear_it = data.find(element);

            if ( element_pear_it == data.end() )
            {
                if (flags & print_count_part_1)
                    values << ",<element " << element << " absent>";

                if (flags & print_avg_part_1)
                    values << ",<element " << element << " absent>";

                if (flags & print_min_max_stddev_1)
                {
                    values
                        << ",<element " << element << " absent>"
                        << ",<element " << element << " absent>"
                        << ",<element " << element << " absent>"
                        ;
                }
            }
            else
            { // we did find the element in the subsystem data
                auto metric_pear_it = element_pear_it->second.find(metric);

                if (metric_pear_it == element_pear_it->second.end())
                {
                    if (flags & print_count_part_1)
                        values << ",<metric " << metric << " absent>";

                    if (flags & print_avg_part_1)
                        values << ",<metric " << metric << " absent>";

                    if (flags & print_min_max_stddev_1)
                    {
                        values
                            << ",<metric " << metric << " absent>"
                            << ",<metric " << metric << " absent>"
                            << ",<metric " << metric << " absent>"
                            ;
                    }
                }
                else
                { // we found the element and the metric

                    RunningStat<ivy_float,ivy_int>& rs = metric_pear_it->second;

                    if (flags & print_count_part_1)
                        values << ',' << ( ((ivy_float) rs.count()) / ((ivy_float) divide_count_by) );

                    if (flags & print_avg_part_1)
                    {
                        if (rs.count() > 0)
                        {
                            if (endsWith(metric,"percent") || endsWith(metric,"%"))
                            {
                                values << ',' << (100. * rs.avg()) << '%';
                            }
                            else
                            {
                                values << ',' << rs.avg();
                            }
                        }
                    }

                    if (flags & print_min_max_stddev_1)
                    {
                        if (rs.count() > 0)
                        {
                            if (endsWith(metric,"percent") || endsWith(metric,"%"))
                            {
                                values
                                    << ',' << (100. * rs.min()) << '%'
                                    << ',' << (100. * rs.max()) << '%'
                                    << ',' << (100. * rs.standardDeviation()) << '%';
                            }
                            else
                            {
                                values
                                    << ',' << rs.min()
                                    << ',' << rs.max()
                                    << ',' << rs.standardDeviation();
                            }
                        }
                        else values << ",,,";
                    }
                }
            }
        } // end of loop for subsystem_summary_metrics metric
    } // end of loop for subsystem_summary_metrics element

    return values.str();
}

std::string subsystem_summary_data::csvValuesPartTwo(unsigned int divide_count_by)
{
    std::ostringstream values;

    for (auto& e_pear : m_s.subsystem_summary_metrics)
    {
        std::string& element = e_pear.first;

        for (const std::pair<std::string,unsigned char>& metric_pair : e_pear.second)
        {
            const std::string& metric = metric_pair.first;
            unsigned char flags       = metric_pair.second;

            auto element_pear_it = data.find(element);

            if ( element_pear_it == data.end() )
            {
                if (flags & print_count_part_2)
                    values << ",<element " << element << " absent>";

                if (flags & print_avg_part_2)
                    values << ",<element " << element << " absent>";

                if (flags & print_min_max_stddev_2)
                {
                    values
                        << ",<element " << element << " absent>"
                        << ",<element " << element << " absent>"
                        << ",<element " << element << " absent>"
                        ;
                }
            }
            else
            { // we did find the element in the subsystem data
                auto metric_pear_it = element_pear_it->second.find(metric);

                if (metric_pear_it == element_pear_it->second.end())
                {
                    if (flags & print_count_part_2)
                        values << ",<metric " << metric << " absent>";

                    if (flags & print_avg_part_2)
                        values << ",<metric " << metric << " absent>";

                    if (flags & print_min_max_stddev_2)
                    {
                        values
                            << ",<metric " << metric << " absent>"
                            << ",<metric " << metric << " absent>"
                            << ",<metric " << metric << " absent>"
                            ;
                    }
                }
                else
                { // we found the element and the metric

                    RunningStat<ivy_float,ivy_int>& rs = metric_pear_it->second;

                    if (flags & print_count_part_2)
                        values << ',' << ( ((ivy_float) rs.count()) / ((ivy_float) divide_count_by) );

                    if (flags & print_avg_part_2)
                    {
                        if (rs.count() > 0)
                        {
                            if (endsWith(metric,"percent") || endsWith(metric,"%"))
                            {
                                values << ',' << (100. * rs.avg()) << '%';
                            }
                            else
                            {
                                values << ',' << rs.avg();
                            }
                        }
                    }

                    if (flags & print_min_max_stddev_2)
                    {
                        if (rs.count() > 0)
                        {
                            if (endsWith(metric,"percent") || endsWith(metric,"%"))
                            {
                                values
                                    << ',' << (100. * rs.min()) << '%'
                                    << ',' << (100. * rs.max()) << '%'
                                    << ',' << (100. * rs.standardDeviation()) << '%';
                            }
                            else
                            {
                                values
                                    << ',' << rs.min()
                                    << ',' << rs.max()
                                    << ',' << rs.standardDeviation();
                            }
                        }
                        else values << ",,,";
                    }
                }
            }
        } // end of loop for subsystem_summary_metrics metric
    } // end of loop for subsystem_summary_metrics element


    return values.str();
}

std::string subsystem_summary_data::thumbnail() const // shows on the command line
{
    std::ostringstream o;
    bool need_comma = false;

    o << "Subsystem: ";

    //This bit commented out as there's a separate thumbnail for CLPR data
//    auto clpr_it = data.find("CLPR");
//    if (clpr_it != data.end())
//    {
//        auto busy_it = clpr_it->second.find("WP_percent");
//        if (busy_it != clpr_it->second.end())
//        {
//            const RunningStat<ivy_float,ivy_int>& rs = busy_it->second;
//            if (need_comma) o << ", ";
//            o << rs.count() << " CLPR"; if (rs.count()>1) o << "s"; o << " at " << std::fixed << std::setprecision(2) << (100.*rs.avg()) << " % WP";
//            need_comma = true;
//        }
//    }
//    else o << "< no CLPR data >";


    auto pg_it = data.find("PG");
    if (pg_it != data.end())
    {
        auto busy_it = pg_it->second.find("busy_percent");
        if (busy_it != pg_it->second.end())
        {
            const RunningStat<ivy_float,ivy_int>& rs = busy_it->second;
            if (need_comma) o << ", ";
            o << rs.count() << " PG"; if (rs.count()>1) o << "s"; o << " average " << std::fixed << std::setprecision(2) << (100.*rs.avg()) << " % busy";
            need_comma = true;
        }
    }
    else o << "< no PG data >";

    auto mp_it = data.find("MP_core");
    if (mp_it != data.end())
    {
        auto busy_it = mp_it->second.find("busy_percent");
        if (busy_it != mp_it->second.end())
        {
            const RunningStat<ivy_float,ivy_int>& rs = busy_it->second;
            if (need_comma) o << ", ";
            o << rs.count() << " MP cores average " << std::fixed << std::setprecision(2) << (100.*rs.avg()) << " % busy";
            need_comma = true;
        }
    }
    else o << " < no MP_core data >";

    auto ldev_it = data.find("LDEV");
    if (ldev_it != data.end())
    {
        // we take the count field from "random_read_IOPS", because this comes from higher up the food chain, rather than "IOPS" which is derived in part from random_read_IOPS.

        std::string count_source_metric {"random_read_IOPS"};

        auto ait = ldev_it->second.find(count_source_metric);
        if (ait == ldev_it->second.end())
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << count_source_metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        const RunningStat<ivy_float,ivy_int>& rsa = ait->second;
        ivy_float LDEV_count = ( (ivy_float) rsa.count() )     /    repetition_factor;


        std::string metric  = "service_time_ms";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float service_time_ms =  ait->second.avg();

        metric = "IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float IOPS =  ait->second.sum() / repetition_factor;

        metric  = "decimal_MB_per_second";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float decimal_MB_per_second =  ait->second.sum() / repetition_factor;

        metric  = "read_service_time_ms";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float read_service_time_ms =  ait->second.avg();

        metric  = "read_IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float read_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "read_decimal_MB_per_second";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float read_decimal_MB_per_second =  ait->second.sum() / repetition_factor;

        metric  = "write_service_time_ms";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float write_service_time_ms =  ait->second.avg();

        metric  = "write_IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float write_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "write_decimal_MB_per_second";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float write_decimal_MB_per_second =  ait->second.sum() / repetition_factor;

        metric  = "random_blocksize_KiB";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_blocksize_KiB =  ait->second.avg();


        metric  = "random_IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "random_decimal_MB_per_second";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_decimal_MB_per_second =  ait->second.sum() / repetition_factor;


        metric  = "sequential_blocksize_KiB";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_blocksize_KiB =  ait->second.avg();


        metric  = "sequential_IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "sequential_decimal_MB_per_second";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_decimal_MB_per_second =  ait->second.sum() / repetition_factor;


//-------------------------------------------------------------------------------
        metric  = "random_read_blocksize_KiB";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_read_blocksize_KiB =  ait->second.avg();

        metric  = "random_read_IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_read_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "random_read_decimal_MB_per_second";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_read_decimal_MB_per_second =  ait->second.sum() / repetition_factor;


        metric  = "random_write_blocksize_KiB";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_write_blocksize_KiB =  ait->second.avg();


        metric  = "random_write_IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_write_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "random_write_decimal_MB_per_second";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_write_decimal_MB_per_second =  ait->second.sum() / repetition_factor;

//-------------------------------------------------------------------------------
        metric  = "sequential_read_blocksize_KiB";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_read_blocksize_KiB =  ait->second.avg();


        metric  = "sequential_read_IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_read_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "sequential_read_decimal_MB_per_second";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_read_decimal_MB_per_second =  ait->second.sum() / repetition_factor;


        metric  = "sequential_write_blocksize_KiB";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_write_blocksize_KiB =  ait->second.avg();


        metric  = "sequential_write_IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_write_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "sequential_write_decimal_MB_per_second";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_write_decimal_MB_per_second =  ait->second.sum() / repetition_factor;


        if (detailed_thumbnail)
        {   // not detailed_thumbnail
            o << ", " << LDEV_count << " LDEVs, totals over LDEVs:" << std::endl;

            if (IOPS>0.0) o << std::fixed << std::setw(10) << std::setprecision(3) << service_time_ms;
            else          o << "     -.---";
            o << " ms service time,";
            o << std::fixed << std::setw(12) << std::setprecision(2) << IOPS << " IOPS,";
            o << std::fixed << std::setw(10) << std::setprecision(2) << decimal_MB_per_second << " decimal MB/s.";
            o << std::endl << std::endl; // -----------------------------------------
            o << "subsystem read:             ";
            if (read_IOPS>0.0) o << std::fixed << std::setw(10) << std::setprecision(3) << read_service_time_ms;
            else               o <<"     -.---";
            o << " ms service time,";
            o << std::fixed << std::setw(12) << std::setprecision(2) << read_IOPS << " IOPS,";
            o << std::fixed << std::setw(10) << std::setprecision(2) << read_decimal_MB_per_second << " decimal MB/s.";
            o << std::endl; // -----------------------------------------
            o << "subsystem write:            ";
            if (write_IOPS>0.0) o << std::fixed << std::setw(10) << std::setprecision(3) << write_service_time_ms;
            else                o << "     -.---";
            o << " ms service time,";
            o << std::fixed << std::setw(12) << std::setprecision(2) << write_IOPS << " IOPS,";
            o << std::fixed << std::setw(10) << std::setprecision(2) << write_decimal_MB_per_second << " decimal MB/s.";
            o << std::endl << std::endl; // -----------------------------------------
            o << "subsystem random:           ";
            if (random_IOPS > 0.0 ) o << std::fixed << std::setw(10) << std::setprecision(3) << random_blocksize_KiB;
            else                    o << "     -.---";
            o << " KiB blocksize,  ";
            o << std::fixed << std::setw(12) << std::setprecision(2) << random_IOPS << " IOPS,";
            o << std::fixed << std::setw(10) << std::setprecision(2) << random_decimal_MB_per_second << " decimal MB/s.";
            o << std::endl;
            o << "subsystem sequential:       ";
            if (sequential_IOPS > 0.0) o << std::fixed << std::setw(10) << std::setprecision(3) << sequential_blocksize_KiB;
            else                       o << "     -.---";
            o << " KiB blocksize,  ";
            o << std::fixed << std::setw(12) << std::setprecision(2) << sequential_IOPS << " IOPS,";
            o << std::fixed << std::setw(10) << std::setprecision(2) << sequential_decimal_MB_per_second << " decimal MB/s.";
            o << std::endl << std::endl; // -----------------------------------------
            o << "subsystem random read:      ";
            if (random_read_IOPS >0.0) o << std::fixed << std::setw(10) << std::setprecision(3) << random_read_blocksize_KiB;
            else                       o << "     -.---";
            o << " KiB blocksize,  ";
            o << std::fixed << std::setw(12) << std::setprecision(2) << random_read_IOPS << " IOPS,";
            o << std::fixed << std::setw(10) << std::setprecision(2) << random_read_decimal_MB_per_second << " decimal MB/s.";
            o << std::endl; // -----------------------------------------
            o << "subsystem random write:     ";
            if (random_write_IOPS > 0.0) o << std::fixed << std::setw(10) << std::setprecision(3) << random_write_blocksize_KiB;
            else                         o << "     -.---";
            o << " KiB blocksize,  ";
            o << std::fixed << std::setw(12) << std::setprecision(2) << random_write_IOPS << " IOPS,";
            o << std::fixed << std::setw(10) << std::setprecision(2) << random_write_decimal_MB_per_second << " decimal MB/s.";
            o << std::endl;
            o << std::endl; // -----------------------------------------
            o << "subsystem sequential read:  ";
            if (sequential_read_IOPS > 0.0) o << std::fixed << std::setw(10) << std::setprecision(3) << sequential_read_blocksize_KiB;
            else                            o << "     -.---";
             o << " KiB blocksize,  ";
            o << std::fixed << std::setw(12) << std::setprecision(2) << sequential_read_IOPS << " IOPS,";
            o << std::fixed << std::setw(10) << std::setprecision(2) << sequential_read_decimal_MB_per_second << " decimal MB/s.";
            o << std::endl; // -----------------------------------------
            o << "subsystem sequential write: ";
            if (sequential_write_IOPS > 0.0) o << std::fixed << std::setw(10) << std::setprecision(3) << sequential_write_blocksize_KiB;
            else                             o << "     -.---";
            o << " KiB blocksize,  ";
            o << std::fixed << std::setw(12) << std::setprecision(2) << sequential_write_IOPS << " IOPS,";
            o << std::fixed << std::setw(10) << std::setprecision(2) << sequential_write_decimal_MB_per_second << " decimal MB/s.";
            o << std::endl; // -----------------------------------------
        }
        else
        {
            o << ", " << LDEV_count << " LDEVs: ";

            if (IOPS <= 0.0)
            {
                o << "IOPS = 0.0";
            }
            else
            {
                o << "IOPS = " << std::fixed << std::setprecision(2) << IOPS;
                o << "; service time = " << std::fixed << std::setprecision(3) << service_time_ms << " ms";
                o << "; transfer rate = " << std::fixed << std::setprecision(2) << decimal_MB_per_second << " decimal MB/s.";

                if (random_IOPS > 0.0 )
                {
                    o << "; random blocksize = " << std::fixed << std::setprecision(3) << random_blocksize_KiB << " KiB";
                }
                if (sequential_IOPS > 0.0 )
                {
                    o << "; sequential blocksize = " << std::fixed << std::setprecision(3) << sequential_blocksize_KiB << " KiB";
                }
            }
        }
    }
    else o << "< no LDEV data >";

    o << std::endl;

//    {
//        o << std::endl << std::endl << "vomit entire contents of subsystem_summary_data count/average:" << std::endl;
//
//        for (auto& pear : data)
//        {
//            o << pear.first << " { ";
//            bool need_inner_comma {false};
//            for (auto& peach : pear.second)
//            {
//                if (need_inner_comma) o << ", ";
//                need_inner_comma = true;
//                o << std::endl <<  peach.first << " count = " << peach.second.count() << " / avg = " << peach.second.avg() << " / sum = " << peach.second.sum();
//            }
//            o << std::endl << " }" << std::endl << std::endl;
//        }
//    }

    return o.str();
}

ivy_float subsystem_summary_data::IOPS()
{
    auto ldev_it = data.find("LDEV");
    if (ldev_it == data.end()) return -1.0;

    auto ait = ldev_it->second.find("IOPS");
    if ( ait == ldev_it->second.end() ) return -1.0;

    return ait->second.sum() / repetition_factor;
}

ivy_float subsystem_summary_data::decimal_MB_per_second()
{
    auto ldev_it = data.find("LDEV");
    if (ldev_it == data.end()) return -1.0;

    auto ait = ldev_it->second.find("decimal_MB_per_second");
    if ( ait == ldev_it->second.end() ) return -1.0;

    return ait->second.sum() / repetition_factor;
}

ivy_float subsystem_summary_data::service_time_ms()
{
    auto ldev_it = data.find("LDEV");
    if (ldev_it == data.end()) return -1.0;

    auto ait = ldev_it->second.find("service_time_ms");
    if ( ait == ldev_it->second.end() ) return -1.0;

    return ait->second.avg();
}

