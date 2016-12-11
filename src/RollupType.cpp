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
#include "LUN.h"
#include "LUNpointerList.h"
#include "GatherData.h"
#include "Subsystem.h"
#include "pipe_driver_subthread.h"
#include "WorkloadTracker.h"
#include "WorkloadTrackers.h"
#include "Subinterval_CPU.h"
#include "RollupInstance.h"
#include "RollupType.h"
#include "RollupSet.h"
#include "master_stuff.h"
#include "HM800_tables.h"

extern bool routine_logging, trace_evaluate;

void RollupType::printMe(std::ostream& o)
{
    o << std::endl << std::endl << "RollupType::printMe(ostream& o) for " << attributeNameCombo.attributeNameComboID;

    if (nocsv)
    {
        o << " nocsv=true";
    }
    else
    {
        o << " nocsv=false";
    }

    if (haveQuantityValidation)
    {
        o << ", haveQuantityValidation=true with quantityRequired=" << quantityRequired;
    }
    else
    {
        o << ", haveQuantityValidation=false";
    }

    if (have_maxDroopMaxtoMinIOPS)
    {
        o << ", have_maxDroopMaxtoMinIOPS=true with maxDroopMaxToMinIOPS=" << maxDroopMaxToMinIOPS;
    }
    else
    {
        o << ", have_maxDroopMaxtoMinIOPS=false";
    }

    o << std::endl;

    o << "For std::map<std::string /*rollupInstanceName*/, RollupInstance*> instances:" << std::endl;

    for (auto& pear : instances)
    {
        o << "instances[" << pear.first << "]: " << std::endl;
        pear.second->printMe(o);
    }


    o << std::endl << "For std::map<std::string /*workloadID*/, RollupInstance*> rollupInstanceByWorkloadID:" << std::endl;

    for (auto& pear : rollupInstanceByWorkloadID)
    {
        o << "rollupInstanceByWorkloadID[" << pear.first << "] = " << pear.second-> attributeNameComboID << " : " << pear.second->rollupInstanceID << std::endl;
    }
}


RollupType::RollupType
(
    RollupSet* prs
    , AttributeNameCombo& ANC
    , bool nocsvSection
    , bool quantitySection
    , bool maxIOPSdroopSection
    , int quantity
    , ivy_float maxIOPSdroop
) : pRollupSet(prs)
    , nocsv(nocsvSection)
    , haveQuantityValidation(quantitySection)
    , quantityRequired(quantity)
    , have_maxDroopMaxtoMinIOPS(maxIOPSdroopSection)
    , maxDroopMaxToMinIOPS(maxIOPSdroop)
{
    attributeNameCombo.clone(ANC);
    rebuild();
}

bool RollupType::add_workload_detail_line(std::string& callers_error_message, WorkloadID& wID, IosequencerInput& iI, SubintervalOutput& sO)
{
    std::string error_message;

    auto it = rollupInstanceByWorkloadID.find(toLower(wID.workloadID));
    if (rollupInstanceByWorkloadID.end() == it)
    {
        std::ostringstream o;
        o << "RollupType::add_workload_detail_line() for rollup type " << attributeNameCombo.attributeNameComboID << " - failed - workload ID " << wID.workloadID << "\" was not found in rollupInstanceByWorkloadID.";
        callers_error_message = o.str();
        return false;
    }

    RollupInstance* pRollupInstance = (*it).second;

    if (!pRollupInstance->add_workload_detail_line(error_message, wID, iI, sO))
    {
        std::ostringstream o;
        o << "RollupType::add_workload_detail_line() for rollup type " << attributeNameCombo.attributeNameComboID << " - failed calling  add_workload_detail_line() for Rollup Instance " << (*it).first << " for WorkloadID " << wID.workloadID << " - " << error_message;
        callers_error_message = o.str();
        return false;
    }

    return true;
}


