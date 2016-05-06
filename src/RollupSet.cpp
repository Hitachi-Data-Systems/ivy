//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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
#include "IogeneratorInputRollup.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "IogeneratorInput.h"
#include "IogeneratorInputRollup.h"
#include "SubintervalOutput.h"
#include "SubintervalRollup.h"
#include "SequenceOfSubintervalRollup.h"
#include "WorkloadID.h"
#include "ListOfWorkloadIDs.h"
#include "RollupInstance.h"
#include "AttributeNameCombo.h"
#include "RollupType.h"
#include "ivylinuxcpubusy.h"
#include "Select.h"
#include "LUNpointerList.h"
#include "GatherData.h"
#include "Subsystem.h"
#include "pipe_driver_subthread.h"
#include "WorkloadTracker.h"
#include "WorkloadTrackers.h"
#include "Subinterval_CPU.h"
#include "RollupSet.h"
#include "master_stuff.h"

void RollupSet::rebuild()
{
    not_participating.clear();

    for (auto& pear : rollups) pear.second->rebuild();
}

void RollupSet::printMe(std::ostream& o)
{
    o << "RollupSet::printMe()" <<std::endl;

    ivytime duration = measurement_starting_ending_times.second - measurement_starting_ending_times.first;

    o << "Measurement start time " << measurement_starting_ending_times.first.format_as_datetime_with_ns()
      << " to " << measurement_starting_ending_times.second.format_as_datetime_with_ns()
      << " duration " << duration.format_as_duration_HMMSSns()
      << std::endl;

    for (unsigned int i=0; i < starting_ending_times.size(); i++)
    {
        duration = starting_ending_times[i].second - starting_ending_times[i].first;
        o << "subinterval " << i << " start time " << starting_ending_times[i].first.format_as_datetime_with_ns()
          << " to " << starting_ending_times[i].second.format_as_datetime_with_ns()
          << " duration " << duration.format_as_duration_HMMSSns()
          << std::endl;
    }

    o << "rollups.size() = " << rollups.size() << std::endl;

    for (auto& pear : rollups)
    {
        o << std::endl << "for rollups key = \"" << pear.first << "\":" << std::endl;
        pear.second->printMe(o);
    }

    return;
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


bool RollupSet::initialize(std::string& callers_error_message)
{
    callers_error_message.clear();

    if (initialized)
    {
        callers_error_message = "RollupSet::initialize() called on an already-initialized RollupSet object. - return false and doing nothing.";
        return false;
    }

    std::string my_error_message;

    if (!addRollupType(my_error_message,"all", false, false, false, "", 1, 1.0))
    {
        callers_error_message = "RollupSet::initialize() - addRollupType(\"all\") failed saying: " + my_error_message;
        return false;
    }

    return true;
}


bool RollupSet::add_workload_detail_line(std::string& callers_error_message, WorkloadID& wID, IogeneratorInput& iI, SubintervalOutput& sO)
{
    std::string error_message;

    for (auto& pear : rollups)
    {
        RollupType* pRollupType = pear.second;

        if (!pRollupType->add_workload_detail_line(error_message, wID, iI, sO))
        {
            std::ostringstream o;
            o << "RollupSet::add_workload_detail_line() for WorkloadID " << wID.workloadID << " add_workload_detail_line() for " << pear.first << " failed - " << error_message;
            callers_error_message = o.str();
        }
    }

    return true;
}


bool RollupSet::addRollupType
(
    std::string& error_message
    , std::string attributeNameComboText
    , bool nocsvSection
    , bool quantitySection
    , bool maxDroopMaxtoMinIOPSSection
    , std::string nocsvText
    , ivy_int quantity
    , ivy_float maxDroop
)
// returns true if the rollup was created successfully.
// [CreateRollup] Serial_Number+LDEV
// [CreateRollup] host               [nocsv] [quantity] 16 [maxDroopMaxtoMinIOPS] 25 %
// error_message is clear()ed upon entry and is set with an error message if it returns false
{
//*debug*/ std::cout << "bool RollupSet::addRollupType (callers_error_message,\"" <<  attributeNameComboText << "\",";
//*debug*/ if (nocsvSection) std::cout << "true,"; else std::cout << "false,";
//*debug*/ if (quantitySection) std::cout << "true,"; else std::cout << "false,";
//*debug*/ if (maxDroopMaxtoMinIOPSSection) std::cout << "true,"; else std::cout << "false,\"";
//*debug*/ std::cout << nocsvText << "\",\"" << quantityText << "\",\"" << maxDroopMaxtoMinIOPSText << "\")" << std::endl;
    AttributeNameCombo aNC;
    std::string setErrorMessage;

    error_message.clear();

    if ( 0 != nocsvText.length() )
    {
        error_message = "The [nocsv] section must be left blank.";
        return false;
    }

    if (!aNC.set(setErrorMessage, attributeNameComboText, &(m_s.TheSampleLUN)))
    {
        std::ostringstream o;
        o << "RollupSet::addRollupType - invalid attribute name combo - \"" << attributeNameComboText << "\" - " << setErrorMessage;
        error_message = o.str();
        return false;
    }

    if (!aNC.isValid)
    {
        std::ostringstream o;
        o << "RollupSet::addRollupType - no such attribute name combo - \"" << attributeNameComboText << "\" - " << setErrorMessage;
        error_message = o.str();
        return false;
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
        o << "- [CreateRollup] Dreadfully sorry.  Seem to have had a bit of a cock-up in the back office.  New RollupType wasn\'t valid.  AAAaaaarrgggghhhh.   " << std::endl;
        error_message = o.str();
        std::cerr << o.str() << std::endl;
        delete pRollupType;
        return false;
    }

    rollups[toLower(aNC.attributeNameComboID)] = pRollupType;

    return true;
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

bool RollupSet::deleteRollup(std::string& callers_error_message, std::string attributeNameComboText)
{
    auto it = rollups.find(toLower(attributeNameComboText));
    if (it == rollups.end())
    {
        std::ostringstream o;
        o << "deleteRollup(\"" << attributeNameComboText << "\") - no such RollupType.";
        callers_error_message = o.str();
        return false;
    }
    rollups.erase(it);
    return true;
}


void RollupSet::resetSubintervalSequence()
{
    starting_ending_times.clear();

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
            instance_pair.second->error_integral = 0.0;
        }
    }
}


