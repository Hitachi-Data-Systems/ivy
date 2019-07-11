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
#include <stdexcept>
#include <sys/stat.h>
#include <cstring>

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
#include "RollupType.h"
#include "RollupSet.h"
#include "ivy_engine.h"

extern bool routine_logging, trace_evaluate;

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
    auto it = rollupInstanceByWorkloadID.find(toLower(wID.workloadID));
    if (rollupInstanceByWorkloadID.end() == it)
    {
        std::ostringstream o;
        o << "RollupType::add_workload_detail_line() for rollup type " << attributeNameCombo.attributeNameComboID << " - failed - workload ID " << wID.workloadID << "\" was not found in rollupInstanceByWorkloadID.";
        callers_error_message = o.str();
        return false;
    }

    RollupInstance* pRollupInstance = (*it).second;

    auto retval = pRollupInstance->add_workload_detail_line(wID, iI, sO);
    if (!retval.first)
    {
        std::ostringstream o;
        o << "RollupType::add_workload_detail_line() for rollup type " << attributeNameCombo.attributeNameComboID
            << " - failed calling  add_workload_detail_line() for Rollup Instance " << (*it).first
            << " for WorkloadID " << wID.workloadID << " - " << retval.second;
        callers_error_message = o.str();
        return false;
    }

    return true;
}


void RollupType::rebuild()
{
    for (auto& pear : instances) delete pear.second; // RollupInstance*;
    instances.clear();
    rollupInstanceByWorkloadID.clear();

    std::string rollupInstanceKey;

    for (auto& pear : m_s.workloadTrackers.workloadTrackerPointers)
    {
        WorkloadTracker* pWorkloadTracker = pear.second;

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

                if ( !first ) rollupInstanceKey.push_back( '+' );
                first=false;

                rollupInstanceKey += pWorkloadTracker->workloadLUN.attribute_value(attribute_name_token);
            }
        }

        // Now we see if we already have that RollupInstance

        RollupInstance* pRollupInstance;

        auto it = instances.find(toLower(rollupInstanceKey));
        if (instances.end() == it)
        {
            pRollupInstance = new RollupInstance(this, pRollupSet, attributeNameCombo.attributeNameComboID,rollupInstanceKey);
            instances[toLower(rollupInstanceKey)] = pRollupInstance;
        }
        else
        {
            pRollupInstance = (*it).second;
        }

        pRollupInstance->workloadIDs.workloadIDs.push_back(pWorkloadTracker->workloadID);
        rollupInstanceByWorkloadID[toLower(pWorkloadTracker->workloadID.workloadID)] = pRollupInstance;

        std::string serial = pWorkloadTracker->workloadLUN.attribute_value("serial number");

        auto subsystem_it = m_s.subsystems.find(serial);

        if (subsystem_it != m_s.subsystems.end())
        {

            for (auto& pear : pWorkloadTracker->workloadLUN.attributes)
            {
                pRollupInstance->config_filter[serial][toLower(pear.second.first)].insert(pear.second.second);
            }

            if (pWorkloadTracker->workloadLUN.contains_attribute_name("MP core set"))
            {
                std::string s = pWorkloadTracker->workloadLUN.attribute_value("MP core set");

                unsigned int cursor = 0;

                while (true)
                {
                    auto rc = get_next_field(s,cursor,'+');

                    if (!rc.first) break;

                    if (routine_logging)
                    {
                        std::ostringstream o;
                        o << "RollupType " << attributeNameCombo.attributeNameComboID << " RollupInstance " << pRollupInstance->rollupInstanceID
                          << " workloadID " << pWorkloadTracker->workloadID.workloadID << " inserting MP core " << rc.second
                          << " into config_filter[serial=" << serial << "][MP core].";
                        log(m_s.masterlogfile,o.str());
                    }
                    pRollupInstance->config_filter[serial][toLower("MP core")].insert(rc.second);
                }
            }
            else
            {
                std::ostringstream o;
                o << "RollupType::rebuild() for WorkloadTracker " << pWorkloadTracker->workloadID << " did not find attribute \"" << "MP core set" << "\"." << std::endl
                    << "This Workload\'s LUN has attributes " <<  pWorkloadTracker->workloadLUN.toString() << std::endl;
                log(m_s.masterlogfile, o.str());
            }

            if (pWorkloadTracker->workloadLUN.contains_attribute_name("MP number set"))
            {
                std::string s = pWorkloadTracker->workloadLUN.attribute_value("MP number set");

                unsigned int cursor = 0;

                while (true)
                {
                    auto rc = get_next_field(s,cursor,'+');

                    if (!rc.first) break;

                    if (routine_logging)
                    {
                        std::ostringstream o;
                        o << "RollupType " << attributeNameCombo.attributeNameComboID << " RollupInstance " << pRollupInstance->rollupInstanceID
                          << " workloadID " << pWorkloadTracker->workloadID.workloadID << " inserting MP number " << rc.second
                          << " into config_filter[serial=" << serial << "][MP number].";
                        log(m_s.masterlogfile,o.str());
                    }
                    pRollupInstance->config_filter[serial][toLower("MP number")].insert(rc.second);
                }
            }

            if (pWorkloadTracker->workloadLUN.contains_attribute_name("PG names"))
            {
                std::string s = pWorkloadTracker->workloadLUN.attribute_value("PG names");

                unsigned int cursor = 0;

                while (true)
                {
                    auto rc = get_next_field(s,cursor,'/');

                    if (!rc.first) break;

                    if (routine_logging)
                    {
                        std::ostringstream o;
                        o << "RollupType " << attributeNameCombo.attributeNameComboID << " RollupInstance " << pRollupInstance->rollupInstanceID
                          << " workloadID " << pWorkloadTracker->workloadID.workloadID << " inserting internal LDEV PG " << rc.second
                          << " into config_filter[serial=" << serial << "][pg].";
                        log(m_s.masterlogfile,o.str());
                    }
                    pRollupInstance->config_filter[serial][toLower("PG")].insert(rc.second);
                }
            }
        }

        if (routine_logging)
        {
            std::ostringstream o;
            o << "RollupInstance " << pRollupInstance->attributeNameComboID << "=" << pRollupInstance->rollupInstanceID << " config_filter_contents() = " << pRollupInstance->config_filter_contents() << std::endl;
            log(m_s.masterlogfile,o.str());
        }
    } //for (auto& pear : m_s.workloadTrackers.workloadTrackerPointers)

    for (auto& pear : instances) pear.second->rebuild_test_config_thumbnail();
}


