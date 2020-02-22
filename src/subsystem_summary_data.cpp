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

#include "ivytypes.h"
#include "ivyhelpers.h"
#include "ivy_engine.h"

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
            if (metric_pair.second & print_avg_part_1         )
            {
                headers << ",subsystem " << np << "avg " << element << ' ' << metric;

                if (element == "MP core" && metric == "Busy %")
                {
                    headers << ",Subsystem MP core busy microseconds per I/O";
                }
            }

            if (metric_pair.second & print_min_max_stddev_1   )
            {
                headers << ",subsystem " << np << "Min " << element << ' ' << metric;
                headers << ",subsystem " << np << "Max " << element << ' ' << metric;
                headers << ",subsystem " << np << "Std Dev " << element << ' ' << metric;
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
                    values << ",";

                if (flags & print_avg_part_1)
                {
                    values << ",";

                    if (element == "MP core" && metric == "Busy %")
                    {
                        values << ",";
                    }
                }

                if (flags & print_min_max_stddev_1)
                {
                    values
                        << ","
                        << ","
                        << ","
                        ;
                }
            }
            else
            { // we did find the element in the subsystem data
                auto metric_pear_it = element_pear_it->second.find(metric);

                if (metric_pear_it == element_pear_it->second.end())
                {
                    if (flags & print_count_part_1)
                        values << ",";

                    if (flags & print_avg_part_1)
                    {
                        values << ",";

                        if (element == "MP core" && metric == "Busy %")
                        {
                            values << ",";
                        }
                    }

                    if (flags & print_min_max_stddev_1)
                    {
                        values
                            << ","
                            << ","
                            << ","
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
                        else
                        {
                            values << ',' << rs.avg();
                        }

                        if (element == "MP core" && metric == "Busy %")
                        {
                            ivy_float microseconds = MP_microseconds_per_IO();

                            if (microseconds > 0) { values << "," << microseconds; }
                            else                  { values << ","; }
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
                    values << ",";

                if (flags & print_avg_part_2)
                    values << ",";

                if (flags & print_min_max_stddev_2)
                {
                    values
                        << ","
                        << ","
                        << ","
                        ;
                }
            }
            else
            { // we did find the element in the subsystem data
                auto metric_pear_it = element_pear_it->second.find(metric);

                if (metric_pear_it == element_pear_it->second.end())
                {
                    if (flags & print_count_part_2)
                        values << ",";

                    if (flags & print_avg_part_2)
                        values << ",";

                    if (flags & print_min_max_stddev_2)
                    {
                        values
                            << ","
                            << ","
                            << ","
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

    o << "subsystem data: ";

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
//    else o << " < no CLPR data >";


    auto pg_it = data.find("PG");
    if (pg_it != data.end())
    {
        auto busy_it = pg_it->second.find("busy %");
        if (busy_it != pg_it->second.end())
        {
            const RunningStat<ivy_float,ivy_int>& rs = busy_it->second;
            if (need_comma) o << ", ";
            o << rs.count() << " PG"; if (rs.count()>1) o << "s"; o << " " << std::fixed << std::setprecision(2) << (100.*rs.avg()) << " % busy";
            need_comma = true;
        }
    }
    else o << "< no PG data >";

    auto mp_it = data.find("MP core");
    if (mp_it != data.end())
    {
        auto busy_it = mp_it->second.find("Busy %");
        if (busy_it != mp_it->second.end())
        {
            const RunningStat<ivy_float,ivy_int>& rs = busy_it->second;
            if (need_comma) o << ", ";
            o << rs.count() << " MPs " << std::fixed << std::setprecision(2) << (100.*rs.avg()) << " % busy";
            need_comma = true;
        }
    }
    else o << " < no MP core data >";

    auto ldev_it = data.find("LDEV");
    if (ldev_it != data.end())
    {
        // we take the count field from "random read IOPS", because this comes from higher up the food chain, rather than "IOPS" which is derived in part from random_read_IOPS.

        std::string count_source_metric {"random read IOPS"};

        auto ait = ldev_it->second.find(count_source_metric);
        if (ait == ldev_it->second.end())
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << count_source_metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        const RunningStat<ivy_float,ivy_int>& rsa = ait->second;
        ivy_float LDEV_count = ( (ivy_float) rsa.count() )     /    repetition_factor;


        std::string metric  = "service time ms";
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

        metric  = "decimal MB/sec";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float decimal_MB_per_second =  ait->second.sum() / repetition_factor;

        metric  = "read service time ms";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float read_service_time_ms =  ait->second.avg();

        metric  = "read IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float read_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "read decimal MB/sec";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float read_decimal_MB_per_second =  ait->second.sum() / repetition_factor;

        metric  = "write service time ms";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float write_service_time_ms =  ait->second.avg();

        metric  = "write IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float write_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "write decimal MB/sec";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float write_decimal_MB_per_second =  ait->second.sum() / repetition_factor;

        metric  = "random blocksize KiB";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_blocksize_KiB =  ait->second.avg();


        metric  = "random IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "random decimal MB/sec";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_decimal_MB_per_second =  ait->second.sum() / repetition_factor;


        metric  = "seq blocksize KiB";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_blocksize_KiB =  ait->second.avg();


        metric  = "seq IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "seq decimal MB/sec";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_decimal_MB_per_second =  ait->second.sum() / repetition_factor;


//-------------------------------------------------------------------------------
        metric  = "random read blocksize KiB";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_read_blocksize_KiB =  ait->second.avg();

        metric  = "random read IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_read_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "random read decimal MB/sec";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_read_decimal_MB_per_second =  ait->second.sum() / repetition_factor;


        metric  = "random write blocksize KiB";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_write_blocksize_KiB =  ait->second.avg();


        metric  = "random write IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_write_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "random write decimal MB/sec";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float random_write_decimal_MB_per_second =  ait->second.sum() / repetition_factor;

//-------------------------------------------------------------------------------
        metric  = "seq read blocksize KiB";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_read_blocksize_KiB =  ait->second.avg();


        metric  = "seq read IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_read_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "seq read decimal MB/sec";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_read_decimal_MB_per_second =  ait->second.sum() / repetition_factor;


        metric  = "seq write blocksize KiB";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_write_blocksize_KiB =  ait->second.avg();


        metric  = "seq write IOPS";
        ait = ldev_it->second.find(metric);
        if ( ait == ldev_it->second.end() )
        {
            std::ostringstream errah; errah << "<Error> subsystem_summary_data::thumbnail() - have LDEV element but not \"" << metric << "\" metric >";
            log(m_s.masterlogfile, errah.str());
            throw std::runtime_error(errah.str());
        }
        ivy_float sequential_write_IOPS =  ait->second.sum() / repetition_factor;

        metric  = "seq write decimal MB/sec";
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
            o << ", " << std::fixed << std::setprecision(0) << LDEV_count << " LDEVs, totals over LDEVs:" << std::endl;

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
        {   // not detailed_thumbnail

            o << ", " << std::fixed << std::setprecision(0) << LDEV_count << " LDEVs: ";

            if (IOPS <= 0.0)
            {
                o << "IOPS = 0.0";
            }
            else
            {
                o << "IOPS = " << std::fixed << std::setprecision(2) << IOPS;
                o << "; serv = " << std::fixed << std::setprecision(3) << service_time_ms << " ms";
                o << "; " << std::fixed << std::setprecision(2) << (decimal_MB_per_second/(1.024*1.024)) << " MiB/s";

                if (IOPS > 0.0 && decimal_MB_per_second > 0.0)
                {
                    o << "; blk = " << std::fixed << std::setprecision(1) << ((1e6 * decimal_MB_per_second/IOPS)/1024.0) << " KiB";
                }

//                if (random_IOPS > 0.0 )
//                {
//                    o << "; rand blk = " << std::fixed << std::setprecision(random_blocksize_KiB == ceil(random_blocksize_KiB) ? 0 : 2) << random_blocksize_KiB << " KiB";
//                }
//                if (sequential_IOPS > 0.0 )
//                {
//                    o << "; seq blk = " << std::fixed << std::setprecision(sequential_blocksize_KiB == ceil(sequential_blocksize_KiB) ? 0 : 2) << sequential_blocksize_KiB << " KiB";
//                }
            }
        }
    }
    else
    {
        o << " < no LDEV data >";
    }

    o << std::endl;

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

    auto ait = ldev_it->second.find("decimal MB/sec");
    if ( ait == ldev_it->second.end() ) return -1.0;

    return ait->second.sum() / repetition_factor;
}

ivy_float subsystem_summary_data::service_time_ms()
{
    auto ldev_it = data.find("LDEV");
    if (ldev_it == data.end()) return -1.0;

    auto ait = ldev_it->second.find("service time ms");
    if ( ait == ldev_it->second.end() ) return -1.0;

    return ait->second.avg();
}

ivy_float subsystem_summary_data::MP_microseconds_per_IO()
{
    ivy_float total_IOPS = IOPS();

    if (total_IOPS <= 0.0) return -1.0;

    auto MP_it = data.find("MP core");
    if (MP_it == data.end()) return -1.0;

    auto metric_it = MP_it->second.find("Busy %");
    if ( metric_it == MP_it->second.end() ) return -1.0;

    if (metric_it ->second.count() == 0) return -1.0;

    return 1E6 * (metric_it->second.sum() / repetition_factor) / total_IOPS;
}

ivy_float subsystem_summary_data::avg_MP_core_busy_fraction()  // returns -1.0 if no data.
{
    auto MP_it = data.find("MP core");
    if (MP_it == data.end()) return -1.0;

    auto metric_it = MP_it->second.find("Busy %");
    if ( metric_it == MP_it->second.end() ) return -1.0;

    if (metric_it ->second.count() == 0) return -1.0;

    return metric_it ->second.avg();
}









