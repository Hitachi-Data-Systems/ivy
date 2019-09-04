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
#include <iostream>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <cmath> // for abs()
#include <cstdio>

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
#include "RollupType.h"
#include "ivy_engine.h"
#include "studentsTdistribution.h"
#include "ivybuilddate.h"

extern std::string masterlogfile();

extern bool routine_logging;
extern void say_and_log(const std::string&);


std::pair<bool,std::string> RollupInstance::makeMeasurementRollup()
{
    measurement& m = m_s.current_measurement();

    if
    (
        subintervals.sequence.size() < 1
        || m.firstMeasurementIndex < 0
        || m.firstMeasurementIndex > m.lastMeasurementIndex
        || m.lastMeasurementIndex >= ((int)subintervals.sequence.size())
    )
    {
        std::ostringstream o;
        o << "RollupInstance::makeMeasurementRollup() - Invalid first (" << m.firstMeasurementIndex << ") and last (" << m.lastMeasurementIndex << ") measurement period indices "
        << "with " << subintervals.sequence.size() << " subintervals of data." << std::endl;
        return std::make_pair(false,o.str());
    }

    things_by_measurement.emplace_back();

    achieved_IOPS_as_percent_of_Total_IOPS_settting.clear();

    {
        measurement_things& mt = things_by_measurement.back();
        SubintervalRollup& mr = mt.measurementRollup;
        subsystem_summary_data& ssd = mt.subsystem_data;

        for (unsigned int i = ((unsigned int) m.firstMeasurementIndex); i <= ((unsigned int)m.lastMeasurementIndex); i++)
        {
            mr.addIn(subintervals.sequence[i]);
        }

        ivy_float seconds = mr.durationSeconds();


        {
            mt.IOPS_setting = mr.inputRollup.get_Total_IOPS_Setting(1 + m.lastMeasurementIndex - m.firstMeasurementIndex);
                // this returns -1.0 if any IOPS setting was "max".  Returns 0.0 if no IOPS settings were found.

            mt.Total_IOPS_setting_string = IosequencerInputRollup::Total_IOPS_Setting_toString(mt.IOPS_setting); // returns "max" for -1.0.

            const RunningStat<ivy_float,ivy_int>& st = mr.outputRollup.overall_service_time_RS();

            mt.application_IOPS = st.count() / seconds;

            if (is_all_equals_all_instance())
            {
                m.all_equals_all_Total_IOPS = mt.application_IOPS;
            }

            if (m_s.haveCmdDev)
            {
                ssd.repetition_factor = 0.0;

                for (unsigned int i = ((unsigned int) m.firstMeasurementIndex); i <= ((unsigned int)m.lastMeasurementIndex); i++)
                {
                    if (subsystem_data_by_subinterval.size() > i) ssd.addIn(subsystem_data_by_subinterval[i]);
                }

                mt.subsystem_IOPS = ssd.IOPS();

                if (mt.subsystem_IOPS > 0 && mt.application_IOPS > 0)
                {
                    mt.subsystem_IOPS_as_fraction_of_host_IOPS = mt.subsystem_IOPS / mt.application_IOPS;

                    mt.subsystem_IOPS_as_fraction_of_host_IOPS_failure
                        =  mt.subsystem_IOPS_as_fraction_of_host_IOPS < 0.85
                        || mt.subsystem_IOPS_as_fraction_of_host_IOPS > 1.15;
                    if (mt.subsystem_IOPS_as_fraction_of_host_IOPS_failure) m.subsystem_IOPS_as_fraction_of_host_IOPS_failure = true;
                }
            }

            if (mt.IOPS_setting > 0.0 && mt.application_IOPS > 0.0)
            {
                {
                    std::ostringstream o;
                    o << std::fixed << std::setprecision(2) << (100.0 * (mt.application_IOPS / mt.IOPS_setting)) << "%";
                    achieved_IOPS_as_percent_of_Total_IOPS_settting = o.str();
                }

                if
                (
                       mt.application_IOPS < (0.999 * mt.IOPS_setting)
                    || mt.application_IOPS > (1.001 * mt.IOPS_setting)
                )
                {
                    m.failed_to_achieve_total_IOPS_setting = mt.failed_to_achieve_total_IOPS_setting = true;
                }
            }
        }
    }

    return std::make_pair(true,"");
}


std::pair<bool,std::string> RollupInstance::add_workload_detail_line(WorkloadID& wID, IosequencerInput& iI, SubintervalOutput& sO)
{
	std::string my_error_message;

	int index = -1 + subintervals.sequence.size();

	if (-1 == index)
	{
		return std::make_pair(false,"RollupInstance::add_workload_detail_line() failed because the RollupInstance subinterval sequence was empty.");
	}

	SubintervalRollup& subintervalRollup = subintervals.sequence[index];

    auto retval = subintervalRollup.inputRollup.add(iI.getParameterNameEqualsTextValueCommaSeparatedList());
	if ( !retval.first )
	{
		std::ostringstream o;
		o << "RollupInstance::add_workload_detail_line() - failed adding in IosequencerInput.getParameterNameEqualsTextValueCommaSeparatedList()=\""
		  << iI.getParameterNameEqualsTextValueCommaSeparatedList() << "\" to the last SubintervalRollup in the sequence of " << subintervals.sequence.size() << ".";
		return std::make_pair(false,o.str());
	}

	subintervalRollup.outputRollup.add(sO);  // for two existing SubintervalOutput objects, this call doesn't fail.

	return std::make_pair(true,"");
}


std::string RollupInstance::config_filter_contents()
{
    std::ostringstream o;

    o << "RollupInstance::config_filter = {";

    for (auto& serial_pair : config_filter)
    {
        o << "    { serial = \"" << serial_pair.first << std::endl;

        for (auto& element_pair : serial_pair.second)
        {
            o << "         { element = \"" << element_pair.first << "\", instances = { ";

            bool first_instance {true};

            for (auto& instance : element_pair.second)
            {
                if (!first_instance)
                {
                    o << ", ";
                }
                first_instance = false;
                o << "\"" << instance << "\"";
            }

            o << " } }" << std::endl;

        }

        o << "    }" << std::endl;
    }

    o << '}' << std::endl;

    return o.str();
}


void RollupInstance::rebuild_test_config_thumbnail()
{
    test_config_thumbnail.clear();
    for (WorkloadID& x : workloadIDs.workloadIDs)
    {
        auto wit = m_s.workloadTrackers.workloadTrackerPointers.find(x.workloadID);
        if (wit != m_s.workloadTrackers.workloadTrackerPointers.end())
        {
            WorkloadTracker* pWT = wit->second;
            test_config_thumbnail.add(&(pWT->workloadLUN));
        }
    }
}


ivy_float RollupInstance::get_focus_metric(unsigned int subinterval_number)
{

    Accumulators_by_io_type* p_acc;

    // These next 3 declarations were pulled up out of the switch statement to avoid "jump to case label" error message
    std::string s;
    SubintervalRollup& subinterval_rollup = subintervals.sequence[subinterval_number];
    SubintervalOutput& so = subinterval_rollup.outputRollup;

    switch(m_s.source)
    {
        case source_enum::workload:
        {
            if (subinterval_number >= subintervals.sequence.size())
            {
                std::ostringstream o;
                o << "<Error> Requested subinterval number " << subinterval_number << ", starting from zero, is too big.  The number of subintervals is " <<  subintervals.sequence.size() << " - source code ref RollupInstance::get_focus_metric()." << std::endl;
                log(m_s.masterlogfile,o.str());
                std::cout << o.str();
                m_s.kill_subthreads_and_exit();
            }

            switch (m_s.accumulator_type)
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
                {
                    std::ostringstream o;
                    o << "<Error> internal programming error in RollupInstance::get_focus_metric() - invalid accumulator_type enum value " << accumulator_type_to_string(m_s.accumulator_type) << std::endl;
                    log(m_s.masterlogfile,o.str());
                    std::cout << o.str();
                    m_s.kill_subthreads_and_exit();
                    p_acc = & so.u.a.response_time; // this is just to make the warning message go away, warning that p_acc may be used uninitialized
                }
            }

            // Sorry about this next bit.  I had forgotten I had already done it a different way, and this was a quick way to glue two recycled pieces of code together reliably

            switch (m_s.category)
            {
                case category_enum::overall:          s = "Overall"; break;
                case category_enum::random:           s = "Random"; break;
                case category_enum::sequential:       s = "Sequential"; break;
                case category_enum::read:             s = "Read"; break;
                case category_enum::write:            s = "Write"; break;
                case category_enum::random_read:      s = "Random Read"; break;
                case category_enum::random_write:     s = "Random Write"; break;
                case category_enum::sequential_read:  s = "Sequential Read"; break;
                case category_enum::sequential_write: s = "Sequential Write"; break;
                default:
                {
                    std::ostringstream o;
                    o << "<Error> internal programming error in RollupInstance::get_focus_metric() - could not translate category enum value " << m_s.category_parameter << std::endl;
                    log(m_s.masterlogfile,o.str());
                    std::cout << o.str();
                    m_s.kill_subthreads_and_exit();
                }
            }

            for (int i = 0; i <= Accumulators_by_io_type::max_category_index(); i++)
            {
                if (stringCaseInsensitiveEquality(s, Accumulators_by_io_type::getRunningStatTitleByCategory(i)))
                {
                    RunningStat<ivy_float, ivy_int> rs = p_acc->getRunningStatByCategory(i);

                    switch (m_s.accessor)
                    {

                        case accessor_enum::count:             return (ivy_float) rs.count();
                        case accessor_enum::min:               return rs.min();
                        case accessor_enum::max:               return rs.max();
                        case accessor_enum::sum:               return rs.sum();
                        case accessor_enum::avg:               return rs.avg();
                        case accessor_enum::variance:          return rs.variance();
                        case accessor_enum::standardDeviation: return rs.standardDeviation();
                        default:
                        {
                            std::ostringstream o;
                            o << "<Error> internal programming error in RollupInstance::get_focus_metric() - unknown accessor enum value " << m_s.accessor_parameter << std::endl;
                            log(m_s.masterlogfile,o.str());
                            std::cout << o.str();
                            m_s.kill_subthreads_and_exit();
                        }
                    }
                }
            }
            {
                std::ostringstream o;
                o << "<Error> internal programming error in RollupInstance::get_focus_metric() - glue code didn\'t work for category type as string \"" << s << "\"." << std::endl;
                log(m_s.masterlogfile,o.str());
                std::cout << o.str();
                m_s.kill_subthreads_and_exit();
            }
        }
        break;  // end of source_enum::workload

        case source_enum::RAID_subsystem:
        {

            if (subinterval_number >= subsystem_data_by_subinterval.size())
            {
                std::ostringstream o;
                o << "<Error> Requested subinterval number " << subinterval_number << " starting from zero is too big.  The number of subsystem data subintervals for this rollup instance is " <<  subsystem_data_by_subinterval.size() << " - source code ref RollupInstance::get_focus_metric()." << std::endl;
                log(m_s.masterlogfile,o.str());
                std::cout << o.str();
                m_s.kill_subthreads_and_exit();
            }


            if (subinterval_number == 0) return -1.0;

            // The first subsystem data shows up in subinterval 1.

            // This is because the gather at t=0 is asynchronous, so we can't use it as a subinterval starting value.

            // At the end of subinterval 1, we have the subinterval 1 ending counter values to compare with
            // the subinterval 0 ending countner values, which were both collected synchronously.


            const subsystem_summary_data& subsystem_data = subsystem_data_by_subinterval[subinterval_number];

            auto element_it = subsystem_data.data.find(m_s.subsystem_element);
            if (element_it == subsystem_data.data.end())
            {
                std::ostringstream o;
                o   << "<Warning> RollupInstance::get_focus_metric( unsigned int subinterval_number = " << subinterval_number
                    << " ) - returning -1.0 because subsystem element \"" << m_s.subsystem_element << "\" not found for this subinterval." << std::endl
                    << "Available elements for this subinterval are: "; for (auto& pear : subsystem_data.data) o << " \"" << pear.first << "\" "; o << std::endl;
                m_s.warning_messages.insert(o.str());
                std::cout << o.str();
                log(m_s.masterlogfile, o.str());
                return -1.0;
            }

            auto metric_it = element_it->second.find(m_s.element_metric);
            if (metric_it == element_it->second.end())
            {
                std::ostringstream o;
                o  << "<Warning> RollupInstance::get_focus_metric( unsigned int subinterval_number = " << subinterval_number
                   << ") - subsystem data this subinterval for element \"" << m_s.subsystem_element << "\" did not have \"" << m_s.element_metric << "\" metric." << std::endl;
                o << "Available metrics for this element are: "; for (auto& pear : element_it->second) o << " \"" << pear.first << "\" "; o << std::endl;
                m_s.warning_messages.insert(o.str());std::cout << o.str();
                log(m_s.masterlogfile, o.str());
                return -1.0;
            }

            return metric_it->second.avg();
        }
        break;

        default:
        {
            std::ostringstream o;
            o << "<Error> internal programming error in RollupInstance::get_focus_metric() - invalid source_enum enum value " << m_s.source_parameter << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }
    }
    return 0.0;  // control never reaches here, but this is to keep the compiler happy
}