void RollupType::rebuild()
{
//*debug*/std::cout << std::endl << "void RollupType::rebuild() for " << attributeNameCombo.attributeNameComboID << std::endl;
    for (auto& pear : instances) delete pear.second; // RollupInstance*;
    instances.clear();
    rollupInstanceByWorkloadID.clear();

    std::string rollupInstanceKey;

    for (auto& pear : m_s.workloadTrackers.workloadTrackerPointers)
    {
        WorkloadTracker* pWorkloadTracker = pear.second;

//*debug*/{std::ostringstream o; o << std::endl << "RollupType::rebuild() - rollup = \""<< attributeNameCombo.attributeNameComboID << "\" - processing WorkloadTrackerPointers key=\"" << pear.first << "\", pWorkloadTracker->workloadID.workloadID=\"" << pWorkloadTracker->workloadID.workloadID << "\".  WorkloadLUN is " << pWorkloadTracker->workloadLUN.toString() << std::endl; log(m_s.masterlogfile,o.str()); std::cout << o.str();}

        rollupInstanceKey.clear();

        if (stringCaseInsensitiveEquality(std::string("all"), attributeNameCombo.attributeNameComboID))
        {
            rollupInstanceKey=std::string("all");
        }
        else
        {
            // for each attribute name in our AttributeNameCombo

            bool first {true};

            for (auto& attribute_name_token : attributeNameCombo.attributeNames)
            {
                // build up RollupInstanceKey

//*debug*/std::cout << "void RollupType::rebuild() attribute_name_token = " << attribute_name_token << std::endl;

                if ( !first ) rollupInstanceKey.push_back( '+' );
                first=false;

                rollupInstanceKey += pWorkloadTracker->workloadLUN.attribute_value(attribute_name_token);
            }
        }
//*debug*/std::cout << "void RollupType::rebuild() rollupInstanceKey = \"" << rollupInstanceKey << "\"." << std::endl;

        // Now we see if we already have that RollupInstance

        RollupInstance* pRollupInstance;

        auto it = instances.find(toLower(rollupInstanceKey));
        if (instances.end() == it)
        {

//*debug*/std::cout << "void RollupType::rebuild() instances[toLower(rollupInstanceKey=\"" << toLower(rollupInstanceKey) << "\"] = new RollupInstance(this, pRollupSet, attributeNameCombo.attributeNameComboID=\""<< attributeNameCombo.attributeNameComboID << "\", rollupInstanceKey=\""  << rollupInstanceKey << "\")" << std::endl;

            pRollupInstance = new RollupInstance(this, pRollupSet, attributeNameCombo.attributeNameComboID,rollupInstanceKey);
            instances[toLower(rollupInstanceKey)] = pRollupInstance;
        }
        else
        {
//*debug*/std::cout << "void RollupType::rebuild() using existing rollup instance key = \"" << rollupInstanceKey << "\"." << std::endl;
            pRollupInstance = (*it).second;
        }

        pRollupInstance->workloadIDs.workloadIDs.push_back(pWorkloadTracker->workloadID);
        rollupInstanceByWorkloadID[toLower(pWorkloadTracker->workloadID.workloadID)] = pRollupInstance;

        std::string serial = pWorkloadTracker->workloadLUN.attribute_value("serial_number");

        auto subsystem_it = m_s.subsystems.find(serial);

        for (auto& attribute_value_pair : pWorkloadTracker->workloadLUN.attributes)
        {
            pRollupInstance->config_filter[serial][attribute_value_pair.first].insert(attribute_value_pair.second);
        }

        // Need to put the next bit into its own method ...

        // Now we augment the config_filter with indirect configuration element instances.
        // For example, if an LDEV is on an HM800 and we see that we have MPU "0" then we add the appropriate MP_core instances depending on the subsystem model.

        auto sub_model_it = pWorkloadTracker->workloadLUN.attributes.find("sub_model");
        if (sub_model_it != pWorkloadTracker->workloadLUN.attributes.end())
        {
            // Post MP_core / MP_number elements for this workload
            std::string& sub_model = sub_model_it->second;

            if (startsWith(sub_model,"HM800"))
            {
                if (pWorkloadTracker->workloadLUN.contains_attribute_name("MPU"))
                {
                    std::string MPU = pWorkloadTracker->workloadLUN.attribute_value("MPU");

                    auto table_submodel_it = HM800_MPU_map.find(sub_model);
                    if (table_submodel_it == HM800_MPU_map.end())
                    {
                        std::ostringstream o;
                        o << "RollupType::rebuild(), when putting in HM800 MP_core and MP# parameters, did not find sub_model \"" << sub_model << "\" in HM800_MPU_map." << std::endl;
                        std::cout << o.str();
                        m_s.kill_subthreads_and_exit();
                    }

                    auto table_MPU_it = table_submodel_it->second.find(MPU);
                    if (table_MPU_it == table_submodel_it->second.end())
                    {
                        std::ostringstream o;
                        o << "RollupType::rebuild(), when putting in HM800 MP_core and MP# parameters, did not find MPU \"" << MPU << "\" for sub_model \"" << sub_model << "\" in HM800_MPU_map." << std::endl;
                        std::cout << o.str();
                        m_s.kill_subthreads_and_exit();
                    }

                    for (auto& pear : table_MPU_it->second)
                    {
                        std::string& MP_number = pear.first;
                        std::string& MP_core = pear.second;
                        pRollupInstance->config_filter[serial][toLower("MP_number")].insert(MP_number);
                        pRollupInstance->config_filter[serial][toLower("MP_core")].insert(MP_core);
                    }
                }
            }
        }

        extern std::map<std::string, std::vector<std::string>> RAID800_MP_cores_by_MPU;
        //        {
        //            "0",
        //            {
        //                "MPU 00 1MA MP#0(MP#00)",
        //                "MPU 00 1MA MP#4(MP#04)",
        //                "MPU 00 1MA MP#1(MP#01)",
        //                "MPU 00 1MA MP#5(MP#05)",
        //                "MPU 00 1MA MP#2(MP#02)",
        //                "MPU 00 1MA MP#6(MP#06)",
        //                "MPU 00 1MA MP#3(MP#03)",
        //                "MPU 00 1MA MP#7(MP#07)"
        //            }
        //        }

        if ( pWorkloadTracker->workloadLUN.contains_attribute_name("hitachi_product") )
        {
            if ( 0 == std::string("RAID800").compare(pWorkloadTracker->workloadLUN.attribute_value("hitachi_product")) )
            {
                if (pWorkloadTracker->workloadLUN.contains_attribute_name("MPU"))
                {
                    std::string MPU = pWorkloadTracker->workloadLUN.attribute_value("MPU");
                    auto it = RAID800_MP_cores_by_MPU.find(MPU);
                    if (it == RAID800_MP_cores_by_MPU.end())
                    {
                        std::ostringstream o;
                        o << "RollupType::rebuild(), when putting in RAID800 MP_core and MP# parameters, did not MPU \"" << MPU << "\" in RAID800_MP_cores_by_MPU." << std::endl;
                        std::cout << o.str();
                        m_s.kill_subthreads_and_exit();
                    }
                    for (auto& s : it->second)
                    {
                        std::string MP_core = s.substr(0,15);
                        std::string MP_number = s.substr(16,5);
                        pRollupInstance->config_filter[serial][toLower("MP_core")].insert(MP_core);
                        pRollupInstance->config_filter[serial][toLower("MP_number")].insert(MP_number);
                    }
                }
            }
        }

        auto pg_names_it = pWorkloadTracker->workloadLUN.attributes.find("pg_names");
        if (pg_names_it != pWorkloadTracker->workloadLUN.attributes.end())
        {
            std::string s = pg_names_it->second;

            while (s.length() > 0)
            {
                size_t pluspos = s.find('+');

                if (pluspos != std::string::npos)
                {
                    std::string piece = s.substr(0,pluspos);
                    s.erase(0,1+pluspos);
                    if (trace_evaluate)
                    {
                        std::ostringstream o;
                        o << "RollupType " << attributeNameCombo.attributeNameComboID << " RollupInstance " << pRollupInstance->rollupInstanceID
                          << " workloadID " << pWorkloadTracker->workloadID.workloadID << " inserting internal LDEV PG " << piece
                          << " into config_filter[serial=" << serial << "][pg].";
                        log(m_s.masterlogfile,o.str());
                    }
                    pRollupInstance->config_filter[serial][toLower("PG")].insert(piece);
                    continue;
                }

                pRollupInstance->config_filter[serial][toLower("PG")].insert(s);
                if (trace_evaluate)
                {
                    std::ostringstream o;
                    o << "RollupType " << attributeNameCombo.attributeNameComboID << " RollupInstance " << pRollupInstance->rollupInstanceID << " workloadID " << pWorkloadTracker->workloadID.workloadID << " inserting internal LDEV PG " << s << " into config_filter[serial=" << serial << "][pg].";
                    log(m_s.masterlogfile,o.str());
                }
                break;
            }
        }

        //- connect from DP-vols to the PGs in the associated pools.

        // - for each workload LUN in a RollupInstance
        //   	- if it has a "pool_id" attribute (is a DP-Vol, as pool vols can't show as LUNs.)
        //   		- spin through that serial number's config's LDEVs.
        //   			- if config LDEV is a pool vol with a matching poolID
        //   				- for each PG behind that pool vol (up to 4 concatenations);
        //   					- add PG to config_filter

        if ( pWorkloadTracker->workloadLUN.contains_attribute_name("dp_vol") )
        {
            const std::string& pool_id = pWorkloadTracker->workloadLUN.attribute_value("pool_id");

            if (0 == pool_id.length())
            {
                std::ostringstream o;
                o << "In RollupType::rebuild(), when connecting DP vols to their underlying PGs, the workload LUN for " << pWorkloadTracker->workloadID.workloadID
                  << ", which does have a \"dp_vol\" attribute, does not have a non-empty \"pool_id\" attribute." << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile, o.str());
                m_s.kill_subthreads_and_exit();
            }

            // Now we post the pool vol PGs into the config filter - indirect PG
            if (m_s.subsystems.end() != subsystem_it)
            {
                Subsystem* pSubsystem = subsystem_it->second;
                if (0 == std::string("Hitachi RAID").compare(pSubsystem->type()))
                {
                    Hitachi_RAID_subsystem* pRAID = (Hitachi_RAID_subsystem*) pSubsystem;
                    GatherData& configGD = pRAID->configGatherData;
                    for (auto& LDEV_pair : configGD.data["LDEV"])
                    {
                        const std::string& LDEV = LDEV_pair.first;
                        const std::map<std::string,metric_value>& att = LDEV_pair.second;
//                        {
//                            std::ostringstream o;
//                            o << "In RollupType::rebuild(), examining subsystem ldev "  << LDEV << " to see if is a Pool Vol." << std::endl;
//                            std::cout << o.str();
//                            log(m_s.masterlogfile, o.str());
//                        }
                        auto subsystem_pool_vol_it = att.find("Pool_Vol");
                        if (subsystem_pool_vol_it != att.end())
                        {
//                            {
//                                std::ostringstream o;
//                                o << "In RollupType::rebuild(), subsystem ldev "  << LDEV << " has the \"Pool_Vol\" attribute." << std::endl;
//                                std::cout << o.str();
//                                log(m_s.masterlogfile, o.str());
//                            }

                            auto subsystem_pool_id_it = att.find("Pool_ID");
                            if (att.end() != subsystem_pool_id_it)
                            {
                                const std::string& subsystem_pool_id = subsystem_pool_id_it->second.string_value();
                                if (0 == pool_id.compare(subsystem_pool_id))
                                {
                                    // This is a pool vol for our DP-Vol.  Add its
                                    unsigned int concat;
                                    auto concat_it = att.find("PG_concat");
                                    if (att.end() != concat_it)
                                    {
                                        std::istringstream ci(concat_it->second.string_value());
                                        ci >> concat;
                                        for (unsigned int i=0; i < concat; i++)
                                        {
                                            std::ostringstream interleave;
                                            interleave << "PG_concat_" << (1+i);
                                            auto it = att.find(interleave.str());
                                            if (it != att.end())
                                            {
//                                                {
//                                                    std::ostringstream o;
//                                                    o << "RollupType " << attributeNameCombo.attributeNameComboID << " RollupInstance " << pRollupInstance->rollupInstanceID
//                                                      << " workloadID " << pWorkloadTracker->workloadID.workloadID
//                                                      << " inserting pool LDEV " << LDEV << " indirect pool PG " << it->second.string_value()
//                                                      << " into config_filter[serial=" << serial << "][pg]." << std::endl;
//                                                    log(m_s.masterlogfile,o.str());
//                                                    std::cout << o.str();
//                                                }
                                                pRollupInstance->config_filter[serial][toLower("PG")].insert(it->second.string_value());
                                            }
                                            else
                                            {
                                                std::ostringstream o;
                                                o << "RollupType::rebuild() - subsystem pool volume " << serial << " " << LDEV << " has " << concat << " PG concatenations but does not have parity group attribute " << interleave.str() << std::endl;
                                                std::cout << o.str();
                                                log (m_s.masterlogfile, o.str());
                                                m_s.kill_subthreads_and_exit();
                                            }
                                        }
                                    }
                                    else
                                    {
                                        std::ostringstream o;
                                        o << "In RollupType::rebuild(), subsystem " << serial << " Pool Vol ldev "  << LDEV << " does not have the \"PG_concat\" attribute." << std::endl;
                                        std::cout << o.str();
                                        log(m_s.masterlogfile, o.str());
                                        m_s.kill_subthreads_and_exit();
                                    }
                                }
//                                else
//                                {
//                                    std::ostringstream o;
//                                    o << "In RollupType::rebuild(), subsystem " << serial << " Pool_Vol ldev "  << LDEV << " is for Pool_ID = " << subsystem_pool_id << ", which is not the Pool_ID we are looking for, which is " << pool_id << "." << std::endl;
//                                    std::cout << o.str();
//                                    log(m_s.masterlogfile, o.str());
//                                }
                            }
                            else
                            {
                                std::ostringstream o;
                                o << "In RollupType::rebuild(), subsystem " << serial << " Pool_Vol ldev "  << LDEV << " does not have the \"Pool_ID\" attribute." << std::endl;
                                std::cout << o.str();
                                log(m_s.masterlogfile, o.str());
                                m_s.kill_subthreads_and_exit();
                            }
                        }
//                        else
//                        {
//                            std::ostringstream o;
//                            o << "In RollupType::rebuild(), subsystem ldev "  << LDEV << " does not have the \"Pool_Vol\" attribute." << std::endl;
//                            std::cout << o.str();
//                            log(m_s.masterlogfile, o.str());
//                        }
                        // end - for each LDEV pair in subsystem config data
                    }
                }
                else
                {
                    std::ostringstream o;
                    o << "In RollupType::rebuild(), when connecting DP vols to their underlying PGs, the workload LUN for " << pWorkloadTracker->workloadID.workloadID
                      << " is for the same subsystem serial number " << serial << " as the DP vol, but this subsystem type is not \"Hitachi RAID\"." << std::endl;
                    std::cout << o.str();
                    log(m_s.masterlogfile, o.str());
                    m_s.kill_subthreads_and_exit();
                }
            }
            else
            {
                std::ostringstream o;
                o << "In RollupType::rebuild(), when connecting DP vols to their underlying PGs for the DP-Vol workload LUN for " << pWorkloadTracker->workloadID.workloadID
                  << ", did not find a subsystem serial number " << serial << ".." << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile, o.str());
                m_s.kill_subthreads_and_exit();

            }
        }

    } //for (auto& pear : m_s.workloadTrackers.workloadTrackerPointers)

    for (auto& pear : instances) pear.second->rebuild_test_config_thumbnail();
}


