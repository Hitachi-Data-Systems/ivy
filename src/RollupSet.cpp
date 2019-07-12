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

using namespace std;

#include "ivytime.h"
#include "ivydefines.h"
#include "ivyhelpers.h"
#include "LUN.h"
#include "AttributeNameCombo.h"
#include "IosequencerInputRollup.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "IosequencerInput.h"
#include "IosequencerInputRollup.h"
#include "SubintervalOutput.h"
#include "SubintervalRollup.h"
#include "SequenceOfSubintervalRollup.h"
#include "WorkloadID.h"
#include "ListOfWorkloadIDs.h"
#include "RollupInstance.h"
#include "AttributeNameCombo.h"
#include "RollupType.h"
#include "ivylinuxcpubusy.h"
#include "LUNpointerList.h"
#include "GatherData.h"
#include "Subsystem.h"
#include "pipe_driver_subthread.h"
#include "WorkloadTracker.h"
#include "WorkloadTrackers.h"
#include "Subinterval_CPU.h"
#include "RollupSet.h"
#include "ivy_engine.h"

void RollupSet::rebuild()
{
    not_participating.clear();

    for (auto& pear : rollups) pear.second->rebuild();
}



std::string RollupSet::debugListRollups()
{
    std::ostringstream o;

    o << "Rollups are:";

    for (auto& pear : rollups)
    {
        o << " <key = \"" << pear.first << "\", object\'s internal name = \"" << pear.second->getAttributeNameCombo().attributeNameComboID << "\">";
    }

    return o.str();
}


std::pair<bool,std::string> RollupSet::initialize()
{
    if (initialized)
    {
        return std::make_pair(false,"RollupSet::initialize() called on an already-initialized RollupSet object. - return false and doing nothing.");
    }

    auto rc = addRollupType("all", false, false, false, 1, 1.0);

    if (!rc.first)
    {
        std::ostringstream o;
        o << "RollupSet::initialize() - addRollupType(\"all\") failed saying: " << rc.second;
        return std::make_pair(false,o.str());
    }

    return std::make_pair(true,"");
}


std::pair<bool,std::string> RollupSet::add_workload_detail_line(WorkloadID& wID, IosequencerInput& iI, SubintervalOutput& sO)
{
    std::string error_message;

    for (auto& pear : rollups)
    {
        RollupType* pRollupType = pear.second;

        if (!pRollupType->add_workload_detail_line(error_message, wID, iI, sO))
        {
            std::ostringstream o;
            o << "RollupSet::add_workload_detail_line() for WorkloadID " << wID.workloadID << " add_workload_detail_line() for " << pear.first << " failed - " << error_message;
            return std::make_pair(false,o.str());
        }
    }

    return std::make_pair(true,"");
}


std::pair<bool,std::string> RollupSet::addRollupType
(
      std::string attributeNameComboText
    , bool nocsvSection
    , bool quantitySection
    , bool maxDroopMaxtoMinIOPSSection
    , ivy_int quantity
    , ivy_float maxDroop
)
// returns true if the rollup was created successfully.
// [CreateRollup] Serial_Number+LDEV
// [CreateRollup] host               [nocsv] [quantity] 16 [maxDroopMaxtoMinIOPS] 25 %
// error_message is clear()ed upon entry and is set with an error message if it returns false
{
    AttributeNameCombo aNC;

    std::pair<bool,std::string> retval = aNC.set(attributeNameComboText, &(m_s.TheSampleLUN));

    if (!retval.first)
    {
        std::ostringstream o;
        o << "RollupSet::addRollupType - invalid attribute name combo - \"" << attributeNameComboText << "\" - " << retval.second;
        return std::make_pair(false,o.str());
    }

    if (!aNC.isValid)
    {
        std::ostringstream o;
        o << "RollupSet::addRollupType - no such attribute name combo - \"" << attributeNameComboText << "\" - " << retval.second;
        return std::make_pair(false,o.str());
    }

    // OK, all the fields are validated, let's build the rollup
    RollupType* pRollupType = new RollupType
    (
          this
        , aNC
        , nocsvSection
        , quantitySection
        , maxDroopMaxtoMinIOPSSection
        , quantity
        , maxDroop
    );

    if (!pRollupType->isValid())
    {
        std::ostringstream o;
        o << "- [CreateRollup] Dreadfully sorry.  Seem to have had a bit of a cock-up in the back office.  New RollupType wasn\'t valid.  AAAaaaarrgggghhhh.   " << std::endl
            << std::endl << "Source code reference: function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
        std::cout << o.str() << std::endl;
        delete pRollupType;
        return std::make_pair(false,o.str());
    }

    rollups[toLower(aNC.attributeNameComboID)] = pRollupType;

    return std::make_pair(true,"");
}

class RollupInstance;

