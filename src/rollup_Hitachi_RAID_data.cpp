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

void rollup_Hitachi_RAID_data(logger& logfilename, Hitachi_RAID_subsystem* p_Hitachi_RAID_subsystem, GatherData& currentGD)
{
    for (auto& rolluptypepear : m_s.rollups.rollups)
    {
        bool is_all_type { stringCaseInsensitiveEquality("all",rolluptypepear.first) };

        for (auto& rollupinstancepear : rolluptypepear.second->instances)
        {
            if (false && routine_logging)
            {
                log(logfilename,rollupinstancepear.second->config_filter_contents());
            }

            RollupInstance& ri = *(rollupinstancepear.second);

            bool is_all_instance { is_all_type && stringCaseInsensitiveEquality("all",rollupinstancepear.first) };

            for (auto& mpear : m_s.subsystem_summary_metrics)
            {
                std::string summary_element = mpear.first;

                std::vector<std::pair<std::string, unsigned char> >& summary_metrics = mpear.second;

                // iterate over all gathered instances and see if they fit this RollupInstance
                auto gd_el_it = currentGD.data.find(summary_element);

                if ( gd_el_it == currentGD.data.end() )
                {
                    continue;
                }

                for (auto& gd_el_instance_pear : gd_el_it->second)
                {
                    const std::string& gd_el_instance = gd_el_instance_pear.first;

                    std::map<std::string,metric_value>& gd_el_instance_metrics = gd_el_instance_pear.second;

                    std::string& serial = p_Hitachi_RAID_subsystem->serial_number;

                    // Does this gathered element belong to this RollupInstance?

                    auto ri_s_it = ri.config_filter.find(serial);

                    if ( ri_s_it != ri.config_filter.end() )
                    {
                        auto ri_el_it = ri_s_it->second.find(toLower(summary_element));
                        if ( ri_el_it != ri_s_it->second.end() )
                        {
                            // have this element in the RollupInstance

                            auto ri_el_in_it = ri_el_it->second.find(gd_el_instance);
                            if ( ri_el_in_it != ri_el_it->second.end())
                            {
                                // this gather data element instance belongs to this RollupInstance

                                // For each metric for this summary element

                                for (std::pair<std::string,unsigned char> metric_pair : summary_metrics)
                                {

                                    if (!(metric_pair.second & fetch_metric)) continue;

                                    const std::string& metric = metric_pair.first;

                                    auto hunh_is_your_head_exploding_yet = gd_el_instance_metrics.find(metric);


                                    if (hunh_is_your_head_exploding_yet == gd_el_instance_metrics.end())
                                    {
                                        if (routine_logging)
                                        {
                                            std::ostringstream o;
                                            o << "pipe_driver_subthread - when rolling up subsystem perf data by RollupInstance, for summary data configuration element type \""
                                              << summary_element << "\", metric \"" << metric << "\" was not found in the data just gathered from the subsystem.";

                                            o << "Valid metrics are";
                                            for (auto& pear : gd_el_instance_metrics)
                                            {
                                                o << " \"" << pear.first << "\" ";
                                            }
                                            o << std::endl;
                                            log(logfilename,o.str());
                                        }
                                    }
                                    else
                                    {
                                        std::ostringstream o;
                                        o <<  "rollup_Hitachi_RAID_data() summary_element=\"" << summary_element << "\", metric = \"" << metric << "\""
                                            << ", value = " <<  hunh_is_your_head_exploding_yet->second.toString();
                                        ri.subsystem_data_by_subinterval.back().data[summary_element][metric].push(hunh_is_your_head_exploding_yet->second.numeric_value(o.str()));
                                    }
                                }
                            } // if ( ri_el_in_it != ri_el_it->second.end())
                            else
                            {
                                if (is_all_instance)
                                {
                                    if (routine_logging)
                                    {
                                        std::ostringstream o;
                                        o << "all=all instance, posting into RollupSet::non_participating for element \"" << summary_element << "\", instance \"" << gd_el_instance << "\"."  << std::endl;
                                        log(logfilename,o.str());
                                    }

                                    // For each metric for this summary element

                                    for (const std::pair<std::string,unsigned char>& metric_pair : summary_metrics)
                                    {
                                        if (!(metric_pair.second & fetch_metric)) continue;

                                        const std::string& metric = metric_pair.first;

                                        auto hunh_is_your_head_exploding_yet = gd_el_instance_metrics.find(metric);

                                        if (hunh_is_your_head_exploding_yet == gd_el_instance_metrics.end())
                                        {
                                            if (routine_logging)
                                            {
                                                std::ostringstream o;
                                                o << "pipe_driver_subthread - when rolling up subsystem perf data by RollupInstance - for \"all\" instance, for summary data configuration element type \""
                                                  << summary_element << "\", metric \"" << metric << "\" was not found in the data just gathered from the subsystem.";
                                                o << "Valid metrics are";
                                                for (auto& pear : gd_el_instance_metrics)
                                                {
                                                    o << " \"" << pear.first << "\" ";
                                                }
                                                o << std::endl;

                                                log(logfilename,o.str());
                                            }
                                        }
                                        else
                                        {
                                            std::ostringstream o;
                                            o <<  "rollup_Hitachi_RAID_data() summary_element=\"" << summary_element << "\", metric = \"" << metric << "\""
                                                << ", value = " <<  hunh_is_your_head_exploding_yet->second.toString();

                                            m_s.rollups.not_participating.back().data[summary_element][metric].push(hunh_is_your_head_exploding_yet->second.numeric_value(o.str()));
                                        }
                                    }
                                }
                            } // else - if ( ri_el_in_it != ri_el_it->second.end())
                        }
                    }
                }
            } // for (auto& mpear : m_s.subsystem_summary_metrics)
        } // for (auto& rollupinstancepear : rolluptypepear.second->instances)
    } // for (auto& rolluptypepear : m_s.rollups.rollups)


    // now compute derived metrics

    for (auto& pear : m_s.rollups.rollups)
    {
//        const std::string& rollup_type_name = pear.first;
        RollupType* pRollupType = pear.second;

        for (auto& peach : pRollupType->instances)
        {
            const std::string rollup_instance_name=peach.first;
            RollupInstance* pRollupInstance = peach.second;

            if (pRollupInstance->subsystem_data_by_subinterval.size()>0)
            {
                subsystem_summary_data& ssd = pRollupInstance->subsystem_data_by_subinterval.back();

                auto ldev_it = pRollupInstance->subsystem_data_by_subinterval.back().data.find("LDEV");
                if (ldev_it != ssd.data.end())
                {
                    std::string metric_name;

                    ivy_float random_read_IOPS;
                    auto metric_it = ldev_it->second.find("random read IOPS");
                    if (metric_it == ldev_it->second.end()) random_read_IOPS = 0.0;
                    else                                    random_read_IOPS = metric_it->second.sum() / ssd.repetition_factor;

                    ivy_float random_write_IOPS;
                    metric_it = ldev_it->second.find("random write IOPS");
                    if (metric_it == ldev_it->second.end()) random_write_IOPS = 0.0;
                    else                                    random_write_IOPS = metric_it->second.sum() / ssd.repetition_factor;

                    ivy_float sequential_read_IOPS;
                    metric_it = ldev_it->second.find("seq read IOPS");
                    if (metric_it == ldev_it->second.end()) sequential_read_IOPS = 0.0;
                    else                                    sequential_read_IOPS = metric_it->second.sum() / ssd.repetition_factor;

                    ivy_float sequential_write_IOPS;
                    metric_it = ldev_it->second.find("seq write IOPS");
                    if (metric_it == ldev_it->second.end()) sequential_write_IOPS = 0.0;
                    else                                    sequential_write_IOPS = metric_it->second.sum()/ ssd.repetition_factor;

                    ivy_float read_IOPS_x_response_time_ms;
                    metric_it = ldev_it->second.find("read IOPS x response time ms");
                    if (metric_it == ldev_it->second.end()) read_IOPS_x_response_time_ms = 0.0;
                    else                                    read_IOPS_x_response_time_ms = metric_it->second.sum()/ ssd.repetition_factor;

                    ivy_float write_IOPS_x_response_time_ms;
                    metric_it = ldev_it->second.find("write IOPS x response time ms");
                    if (metric_it == ldev_it->second.end()) write_IOPS_x_response_time_ms = 0.0;
                    else                                    write_IOPS_x_response_time_ms = metric_it->second.sum() / ssd.repetition_factor;

                    ivy_float random_read_bytes_per_second;
                    metric_it = ldev_it->second.find("random read bytes/sec");
                    if (metric_it == ldev_it->second.end()) random_read_bytes_per_second = 0.0;
                    else                                    random_read_bytes_per_second = metric_it->second.sum() / ssd.repetition_factor;

                    ivy_float random_write_bytes_per_second;
                    metric_it = ldev_it->second.find("random write bytes/sec");
                    if (metric_it == ldev_it->second.end()) random_write_bytes_per_second = 0.0;
                    else                                    random_write_bytes_per_second = metric_it->second.sum() / ssd.repetition_factor;

                    ivy_float sequential_read_bytes_per_second;
                    metric_it = ldev_it->second.find("seq read bytes/sec");
                    if (metric_it == ldev_it->second.end()) sequential_read_bytes_per_second = 0.0;
                    else                                    sequential_read_bytes_per_second = metric_it->second.sum() / ssd.repetition_factor;

                    ivy_float sequential_write_bytes_per_second;
                    metric_it = ldev_it->second.find("seq write bytes/sec");
                    if (metric_it == ldev_it->second.end()) sequential_write_bytes_per_second = 0.0;
                    else                                    sequential_write_bytes_per_second = metric_it->second.sum() / ssd.repetition_factor;

                    ivy_float read_IOPS = random_read_IOPS + sequential_read_IOPS;
                    ivy_float write_IOPS = random_write_IOPS + sequential_write_IOPS;

                    ivy_float random_IOPS = random_read_IOPS + random_write_IOPS;
                    ivy_float sequential_IOPS = sequential_read_IOPS + sequential_write_IOPS;

                    ivy_float IOPS = read_IOPS + write_IOPS;

                    ivy_float random_read_decimal_MB_per_second = random_read_bytes_per_second / 1E6;
                    ivy_float random_write_decimal_MB_per_second = random_write_bytes_per_second / 1E6;

                    ivy_float sequential_read_decimal_MB_per_second = sequential_read_bytes_per_second / 1E6;
                    ivy_float sequential_write_decimal_MB_per_second = sequential_write_bytes_per_second / 1E6;

                    ivy_float read_decimal_MB_per_second = random_read_decimal_MB_per_second + sequential_read_decimal_MB_per_second;
                    ivy_float write_decimal_MB_per_second = random_write_decimal_MB_per_second + sequential_write_decimal_MB_per_second;

                    ivy_float random_decimal_MB_per_second = random_read_decimal_MB_per_second + random_write_decimal_MB_per_second;
                    ivy_float sequential_decimal_MB_per_second = sequential_read_decimal_MB_per_second + sequential_write_decimal_MB_per_second;

                    ivy_float decimal_MB_per_second = read_decimal_MB_per_second + write_decimal_MB_per_second;

                    ivy_float service_time_ms, read_service_time_ms, write_service_time_ms;

                    if ( read_IOPS <=0 ) read_service_time_ms = 0.0;
                    else                 read_service_time_ms = read_IOPS_x_response_time_ms / read_IOPS;

                    if ( write_IOPS <=0 ) write_service_time_ms = 0.0;
                    else                  write_service_time_ms = write_IOPS_x_response_time_ms / write_IOPS;

                    if (IOPS > 0.0) service_time_ms = (read_IOPS * read_service_time_ms + write_IOPS * write_service_time_ms) / IOPS;
                    else            service_time_ms = 0.0;

                    ivy_float random_read_blocksize_KiB {0.0};
                    if ( random_read_IOPS > 0 ) random_read_blocksize_KiB = (1.0/1024.0) * random_read_bytes_per_second / random_read_IOPS;

                    ivy_float random_write_blocksize_KiB {0.0};
                    if ( random_write_IOPS > 0 ) random_write_blocksize_KiB = (1.0/1024.0) * random_write_bytes_per_second / random_write_IOPS;

                    ivy_float random_blocksize_KiB {0.0};
                    if (random_IOPS > 0.0) random_blocksize_KiB = ( (random_read_blocksize_KiB * random_read_IOPS) + (random_write_blocksize_KiB * random_write_IOPS) ) / random_IOPS;

                    ivy_float sequential_read_blocksize_KiB {0.0};
                    if ( sequential_read_IOPS > 0.0 ) sequential_read_blocksize_KiB = (1.0/1024.0) * sequential_read_bytes_per_second / sequential_read_IOPS;

                    ivy_float sequential_write_blocksize_KiB {0.0};
                    if ( sequential_write_IOPS > 0.0 ) sequential_write_blocksize_KiB = (1.0/1024.0) * sequential_write_bytes_per_second / sequential_write_IOPS;

                    ivy_float sequential_blocksize_KiB {0.0};
                    if (sequential_IOPS > 0.0) sequential_blocksize_KiB = ( (sequential_read_blocksize_KiB * sequential_read_IOPS) + (sequential_write_blocksize_KiB * sequential_write_IOPS) ) / sequential_IOPS;


                    {auto& x = ssd.data["LDEV"]["IOPS"];                                   x.clear(); x.push(IOPS);}
                    {auto& x = ssd.data["LDEV"]["decimal MB/sec"];                  x.clear(); x.push(decimal_MB_per_second);}

                    {auto& x = ssd.data["LDEV"]["read IOPS"];                              x.clear(); x.push(read_IOPS);}
                    {auto& x = ssd.data["LDEV"]["read decimal MB/sec"];             x.clear(); x.push(read_decimal_MB_per_second);}

                    {auto& x = ssd.data["LDEV"]["write IOPS"];                             x.clear(); x.push(write_IOPS);}
                    {auto& x = ssd.data["LDEV"]["write decimal MB/sec"];            x.clear(); x.push(write_decimal_MB_per_second);}

                    {auto& x = ssd.data["LDEV"]["random IOPS"];                            x.clear(); x.push(random_IOPS);}
                    {auto& x = ssd.data["LDEV"]["random decimal MB/sec"];           x.clear(); x.push(random_decimal_MB_per_second);}

                    {auto& x = ssd.data["LDEV"]["seq IOPS"];                        x.clear(); x.push(sequential_IOPS);}
                    {auto& x = ssd.data["LDEV"]["seq decimal MB/sec"];       x.clear(); x.push(sequential_decimal_MB_per_second);}

                    {auto& x = ssd.data["LDEV"]["random read decimal MB/sec"];      x.clear(); x.push(random_read_decimal_MB_per_second);}
                    {auto& x = ssd.data["LDEV"]["random write decimal MB/sec"];     x.clear(); x.push(random_write_decimal_MB_per_second);}
                    {auto& x = ssd.data["LDEV"]["random read blocksize KiB"];              x.clear(); x.push(random_read_blocksize_KiB);}
                    {auto& x = ssd.data["LDEV"]["random write blocksize KiB"];             x.clear(); x.push(random_write_blocksize_KiB);}
                    {auto& x = ssd.data["LDEV"]["random blocksize KiB"];                   x.clear(); x.push(random_blocksize_KiB);}

                    {auto& x = ssd.data["LDEV"]["seq read decimal MB/sec"];  x.clear(); x.push(sequential_read_decimal_MB_per_second);}
                    {auto& x = ssd.data["LDEV"]["seq write decimal MB/sec"]; x.clear(); x.push(sequential_write_decimal_MB_per_second);}
                    {auto& x = ssd.data["LDEV"]["seq read blocksize KiB"];          x.clear(); x.push(sequential_read_blocksize_KiB);}
                    {auto& x = ssd.data["LDEV"]["seq write blocksize KiB"];         x.clear(); x.push(sequential_write_blocksize_KiB);}
                    {auto& x = ssd.data["LDEV"]["seq blocksize KiB"];               x.clear(); x.push(sequential_blocksize_KiB);}

                    {auto& x = ssd.data["LDEV"]["service time ms"];                        x.clear(); x.push(service_time_ms);}
                    {auto& x = ssd.data["LDEV"]["read service time ms"];                   x.clear(); x.push(read_service_time_ms);}
                    {auto& x = ssd.data["LDEV"]["write service time ms"];                  x.clear(); x.push(write_service_time_ms);}
                }
            }
        }
    }
    return;
}