void RollupInstance::collect_and_push_focus_metric()
{
    unsigned int subinterval_count = subintervals.sequence.size();

    if (subinterval_count == 0) throw std::runtime_error(
        "<Error> internal programming error - RollupInstance::collect_and_push_focus_metric() - There are no subintervals to collect from.  subintervals.sequence.size() == 0."
    );

    if (subinterval_count != (1 + focus_metric_vector.size()))
    {
        std::ostringstream o;
        o << "<Error> internal programming error - RollupInstance::collect_and_push_focus_metric() - Not being called exactly once after each subinterval."
            << "subintervals.sequence.size() = " << subintervals.sequence.size() << ", focus_metric_vector.size() = " << focus_metric_vector.size() << ".";
        std::cout << o.str();
        log(m_s.masterlogfile,o.str());
        m_s.kill_subthreads_and_exit();
    }

    focus_metric_vector.push_back(get_focus_metric(subinterval_count - 1));
}

void RollupInstance::perform_PID()
{
    unsigned int subinterval_count = focus_metric_vector.size();

    if ( subinterval_count < 3)
    {
        // RollupInstance::reset() already prepares most things.
        m_s.last_gain_adjustment_subinterval = -1;
    }
    else
    {
        // third or later subinterval when we compute a new IOPS value

        total_IOPS_last_time = total_IOPS;

        this_measurement = focus_metric_vector[subinterval_count-1];
        prev_measurement = focus_metric_vector[subinterval_count-2];

        error_signal = this_measurement - m_s.target_value;
        error_integral += error_signal;
        error_derivative = (this_measurement - prev_measurement) / m_s.subinterval_seconds;

        p_times_error = P * error_signal;
        i_times_integral = I * error_integral;
        d_times_derivative = D * error_derivative;

        total_IOPS = p_times_error + i_times_integral + d_times_derivative;
    }

    if (total_IOPS < per_instance_low_IOPS)
    {
        std::ostringstream o;
        o << "<Warning>"
            << " For rollup \"" << attributeNameComboID << "\", instance \"" << rollupInstanceID << "\""
            << " could not apply newly calculated PID loop IOPS value " << std::fixed << std::setprecision(2) << total_IOPS
            << ", instead PID new IOPS clamped at the minimum " << std::fixed << std::setprecision(2) << per_instance_low_IOPS
            << " calculated as the PID \"min_IOPS\" setting divided by the number of instances in the rollup we are performing PID on."
            << std::endl << std::endl;
        m_s.warning_messages.insert(o.str());
        std::cout << o.str();
        log(m_s.masterlogfile,o.str());

        total_IOPS = per_instance_low_IOPS;
        error_integral = total_IOPS/I;
    }
    else if (total_IOPS > per_instance_max_IOPS)
    {
        std::ostringstream o;
        o << "<Warning>"
            << " For rollup \"" << attributeNameComboID << "\", instance \"" << rollupInstanceID << "\""
            << " could not apply newly calculated PID loop IOPS value " << std::fixed << std::setprecision(2) << total_IOPS
            << ", instead new PID new IOPS clamped at the maximum " << std::fixed << std::setprecision(2) << per_instance_max_IOPS
            << " calculated as the optional PID \"max_IOPS\" setting if present, otherwise the high_IOPS setting, divided by the number of instances in the rollup we are performing PID on."
            << std::endl << std::endl;
        m_s.warning_messages.insert(o.str());
        std::cout << o.str();
        log(m_s.masterlogfile,o.str());

        total_IOPS = per_instance_max_IOPS;
        error_integral = total_IOPS/I;
    }

    // The next bit is how we run an adaptive algorithm to reduce the "I" gain if we are seeing
    // excessive swings in IOPS, or to increase the gain if we haven't reached the target yet.

    // The mechanism works on the basis of successive "adaptive PID subinterval cycles" where
    // every time we make a gain adjustment (up or down), we start a new subinterval cycle.

    // A "swing" is the cumulative distance that IOPS moves in one or more steps
    // in the same direction up or down ("monotonically") before changing direction
    // at an "inflection point".

    // If we see IOPS swinging up and down repeatedly (at least two up-swings and two down-swings
    // within the current adaptive PID cycle) and the average swing size in each direction
    // both up and down is "excessive ", then we reduce the gain and start a new adaptive PID cycle.

    // "Excessive" means that the average up swing expressed as a fraction of the midpoint of
    // the swing is bigger than "max_ripple" (default 1% at time of writing), and likewise
    // the average down swing is also bigger than max_ripple.

    // Looking for excessive swings in both directions to identify instability is used to reject
    // noise in the measurement value from subinterval to subinterval that might cause small swings
    // in the opposite direction while we are making big movements in one direction because
    // we haven't reached the target value yet and we are steering IOPS up or down.

    // If we see average size of swing over multiple swings in both directions is too high
    // we reduce the "I" gain by "gain_step", e.g. 2.0.

    // The gain reduction multiplicative factor we use is min( gain_step, 1 / gain_step ).
    // In other words, you can either say 4.0 or 0.25, it works exactly the same either way.

    // When for the first time we see these excessive swings both up and down,
    // this is taken as evidence that we have "arrived on station".

        // (At time of writing this, donn't know if this will be sufficient
        //  and maybe we'll need to do something else to know that we have arrived
        //  at the target either by further analysis of the changes in IOPS
        //  or by actually looking at the error signal.)

    // Before we have arrived on station as evidenced by repeated excessive IOPS
    // swings in both directions during at least one adaptive PID cycle,
    // we keep increasing the gain and starting new adaptive PID cycles triggered
    // two ways:

    // 1) Within the same adaptive PID subinterval cycle, if the IOPS is either flat
    //    or moves exclusively in one direction up or down for more than "max_monotone" subintervals,
    //    this triggers a gain increase and the start of a new adaptive PID cycle.

    // 2) Within the same adaptive PID subinterval cycle, if the number of subintervals
    //    where IOPS is adjusted "up" is less than 1/3 of the number of subintervals where an IOPS
    //    change was made, or if the number of "down" steps is less than 1/3 the total steps
    //    over a period of "balanced_step_direction_by" (at time of writing defaulting to 12)
    //    total either flat or moves exclusively in one direction up or down for more
    //    than "max_monotone" subintervals, this triggers a gain increase and
    //    the start of a new adaptive PID cycle.

    //    The reason we have this as well as max_monotone is that there may be enough
    //    noise in the measurement from subinterval to subinterval to cause some
    //    erratic movement of IOPS over and above the drift towards the target as
    //    we are actively steering in one direction most of the time.

    //    Also as you get closer to the target, the smaller the correction the PID
    //    loop is going to make, and thus the louder the inherent noise in the measurement.

    //    So if you are way far away, then max_monotone should trigger in a short
    //    number of subintervals, making initial gain increases frequently as
    //    long as IOPS keeps rising monotonically.

    //    And then when you get close to the target, but you're not actually on
    //    target yet, IOPS is going to bobble around a bit due to the inherent noise
    //    in the measurement signal, but we decide we are still steering towards
    //    the target if over 2/3 of the individual IOPS changes we make are in one direction
    //    over a longer period "balanced_step_direction_by" (default 12) .

    //    So the idea is that if we are on target, we will be making roughly the
    //    same number of "up" individual IOPS adjustments as individual "down" adjustments
    //    regardless of their packaging into "swings" which are the monotonic
    //    subsequences joined by "inflection points" where the IOPS adjustment
    //    changes direction.  Thus if over two thirds of the adjustments are
    //    the same direction we increase the gain.

    // The default value of "balanced_step_direction_by" (12) is longer than
    // the default value of "max_monotone" (5) and this is consistent with it taking
    // longer in the presence of noise to decode the signal.


    // The output IOPS value that is set by the PID loop mechanism is contrained (limited)
    // to range from "low_IOPS" to "high_IOPS".  A calculated IOPS value that is outside
    // this range must be as a result of some noise or transient by definition,
    // so to promote PID loop stability and to ensure that we will always be
    // running enough I/O so we always get things like service time.
    // The [Go] statement low_IOPS and high_IOPS values are divided by the number
    // of instances of the focus_rollup to arrive at per RollupInstance low and high IOPS numbers.


    if (subinterval_count > 3)
    {
        adaptive_PID_subinterval_count++;

        if (adaptive_PID_subinterval_count == 1)
        {
            // starting a new adaptive PID cycle.

            on_way_down = false;
            on_way_up =  false;
            have_previous_inflection = false;

            up_movement.clear();
            down_movement.clear();
            fractional_up_swings.clear();
            fractional_down_swings.clear();

            monotone_count = 0;

            {std::ostringstream o; o << "Starting new adaptive PID cycle at overall subinterval " << subinterval_count << "." << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
        }
        else
        {
//            {
//                std::ostringstream o;
//                o << "Entering adaptive PID mechanism for " << attributeNameComboID << " = " << rollupInstanceID
//                    << "  in " << (have_reduced_gain ? "settling " : "initial gain increase" ) << " mode with "
//                    << "\"max_monotone\" gain increases = " << m_count
//                    << ", \"balanced_step_direction_by\" gain increases = " << b_count
//                    << ", \"max_ripple\" gain decreases = " << d_count
//                    << "\n\n";
//                std::cout << o.str();
//                if (routine_logging) { log(m_s.masterlogfile, o.str()); }
//            }

            // This is at least the second time we have set an IOPS value in this adaptive PID cycle.

            ivy_float delta = total_IOPS - total_IOPS_last_time;

            if      ( delta > 0.0 ) { up_movement.push(abs(delta)); }
            else if ( delta < 0.0 ) { down_movement.push(abs(delta)); }

            ivy_float delta_percent = 100 * abs(delta) / total_IOPS_last_time;

            if ( (!on_way_down) && (!on_way_up) )
            {
                // This always happens on the second time through, and it's theoretically possible to keep happening if we always compute the exact same IOPS value

                if (total_IOPS > total_IOPS_last_time)
                {
                    on_way_up = true;
                    monotone_count = 1;
                    {std::ostringstream o; o << "PID loop for " << attributeNameComboID << "=" << rollupInstanceID << " - focus metric value = " << std::fixed << this_measurement << " - initial IOPS movement is upwards (" << std::fixed << std::setprecision(2) << delta_percent << "%) at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                }
                else if (total_IOPS < total_IOPS_last_time)
                {
                    on_way_down = true;
                    monotone_count = 1;
                    {std::ostringstream o; o << "PID loop for " << attributeNameComboID << "=" << rollupInstanceID << " - focus metric value = " << std::fixed << this_measurement << " - initial IOPS movement is downwards (" << std::fixed << std::setprecision(2) << delta_percent << "%) at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                }
                else
                {
                    {std::ostringstream o; o << "PID loop for " << attributeNameComboID << "=" << rollupInstanceID << " - focus metric value = " << std::fixed << this_measurement << " - always calculated exactly same IOPS this adaptive PID cycle at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                }
            }
            else
            {
                // We already had a direction of motion up or down before seeing this new IOPS setting.

                unsigned int total_steps = up_movement.count() + down_movement.count();

                if (  !have_reduced_gain
                    && total_steps > m_s.balanced_step_direction_by
                    && (
                         ( up_movement.count()   < (total_steps/3) )
                      || ( down_movement.count() < (total_steps/3) )
                       )
                   )
                {
                    // need to increase gain due to balanced_step_direction_by
                    I *= max(m_s.gain_step, 1.0/m_s.gain_step);
                    error_integral = total_IOPS / I;       // adjusts the cumulative error total to according to new I, new IOPS.
                    adaptive_PID_subinterval_count = 0;    // other things get reset from this
                    m_s.last_gain_adjustment_subinterval = m_s.harvesting_subinterval; // warmup extends until at least the last gain adjustmenty
                    {std::ostringstream o; o << "PID loop for " << attributeNameComboID << "=" << rollupInstanceID << " - focus metric value = " << std::fixed << this_measurement << " - increasing gain because over at least  "
                        << m_s.balanced_step_direction_by << " IOPS adjustments, over two thirds of the adjustments were in the same direction"
                        << " at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle."
                        << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                    b_count++;

                    gain_history
                        << attributeNameComboID << " = " << rollupInstanceID
                        << " \"balanced_step_direction_by\" increase at subinterval " << m_s.harvesting_subinterval
                        << ", adaptive PID cycle subinterval " << adaptive_PID_subinterval_count
                        << ", I = " << I
                        << ", \"max_monotone\" increases = " << m_count
                        << ", \"balanced_step_direction_by\" increases = " << b_count
                        << ", \"max_ripple\" decreases = " << d_count
                        << std::endl;

                }
                else
                {
                    // If we have changed direction, we are seeing a new inflection point.
                    if (     (   (on_way_up)   && (total_IOPS < total_IOPS_last_time)  )
                         ||  (   (on_way_down) && (total_IOPS > total_IOPS_last_time)  ))
                    {
                        // we see an inflection point where we changed direction.

                        on_way_up = !on_way_up;
                        on_way_down = !on_way_down;
                        monotone_count = 1;

                        ivy_float this_inflection = total_IOPS_last_time;  // This was the farthest point reached.

                        if (have_previous_inflection)
                        {
                            // This is the ending point of a swing


                            ivy_float swing =  this_inflection - previous_inflection;

                            ivy_float base_IOPS = 0.5 * (previous_inflection + this_inflection); // average of the swing endpoint IOPS values

                            ivy_float swing_fraction_of_IOPS = abs(swing) / base_IOPS;

                            if (swing > 0.0) { fractional_up_swings.push(swing_fraction_of_IOPS); }
                            else             { fractional_down_swings.push(swing_fraction_of_IOPS); }

                            if ( (fractional_up_swings.count() >=2 && fractional_up_swings.avg() > m_s.max_ripple)
                              && (fractional_down_swings.count() >=2 && fractional_down_swings.avg() > m_s.max_ripple) )
                            {
                                // the average swing size in both directions is too big and we need to reduce the gain and start a new adaptive PID cycle
                                have_reduced_gain = true;
                                I *= min(m_s.gain_step, 1.0/m_s.gain_step);
                                total_IOPS = base_IOPS;              // set new starting IOPS to be at midpoint of swing.
                                error_integral = total_IOPS / I;     // adjusts the cumulative error total to according to new I, new IOPS.
                                m_s.last_gain_adjustment_subinterval = m_s.harvesting_subinterval; // warmup extends until at least the last gain adjustmenty
                                {std::ostringstream o; o << "PID loop for " << attributeNameComboID << "=" << rollupInstanceID << " - focus metric value = " << std::fixed << this_measurement << " - saw inflection " << (on_way_up ? "upwards" : "downwards")
                                    << ", size of swing is " << std::fixed << std::setprecision(2) << (100.*swing_fraction_of_IOPS) << "% of its midpoint IOPS.  "
                                    << "Due to excessive swing size in both directions reducing gain at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                                adaptive_PID_subinterval_count = 0;  // other things get reset from this
                                d_count++;

                                gain_history
                                    << attributeNameComboID << " = " << rollupInstanceID
                                    << " \"max_ripple\" decrease at subinterval " << m_s.harvesting_subinterval
                                    << ", adaptive PID cycle subinterval " << adaptive_PID_subinterval_count
                                    << ", I = " << I
                                    << ", \"max_monotone\" increases = " << m_count
                                    << ", \"balanced_step_direction_by\" increases = " << b_count
                                    << ", \"max_ripple\" decreases = " << d_count
                                    << std::endl;

                            }
                            else
                            {
                                // The swing was OK, keep going in the same adaptive PID cycle.

                                // This becomes the starting point for a new swing in the other direction.
                                previous_inflection = this_inflection;
                                {std::ostringstream o; o << "PID loop for " << attributeNameComboID << "=" << rollupInstanceID << " - focus metric value = " << std::fixed << this_measurement << " - saw inflection " << (on_way_up ? "upwards" : "downwards")
                                    << ", size of swing is " << std::fixed << std::setprecision(2) << (100.*swing_fraction_of_IOPS) << "% of its midpoint IOPS"
                                    << ", starting new swing at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                            }
                        }
                        else // this is the first inflection point this adaptive PID cycle, start the first swing.
                        {
                            previous_inflection = this_inflection;
                            have_previous_inflection = true;
                            {std::ostringstream o; o << "PID loop for " << attributeNameComboID << "=" << rollupInstanceID << " - focus metric value = " << std::fixed << this_measurement << " - saw first inflection (" << (on_way_up ? "upwards" : "downwards") << ") this adaptive PID cycle at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                        }
                    }
                    else
                    {
                        // we have kept going in the same direction.

                        monotone_count++;

                        if ( (!have_reduced_gain) && (monotone_count > m_s.max_monotone))
                        {
                            // need to increase gain due to max_monotone
                            I *= max(m_s.gain_step, 1.0/m_s.gain_step);
                            error_integral = total_IOPS / I;       // adjusts the cumulative error total to according to new I, new IOPS.
                            adaptive_PID_subinterval_count = 0;    // other things get reset from this
                            m_s.last_gain_adjustment_subinterval = m_s.harvesting_subinterval; // warmup extends until at least the last gain adjustmenty
                            {std::ostringstream o; o << "PID loop for " << attributeNameComboID << "=" << rollupInstanceID << " - focus metric value = " << std::fixed << this_measurement << " - increasing gain because over at least "
                                << m_s.max_monotone << " consecutive subintervals, all IOPS adjustments were in the same direction"
                                << " at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                            m_count++;

                            gain_history
                                << attributeNameComboID << " = " << rollupInstanceID
                                << " \"max_monotone\" increase at subinterval " << m_s.harvesting_subinterval
                                << ", adaptive PID cycle subinterval " << adaptive_PID_subinterval_count
                                << ", I = " << I
                                << ", \"max_monotone\" increases = " << m_count
                                << ", \"balanced_step_direction_by\" increases = " << b_count
                                << ", \"max_ripple\" decreases = " << d_count
                                << std::endl;

                        }
                        else
                        {
                            std::ostringstream o; o << "PID loop for " << attributeNameComboID << "=" << rollupInstanceID << " - focus metric value = " << std::fixed << this_measurement << " - IOPS kept going "
                                << (on_way_up ? "upwards" : "downwards") << " (" << std::fixed << std::setprecision(2) << delta_percent << "%) at subinterval "
                                << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl;
                            std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str());
                        }
                    }
                }
            }
        }
    }


    // Finished calculation of new total_IOPS value, now send it out

    {
        std::ostringstream o;
        o << "total_IOPS=" << std::fixed << std::setprecision(6) << total_IOPS;
        std::string s = attributeNameComboID + std::string("=") + rollupInstanceID;
        std::pair<bool,std::string> rc = m_s.edit_rollup(s, o.str(),true /*meaning don't log this is the ivy engine API call log, as this is an internally generated call.*/);
        if (!rc.first)
        {
            std::ostringstream o2;
            o2 << "<Error> In PID loop for subinterval " << (subinterval_count-1)
                << ", ivy engine API edit_rollup(\"" << s << "\", \"" << o.str() << "\") failed - " << rc.second << std::endl
                << "in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
            std::cout << o2.str();
            log(m_s.masterlogfile, o2.str());
            m_s.kill_subthreads_and_exit();
        }
    }

    print_subinterval_column();
}

std::pair<bool,ivy_float> RollupInstance::isValidMeasurementStartingFrom(unsigned int n)
{
   	RunningStat<ivy_float, ivy_int> sample;
	ivy_float plusminusfraction;

	for (unsigned int index = n; index < focus_metric_vector.size(); index++)
	{
        sample.push(focus_metric_vector[index]);
	}

    ivy_float students_t_multiplier;

    {
        std::string my_error_message;
        if (!lookupDoubleSidedStudentsTMultiplier(my_error_message, students_t_multiplier, sample.count(), m_s.confidence_parameter))
        {
            std::ostringstream o;
            o << "<Error> in RollupInstance::isValidMeasurementStartingFrom().  Student's t distribution multiplier lookup failed:  " << my_error_message << std::endl;
            std::cout << o.str();
            log (m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
        }
    }

    if (0.0 == sample.avg())
    {
        std::ostringstream o;
        o << "<Warning> in RollupInstance::isValidMeasurementStartingFrom( n = " << n << " ).  Last subinterval is " << (focus_metric_vector.size()-1) << "." << std::endl
            << "Average over sample set is zero, so we can\'t perform the measurement validation calculation which divides by the average over the sample set.  Number of samples in set is " << sample.count() << "." << std::endl;
        m_s.warning_messages.insert(o.str());
        log (m_s.masterlogfile,o.str());
        return std::make_pair(false,-1);
    }


    plusminusfraction =  (  students_t_multiplier * sample.standardDeviation() / sqrt((ivy_float) sample.count())   ) / sample.avg();

    plusminusfraction *= m_s.non_random_sample_correction_factor;
        // This adjusts the plus-minus magnitude to compensate for the fact that there is a correlation between the measurements in successive subintervals
        // making the excursions locally smaller than if you had run infinitely many subintervals and you selected subintervals at random from that population,
        // which is what the math (student's T distribution) is based on.

    plusminusfraction_percent_above_below_target = 100.*(plusminusfraction-m_s.accuracy_plus_minus_fraction) / m_s.accuracy_plus_minus_fraction;

    if (plusminusfraction <= m_s.accuracy_plus_minus_fraction)
    {
        return std::make_pair(true,sample.avg());
    }
    else
    {
        return std::make_pair(false,-1.);
    }
}

void RollupInstance::print_console_info_line()
{
    std::ostringstream o;

    o << std::endl << "RollupInstance: source=\"" << m_s.source_parameter << "\"";

    switch (m_s.source)
    {
        case source_enum::workload:
            o << ", rollup_type=\"" << attributeNameComboID << "\", rollup_instance=\"" << rollupInstanceID
                << "\", accumulator_type=\"" << m_s.accumulator_type_parameter << "\", category=\"" << m_s.category_parameter << "\", accessor=\"" << m_s.accessor_parameter << "\"";
            break;
        case source_enum::RAID_subsystem:
            o << ", subsystem element = \"" << m_s.subsystem_element << "\", element_metric = \"" << m_s.element_metric << "\"";
            break;
        default:
            o << "<Error> internal programming error - RollupInstance::print_console_info_line() - unknown PID source." << std::endl;
    }

    o << ", IOPS last time = " << total_IOPS_last_time << ", target_value = " << m_s.target_value << ", measurement = " << this_measurement << ", error = " << error_signal;

    if (m_s.target_value !=0) o << " ( " << (100. * (error_signal / m_s.target_value)) << "% ) ";

    o << ", error integral = " << error_integral << ", error_derivative = " << error_derivative << std::endl
    << "I = " << I << ", new total IOPS = " << total_IOPS << std::endl << std::endl;

    std::cout << o.str();
    log(m_s.masterlogfile,o.str());
    return;
}


void RollupInstance::reset() // gets called by go statement, when it assigns the DFC
{
    focus_metric_vector.clear();

    if (m_s.have_pid)
    {
        error_signal = 0.0;

        error_derivative = 0.0;

        ivy_float initial_error = -1.0 * m_s.target_value - m_s.low_target;

        error_integral = 0.5 * initial_error * (m_s.ballpark_seconds/m_s.subinterval_seconds);

        ivy_float fraction_through_range = (m_s.target_value - m_s.low_target)/(m_s.high_target - m_s.low_target);

        per_instance_low_IOPS = m_s.low_IOPS / ( (ivy_float) pRollupType->instances.size());
        per_instance_high_IOPS = m_s.high_IOPS / ( (ivy_float) pRollupType->instances.size());
        per_instance_max_IOPS = m_s.max_IOPS / ( (ivy_float) pRollupType->instances.size());
            // We are making the assumption that the various instances contribute for practical purposes in a balanced way.


        total_IOPS = per_instance_low_IOPS + fraction_through_range * (per_instance_high_IOPS - per_instance_low_IOPS);  // straight line is just a rough approximation

        auto n = pRollupType->instances.size(); // The calibration IOPS values are for the all=all rollup, so we divide by the number of instances in the focus rollup.

        total_IOPS /= (ivy_float) n;

        total_IOPS_last_time = total_IOPS;

        P = 0;
        I = total_IOPS / error_integral;
        D = 0;

        adaptive_PID_subinterval_count = 0;
        m_count = b_count = d_count = 0;
        have_reduced_gain = false;

        subinterval_number_sstream.str("");
        subinterval_number_sstream.clear();
        target_metric_value_sstream.str("");
        target_metric_value_sstream.clear();
        metric_value_sstream.str("");
        metric_value_sstream.clear();
        error_sstream.str("");
        error_sstream.clear();
        error_integral_sstream.str("");
        error_integral_sstream.clear();
        error_derivative_sstream.str("");
        error_derivative_sstream.clear();
        p_times_error_sstream.str("");
        p_times_error_sstream.clear();
        i_times_integral_sstream.str("");
        i_times_integral_sstream.clear();
        d_times_derivative_sstream.str("");
        d_times_derivative_sstream.clear();
        total_IOPS_sstream.str("");
        total_IOPS_sstream.clear();
        p_sstream.str("");
        p_sstream.clear();
        i_sstream.str("");
        i_sstream.clear();
        d_sstream.str("");
        d_sstream.clear();
        gain_history.str("");
        gain_history.clear();

        subinterval_number_sstream << "subinterval number";print_common_columns(subinterval_number_sstream);
        target_metric_value_sstream<< "target value";      print_common_columns(target_metric_value_sstream);
        metric_value_sstream       << "measurement";       print_common_columns(metric_value_sstream);
        error_sstream              << "error signal";      print_common_columns(error_sstream);
        error_integral_sstream     << "error integral";    print_common_columns(error_integral_sstream);
        error_derivative_sstream   << "error derivative";  print_common_columns(error_derivative_sstream);
        p_times_error_sstream      << "p times error";     print_common_columns(p_times_error_sstream);
        i_times_integral_sstream   << "i times integral";  print_common_columns(i_times_integral_sstream);
        d_times_derivative_sstream << "d times derivative";print_common_columns(d_times_derivative_sstream);
        total_IOPS_sstream         << "total IOPS setting";print_common_columns(total_IOPS_sstream);
        p_sstream                  << "P";                 print_common_columns(p_sstream);
        i_sstream                  << "I";                 print_common_columns(i_sstream);
        d_sstream                  << "D";                 print_common_columns(d_sstream);
    }
    else
    {
        total_IOPS = 0.;
        total_IOPS_last_time = 0.;
    }

    this_measurement = 0.0;
    prev_measurement = 0.0;


    ivytime t; t.setToNow();
    timestamp = t.format_as_datetime_with_ns();


    best_first_last_valid = false;

    things_by_measurement.clear();
}

void RollupInstance::print_common_columns(std::ostream& os)
{
    os << "," << timestamp;
    os << "," << m_s.focus_metric_ID();
    os << "," << m_s.target_value;

    std::string P_string; {std::ostringstream o; o << P; P_string = o.str();}
    std::string I_string; {std::ostringstream o; o << I; I_string = o.str();}
    std::string D_string; {std::ostringstream o; o << D; D_string = o.str();}

    os << "," << quote_wrap_LDEV_PG_leading_zero_number( std::string("p=") + P_string + std::string("/i=") + I_string + std::string("/d=") + D_string ,m_s.formula_wrapping);
    os << "," << quote_wrap_LDEV_PG_leading_zero_number(P_string,m_s.formula_wrapping);
    os << "," << quote_wrap_LDEV_PG_leading_zero_number(I_string,m_s.formula_wrapping);
    os << "," << quote_wrap_LDEV_PG_leading_zero_number(D_string,m_s.formula_wrapping);
    os << "," << quote_wrap_LDEV_PG_leading_zero_number(attributeNameComboID,m_s.formula_wrapping);
    os << "," << quote_wrap_LDEV_PG_leading_zero_number(rollupInstanceID,m_s.formula_wrapping);
    os << "," << quote_wrap_LDEV_PG_leading_zero_number(m_s.testName,m_s.formula_wrapping);
    os << "," << quote_wrap_LDEV_PG_leading_zero_number(m_s.stepName,m_s.formula_wrapping);

}

void RollupInstance::print_subinterval_column()
{
    subinterval_number_sstream  << ',' << (subintervals.sequence.size()-1);
    target_metric_value_sstream << ',' << m_s.target_value;
    metric_value_sstream        << ',' << this_measurement;
    error_sstream               << ',' << error_signal;
    error_integral_sstream      << ',' << error_integral;
    error_derivative_sstream    << ',' << error_derivative;
    p_times_error_sstream       << ',' << p_times_error;
    i_times_integral_sstream    << ',' << i_times_integral;
    d_times_derivative_sstream  << ',' << d_times_derivative;
    total_IOPS_sstream          << ',' << total_IOPS_last_time;
    p_sstream                   << ',' << P;
    i_sstream                   << ',' << I;
    d_sstream                   << ',' << D;
}

void RollupInstance::print_pid_csv_files()
{
    // we print the  header line and one set of the csv file lines in a file in the step folder.
    // but we also print the header line if it's not already there in the overall test folder,
    // and then print one copy of the csv lines there too.

    // gets called once when the RollupInstance declares success or failure with have_pid set.

    const std::string headers {"item,timestamp,focus metric,target value,pid combo token,p,i,d,rollup type,rollup instance,test name,step name,measurements ...\n"};

    std::string test_level_filename, step_level_filename, adaptive_gain_test_filename, adaptive_gain_step_filename;

    test_level_filename         = m_s.testFolder + "/" + m_s.testName + ".PID.csv";
    adaptive_gain_test_filename = m_s.testFolder + "/" + m_s.testName + ".adaptive_gain.txt";

    step_level_filename         = m_s.stepFolder + "/" + m_s.testName + "." + m_s.stepNNNN + "." + m_s.stepName + "." + attributeNameComboID + "." + rollupInstanceID + ".PID.csv";
    adaptive_gain_step_filename = m_s.stepFolder + "/" + m_s.testName + "." + m_s.stepNNNN + "." + m_s.stepName + "." + attributeNameComboID + "." + rollupInstanceID + ".adaptive_gain.txt";

    {
        std::ostringstream o;
        o << m_s.stepNNNN << " " << m_s.stepName
            << " adaptive PID for " << attributeNameComboID << " = " << rollupInstanceID
            << " \"max_monotone\" gain increase cycles = " << m_count
            << ", \"balanced_step_direction_by\" gain increase cycles = " << b_count
            << ", \"max_ripple\" gain decreases = " << d_count
            << ", gain adjustment stopped at subinterval " << m_s.last_gain_adjustment_subinterval
            << ", followed by " << (m_s.harvesting_subinterval - m_s.last_gain_adjustment_subinterval) << " subintervals available for measurement"
            << ", total step subintervals = " << ( 1 + m_s.harvesting_subinterval )
            << "\n";

        log(m_s.masterlogfile, o.str());

        o << gain_history.str();

        log(adaptive_gain_test_filename, o.str());
        log(adaptive_gain_step_filename, o.str());
    }

    struct stat struct_stat;
    if ( 0 != stat(test_level_filename.c_str(),&struct_stat))
    {
        // file does not already exist (first test step), print header line
        fileappend(test_level_filename,headers);
    }

    {
        std:: ostringstream o;
        o << total_IOPS_sstream.str() << std::endl;

        fileappend(test_level_filename,o.str());
    }

    {
        std:: ostringstream o;
        o << total_IOPS_sstream.str() << std::endl;
        o << subinterval_number_sstream.str() << std::endl;
        o << metric_value_sstream.str() << std::endl;
        o << target_metric_value_sstream.str() << std::endl;
        o << error_sstream.str() << std::endl;
        o << error_integral_sstream.str() << std::endl;
        o << error_derivative_sstream.str() << std::endl;
        o << p_times_error_sstream.str() << std::endl;
        o << i_times_integral_sstream.str() << std::endl;
        o << d_times_derivative_sstream.str() << std::endl;
        o << p_sstream.str() << std::endl;
        o << i_sstream.str() << std::endl;
        o << d_sstream.str() << std::endl;

        fileappend(step_level_filename,headers);
        fileappend(step_level_filename,o.str());
    }

    return;
}

void RollupInstance::set_by_subinterval_filename()
{
    std::string instanceFilename = rollupInstanceID;
    std::replace(instanceFilename.begin(),instanceFilename.end(),'/','_');
    std::replace(instanceFilename.begin(),instanceFilename.end(),'\\','_');

    std::ostringstream filename_prefix;
    filename_prefix << pRollupType->step_subfolder_name << "/"
                    //<< edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(m_s.testName)
                    << m_s.testName
                    << '.'
                    << m_s.stepNNNN
                    << '.'
                    << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(m_s.stepName)
                    << '.'
                    << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(attributeNameComboID)
                    << '='
                    << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(rollupInstanceID)
                    << '.';

    csv_filename_prefix = filename_prefix.str();

    by_subinterval_csv_filename = csv_filename_prefix + std::string("csv");
}

void RollupInstance::print_by_subinterval_header()
{
    std::ostringstream o;
    o << "ivy version,build date,";
    if (m_s.command_device_etc_version.size() > 0 ) o << "subsystem version,";
    o << "Test Name,Step Number,Step Name,Start,Measurement Duration,Write Pending,Subinterval Number,Phase,Rollup Type,Rollup Instance,Workload Count";
    if ( m_s.haveCmdDev ) { o << Test_config_thumbnail::csv_headers(); }
    o << IosequencerInputRollup::CSVcolumnTitles();
    o << ",Rollup Total IOPS Setting";
    o << avgcpubusypercent::csvTitles();
    o << Subinterval_CPU_temp::csvTitles();
    if (m_s.haveCmdDev)
    {
        o << subsystem_summary_data::csvHeadersPartOne();
        o << ",host IOPS per drive,host decimal MB/s per drive,application service time Littles law q depth per drive";
        if ( ! m_s.suppress_subsystem_perf ) o << ",Subsystem IOPS as % of application IOPS,Subsystem MB/s as % of application MB/s,Subsystem service time as % of application service time,OS Path Latency = application minus subsystem service time (ms)";
    }

    o << SubintervalOutput::csvTitles();

    if (m_s.haveCmdDev)
    {
        o << subsystem_summary_data::csvHeadersPartTwo();
        o << subsystem_summary_data::csvHeadersPartOne( "non-participating ");
        o << subsystem_summary_data::csvHeadersPartTwo( "non-participating ");
    }

    o << ",IOPS histogram by service time scaled by bucket width (ms):";
    for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
    o << ",IOPS histogram by submit time scaled by bucket width (ms):";
    for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
    o << ",IOPS histogram by random read service time scaled by bucket width ms):";
    for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
    o << ",IOPS histogram by random write service time (scaled by bucket width ms):";
    for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
    o << ",IOPS histogram by sequential read service time (scaled by bucket width ms):";
    for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
    o << ",IOPS histogram by sequential write service time scaled by bucket width (ms):";
    for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);

    o << std::endl;
    fileappend( by_subinterval_csv_filename, o.str() );
}

void RollupInstance::print_config_thumbnail() //
{
    std::ostringstream o;
    o << test_config_thumbnail; // The thumbnail is an object that knows how to print itself.
    ofstream ofs(csv_filename_prefix + std::string("config_thumbnail.txt"));
    ofs << o.str();
    ofs.close();
}

void RollupInstance::print_subinterval_csv_line(
    unsigned int i,     // subinterval_number from zero
    bool is_provisional // if provisional, don't try to print "warmup" / "measurement" / "cooldown"
)
{
    std::ostringstream csvline;

    csvline << ivy_version;
    csvline << "," << IVYBUILDDATE;

    if (m_s.command_device_etc_version.size() > 0 ) csvline << "," << m_s.command_device_etc_version;

    csvline << "," << m_s.testName;
    csvline << "," << m_s.stepNNNN;
    csvline << "," << m_s.stepName;

    const auto& start_end = m_s.subinterval_start_end_times[i];

    ivytime duration = start_end.second - start_end.first;

    ivy_float seconds = duration.getlongdoubleseconds();

    csvline << ',' << start_end.first.format_as_datetime()
            << ',' << duration.format_as_duration_HMMSSns()
            << ',' ; // beginning of the Write Pending field

    if ( m_s.cooldown_WP_watch_set.size() && (!m_s.suppress_subsystem_perf) )
    {
        try
        {
            csvline << m_s.getCLPRthumbnail(i);
        }
        catch (std::invalid_argument& iae)
        {
            std::ostringstream o;
            o << std::endl << "writhe and fail, gasping that when printing csv line, when getting Write Pending thumbnail, failed saying - " << iae.what() << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
            exit(-1);
        }
    }

    csvline << ',' << i;

    csvline << ',';
    if (!is_provisional)
    {
        if (i < m_s.measurement_by_subinterval.size())
        {
            csvline << m_s.measurements[m_s.measurement_by_subinterval[i]].phase(i);
        }
        else
        {
            csvline << "cooldown";
        }
    }
    csvline << ',' << pRollupType->attributeNameCombo.attributeNameComboID;

    csvline << ',' << rollupInstanceID;

    csvline << "," << workloadIDs.workloadIDs.size();

    if (m_s.haveCmdDev) { csvline << test_config_thumbnail.csv_columns(); }

    csvline << subintervals.sequence[i].inputRollup.CSVcolumnValues(true); // true shows how many occurences of each value, false shows "multiple"

    ivy_float total_IOPS_setting = subintervals.sequence[i].inputRollup.get_Total_IOPS_Setting();

    csvline << "," << IosequencerInputRollup::Total_IOPS_Setting_toString(total_IOPS_setting);

    {
        csvline << ','; // host CPU % busy - cols

        if (stringCaseInsensitiveEquality(std::string("host"),pRollupType->attributeNameCombo.attributeNameComboID))
        {
            csvline << m_s.cpu_by_subinterval[i].csvValues(toLower(rollupInstanceID));
            csvline << m_s.cpu_degrees_C_from_critical_temp_by_subinterval[i].csvValues(toLower(rollupInstanceID));
        }
        else
        {
            csvline << m_s.cpu_by_subinterval[i].csvValuesAvgOverHosts();
            csvline << m_s.cpu_degrees_C_from_critical_temp_by_subinterval[i].csvValuesAvgOverHosts();
        }
    }

    if (m_s.haveCmdDev)
    {
        csvline << subsystem_data_by_subinterval[i].csvValuesPartOne(1);


        unsigned int overall_drive_count = test_config_thumbnail.total_drives();

        if (overall_drive_count == 0)
        {
            csvline << ",,,";
        }
        else
        {
            RunningStat<ivy_float,ivy_int> st = subintervals.sequence[i].outputRollup.overall_service_time_RS();
            RunningStat<ivy_float,ivy_int> bt = subintervals.sequence[i].outputRollup.overall_bytes_transferred_RS();
            ivy_float iops_per_drive = (st.count()/seconds) /  ((ivy_float) overall_drive_count);
            csvline << "," <<  iops_per_drive;
            csvline << "," <<  (   1e-6 /* decimal MB/s */ * (bt.sum()/seconds)   /   ((ivy_float) overall_drive_count)     );

            //, host service time Littles law q depth per drive
            csvline << "," << std::fixed << std::setprecision(1) << (st.avg()*iops_per_drive);
        }

        if ( ! m_s.suppress_subsystem_perf )
        {
            // print the comparisions between application & subsystem IOPS, MB/s, service time

            ivy_float subsystem_IOPS = subsystem_data_by_subinterval[i].IOPS();
            ivy_float subsystem_decimal_MB_per_second = subsystem_data_by_subinterval[i].decimal_MB_per_second();
            ivy_float subsystem_service_time_ms = subsystem_data_by_subinterval[i].service_time_ms();

            // ",Subsystem IOPS as % of application IOPS,Subsystem MB/s as % of application MB/s,Subsystem service time as % of application service time,Path latency = application minus subsystem service time (ms)";

            csvline << ','; // subsystem IOPS as % of application IOPS
            if (subsystem_IOPS > 0.0)
            {
                RunningStat<ivy_float,ivy_int> st = subintervals.sequence[i].outputRollup.overall_service_time_RS();
                ivy_float application_IOPS = st.count() / seconds;
                if (application_IOPS > 0.0)
                {
                    csvline << std::fixed << std::setprecision(2) << (subsystem_IOPS / application_IOPS) * 100.0 << '%';
                }
            }

            csvline << ','; // Subsystem MB/s as % of application MB/s
            if (subsystem_decimal_MB_per_second > 0.0)
            {
                RunningStat<ivy_float,ivy_int> bt = subintervals.sequence[i].outputRollup.overall_bytes_transferred_RS();
                ivy_float application_decimal_MB_per_second = 1E-6 * bt.sum() / seconds;
                if (application_decimal_MB_per_second > 0.0)
                {
                    csvline << std::fixed << std::setprecision(2) << (subsystem_decimal_MB_per_second / application_decimal_MB_per_second) * 100.0 << '%';
                }
            }

            csvline << ','; // Subsystem service time as % of application service time
            if (subsystem_service_time_ms > 0.0)
            {
                RunningStat<ivy_float,ivy_int> st = subintervals.sequence[i].outputRollup.overall_service_time_RS();
                ivy_float application_service_time_ms = 1000.* st.avg();
                if (application_service_time_ms > 0.0)
                {
                    csvline << std::fixed << std::setprecision(2) << (subsystem_service_time_ms / application_service_time_ms) * 100.0 << '%';
                }
            }

            csvline << ','; // Path latency = application minus subsystem service time (ms)
            if (subsystem_service_time_ms > 0.0)
            {
                RunningStat<ivy_float,ivy_int> st = subintervals.sequence[i].outputRollup.overall_service_time_RS();
                ivy_float application_service_time_ms = 1000. * st.avg();
                if (application_service_time_ms > 0.0)
                {
                    csvline << std::fixed << std::setprecision(2) << (application_service_time_ms - subsystem_service_time_ms);
                }
            }
        }
    }

    csvline << subintervals.sequence[i].outputRollup.csvValues( seconds );

    if ( m_s.haveCmdDev )
    {
        csvline << subsystem_data_by_subinterval[i].csvValuesPartTwo();
        csvline << m_s.rollups.not_participating[i].csvValuesPartOne();
        csvline << m_s.rollups.not_participating[i].csvValuesPartTwo();
    }

    csvline << ",subinterval " << i;
    for (int bucket=0; bucket < io_time_buckets; bucket++)
    {
        csvline << ',';
        RunningStat<ivy_float,ivy_int>
        rs  = subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[0][0][bucket]; // random read
        rs += subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[0][1][bucket]; // random write
        rs += subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[1][0][bucket]; // sequential read
        rs += subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[1][1][bucket]; // sequential write
        if (rs.count()>0) csvline << (histogram_bucket_scale_factor(bucket) * (ivy_float) rs.count() / seconds);
    }

    csvline << ",subinterval " << i;
    for (int bucket=0; bucket < io_time_buckets; bucket++)
    {
        csvline << ',';
        RunningStat<ivy_float,ivy_int>
        rs  = subintervals.sequence[i].outputRollup.u.a.submit_time.rs_array[0][0][bucket];
        rs += subintervals.sequence[i].outputRollup.u.a.submit_time.rs_array[0][1][bucket];
        rs += subintervals.sequence[i].outputRollup.u.a.submit_time.rs_array[1][0][bucket];
        rs += subintervals.sequence[i].outputRollup.u.a.submit_time.rs_array[1][1][bucket];
        if (rs.count()>0) csvline << (histogram_bucket_scale_factor(bucket) * (ivy_float) rs.count() / seconds);
    }

    csvline << ",subinterval " << i;
    for (int bucket=0; bucket < io_time_buckets; bucket++) // random read
    {
        csvline << ',';
        RunningStat<ivy_float,ivy_int> rs = subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[0][0][bucket];
        if (rs.count()>0) csvline << (histogram_bucket_scale_factor(bucket) * (ivy_float) rs.count() / seconds);
    }

    csvline << ",subinterval " << i;
    for (int bucket=0; bucket < io_time_buckets; bucket++) // random write
    {
        csvline << ',';
        RunningStat<ivy_float,ivy_int> rs = subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[0][1][bucket];
        if (rs.count()>0) csvline << (histogram_bucket_scale_factor(bucket) * (ivy_float) rs.count() / seconds);
    }

    csvline << ",subinterval " << i;
    for (int bucket=0; bucket < io_time_buckets; bucket++) // sequential read
    {
        csvline << ',';
        RunningStat<ivy_float,ivy_int> rs = subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[1][0][bucket];
        if (rs.count()>0) csvline << (histogram_bucket_scale_factor(bucket) * (ivy_float) rs.count() / seconds);
    }

    csvline << ",subinterval " << i;
    for (int bucket=0; bucket < io_time_buckets; bucket++) // sequential write
    {
        csvline << ',';
        RunningStat<ivy_float,ivy_int> rs = subintervals.sequence[i].outputRollup.u.a.service_time.rs_array[1][1][bucket];
        if (rs.count()>0) csvline << (histogram_bucket_scale_factor(bucket) * (ivy_float) rs.count() / seconds);
    }

    {
        std::ostringstream o; o << quote_wrap_csvline_LDEV_PG_leading_zero_number(csvline.str(), m_s.formula_wrapping) << std::endl;
        fileappend(by_subinterval_csv_filename,o.str());
    }
}


void RollupInstance::print_measurement_summary_csv_line(unsigned int measurement_index)
{
    measurement_things& mt = things_by_measurement[measurement_index];
    SubintervalRollup& mr = mt.measurementRollup;
    SubintervalOutput& mro = mr.outputRollup;
    subsystem_summary_data& ssd = mt.subsystem_data;

    const std::string& rollupTypeName = pRollupType->attributeNameCombo.attributeNameComboID;
    struct stat struct_stat;

    if (routine_logging)
    {
        std::ostringstream o;
        o << std::endl << "Making csv files for " << attributeNameComboID << "=" << rollupInstanceID << "." << std::endl;
        log(m_s.masterlogfile,o.str());
    }

    // First print the by-test_step measurement period csv file lines in the appropriate RollupType subfolder of output folder
    // Write the csv header line if necessary
    // Write the csv data line for the measurement rollup
    {
        std::string instanceFilename = rollupInstanceID;
        std::replace(instanceFilename.begin(),instanceFilename.end(),'/','_');
        std::replace(instanceFilename.begin(),instanceFilename.end(),'\\','_');

        std::ostringstream filename_prefix;
        filename_prefix << pRollupType->measurementRollupFolder << "/"
                        //<< edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(m_s.testName )
                        << m_s.testName
                        << '.'
                        << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(rollupTypeName)
                        << '='
                        << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(instanceFilename)
                        << '.';

        std::ostringstream type_filename_prefix;
        type_filename_prefix << pRollupType->measurementRollupFolder << "/"
                        //<< edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(m_s.testName )
                        << m_s.testName
                        << '.'
                        << edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(rollupTypeName)
                        << '.';

        measurement_rollup_by_test_step_csv_filename      =      filename_prefix.str() + std::string("summary.csv");
        measurement_rollup_by_test_step_csv_type_filename = type_filename_prefix.str() + std::string("summary.csv");

        bool need_header      = (0 != stat(measurement_rollup_by_test_step_csv_filename.c_str(),&struct_stat));
        bool need_type_header = (0 != stat(measurement_rollup_by_test_step_csv_type_filename.c_str(),&struct_stat));

        if ( need_header || need_type_header)
        {
            // we need to print the header line.
            {
                std::ostringstream o;

                o << "ivy version,build date,";

                if (m_s.command_device_etc_version.size() > 0 ) o << "subsystem version,";

                o << "Test Name,Step Number,Step Name,Start,Warmup,Measurement Duration,Cooldown,Write Pending,valid or invalid,invalid reason,Rollup Type,Rollup Instance,Workload Count";

                if (m_s.haveCmdDev) { o << test_config_thumbnail.csv_headers(); }
                o << IosequencerInputRollup::CSVcolumnTitles();
                o << ",Rollup Total IOPS Setting";
                o << m_s.measurements[measurement_index].measurement_rollup_CPU.csvTitles();
                o << Subinterval_CPU_temp::csvTitles();

                if (std::string("all") == toLower(attributeNameComboID) && std::string("all") == toLower(rollupInstanceID))
                {
                    o << ",Test Host CPU time per I/O microseconds";
                }

                if (m_s.haveCmdDev)
                {
                    o << subsystem_summary_data::csvHeadersPartOne();
                    o << ",host IOPS per drive,host decimal MB/s per drive, application service time Littles law q depth per drive";
                    o << ",Subsystem IOPS as % of application IOPS,Subsystem MB/s as % of application MB/s,Subsystem service time as % of application service time"
                        << ",OS Path Latency = application minus subsystem service time (ms)";
                }

                o << ",non random sample correction factor";
                o << ",plus minus series statistical confidence";

                o << ",achieved IOPS as % of Total_IOPS setting";

                o << SubintervalOutput::csvTitles(true);
                if (m_s.haveCmdDev)
                {
                    o << subsystem_summary_data::csvHeadersPartTwo();
                    o << subsystem_summary_data::csvHeadersPartOne("non-participating ");
                    o << subsystem_summary_data::csvHeadersPartTwo("non-participating ");
                }
                o << ",service time true IOPS histogram:";
                for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                o << ",service time histogram normalized by step total IOPS:";
                for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                o << ",service time histogram scaled by bucket width:";
                for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                o << ",service time histogram scaled by bucket width and normalized by step total IOPS:";
                for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);

                o << ",submit time true IOPS histogram:";
                for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                o << ",submit time histogram normalized by step total IOPS:";
                for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                o << ",submit time histogram scaled by bucket width:";
                for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                o << ",submit time histogram scaled by bucket width and normalized by step total IOPS:";
                for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);

                o << ",random read service time true IOPS histogram:";
                for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                o << ",random write service time true IOPS histogram:";
                for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                o << ",sequential read service time true IOPS histogram:";
                for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                o << ",sequential write service time true IOPS histogram:";
                for (int i=0; i<io_time_buckets; i++) o << ',' << std::get<0>(io_time_clip_levels[i]);
                o << std::endl;

                if (need_header)      fileappend( measurement_rollup_by_test_step_csv_filename, o.str() );
                if (need_type_header) fileappend( measurement_rollup_by_test_step_csv_type_filename, o.str() );
            }
        }

        std::ostringstream csvline;

        csvline << ivy_version << ',' << IVYBUILDDATE;

        if (m_s.command_device_etc_version.size() > 0 ) { csvline << ","; csvline << m_s.command_device_etc_version; }

        csvline << "," << m_s.testName;
        csvline << "," << m_s.stepNNNN; if (m_s.have_IOPS_staircase) {csvline << "#" << measurement_index;}
        csvline << "," << m_s.stepName; if (m_s.have_IOPS_staircase) {csvline << ":" << convert_commas_to_semicolons(m_s.measurements[measurement_index].edit_rollup_text);}

        ivy_float seconds = mr.durationSeconds();

        ivy_float subsystem_IOPS = 0.0;
        ivy_float application_IOPS = 0.0;
        {
            const RunningStat<ivy_float,ivy_int>& st = mr.outputRollup.overall_service_time_RS();
            application_IOPS = st.count() / seconds;
        }
        ivy_float subsystem_IOPS_as_fraction_of_host_IOPS = -1.0;

        if (m_s.haveCmdDev)
        {
            subsystem_IOPS = ssd.IOPS();
            if (subsystem_IOPS > 0)
            {
                if (application_IOPS > 0)
                {
                    subsystem_IOPS_as_fraction_of_host_IOPS = subsystem_IOPS / application_IOPS;
                }
            }
        }

        if (m_s.measurements[measurement_index].success || m_s.measurements[measurement_index].have_timeout_rollup)
        {
            csvline << ',' << m_s.measurements[measurement_index].warmup_start.format_as_datetime();
            csvline << ',' << m_s.measurements[measurement_index].warmup_duration().format_as_duration_HMMSS();
            csvline << ',' << m_s.measurements[measurement_index].measure_duration().format_as_duration_HMMSS();
            csvline << ',' << m_s.measurements[measurement_index].cooldown_duration().format_as_duration_HMMSS();
            csvline << ','; // Write Pending only shows in by-subinterval csv lines

            measurement_things& mt = things_by_measurement[measurement_index];

            if
            (
                   m_s.measurements[measurement_index].have_timeout_rollup
                || !m_s.rollups.passesDataVariationValidation().first
                || mt.subsystem_IOPS_as_fraction_of_host_IOPS_failure
                || mt.failed_to_achieve_total_IOPS_setting
            )
            {
                csvline << ",invalid"; // subintervalIndex;
            }
            else
            {
                csvline << ",valid"; // subintervalIndex;
            }

            std::string validation_errors {};

            if (m_s.measurements[measurement_index].have_timeout_rollup) validation_errors = "[measurement timeout]";

            validation_errors += m_s.rollups.passesDataVariationValidation().second;

            if (mt.subsystem_IOPS_as_fraction_of_host_IOPS_failure)
                validation_errors += std::string("[subsystem IOPS is more than 15% different from host IOPS.]");

            if (mt.failed_to_achieve_total_IOPS_setting)
                validation_errors += std::string("[Achieved IOPS off by more than .1% from Rollup Total IOPS Setting.]");

            csvline << ',' << validation_errors;

            csvline << ',' << pRollupType->attributeNameCombo.attributeNameComboID;
            csvline << ',' << rollupInstanceID ; // rollupInstanceID;
            csvline << "," << workloadIDs.workloadIDs.size();

            if (m_s.haveCmdDev) { csvline << test_config_thumbnail.csv_columns(); }

            csvline << mr.inputRollup.CSVcolumnValues(true);
                // true shows how many occurences of each value, false shows "multiple"

            csvline << "," << mt.Total_IOPS_setting_string;

            measurement& m = m_s.measurements[measurement_index];

            csvline << ','; // CPU - 2 columns
            if (stringCaseInsensitiveEquality(std::string("host"),pRollupType->attributeNameCombo.attributeNameComboID))
            {
                csvline << m.measurement_rollup_CPU.csvValues(toLower(rollupInstanceID));
                csvline << m.measurement_rollup_CPU_temp.csvValues(toLower(rollupInstanceID));
            }
            else
            {
                csvline << m.measurement_rollup_CPU.csvValuesAvgOverHosts();
                csvline << m.measurement_rollup_CPU_temp.csvValuesAvgOverHosts();
            }

            if (std::string("all") == toLower(attributeNameComboID) && std::string("all") == toLower(rollupInstanceID))
            {
                csvline << "," <<
                (
                    1E6 *   // this turns the answer into microseconds
                    ((m.measurement_rollup_CPU.overallCPU.sum_activecores_total / ((ivy_float) m.measurement_rollup_CPU.rollup_count))/100.0) // this is the fractional sum of overall % busy
                    /
                    (((ivy_float)mr.outputRollup.overall_service_time_RS().count())/mr.durationSeconds()) // this is the IOPS
                );
            }

            if (m_s.haveCmdDev)
            {
                csvline << ssd.csvValuesPartOne( m.lastMeasurementIndex + 1 - m.firstMeasurementIndex );

                unsigned int overall_drive_count = test_config_thumbnail.total_drives();

                if (overall_drive_count == 0)
                {
                    csvline << ",,,";
                }
                else
                {
                    RunningStat<ivy_float,ivy_int> st = mr.outputRollup.overall_service_time_RS();
                    RunningStat<ivy_float,ivy_int> bt = mr.outputRollup.overall_bytes_transferred_RS();

                    ivy_float secs=mr.durationSeconds();
                    ivy_float iops_per_drive, decMBps_per_drive;

                    iops_per_drive = (((ivy_float) st.count()) / secs ) / ((ivy_float) overall_drive_count);
                    decMBps_per_drive = 1e-6 * (bt.sum()/secs)/((ivy_float) overall_drive_count);
                    csvline << "," << std::fixed << std::setprecision(1) <<  iops_per_drive;
                    csvline << "," << std::fixed << std::setprecision(1) <<  decMBps_per_drive;

                    //, application service time Littles law q depth per drive
                    csvline << "," << std::fixed << std::setprecision(1) << (st.avg()*iops_per_drive);
                }
            }


            {
                if (m_s.haveCmdDev)
                {
                    // print the comparisions between application & subsystem IOPS, MB/s, service time


                    ivy_float subsystem_decimal_MB_per_second = ssd.decimal_MB_per_second();
                    ivy_float subsystem_service_time_ms = ssd.service_time_ms();

                    // ",Subsystem IOPS as % of application IOPS,Subsystem MB/s as % of application MB/s,Subsystem service time as % of application service time,Path latency = application minus subsystem service time (ms)";

                    csvline << ','; // subsystem IOPS as % of application IOPS
                    if (subsystem_IOPS > 0.0)
                    {
                        if (application_IOPS > 0.0)
                        {
                            csvline << std::fixed << std::setprecision(3) << (subsystem_IOPS_as_fraction_of_host_IOPS * 100.0) << '%';
                        }
                    }

                    csvline << ','; // Subsystem MB/s as % of application MB/s
                    if (subsystem_decimal_MB_per_second > 0.0)
                    {
                        RunningStat<ivy_float,ivy_int> bt = mro.overall_bytes_transferred_RS();
                        ivy_float application_decimal_MB_per_second = 1E-6 * bt.sum() / seconds;
                        if (application_decimal_MB_per_second > 0.0)
                        {
                            csvline << std::fixed << std::setprecision(3) << (subsystem_decimal_MB_per_second / application_decimal_MB_per_second) * 100.0 << '%';
                        }
                    }

                    csvline << ','; // Subsystem service time as % of application service time
                    if (subsystem_service_time_ms > 0.0)
                    {
                        RunningStat<ivy_float,ivy_int> st = mro.overall_service_time_RS();
                        ivy_float application_service_time_ms = 1000.* st.avg();
                        if (application_service_time_ms > 0.0)
                        {
                            csvline << std::fixed << std::setprecision(3) << (subsystem_service_time_ms / application_service_time_ms) * 100.0 << '%';
                        }
                    }

                    csvline << ','; // Path latency = application minus subsystem service time (ms)
                    if (subsystem_service_time_ms > 0.0)
                    {
                        RunningStat<ivy_float,ivy_int> st = mro.overall_service_time_RS();
                        ivy_float application_service_time_ms = 1000. * st.avg();
                        if (application_service_time_ms > 0.0)
                        {
                            csvline << std::fixed << std::setprecision(3) << (application_service_time_ms - subsystem_service_time_ms);
                        }
                    }
                }

                csvline << "," << m_s.non_random_sample_correction_factor;
                csvline << "," << plus_minus_series_confidence_default;

                csvline << "," << achieved_IOPS_as_percent_of_Total_IOPS_settting;

                csvline << mro.csvValues(seconds,&(mr),m_s.non_random_sample_correction_factor, (!mt.failed_to_achieve_total_IOPS_setting));

                if (m_s.haveCmdDev)
                {
                    auto n = m.lastMeasurementIndex + 1 - m.firstMeasurementIndex ;
                    csvline << ssd.csvValuesPartTwo( n );
                    csvline << m_s.rollups.not_participating_measurement[measurement_index].csvValuesPartOne( n );
                    csvline << m_s.rollups.not_participating_measurement[measurement_index].csvValuesPartTwo();
                }

                // histograms
                {
                    RunningStat<ivy_float,ivy_int> st = mro.overall_service_time_RS();
                    ivy_float total_IOPS = st.count() / seconds;

                    // ",service time true IOPS histogram:";
                    csvline << "," << m_s.stepName;
                    for (int i=0; i < io_time_buckets; i++)
                    {
                        csvline << ',';
                        RunningStat<ivy_float,ivy_int>
                        rs  = mro.u.a.service_time.rs_array[0][0][i];
                        rs += mro.u.a.service_time.rs_array[0][1][i];
                        rs += mro.u.a.service_time.rs_array[1][0][i];
                        rs += mro.u.a.service_time.rs_array[1][1][i];
                        if (rs.count()>0) csvline << ((ivy_float) rs.count() / seconds);
                    }

                    // ",service time histogram normalized by step total IOPS:";
                    csvline << "," << m_s.stepName;
                    for (int i=0; i < io_time_buckets; i++)
                    {
                        csvline << ',';

                        if (total_IOPS > 0.)
                        {
                            RunningStat<ivy_float,ivy_int>
                            rs  = mro.u.a.service_time.rs_array[0][0][i];
                            rs += mro.u.a.service_time.rs_array[0][1][i];
                            rs += mro.u.a.service_time.rs_array[1][0][i];
                            rs += mro.u.a.service_time.rs_array[1][1][i];
                            if (rs.count()>0) csvline << ( ((ivy_float) rs.count() / seconds) / total_IOPS);
                        }
                    }
                    // ",service time histogram scaled by bucket width:";

                    csvline << "," << m_s.stepName;
                    for (int i=0; i < io_time_buckets; i++)
                    {
                        csvline << ',';
                        RunningStat<ivy_float,ivy_int>
                        rs  = mro.u.a.service_time.rs_array[0][0][i];
                        rs += mro.u.a.service_time.rs_array[0][1][i];
                        rs += mro.u.a.service_time.rs_array[1][0][i];
                        rs += mro.u.a.service_time.rs_array[1][1][i];
                        if (rs.count()>0) csvline << (histogram_bucket_scale_factor(i) * (ivy_float) rs.count() / seconds);
                    }

                    // ",service time histogram scaled by bucket width and normalized by step total IOPS:";
                    csvline << "," << m_s.stepName;
                    for (int i=0; i < io_time_buckets; i++)
                    {
                        csvline << ',';

                        if (total_IOPS > 0.)
                        {
                            RunningStat<ivy_float,ivy_int>
                            rs  = mro.u.a.service_time.rs_array[0][0][i];
                            rs += mro.u.a.service_time.rs_array[0][1][i];
                            rs += mro.u.a.service_time.rs_array[1][0][i];
                            rs += mro.u.a.service_time.rs_array[1][1][i];
                            if (rs.count()>0) csvline << ( (histogram_bucket_scale_factor(i) * (ivy_float) rs.count() / seconds) / total_IOPS);
                        }
                    }
                    /////////////////////////////////////////////////////////////////////////////////////
                    // ",submit time true IOPS histogram:";
                    csvline << "," << m_s.stepName;
                    for (int i=0; i < io_time_buckets; i++)
                    {
                        csvline << ',';
                        RunningStat<ivy_float,ivy_int>
                        rs  = mro.u.a.submit_time.rs_array[0][0][i];
                        rs += mro.u.a.submit_time.rs_array[0][1][i];
                        rs += mro.u.a.submit_time.rs_array[1][0][i];
                        rs += mro.u.a.submit_time.rs_array[1][1][i];
                        if (rs.count()>0) csvline << ((ivy_float) rs.count() / seconds);
                    }

                    // ",submit time histogram normalized by step total IOPS:";
                    csvline << "," << m_s.stepName;
                    for (int i=0; i < io_time_buckets; i++)
                    {
                        csvline << ',';

                        if (total_IOPS > 0.)
                        {
                            RunningStat<ivy_float,ivy_int>
                            rs  = mro.u.a.submit_time.rs_array[0][0][i];
                            rs += mro.u.a.submit_time.rs_array[0][1][i];
                            rs += mro.u.a.submit_time.rs_array[1][0][i];
                            rs += mro.u.a.submit_time.rs_array[1][1][i];
                            if (rs.count()>0) csvline << ( ((ivy_float) rs.count() / seconds) / total_IOPS);
                        }
                    }
                    // ",submit time histogram scaled by bucket width:";

                    csvline << "," << m_s.stepName;
                    for (int i=0; i < io_time_buckets; i++)
                    {
                        csvline << ',';
                        RunningStat<ivy_float,ivy_int>
                        rs  = mro.u.a.submit_time.rs_array[0][0][i];
                        rs += mro.u.a.submit_time.rs_array[0][1][i];
                        rs += mro.u.a.submit_time.rs_array[1][0][i];
                        rs += mro.u.a.submit_time.rs_array[1][1][i];
                        if (rs.count()>0) csvline << (histogram_bucket_scale_factor(i) * (ivy_float) rs.count() / seconds);
                    }

                    // ",submit time histogram scaled by bucket width and normalized by step total IOPS:";
                    csvline << "," << m_s.stepName;
                    for (int i=0; i < io_time_buckets; i++)
                    {
                        csvline << ',';

                        if (total_IOPS > 0.)
                        {
                            RunningStat<ivy_float,ivy_int>
                            rs  = mro.u.a.submit_time.rs_array[0][0][i];
                            rs += mro.u.a.submit_time.rs_array[0][1][i];
                            rs += mro.u.a.submit_time.rs_array[1][0][i];
                            rs += mro.u.a.submit_time.rs_array[1][1][i];
                            if (rs.count()>0) csvline << ( (histogram_bucket_scale_factor(i) * (ivy_float) rs.count() / seconds) / total_IOPS);
                        }
                    }
                }
                /////////////////////////////////////////////////////////////////////////////////////
                // ",random read service time true IOPS histogram:";
                csvline << "," << m_s.stepName;
                for (int i=0; i < io_time_buckets; i++)
                {
                    csvline << ',';
                    RunningStat<ivy_float,ivy_int> rs = mro.u.a.service_time.rs_array[0][0][i];
                    if (rs.count()>0) csvline << (histogram_bucket_scale_factor(i) * (ivy_float) rs.count() / seconds);
                }

                // ",random write service time true IOPS histogram:";
                csvline << "," << m_s.stepName;
                for (int i=0; i < io_time_buckets; i++)
                {
                    csvline << ',';
                    RunningStat<ivy_float,ivy_int> rs = mro.u.a.service_time.rs_array[0][1][i];
                    if (rs.count()>0) csvline << (histogram_bucket_scale_factor(i) * (ivy_float) rs.count() / seconds);
                }

                // ",sequential read service time true IOPS histogram:";
                csvline << "," << m_s.stepName;
                for (int i=0; i < io_time_buckets; i++)
                {
                    csvline << ',';
                    RunningStat<ivy_float,ivy_int> rs = mro.u.a.service_time.rs_array[1][0][i];
                    if (rs.count()>0) csvline << (histogram_bucket_scale_factor(i) * (ivy_float) rs.count() / seconds);
                }

                // ",sequential write service time true IOPS histogram:";
                csvline << "," << m_s.stepName;
                for (int i=0; i < io_time_buckets; i++)
                {
                    csvline << ',';
                    RunningStat<ivy_float,ivy_int> rs = mro.u.a.service_time.rs_array[1][1][i];
                    if (rs.count()>0) csvline << (histogram_bucket_scale_factor(i) * (ivy_float) rs.count() / seconds);
                }

            }


            {
                std::ostringstream o; o << quote_wrap_csvline_LDEV_PG_leading_zero_number(csvline.str(),m_s.formula_wrapping) << std::endl;
                fileappend(measurement_rollup_by_test_step_csv_filename,o.str());
                fileappend(measurement_rollup_by_test_step_csv_type_filename,o.str());
            }

            if (0 == std::string("all").compare(attributeNameComboID)
             && 0 == std::string("all").compare(rollupInstanceID)    )
            {
                std::ostringstream o;

                if (m_s.have_IOPS_staircase)
                {
                    o << "Measurement " << measurement_index << " s";
                }
                else
                {
                    o << "S";
                }
                o << "ummary for \"all=all\" rollup: " << mro.thumbnail(mr.durationSeconds()) << std::endl;

                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
            }
        }
        else
        {
            // failed  to declare success with a measurement

            csvline << "," ; // << start.format_as_datetime();

            ivytime warmup_start, warmup_complete, warmup_duration, cooldown_start, cooldown_complete, cooldown_duration;
            if (-1 == m_s.MeasureCtlr_failure_point)
            {
                warmup_start = m_s.rollups.starting_ending_times[0].first;
                warmup_complete = m_s.rollups.starting_ending_times[m_s.rollups.starting_ending_times.size() - 1 ].second;
                warmup_duration = warmup_complete - warmup_start;
                cooldown_duration = ivytime(0);
            }
            else
            {
                warmup_start = m_s.rollups.starting_ending_times[0].first;
                warmup_complete = m_s.rollups.starting_ending_times[m_s.MeasureCtlr_failure_point].second;
                warmup_duration = warmup_complete - warmup_start;
                if (warmup_complete == m_s.rollups.starting_ending_times[m_s.rollups.starting_ending_times.size()-1].second)
                {
                    cooldown_duration = ivytime(0);
                }
                else
                {
                    cooldown_start = m_s.rollups.starting_ending_times[m_s.MeasureCtlr_failure_point].second;
                    cooldown_complete = m_s.rollups.starting_ending_times[m_s.rollups.starting_ending_times.size()-1].second;
                    cooldown_duration = cooldown_complete - cooldown_start;
                }
            }

            csvline << ',' << warmup_duration.format_as_duration_HMMSSns();
            csvline << ',' ; // << duration.format_as_duration_HMMSSns();
            csvline << ',' << cooldown_duration.format_as_duration_HMMSSns();
            csvline << ',' ; // Write Pending only shows in by-subinterval csv lines
            csvline << ',' << "failed - timed out"; // subintervalIndex;
            csvline << ',' << "no measurement"; // phase
            csvline << ',' << pRollupType->attributeNameCombo.attributeNameComboID;
            csvline << ',' << rollupInstanceID ; // rollupInstanceID;

            {
                std::ostringstream o; o << quote_wrap_csvline_LDEV_PG_leading_zero_number(csvline.str(), m_s.formula_wrapping) << std::endl;
                fileappend(measurement_rollup_by_test_step_csv_filename,o.str());
            }
        }
    }

    if (m_s.stepcsv) // Re-write by-subinterval csv files, this time filling in "warmup", "measurment", and "cooldown".
    {
        {
            struct stat struct_stat;
            if(0 == stat(by_subinterval_csv_filename.c_str(),&struct_stat))
            {
                // if the file already exists, delete it
                std::remove(by_subinterval_csv_filename.c_str());
            }
            else
            {
                std::ostringstream o;
                o << "<Error> Did not find provisional copy of \"" << by_subinterval_csv_filename << "\" "
                    << "for " << attributeNameComboID << "=" << rollupInstanceID
                    << " when about to replace it with the final version with the \"warmup\" / \"measurement\" / \"cooldown\" column filled in.\n";
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
                m_s.kill_subthreads_and_exit();
                exit(-1);
            }
        }

        print_by_subinterval_header();

        // print the detail lines for each subinterval
        for (unsigned int i = 0; i < (subintervals.sequence).size(); i++)
        {
            print_subinterval_csv_line(i, false /* is not provisional */);
        }
    }
}