std::pair<bool,std::string> RollupType::makeMeasurementRollup()
{
    measurementRollupAverageIOPSRunningStat.clear();

    measurement& m = m_s.current_measurement();

    long double duration = m.duration().getlongdoubleseconds();

    std::string my_error_message;
    for (auto& pear : instances)
    {
        RollupInstance* pRollupInstance;
        pRollupInstance = pear.second;

        auto retval = pRollupInstance->makeMeasurementRollup();
        if(!retval.first)
        {
            std::ostringstream o;
            o << "RollupType::makeMeasurementRollup() - failure calling makeMeasurementRollup() for RollupInstance \"" << pear.first << "\" - " << retval.second;
            return std::make_pair(false,o.str());
        }

        {
            SubintervalOutput& mro = pRollupInstance->current_measurement().measurementRollup.outputRollup;

            ivy_float overallIOPS = mro.u.a.service_time.overall().count() / duration;

            measurementRollupAverageIOPSRunningStat.push( overallIOPS );

            // OK, this is slightly wacky, tracking BytesPerSecond instead of decimal MB/s.  Reason: impossible to misinterpret units as binary or decimal.
            ivy_float overallBytesPerSec = mro.u.a.bytes_transferred.overall().sum() / duration;

            measurementRollupAverageBytesPerSecRunningStat.push( overallBytesPerSec );

            if (mro.u.a.bytes_transferred.overall().count() >0)
            {
                measurementRollupAverageBlocksizeBytesRunningStat.push
                (
                    mro.u.a.bytes_transferred.overall().sum()
                    / mro.u.a.bytes_transferred.overall().count()
                );
            }

            measurementRollupAverageServiceTimeRunningStat.push( mro.u.a.service_time.overall().avg() );

            if (mro.u.a.response_time.overall().count() >0)
            {
                measurementRollupAverageResponseTimeRunningStat.push( mro.u.a.response_time.overall().avg() );
            }
        }

    }

    return std::make_pair(true,"");

}