bool RollupType::makeMeasurementRollup(std::string callers_error_message, int firstMeasurementIndex, int lastMeasurementIndex)
{

//*debug*/ { std::ostringstream o; o << "RollupType::makeMeasurementRollup[" << attributeNameCombo.attributeNameComboID << "](, " << firstMeasurementIndex << " ," << lastMeasurementIndex << ")" << std::endl; std::cout << o.str(); log(m_s.masterlogfile, o.str());}

    measurementRollupAverageIOPSRunningStat.clear();

    std::string my_error_message;
    for (auto& pear : instances)
    {
        RollupInstance* pRollupInstance;
        pRollupInstance = pear.second;
//*debug*/ { std::ostringstream o; o << "RollupType::makeMeasurementRollup[" << attributeNameCombo.attributeNameComboID << "]() for instance " << pRollupInstance->attributeNameComboID << '=' << pRollupInstance->rollupInstanceID << std::endl; std::cout << o.str(); log(m_s.masterlogfile, o.str());}
        if(!pRollupInstance->makeMeasurementRollup(my_error_message, firstMeasurementIndex, lastMeasurementIndex))
        {
            std::ostringstream o;
            o << "RollupType::makeMeasurementRollup() - failure calling makeMeasurementRollup() for RollupInstance \"" << pear.first << "\" - " << my_error_message;
        }

        ivy_float durSec = pRollupInstance->measurementRollup.durationSeconds();

//*debug*/ { std::ostringstream o; o << "RollupType::makeMeasurementRollup() - pRollupInstance->measurementRollup.durationSeconds() = " << durSec << std::endl; std::cout << o.str(); log(m_s.masterlogfile, o.str());}

        if (durSec <=0)
        {
            std::ostringstream o;
            o << "RollupType::makeMeasurementRollup() - aborting - m_s.measurementRollup.durationSeconds() returned "
              << durSec << " for a start time of \""
              << pRollupInstance->measurementRollup.startIvytime.format_as_datetime_with_ns() << " and ending at "
              << pRollupInstance->measurementRollup.endIvytime.format_as_datetime_with_ns() << ".";
            callers_error_message=o.str();
            return false;
        }


        ivy_float overallIOPS = pRollupInstance->measurementRollup.outputRollup.u.a.service_time.overall().count() / durSec;

//*debug*/ { std::ostringstream o; o << "RollupType::makeMeasurementRollup() - ivy_float overallIOPS = pRollupInstance->measurementRollup.outputRollup.u.a.service_time.overall().count() / durSec = " << overallIOPS << std::endl; std::cout << o.str(); log(m_s.masterlogfile, o.str());}

        measurementRollupAverageIOPSRunningStat.push( overallIOPS );

        // OK, this is slightly wacky, tracking BytesPerSecond instead of decimal MB/s.  Reason: impossible to misinterpret units as binary or decimal.
        ivy_float overallBytesPerSec = pRollupInstance->measurementRollup.outputRollup.u.a.bytes_transferred.overall().sum() / durSec;

//*debug*/ { std::ostringstream o; o << "RollupType::makeMeasurementRollup() - ivy_float overallBytesPerSec = pRollupInstance->measurementRollup.outputRollup.u.a.bytes_transferred.overall().sum() / durSec = " << overallBytesPerSec << std::endl; std::cout << o.str(); log(m_s.masterlogfile, o.str());}

        measurementRollupAverageBytesPerSecRunningStat.push( overallBytesPerSec );

        if (pRollupInstance->measurementRollup.outputRollup.u.a.bytes_transferred.overall().count() >0)
        {
            measurementRollupAverageBlocksizeBytesRunningStat.push
            (
                pRollupInstance->measurementRollup.outputRollup.u.a.bytes_transferred.overall().sum()
                / pRollupInstance->measurementRollup.outputRollup.u.a.bytes_transferred.overall().count()
            );
        }

        measurementRollupAverageServiceTimeRunningStat.push( pRollupInstance->measurementRollup.outputRollup.u.a.service_time.overall().avg() );

        if (pRollupInstance->measurementRollup.outputRollup.u.a.response_time.overall().count() >0)
        {
            measurementRollupAverageResponseTimeRunningStat.push( pRollupInstance->measurementRollup.outputRollup.u.a.response_time.overall().avg() );
        }
    }

    return true;

}