RollupInstance* RollupSet::get_all_equals_all_instance()
{
    auto type_it = rollups.find("all");

    if (type_it == rollups.end())
    {
        std::string msg {"Run time error: unable to find \"all\" RollupType"};
        std::cout << msg << std::endl;
        log(m_s.masterlogfile,msg);
        m_s.kill_subthreads_and_exit();
    }

    auto instance_it = type_it->second->instances.find("all");

    if (instance_it == type_it->second->instances.end())
    {
        std::string msg {"Run time error: unable to find \"all\" RollupInstance within existing RollupType \"all\"."};
        std::cout << msg << std::endl;
        log(m_s.masterlogfile,msg);
        m_s.kill_subthreads_and_exit();
    }
    return instance_it->second;
}

std::pair<bool,std::string> RollupSet::deleteRollup(std::string attributeNameComboText)
{
    AttributeNameCombo anc;

    auto rc = anc.set(attributeNameComboText,&m_s.TheSampleLUN);
    if (!rc.first)
    {
        std::ostringstream o;
        o << "deleteRollup(\"" << attributeNameComboText << "\") - failed - " << rc.second;
        return std::make_pair(false,o.str());
    }

    auto it = rollups.find(toLower(anc.attributeNameComboID));
    if (it == rollups.end())
    {
        std::ostringstream o;
        o << "deleteRollup(\"" << attributeNameComboText << "\") - no such RollupType.";
        return std::make_pair(false,o.str());
    }
    rollups.erase(it);
    return std::make_pair(true,"");
}


void RollupSet::resetSubintervalSequence()
{
    subintervalIndex = -1;

    for  (auto& pear : rollups)
    {
        RollupType* pRollupType	= pear.second;

        pRollupType->rebuild();
    }

    for (auto& type_pear : m_s.rollups.rollups)
    {
        for (auto& instance_pair : type_pear.second->instances)
        {
            instance_pair.second->focus_metric_vector.clear();
        }
    }
}


void RollupSet::startNewSubinterval(ivytime start, ivytime end)
{
    for  (auto& pear : rollups)
    {
        RollupType* pRollupType	= pear.second;

        for (auto& peach : pRollupType->instances)
        {
            RollupInstance* pRollupInstance = peach.second;
            pRollupInstance->subintervals.sequence.emplace_back(start,end);
        }
    }
    subintervalIndex++;
}


std::pair<bool,std::string> RollupSet::makeMeasurementRollup()
{
    measurement& m = m_s.current_measurement();

    {
        std::ostringstream o;
        o << "Making measurement rollup from subinterval " << m.firstMeasurementIndex << " to " << m.lastMeasurementIndex << "." << std::endl;
        log(m_s.masterlogfile, o.str());
        std::cout << o.str();
        log(m_s.masterlogfile, o.str());
    }

    std::string my_message;

    for (auto& pear : rollups)
    {
        RollupType* pRollupType = pear.second;

        auto rv = pRollupType->makeMeasurementRollup();

        if (!rv.first)
        {
            std::ostringstream o;
            o << "RollupSet::makeMeasurementRollup() - failed in makeMeasurementRollup() for RollupType \"" << pear.first << "\" - " << rv.second;
            return std::make_pair(false,o.str());
        }
    }

    if (not_participating.size() > 0)
    {
        if
        (
            not_participating.size() == 0
            || m.firstMeasurementIndex < 0
            || m.firstMeasurementIndex > m.lastMeasurementIndex
            || m.lastMeasurementIndex >= ((int)not_participating.size())
        )
        {
            std::ostringstream o;
            o   << "RollupSet::makeMeasurementRollup() - Invalid first (" << m.firstMeasurementIndex << ") and last (" << m.lastMeasurementIndex << ") measurement period indices "
                << "for \"not_participating\", whose size is " << not_participating.size() << " subintervals." << std::endl << "There must be at least one subinterval of data." << std::endl;
            log(m_s.masterlogfile, o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }

        not_participating_measurement.emplace_back();

        for (int i=m.firstMeasurementIndex; i <= m.lastMeasurementIndex; i++)
        {
            not_participating_measurement.back().addIn(not_participating[i]);
        }
    }

    return std::make_pair(true,"");
}


std::pair<bool,std::string> RollupSet::passesDataVariationValidation()
{
    std::string s {};
    bool retval {true};

    if (rollups.size() == 0)
    {
        std::ostringstream o;
        o << "[internal programming error - RollupSet has no RollupType instances, not even the \"all\" rollup.]";
        return std::make_pair(false,o.str());
    }

    for (auto& pear : rollups)
    {
        std::pair<bool,std::string> r = pear.second->passesDataVariationValidation();
        if (!r.first) { retval = false; s += r.second; }
    }

    return std::make_pair(retval,s);
}

void RollupSet::print_measurement_summary_csv_line(unsigned int measurement_index)
{
    for (auto& pear : rollups)  // For each RollupType
    {
        pear.second->print_measurement_summary_csv_line(measurement_index);
    }

}
