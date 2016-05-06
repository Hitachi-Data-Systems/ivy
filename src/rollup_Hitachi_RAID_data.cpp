//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include <iostream>
#include <iomanip>
#include <sstream>

#include "GatherData.h"
#include "Subsystem.h"
#include "ivyhelpers.h"
#include "master_stuff.h"
#include "RollupInstance.h"

extern bool routine_logging;

void rollup_Hitachi_RAID_data(const std::string& logfilename, Hitachi_RAID_subsystem* p_Hitachi_RAID_subsystem, GatherData& currentGD, GatherData& lastGD)
{
    auto this_element_it = currentGD.data.find(std::string("LDEV"));
    auto last_element_it =    lastGD.data.find(std::string("LDEV"));

    if (this_element_it != currentGD.data.end() && last_element_it != lastGD.data.end())
    {
        for (auto& instance_pair : this_element_it->second)
        {
            const std::string& ldevname = instance_pair.first;
            auto last_instance_it = last_element_it->second.find(ldevname);
            if (last_instance_it == last_element_it->second.end())
            {
                std::ostringstream o;
                o << "When post-processing LDEV I/O for gathers.size() = " << p_Hitachi_RAID_subsystem->gathers.size()
                  << ", for LDEV \"" << ldevname << "\", the previous gather did not have data from such an LDEV." << std::endl;
                throw std::runtime_error(o.str());
            }
            std::map<std::string,metric_value>& this_metrics = instance_pair.second;
            std::map<std::string,metric_value>& last_metrics = last_instance_it->second;

            auto this_PGs_it = this_metrics.find("PGs");
            if(this_PGs_it != this_metrics.end() && this_metrics.end() == this_metrics.find("command_device"))
            {
                uint64_t PGs = this_PGs_it->second.uint64_t_value("this_PGs_it->second");
                if (PGs > 4)
                {
                    std::ostringstream o;
                    o << "When post-processing LDEV I/O for gathers.size() = " << p_Hitachi_RAID_subsystem->gathers.size()
                      << ", for LDEV \"" << ldevname << "\", the PG quantity 0-4 was set to " << PGs << "." << std::endl;
                    throw std::runtime_error(o.str());
                }
                if (PGs > 0)
                {
                    uint64_t delta_DKC
                        = this_metrics["DKC_time_96us_for_PG_busy_count"].uint64_t_value("this_metrics[\"DKC_time_96us_for_PG_busy_count\"]")
                          - last_metrics["DKC_time_96us_for_PG_busy_count"].uint64_t_value("last_metrics[\"DKC_time_96us_for_PG_busy_count\"]");

                    uint64_t delta_random_read
                        = this_metrics["PG_busy_96us_random_read_count"].uint64_t_value("this_metrics[\"PG_busy_96us_random_read_count\"]")
                          - last_metrics["PG_busy_96us_random_read_count"].uint64_t_value("last_metrics[\"PG_busy_96us_random_read_count\"]");

                    uint64_t delta_random_write_data
                        = this_metrics["PG_busy_96us_random_write_data_count"].uint64_t_value("this_metrics[\"PG_busy_96us_random_write_data_count\"]")
                          - last_metrics["PG_busy_96us_random_write_data_count"].uint64_t_value("last_metrics[\"PG_busy_96us_random_write_data_count\"]");

                    uint64_t delta_random_write_parity
                        = this_metrics["PG_busy_96us_random_write_parity_count"].uint64_t_value("this_metrics[\"PG_busy_96us_random_write_parity_count\"]")
                          - last_metrics["PG_busy_96us_random_write_parity_count"].uint64_t_value("last_metrics[\"PG_busy_96us_random_write_parity_count\"]");

                    uint64_t delta_seq_read
                        = this_metrics["PG_busy_96us_seq_read_count"].uint64_t_value("this_metrics[\"PG_busy_96us_seq_read_count\"]")
                          - last_metrics["PG_busy_96us_seq_read_count"].uint64_t_value("last_metrics[\"PG_busy_96us_seq_read_count\"]");

                    uint64_t delta_seq_write_data
                        = this_metrics["PG_busy_96us_seq_write_data_count"].uint64_t_value("this_metrics[\"PG_busy_96us_seq_write_data_count\"]")
                          - last_metrics["PG_busy_96us_seq_write_data_count"].uint64_t_value("last_metrics[\"PG_busy_96us_seq_write_data_count\"]");

                    uint64_t delta_seq_write_parity
                        = this_metrics["PG_busy_96us_seq_write_parity_count"].uint64_t_value("this_metrics[\"PG_busy_96us_seq_write_parity_count\"]")
                          - last_metrics["PG_busy_96us_seq_write_parity_count"].uint64_t_value("last_metrics[\"PG_busy_96us_seq_write_parity_count\"]");

                    long double
                        random_read_busy         {0.0},
                        random_write_data_busy   {0.0},
                        random_write_parity_busy {0.0},
                        seq_read_busy            {0.0},
                        seq_write_data_busy      {0.0},
                        seq_write_parity_busy    {0.0},
                        random_write_busy        {0.0},
                        seq_write_busy           {0.0},
                        busy                     {0.0};

                    if (delta_DKC > 0) // it turns out DKC elapsed time for computing PG busy does not increment if there is no PG activity!
                    {
                        random_read_busy         = ((long double)delta_random_read)         / ((long double) delta_DKC);
                        random_write_data_busy   = ((long double)delta_random_write_data)   / ((long double) delta_DKC);
                        random_write_parity_busy = ((long double)delta_random_write_parity) / ((long double) delta_DKC);
                        seq_read_busy            = ((long double)delta_seq_read)            / ((long double) delta_DKC);
                        seq_write_data_busy      = ((long double)delta_seq_write_data)      / ((long double) delta_DKC);
                        seq_write_parity_busy    = ((long double)delta_seq_write_parity)    / ((long double) delta_DKC);

                        random_write_busy = random_write_data_busy + random_write_parity_busy;
                        seq_write_busy = seq_write_data_busy + seq_write_parity_busy;

                        busy = random_read_busy + random_write_busy + seq_read_busy + seq_write_busy;
                    }
                    else
                    {
                        if
                        (
                               ( delta_random_read         > 0 )
                            || ( delta_random_write_data   > 0 )
                            || ( delta_random_write_parity > 0 )
                            || ( delta_seq_read            > 0 )
                            || ( delta_seq_write_data      > 0 )
                            || ( delta_seq_write_parity    > 0 )
                        )
                        {
                            std::ostringstream o;
                            o << std::endl << "***** In the RMLIB data, delta in DKC time counter for computing parity busy was zero, but the counter delta for at least one of the 6 PG busy metrics was non-zero." << std::endl;
                            o << std::endl << "      Delta_random_read = " << delta_random_read << ", delta_random_write_data = " << delta_random_write_data << ", delta_random_write_parity = " << delta_random_write_parity << "." << std::endl;
                            o << std::endl << "      Delta_seq_read = " << delta_seq_read << ", delta_seq_write_data = " << delta_seq_write_data << ", delta_seq_write_parity = " << delta_seq_write_parity << "." << std::endl;
                            o << std::endl << std::endl;
                            std::cout << o.str();
                            log(m_s.masterlogfile,o.str());
                        }

                    }

                    std::string pgbusy, rrbusy, rwbusy, srbusy, swbusy, rwdbusy, rwpbusy, swdbusy, swpbusy;
                    {
                        std::ostringstream o;
                        o << std::fixed << std::setprecision(4) << (100.*busy) << '%';
                        pgbusy = o.str();
                    }

                    {
                        std::ostringstream o;
                        o << std::fixed << std::setprecision(4) << (100.*random_read_busy) << '%';
                        rrbusy = o.str();
                    }
                    {
                        std::ostringstream o;
                        o << std::fixed << std::setprecision(4) << (100.*random_write_busy) << '%';
                        rwbusy = o.str();
                    }
                    {
                        std::ostringstream o;
                        o << std::fixed << std::setprecision(4) << (100.*seq_read_busy) << '%';
                        srbusy = o.str();
                    }
                    {
                        std::ostringstream o;
                        o << std::fixed << std::setprecision(4) << (100.*seq_write_busy) << '%';
                        swbusy = o.str();
                    }

                    {
                        std::ostringstream o;
                        o << std::fixed << std::setprecision(4) << (100.*random_write_data_busy) << '%';
                        rwdbusy = o.str();
                    }
                    {
                        std::ostringstream o;
                        o << std::fixed << std::setprecision(4) << (100.*random_write_parity_busy) << '%';
                        rwpbusy = o.str();
                    }
                    {
                        std::ostringstream o;
                        o << std::fixed << std::setprecision(4) << (100.*seq_write_data_busy) << '%';
                        swdbusy = o.str();
                    }
                    {
                        std::ostringstream o;
                        o << std::fixed << std::setprecision(4) << (100.*seq_write_parity_busy) << '%';
                        swpbusy = o.str();
                    }

                    this_metrics["busy_percent"].value = pgbusy;
                    this_metrics["random_read_busy_percent"].value = rrbusy;
                    this_metrics["random_write_busy_percent"].value = rwbusy;
                    this_metrics["seq_read_busy_percent"].value = srbusy;
                    this_metrics["seq_write_busy_percent"].value = swbusy;
                    this_metrics["random_write_data_busy_percent"].value = rwdbusy;
                    this_metrics["random_write_parity_busy_percent"].value = rwpbusy;
                    this_metrics["seq_write_data_busy_percent"].value = swdbusy;
                    this_metrics["seq_write_parity_busy_percent"].value = swpbusy;

                    for (unsigned int index = 0; index < PGs && PGs <=4; index++)
                    {
                        // here is where we post the PG busy numbers.
                        std::string pg;
                        switch (index)
                        {
                        case 0:
                            pg="PG";
                            break;
                        case 1:
                            pg="PG+1";
                            break;
                        case 2:
                            pg="PG+2";
                            break;
                        case 3:
                            pg="PG+3";
                            break;
                        }
                        auto& x = currentGD.data["PG"][this_metrics[pg].string_value()];
                        x["busy_percent"].value = pgbusy;
                        x["random_read_busy_percent"].value = rrbusy;
                        x["random_write_busy_percent"].value = rwbusy;
                        x["seq_read_busy_percent"].value = srbusy;
                        x["seq_write_busy_percent"].value = swbusy;
                        x["random_write_data_busy_percent"].value = rwdbusy;
                        x["random_write_parity_busy_percent"].value = rwpbusy;
                        x["seq_write_data_busy_percent"].value = swdbusy;
                        x["seq_write_parity_busy_percent"].value = swpbusy;
                    }
                }
            } // if(this_PGs_it != this_metrics.end() && this_metrics.end() == this_metrics.find("command_device"))


            if ( this_metrics.end() == this_metrics.find("command_device") )
            {
                // Process raw counter values into consumable form

                std::string emulation = this_metrics["emulation"].string_value();



                if (startsWith(emulation,"OPEN"))
                {
                    ivy_float random_read_IOPS, random_write_IOPS, sequential_read_IOPS, sequential_write_IOPS,
                              read_IOPS, write_IOPS,
                              read_response_ms, write_response_ms;

                    uint64_t random_read_delta
                    {
                        this_metrics["random_read_IO_count"].uint64_t_value("this_metrics[\"random_read_IO_count\"]")
                        - last_metrics["random_read_IO_count"].uint64_t_value("last_metrics[\"random_read_IO_count\"]")
                    };
                    uint64_t random_read_hit_delta
                    {
                        this_metrics["random_read_hit_IO_count"].uint64_t_value("this_metrics[\"random_read_hit_IO_count\"]")
                        - last_metrics["random_read_hit_IO_count"].uint64_t_value("last_metrics[\"random_read_hit_IO_count\"]")
                    };
                    uint64_t random_write_delta
                    {
                        this_metrics["random_write_IO_count"].uint64_t_value("this_metrics[\"random_write_IO_count\"]")
                        - last_metrics["random_write_IO_count"].uint64_t_value("last_metrics[\"random_write_IO_count\"]")
                    };
                    uint64_t sequential_read_delta
                    {
                        this_metrics["sequential_read_IO_count"].uint64_t_value("this_metrics[\"sequential_read_IO_count\"]")
                        - last_metrics["sequential_read_IO_count"].uint64_t_value("last_metrics[\"sequential_read_IO_count\"]")
                    };
                    uint64_t sequential_read_hit_delta
                    {
                        this_metrics["sequential_read_hit_IO_count"].uint64_t_value("this_metrics[\"sequential_read_hit_IO_count\"]")
                        - last_metrics["sequential_read_hit_IO_count"].uint64_t_value("last_metrics[\"sequential_read_hit_IO_count\"]")
                    };
                    uint64_t sequential_write_delta
                    {
                        this_metrics["sequential_write_IO_count"].uint64_t_value("this_metrics[\"sequential_write_IO_count\"]")
                        - last_metrics["sequential_write_IO_count"].uint64_t_value("last_metrics[\"sequential_write_IO_count\"]")
                    };
                    uint64_t random_read_block_delta
                    {
                        this_metrics["random_read_block_count"].uint64_t_value("this_metrics[\"random_read_block_count\"]")
                        - last_metrics["random_read_block_count"].uint64_t_value("last_metrics[\"random_read_block_count\"]")
                    };
                    uint64_t random_write_block_delta
                    {
                        this_metrics["random_write_block_count"].uint64_t_value("this_metrics[\"random_write_block_count\"]")
                        - last_metrics["random_write_block_count"].uint64_t_value("last_metrics[\"random_write_block_count\"]")
                    };
                    uint64_t sequential_read_block_delta
                    {
                        this_metrics["sequential_read_block_count"].uint64_t_value("this_metrics[\"sequential_read_block_count\"]")
                        - last_metrics["sequential_read_block_count"].uint64_t_value("last_metrics[\"sequential_read_block_count\"]")
                    };
                    uint64_t sequential_write_block_delta
                    {
                        this_metrics["sequential_write_block_count"].uint64_t_value("this_metrics[\"sequential_write_block_count\"]")
                        - last_metrics["sequential_write_block_count"].uint64_t_value("last_metrics[\"sequential_write_block_count\"]")
                    };

                    {
                        std::ostringstream o;
                        random_read_IOPS = ((ivy_float) random_read_delta) / m_s.subinterval_seconds;
                        o   << std::fixed << std::setprecision(4)
                            << random_read_IOPS;
                        this_metrics["random_read_IOPS"].value = o.str();
                    }

                    if (random_read_delta > 0)
                    {
                        std::ostringstream o;
                        o   << std::fixed << std::setprecision(4)
                            << (100.0 * ((ivy_float) random_read_hit_delta) / ((ivy_float) random_read_delta) )
                            << "%";
                        this_metrics["random_read_hit_percent"].value = o.str();
                    }
                    else this_metrics["random_read_hit_percent"].value = "";

                    {
                        std::ostringstream o;
                        random_write_IOPS = ((ivy_float) random_write_delta) / m_s.subinterval_seconds;
                        o   << std::fixed << std::setprecision(4)
                            << random_write_IOPS;
                        this_metrics["random_write_IOPS"].value = o.str();
                    }

                    {
                        std::ostringstream o;
                        sequential_read_IOPS = ((ivy_float) sequential_read_delta) / m_s.subinterval_seconds;
                        o   << std::fixed << std::setprecision(4)
                            << sequential_read_IOPS;
                        this_metrics["sequential_read_IOPS"].value = o.str();
                    }

                    if (sequential_read_delta > 0)
                    {
                        std::ostringstream o;
                        o   << std::fixed << std::setprecision(4)
                            << (100.0 * ((ivy_float) sequential_read_hit_delta) / ((ivy_float) sequential_read_delta) )
                            << "%";
                        this_metrics["sequential_read_hit_percent"].value = o.str();
                    }
                    else this_metrics["sequential_read_hit_percent"].value = "";

                    {
                        std::ostringstream o;
                        sequential_write_IOPS = ((ivy_float) sequential_write_delta) / m_s.subinterval_seconds;
                        o   << std::fixed << std::setprecision(4)
                            << sequential_write_IOPS;
                        this_metrics["sequential_write_IOPS"].value = o.str();
                    }

                    read_IOPS = random_read_IOPS + sequential_read_IOPS;
                    write_IOPS = random_write_IOPS + sequential_write_IOPS;

                    {
                        std::ostringstream o;
                        o   << std::fixed << std::setprecision(4)
                            << ((ivy_float) (sector_size_bytes*random_read_block_delta)) / m_s.subinterval_seconds;
                        this_metrics["random_read_bytes_per_second"].value = o.str();
                    }
                    {
                        std::ostringstream o;
                        o   << std::fixed << std::setprecision(4)
                            << ((ivy_float) (sector_size_bytes*random_write_block_delta)) / m_s.subinterval_seconds;
                        this_metrics["random_write_bytes_per_second"].value = o.str();
                    }
                    {
                        std::ostringstream o;
                        o   << std::fixed << std::setprecision(4)
                            << ((ivy_float) (sector_size_bytes*sequential_read_block_delta)) / m_s.subinterval_seconds;
                        this_metrics["sequential_read_bytes_per_second"].value = o.str();
                    }
                    {
                        std::ostringstream o;
                        o   << std::fixed << std::setprecision(4)
                            << ((ivy_float) (sector_size_bytes*sequential_write_block_delta)) / m_s.subinterval_seconds;
                        this_metrics["sequential_write_bytes_per_second"].value = o.str();
                    }

                    uint64_t read_30_sec_max_response_096us {0};

                    {
                        std::ostringstream o;
                        auto it = this_metrics.find("read_30_sec_max_response_096us_count");
                        if (it != this_metrics.end())
                        {
                            if (it->second.value.size() > 0)
                            {
                                read_30_sec_max_response_096us = it ->second.uint64_t_value("this_metrics[\"read_30_sec_max_response_096us_count\"]");
                                o   << std::fixed << std::setprecision(4)
                                    << 1000.*(((ivy_float) read_30_sec_max_response_096us) * 0.96E-6);
                            }
                        }
                        this_metrics["read_30_sec_max_response_ms"].value = o.str();

                    };


                    uint64_t write_30_sec_max_response_096us {0};

                    {
                        std::ostringstream o;
                        auto it = this_metrics.find("write_30_sec_max_response_096us_count");
                        if (it != this_metrics.end())
                        {
                            if (it->second.value.size() > 0)
                            {
                                write_30_sec_max_response_096us = this_metrics["write_30_sec_max_response_096us_count"]
                                                                  .uint64_t_value("this_metrics[\"write_30_sec_max_response_096us_count\"]");
                                o   << std::fixed << std::setprecision(4)
                                    << 1000.*(((ivy_float) write_30_sec_max_response_096us) * 0.96E-6);
                            }
                        }
                        this_metrics["write_30_sec_max_response_ms"].value = o.str();
                    };

                    uint64_t read_delta = random_read_delta + sequential_read_delta;
                    uint64_t read_response_96us_delta
                    {
                        this_metrics["read_response_96us_count"].uint64_t_value("this_metrics[\"read_response_96us_count\"]")
                        - last_metrics["read_response_96us_count"].uint64_t_value("last_metrics[\"read_response_96us_count\"]")
                    };
                    {
                        std::ostringstream o;
                        if (read_delta > 0)
                        {
                            read_response_ms = 1000.* (((ivy_float) read_response_96us_delta) * 0.96E-6) / ((ivy_float) read_delta);
                            o   << std::fixed << std::setprecision(4)
                            <<  read_response_ms;
                        }
                        else read_response_ms = 0.; // initialize to any value

                        this_metrics["read_response_ms"].value = o.str();
                    }

                    uint64_t write_delta = random_write_delta + sequential_write_delta;
                    uint64_t write_response_96us_delta
                    {
                        this_metrics["write_response_96us_count"].uint64_t_value("this_metrics[\"write_response_96us_count\"]")
                        - last_metrics["write_response_96us_count"].uint64_t_value("last_metrics[\"write_response_96us_count\"]")
                    };
                    {
                        std::ostringstream o;
                        if (write_delta > 0)
                        {
                            write_response_ms = 1000.*(((ivy_float) write_response_96us_delta) * 0.96E-6)/ ((ivy_float) write_delta);
                            o   << std::fixed << std::setprecision(4)
                            <<  write_response_ms;
                        }
                        else write_response_ms = 0.; // initialize to any value

                        this_metrics["write_response_ms"].value = o.str();
                    }

                    {
                        std::ostringstream o;
                        o << std::fixed << std::setprecision(4)
                          << (read_IOPS * read_response_ms);
                        this_metrics["read_IOPS_x_response_time_ms"].value = o.str();
                    }

                    {
                        std::ostringstream o;
                        o << std::fixed << std::setprecision(4)
                          << (write_IOPS * write_response_ms);
                        this_metrics["write_IOPS_x_response_time_ms"].value = o.str();
                    }

                    uint64_t delta_slot_count
                    {
                        this_metrics["slot_count_in_PDEV_batch_write_for_old_data_count"].uint64_t_value("this_metrics[\"slot_count_in_PDEV_batch_write_for_old_data_count\"]")
                        - last_metrics["slot_count_in_PDEV_batch_write_for_old_data_count"].uint64_t_value("last_metrics[\"slot_count_in_PDEV_batch_write_for_old_data_count\"]")
                    };
                    {
                        std::ostringstream o;
                        o   << std::fixed << std::setprecision(4)
                        <<  (
                            ((ivy_float) delta_slot_count)
                            / m_s.subinterval_seconds
                        );

                        this_metrics["slot_count_in_PDEV_batch_write_for_old_data_per_second"].value = o.str();
                    }


                    uint64_t delta_sum_of_slots
                    {
                        this_metrics["sum_of_slots_in_PDEV_batch_write_count"].uint64_t_value("this_metrics[\"sum_of_slots_in_PDEV_batch_write_count\"]")
                        - last_metrics["sum_of_slots_in_PDEV_batch_write_count"].uint64_t_value("last_metrics[\"sum_of_slots_in_PDEV_batch_write_count\"]")
                    };
                    {
                        std::ostringstream o;
                        o   << std::fixed << std::setprecision(4)
                        <<  (
                            ((ivy_float) delta_sum_of_slots)
                            / m_s.subinterval_seconds
                        );

                        this_metrics["sum_of_slots_in_PDEV_batch_write_count_per_second"].value = o.str();
                    }


                    std::string tiers_string;
                    auto tiers_it = this_metrics.find("tiers");
                    if (this_metrics.end() != tiers_it)
                    {
                        const std::string tiers_string = tiers_it->second.value;
                        if (tiers_string.size() >> 0)
                        {
                            uint64_t tiers = tiers_it->second.uint64_t_value("tiers_it->");
                            for (unsigned int i = 0; i < tiers; i++)
                            {
                                std::ostringstream t;
                                t << (i+1);
                                std::string tier_backend_IO_count = std::string("tier") + t.str() + std::string("_backend_IO_count");

                                if (0 == this_metrics["tier_update_ID"].string_value()
                                        .compare(last_metrics["tier_update_ID"].string_value())) // if the last time we were on the same HDT update cycle
                                {
                                    uint64_t tier_IO_delta
                                    {
                                        this_metrics[tier_backend_IO_count].uint64_t_value(std::string("this_metrics[") + tier_backend_IO_count + std::string("]"))
                                        - last_metrics[tier_backend_IO_count].uint64_t_value(std::string("last_metrics[") + tier_backend_IO_count + std::string("]"))
                                    };
                                    std::ostringstream o;
                                    o << std::fixed << std::setprecision(4)
                                      << ( ((ivy_float) tier_IO_delta) / m_s.subinterval_seconds );

                                    this_metrics[std::string("tier") + t.str() + "_HDT_backend_IOPS"].value = o.str();
                                }
                            }
                        }
                    }


                    uint64_t delta_tracks_cache_to_drive
                    {
                        this_metrics["tracks_cache_to_drive_count"].uint64_t_value("this_metrics[\"tracks_cache_to_drive_count\"]")
                        - last_metrics["tracks_cache_to_drive_count"].uint64_t_value("last_metrics[\"tracks_cache_to_drive_count\"]")
                    };
                    {
                        std::ostringstream o;
                        o   << std::fixed << std::setprecision(4)
                        <<  (
                            ((ivy_float) delta_tracks_cache_to_drive)
                            / m_s.subinterval_seconds
                        );

                        this_metrics["tracks_cache_to_drive_per_second"].value = o.str();
                    }

                    uint64_t delta_tracks_loaded_random_count
                    {
                        this_metrics["tracks_loaded_random_count"].uint64_t_value("this_metrics[\"tracks_loaded_random_count\"]")
                        - last_metrics["tracks_loaded_random_count"].uint64_t_value("last_metrics[\"tracks_loaded_random_count\"]")
                    };
                    {
                        std::ostringstream o;
                        o   << std::fixed << std::setprecision(4)
                        <<  (
                            ((ivy_float) delta_tracks_loaded_random_count)
                            / m_s.subinterval_seconds
                        );

                        this_metrics["tracks_loaded_random_per_second"].value = o.str();
                    }

                    uint64_t delta_tracks_loaded_sequential_count
                    {
                        this_metrics["tracks_loaded_sequential_count"].uint64_t_value("this_metrics[\"tracks_loaded_sequential_count\"]")
                        - last_metrics["tracks_loaded_sequential_count"].uint64_t_value("last_metrics[\"tracks_loaded_sequential_count\"]")
                    };
                    {
                        std::ostringstream o;
                        o   << std::fixed << std::setprecision(4)
                        <<  (
                            ((ivy_float) delta_tracks_loaded_sequential_count)
                            / m_s.subinterval_seconds
                        );

                        this_metrics["tracks_loaded_sequential_per_second"].value = o.str();
                    }


                }
                else
                {
                    // must be mainframe
                }
            } //if ( this_metrics.end() == this_metrics.find("command_device") )
        } // for (auto& instance_pair : this_element_it->second)
    } //if (this_element_it != currentGD.data.end() && last_element_it != lastGD.data.end())


    for (auto& rolluptypepear : m_s.rollups.rollups)
    {
        //{
        //    /*trace*/std::ostringstream o;
        //    o << "filter_perf: for rollup type \"" << rolluptypepear.first << "\":" << std::endl;
        //    log(logfilename,o.str());
        //}

        bool is_all_type { stringCaseInsensitiveEquality("all",rolluptypepear.first) };

        for (auto& rollupinstancepear : rolluptypepear.second->instances)
        {
            if (routine_logging)
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
                                    else ri.subsystem_data_by_subinterval.back().data[summary_element][metric].push(hunh_is_your_head_exploding_yet->second.numeric_value());
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
                                            m_s.rollups.not_participating.back().data[summary_element][metric].push(hunh_is_your_head_exploding_yet->second.numeric_value());
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

                    metric_name = "random_read_IOPS";
                    auto metric_it = ldev_it->second.find(metric_name);
                    if (metric_it == ldev_it->second.end())
                    {
                        std::ostringstream o;
                        o << "rollup_Hitachi_RAID_data() - computing derived metrics - have LDEV metrics but not \"" << metric_name << "\"." << std::endl;
                        o << "Available keys are: "; for (auto& x : ldev_it->second) o << " \"" << x.first << "\" "; o << std::endl;
                        log(m_s.masterlogfile, o.str());
                        throw std::runtime_error(o.str());
                    }
                    ivy_float random_read_IOPS = metric_it->second.sum() / ssd.repetition_factor;

                    metric_name = "random_write_IOPS";
                    metric_it = ldev_it->second.find(metric_name);
                    if (metric_it == ldev_it->second.end())
                    {
                        std::ostringstream o;
                        o << "rollup_Hitachi_RAID_data() - computing derived metrics - have LDEV metrics but not \"" << metric_name << "\"." << std::endl;
                        o << "Available keys are: "; for (auto& x : ldev_it->second) o << " \"" << x.first << "\" "; o << std::endl;
                        log(m_s.masterlogfile, o.str());
                        throw std::runtime_error(o.str());
                    }
                    ivy_float random_write_IOPS = metric_it->second.sum() / ssd.repetition_factor;

                    metric_name = "sequential_read_IOPS";
                    metric_it = ldev_it->second.find(metric_name);
                    if (metric_it == ldev_it->second.end())
                    {
                        std::ostringstream o;
                        o << "rollup_Hitachi_RAID_data() - computing derived metrics - have LDEV metrics but not \"" << metric_name << "\"." << std::endl;
                        o << "Available keys are: "; for (auto& x : ldev_it->second) o << " \"" << x.first << "\" "; o << std::endl;
                        log(m_s.masterlogfile, o.str());
                        throw std::runtime_error(o.str());
                    }
                    ivy_float sequential_read_IOPS = metric_it->second.sum() / ssd.repetition_factor;

                    metric_name = "sequential_write_IOPS";
                    metric_it = ldev_it->second.find(metric_name);
                    if (metric_it == ldev_it->second.end())
                    {
                        std::ostringstream o;
                        o << "rollup_Hitachi_RAID_data() - computing derived metrics - have LDEV metrics but not \"" << metric_name << "\"." << std::endl;
                        o << "Available keys are: "; for (auto& x : ldev_it->second) o << " \"" << x.first << "\" "; o << std::endl;
                        log(m_s.masterlogfile, o.str());
                        throw std::runtime_error(o.str());
                    }
                    ivy_float sequential_write_IOPS = metric_it->second.sum()/ ssd.repetition_factor;

                    metric_name = "read_IOPS_x_response_time_ms";
                    metric_it = ldev_it->second.find(metric_name);
                    if (metric_it == ldev_it->second.end())
                    {
                        std::ostringstream o;
                        o << "rollup_Hitachi_RAID_data() - computing derived metrics - have LDEV metrics but not \"" << metric_name << "\"." << std::endl;
                        o << "Available keys are: "; for (auto& x : ldev_it->second) o << " \"" << x.first << "\" "; o << std::endl;
                        log(m_s.masterlogfile, o.str());
                        throw std::runtime_error(o.str());
                    }
                    ivy_float read_IOPS_x_response_time_ms = metric_it->second.sum()/ ssd.repetition_factor;

                    metric_name = "write_IOPS_x_response_time_ms";
                    metric_it = ldev_it->second.find(metric_name);
                    if (metric_it == ldev_it->second.end())
                    {
                        std::ostringstream o;
                        o << "rollup_Hitachi_RAID_data() - computing derived metrics - have LDEV metrics but not \"" << metric_name << "\"." << std::endl;
                        o << "Available keys are: "; for (auto& x : ldev_it->second) o << " \"" << x.first << "\" "; o << std::endl;
                        log(m_s.masterlogfile, o.str());
                        throw std::runtime_error(o.str());
                    }
                    ivy_float write_IOPS_x_response_time_ms = metric_it->second.sum() / ssd.repetition_factor;

                    metric_name = "random_read_bytes_per_second";
                    metric_it = ldev_it->second.find(metric_name);
                    if (metric_it == ldev_it->second.end())
                    {
                        std::ostringstream o;
                        o << "rollup_Hitachi_RAID_data() - computing derived metrics - have LDEV metrics but not \"" << metric_name << "\"." << std::endl;
                        o << "Available keys are: "; for (auto& x : ldev_it->second) o << " \"" << x.first << "\" "; o << std::endl;
                        log(m_s.masterlogfile, o.str());
                        throw std::runtime_error(o.str());
                    }
                    ivy_float random_read_bytes_per_second = metric_it->second.sum() / ssd.repetition_factor;

                    metric_name = "random_write_bytes_per_second";
                    metric_it = ldev_it->second.find(metric_name);
                    if (metric_it == ldev_it->second.end())
                    {
                        std::ostringstream o;
                        o << "rollup_Hitachi_RAID_data() - computing derived metrics - have LDEV metrics but not \"" << metric_name << "\"." << std::endl;
                        o << "Available keys are: "; for (auto& x : ldev_it->second) o << " \"" << x.first << "\" "; o << std::endl;
                        log(m_s.masterlogfile, o.str());
                        throw std::runtime_error(o.str());
                    }
                    ivy_float random_write_bytes_per_second = metric_it->second.sum() / ssd.repetition_factor;

                    metric_name = "sequential_read_bytes_per_second";
                    metric_it = ldev_it->second.find(metric_name);
                    if (metric_it == ldev_it->second.end())
                    {
                        std::ostringstream o;
                        o << "rollup_Hitachi_RAID_data() - computing derived metrics - have LDEV metrics but not \"" << metric_name << "\"." << std::endl;
                        o << "Available keys are: "; for (auto& x : ldev_it->second) o << " \"" << x.first << "\" "; o << std::endl;
                        log(m_s.masterlogfile, o.str());
                        throw std::runtime_error(o.str());
                    }
                    ivy_float sequential_read_bytes_per_second = metric_it->second.sum() / ssd.repetition_factor;

                    metric_name = "sequential_write_bytes_per_second";
                    metric_it = ldev_it->second.find(metric_name);
                    if (metric_it == ldev_it->second.end())
                    {
                        std::ostringstream o;
                        o << "rollup_Hitachi_RAID_data() - computing derived metrics - have LDEV metrics but not \"" << metric_name << "\"." << std::endl;
                        o << "Available keys are: "; for (auto& x : ldev_it->second) o << " \"" << x.first << "\" "; o << std::endl;
                        log(m_s.masterlogfile, o.str());
                        throw std::runtime_error(o.str());
                    }
                    ivy_float sequential_write_bytes_per_second = metric_it->second.sum() / ssd.repetition_factor;



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
                    {auto& x = ssd.data["LDEV"]["decimal_MB_per_second"];                  x.clear(); x.push(decimal_MB_per_second);}

                    {auto& x = ssd.data["LDEV"]["read_IOPS"];                              x.clear(); x.push(read_IOPS);}
                    {auto& x = ssd.data["LDEV"]["read_decimal_MB_per_second"];             x.clear(); x.push(read_decimal_MB_per_second);}

                    {auto& x = ssd.data["LDEV"]["write_IOPS"];                             x.clear(); x.push(write_IOPS);}
                    {auto& x = ssd.data["LDEV"]["write_decimal_MB_per_second"];            x.clear(); x.push(write_decimal_MB_per_second);}

                    {auto& x = ssd.data["LDEV"]["random_IOPS"];                            x.clear(); x.push(random_IOPS);}
                    {auto& x = ssd.data["LDEV"]["random_decimal_MB_per_second"];           x.clear(); x.push(random_decimal_MB_per_second);}

                    {auto& x = ssd.data["LDEV"]["sequential_IOPS"];                        x.clear(); x.push(sequential_IOPS);}
                    {auto& x = ssd.data["LDEV"]["sequential_decimal_MB_per_second"];       x.clear(); x.push(sequential_decimal_MB_per_second);}

                    {auto& x = ssd.data["LDEV"]["random_read_decimal_MB_per_second"];      x.clear(); x.push(random_read_decimal_MB_per_second);}
                    {auto& x = ssd.data["LDEV"]["random_write_decimal_MB_per_second"];     x.clear(); x.push(random_write_decimal_MB_per_second);}
                    {auto& x = ssd.data["LDEV"]["random_read_blocksize_KiB"];              x.clear(); x.push(random_read_blocksize_KiB);}
                    {auto& x = ssd.data["LDEV"]["random_write_blocksize_KiB"];             x.clear(); x.push(random_write_blocksize_KiB);}
                    {auto& x = ssd.data["LDEV"]["random_blocksize_KiB"];                   x.clear(); x.push(random_blocksize_KiB);}

                    {auto& x = ssd.data["LDEV"]["sequential_read_decimal_MB_per_second"];  x.clear(); x.push(sequential_read_decimal_MB_per_second);}
                    {auto& x = ssd.data["LDEV"]["sequential_write_decimal_MB_per_second"]; x.clear(); x.push(sequential_write_decimal_MB_per_second);}
                    {auto& x = ssd.data["LDEV"]["sequential_read_blocksize_KiB"];          x.clear(); x.push(sequential_read_blocksize_KiB);}
                    {auto& x = ssd.data["LDEV"]["sequential_write_blocksize_KiB"];         x.clear(); x.push(sequential_write_blocksize_KiB);}
                    {auto& x = ssd.data["LDEV"]["sequential_blocksize_KiB"];               x.clear(); x.push(sequential_blocksize_KiB);}

                    {auto& x = ssd.data["LDEV"]["service_time_ms"];                        x.clear(); x.push(service_time_ms);}
                    {auto& x = ssd.data["LDEV"]["read_service_time_ms"];                   x.clear(); x.push(read_service_time_ms);}
                    {auto& x = ssd.data["LDEV"]["write_service_time_ms"];                  x.clear(); x.push(write_service_time_ms);}
                }
            }
        }
    }
    return;
}