bool RollupType::passesDataVariationValidation()
{
    if ( 0 == instances.size() ) return false;

    if (haveQuantityValidation)
    {
        if (quantityRequired != instances.size())
        {
            return false;
        }
    }

    if (have_maxDroopMaxtoMinIOPS)
    {
        ivy_float IOPSdroop = 1.0 - ( measurementRollupAverageIOPSRunningStat.min() / measurementRollupAverageIOPSRunningStat.max() );

        if (IOPSdroop >= maxDroopMaxToMinIOPS)
        {
            return false;
        }
    }

    return true;
}

/*static*/ std::string RollupType::getDataValidationCsvTitles()
{
    std::ostringstream o;
    o 	<< "Have Quantity Validation,Quantity Validation Value,Rollup Instance Count,Quantity Validation Failure,"
        << "Have Max Droop Max To Min Validation,Max Droop Max To Min Validation Value,Average Total IOPS droop Max To Min,Max Droop Max To Min Validation Failure"
        << std::endl;
    return o.str();
}


std::string RollupType::getDataValidationCsvValues()
{
    std::ostringstream o;

    o << (haveQuantityValidation?"true":"false") << ',' << (haveQuantityValidation?quantityRequired:0) << ',' << instances.size() << ',' << ( (haveQuantityValidation && (quantityRequired != instances.size())) ? "true" : "false" );

    ivy_float IOPSdroop = 1.0 - ( measurementRollupAverageIOPSRunningStat.min() / measurementRollupAverageIOPSRunningStat.max() );

    o << ',' << (have_maxDroopMaxtoMinIOPS?"true":"false") << ',' << (have_maxDroopMaxtoMinIOPS?maxDroopMaxToMinIOPS:0.0) << ',' << IOPSdroop
      << ',' << ( (have_maxDroopMaxtoMinIOPS && (maxDroopMaxToMinIOPS < IOPSdroop)) ? "true" : "false" );

    return o.str();
}
