std::pair<bool,std::string> RollupType::passesDataVariationValidation()
{
    if ( 0 == instances.size() )
    {
        std::ostringstream o;
        o << "[internal programming error - RollupType " << attributeNameCombo.attributeNameComboID << " has no instances.]";
        return std::make_pair(false,o.str());
    }

    std::ostringstream o;
    bool retval=true;

    if (haveQuantityValidation)
    {
        if (quantityRequired != instances.size())
        {
            o << "[rollup " << attributeNameCombo.attributeNameComboID << " quantity validation failure - require " << quantityRequired << " but saw " << instances.size() << " instances]";
            retval=false;
        }
    }

    if (have_maxDroopMaxtoMinIOPS)
    {
        ivy_float IOPSdroop = 1.0 - ( measurementRollupAverageIOPSRunningStat.min() / measurementRollupAverageIOPSRunningStat.max() );

        if (IOPSdroop >= maxDroopMaxToMinIOPS)
        {
            o << "[rollup " << attributeNameCombo.attributeNameComboID << " max IOPS drop validation failure - IOPS droop was " << (100.*IOPSdroop) << "% but maxDroop = " << (100.*maxDroopMaxToMinIOPS) << "%.]";
            retval=false;
        }
    }

    return std::make_pair(retval,o.str());
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

void RollupType::make_step_subfolder()
{
    step_subfolder_name = m_s.stepFolder + std::string("/") + attributeNameCombo.attributeNameComboID;

    if (mkdir(step_subfolder_name.c_str(),
          S_IRWXU  // r,w,x user
        | S_IRWXG  // r,w,x group
        | S_IRWXO  // r,w,x other
    ))
    {
        std::ostringstream o;
        o << std::endl << "Writhe and fail, gasping:   Couldn\'t make by-subinterval rollup subfolder "
            << "\"" << step_subfolder_name << "\" within step folder \"" << m_s.stepFolder << "\""
            << " - " << strerror(errno) << std::endl;
        log(m_s.masterlogfile,o.str());
        m_s.kill_subthreads_and_exit();
        exit(-1);
    }

    for (auto& pear : instances)
    {
        pear.second->set_by_subinterval_filename();

        pear.second->print_by_subinterval_header();
    }
}

void RollupType::print_measurement_summary_csv_line(unsigned int measurement_index)
{
    struct stat struct_stat;

    const std::string& rollupTypeName = attributeNameCombo.attributeNameComboID;

    if (routine_logging)
    {
        std::ostringstream o;
        o << std::endl << "Making csv files for " << rollupTypeName << "=" << attributeNameCombo.attributeNameComboID << "." << std::endl;
        log(m_s.masterlogfile,o.str());
    }

    // Make the RollupInstance's RollupType subfolder if necessary
    measurementRollupFolder = m_s.testFolder + std::string("/") + rollupTypeName;

    if(0==stat(measurementRollupFolder.c_str(),&struct_stat))
    {
        // folder name already exists
        if(!S_ISDIR(struct_stat.st_mode))
        {
            std::ostringstream o;
            o << "At line " << __LINE__ << " of source code file " << __FILE__ << " - ";
            o << std::endl << "writhe and fail, gasping measurement rollup folder \"" << measurementRollupFolder << "\" exists but is not a directory.   Uuuugg." << std::endl;
            log(m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
            exit(-1);
        }
    }
    else if (mkdir(measurementRollupFolder.c_str(),
          S_IRWXU  // r,w,x user
        | S_IRWXG  // r,w,x group
        | S_IRWXO  // r,w,x other
    ))
    {
        std::ostringstream o;
        o << "At line " << __LINE__ << " of source code file " << __FILE__ << " - ";
        o << std::endl << "writhe and fail, gasping couldn\'t make measurement rollup folder \"" << measurementRollupFolder << "\".   Uuuugg." << std::endl;
        log(m_s.masterlogfile,o.str());
        m_s.kill_subthreads_and_exit();
        exit(-1);
    }






    // print data validation csv headerline if necessary

    {
        // but first figure out the data validation csv filename
        std::ostringstream data_validation_filename_stream;

        data_validation_filename_stream << measurementRollupFolder << "/" << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(m_s.testName)
                                        << '.' << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(rollupTypeName) << ".data_validation.csv";

        measurement_rollup_data_validation_csv_filename = data_validation_filename_stream.str();
    }

    if ( 0 != stat(measurement_rollup_data_validation_csv_filename.c_str(),&struct_stat))
    {
        // we need to print the header line.

        std::ostringstream o;

        o << "Test Name,Step Number,Step Name,Rollup," << RollupType::getDataValidationCsvTitles();

        fileappend( measurement_rollup_data_validation_csv_filename, o.str() );
    }

    // print data validation csv data line

    {
        std::ostringstream o;
        o 	<< m_s.testName << ',' << m_s.stepNNNN << ',' << m_s.stepName
            << ',' << attributeNameCombo.attributeNameComboID
            << ',' << getDataValidationCsvValues();

        {
            std::ostringstream g; g << quote_wrap_csvline_LDEV_PG_leading_zero_number(o.str(), m_s.formula_wrapping) << std::endl;
            fileappend(measurement_rollup_data_validation_csv_filename,g.str());
        }

    }

    for (auto& peer : instances) peer.second->print_measurement_summary_csv_line(measurement_index);
}