void RollupSet::startNewSubinterval(ivytime start, ivytime end)
{

    SubintervalRollup blankSubintervalRollup(start,end);


    for  (auto& pear : rollups)
    {
        RollupType* pRollupType	= pear.second;

        for (auto& peach : pRollupType->instances)
        {
            RollupInstance* pRollupInstance = peach.second;
            pRollupInstance->subintervals.sequence.push_back(blankSubintervalRollup);
        }
    }
    subintervalIndex++;
    starting_ending_times.push_back(std::pair<ivytime,ivytime>(start,end));
}


bool RollupSet::makeMeasurementRollup(std::string callers_error_message, unsigned int firstMeasurementIndex, unsigned int lastMeasurementIndex)
{
    {
        std::ostringstream o;
        o << "Successful measurement from subinterval " << firstMeasurementIndex << " to " << lastMeasurementIndex << "." << std::endl
            << "Measurement from " << starting_ending_times[firstMeasurementIndex].first.format_as_datetime_with_ns() << " to "
            << starting_ending_times[lastMeasurementIndex].second.format_as_datetime_with_ns() << std::endl;
        log(m_s.masterlogfile, o.str());
        std::cout << o.str();
        log(m_s.masterlogfile, o.str());
    }

    std::string my_message;

    measurement_starting_ending_times.first = starting_ending_times[firstMeasurementIndex].first;
    measurement_starting_ending_times.second = starting_ending_times[lastMeasurementIndex].second;
    measurement_first_index = firstMeasurementIndex;
    measurement_last_index = lastMeasurementIndex;

    for (auto& pear : rollups)
    {
        RollupType* pRollupType = pear.second;

//*debug*/ { std::ostringstream o; o << "RollupSet::makeMeasurementRollup(, " << firstMeasurementIndex << " ," << lastMeasurementIndex << ").  About to make measurement rollup in rollup type " << pRollupType->attributeNameCombo.attributeNameComboID << std::endl; std::cout << o.str(); log(m_s.masterlogfile,o.str());}

        if (!pRollupType->makeMeasurementRollup(my_message, firstMeasurementIndex, lastMeasurementIndex))
        {
            std::ostringstream o;
            o << "RollupSet::makeMeasurementRollup() - failed in makeMeasurementRollup() for RollupType \"" << pear.first << "\" - " << my_message;
            callers_error_message = o.str();
            return false;
        }
    }

    if (not_participating.size() > 0)
    {
        if (not_participating.size() < 3)
        {
            std::ostringstream o;
            o << "RollupSet::makeMeasurementRollup() - The total number of subintervals in the \"not_participating\" sequence is " << not_participating.size() << "." << std::endl
                << "There must be at least three - one warmup subinterval, one measurement subinterval, and one cooldown subinterval," << std::endl
                << "due to TCP/IP network time jitter when each test host hears the \"Go\" command.  This means we don't depend on NTP or clock synchronization." << std::endl;
            log(m_s.masterlogfile, o.str());
            m_s.kill_subthreads_and_exit();
        }

        if
        (!(	// not:
                    firstMeasurementIndex > 0 && firstMeasurementIndex < (not_participating.size()-1)
                    && lastMeasurementIndex > 0 && lastMeasurementIndex < (not_participating.size()-1)
                    && firstMeasurementIndex <= lastMeasurementIndex
        ))
        {
            std::ostringstream o;
            o   << "RollupSet::makeMeasurementRollup() - Invalid first (" << firstMeasurementIndex << ") and last (" << lastMeasurementIndex << ") measurement period indices "
                << "for \"not_participating\", whose size is " << not_participating.size() << "." << std::endl
                << "There must be at least one warmup subinterval, one measurement subinterval, and one cooldown subinterval, " << std::endl
                << "due to TCP/IP network time jitter when each test host hears the \"Go\" command.  This means we don't depend on NTP or clock synchronization.";
            log(m_s.masterlogfile, o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }

        not_participating_measurement.data.clear();

        for (unsigned int i=firstMeasurementIndex; i <= lastMeasurementIndex; i++)
        {
            not_participating_measurement.addIn(not_participating[i]);
        }
    }

    return true;
}


bool RollupSet::passesDataVariationValidation()
{
    if (rollups.size() == 0) return false;
    for (auto& pear : rollups) if (!( pear.second->passesDataVariationValidation() )) return false;
    return true;
}














