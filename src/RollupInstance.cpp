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
#include <iostream>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <cmath> // for abs()

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
#include "master_stuff.h"
#include "studentsTdistribution.h"

extern std::string masterlogfile();

extern bool routine_logging;

void RollupInstance::printMe(std::ostream& o)
{
	o << "RollupInstance::printMe(std::ostream& o) for attributeNameComboID = " << attributeNameComboID << ", rollupInstanceID = " << rollupInstanceID << std::endl;
	o << "RollupInstance\'s pRollupType->attributeNameCombo.attributeNameComboID = " << pRollupType->attributeNameCombo.attributeNameComboID << std::endl;
	o << "Number of subintervals = " << subintervals.sequence.size() << std::endl;
	o << "ListOfWorkloadIDs workloadIDs =";

	ListOfWorkloadIDs* pListOfWorkloadIDs = &workloadIDs;

	for (auto& s : (pListOfWorkloadIDs->workloadIDs))
	{
		std::cout << std::string(" ") << s.workloadID;
	}
	o << std::endl;
}


std::pair<bool,std::string> RollupInstance::makeMeasurementRollup(unsigned int firstMeasurementIndex, unsigned int lastMeasurementIndex)
{
		if (subintervals.sequence.size() < 3)
		{
			std::ostringstream o;
			o << "RollupInstance::makeMeasurementRollup() - The total number of subintervals in the sequence is " << subintervals.sequence.size()
			<< std::endl << "and there must be at least one warmup subinterval, one measurement subinterval, and one cooldown subinterval, "
			<< std::endl << "due to TCP/IP network time jitter when each test host hears the \"Go\" command.  This means we don't depend on NTP or clock synchronization.";
			return std::make_pair(false,o.str());
		}

		if
		(!(	// not:
			   firstMeasurementIndex >0 && firstMeasurementIndex < (subintervals.sequence.size()-1)
			&& lastMeasurementIndex >0 && lastMeasurementIndex < (subintervals.sequence.size()-1)
			&& firstMeasurementIndex <= lastMeasurementIndex
		))
		{
			std::ostringstream o;
			o << "RollupInstance::makeMeasurementRollup() - Invalid first (" << firstMeasurementIndex << ") and last (" << lastMeasurementIndex << ") measurement period indices."
			<< std::endl << " There must be at least one warmup subinterval, one measurement subinterval, and one cooldown subinterval, "
			<< std::endl << "due to TCP/IP network time jitter when each test host hears the \"Go\" command.  This means we don't depend on NTP or clock synchronization.";
			return std::make_pair(false,o.str());
		}

		measurementRollup.clear();
        measurement_subsystem_data.data.clear();
        measurement_subsystem_data.repetition_factor = 0.0;

		for (unsigned int i = firstMeasurementIndex; i <= lastMeasurementIndex; i++)
		{
			measurementRollup.addIn(subintervals.sequence[i]);
            if (subsystem_data_by_subinterval.size() > i) measurement_subsystem_data.addIn(subsystem_data_by_subinterval[i]);
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
            o << "         { element = \"" << element_pair.first << ", instances = { ";

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
                std::cout << o.str();
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

        have_reduced_gain = false;
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

        if (subinterval_count == 3)
        {
            if ( error_signal >= 0. ) error_positive_last_time = true;
            else                      error_positive_last_time = false;
            error_zero_crossing_subinterval.push_back(false);
        }
        else
        {
            if      (   error_positive_last_time  && error_signal < 0)
            {
                error_zero_crossing_subinterval.push_back(true);
                error_positive_last_time = false;
            }
            else if ( (!error_positive_last_time) && error_signal > 0)
            {
                error_zero_crossing_subinterval.push_back(true);
                error_positive_last_time = true;
            }
            else
            {
                error_zero_crossing_subinterval.push_back(false);
            }
        }

        p_times_error = P * error_signal;
        i_times_integral = I * error_integral;
        d_times_derivative = D * error_derivative;

        total_IOPS = p_times_error + i_times_integral + d_times_derivative;
    }

    if (total_IOPS < m_s.min_IOPS) total_IOPS = m_s.min_IOPS;

    //*debug*/ print_console_info_line();


    // The next bit is how we run an adaptive algorithm to reduce the "I" gain if we are seeing
    // excessive swings in IOPS.

    // The mechanism works on the basis of successive "adaptive PID subinterval cycles" where
    // every time we make a gain adjustment (reduction), we start a new subinterval cycle.

    // Maybe later the same adaptive PID cycle could be used for gain increases too ...

    // A "swing" is the cumulative distance that IOPS moves in one or more steps
    // in the same direction up or down ("monotonically") before changing direction.

    // If you see large IOPS swings (oscillations), then the gain is too high.

    // However, there is always some noise in the PID control metric signal for a fixed IOPS setting,
    // and thus the PID loop will always be adjusting the IOPS somewhat in response to noise, even if
    // the gain is set to an optimal value to reduce test time with a stable response.

    // The "max_ripple" parameter (e.g. 2.5%) describes the OK limit for the size of an IOPS swing as a fraction of
    // the base IOPS value.  (The base IOPS value is computed as the midpoint between the ends of the swing.)

    // If we see a ripple that is too big, we reduce the "I" gain by "gain_step", e.g. 2.0.

    // The gain reduction multiplicative factor we use is min( gain_step, 1 / gain_step).
    // In other words, you can either say 4.0 or 0.25, it works exactly the same either way.

    if (subinterval_count > 3)
    {
        adaptive_PID_subinterval_count++;

        if (adaptive_PID_subinterval_count == 1)
        {
            // starting a new adaptive PID cycle.

            on_way_down = false;
            on_way_up =  false;
            have_previous_inflection = false;
            {std::ostringstream o; o << "Starting new adaptive PID cycle at overall subinterval " << subinterval_count << "." << std::endl << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
        }
        else
        {
            // This is at least the second time we have set an IOPS value in this adaptive PID cycle.

            ivy_float delta_percent = 100 * abs(total_IOPS - total_IOPS_last_time) / total_IOPS_last_time;

            if ( (!on_way_down) && (!on_way_up) )
            {
                // This always happens on the second time through, and it's theoretically possible to keep happening if we always compute the exact same IOPS value

                if (total_IOPS > total_IOPS_last_time)
                {
                    on_way_up = true;
                    {std::ostringstream o; o << "PID loop - initial IOPS movement is upwards (" << std::fixed << std::setprecision(2) << delta_percent << "%) at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                }
                else if (total_IOPS < total_IOPS_last_time)
                {
                    on_way_down = true;
                    {std::ostringstream o; o << "PID loop - initial IOPS movement is downwards (" << std::fixed << std::setprecision(2) << delta_percent << "%) at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                }
                else
                {
                    {std::ostringstream o; o << "PID loop - always calculated exactly same IOPS this adaptive PID cycle at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                }
            }
            else
            {
                // We had a previously recorded direction of motion up or down before seeing this new IOPS setting.

                // If we have changed direction, we are seeing a new inflection point.
                if (     (   (on_way_up)   && (total_IOPS < total_IOPS_last_time)  )
                     ||  (   (on_way_down) && (total_IOPS > total_IOPS_last_time)  ))
                {
                    // we see an inflection point where we changed direction.

                    on_way_up = !on_way_up;
                    on_way_down = !on_way_down;

                    ivy_float this_inflection = total_IOPS_last_time;  // This was the farthest point reached.

                    if (have_previous_inflection) // is this the ending point of a swing?
                    {
                        ivy_float swing = abs(previous_inflection - this_inflection);

                        ivy_float base_IOPS = 0.5 * (previous_inflection + this_inflection); // average of the swing endpoint IOPS values

                        ivy_float ripple_fraction = swing / base_IOPS;

                        if (ripple_fraction > m_s.max_ripple)
                        {
                            // we need to reduce the gain and start a new adaptive PID cycle

                            I *= min(m_s.gain_step, 1.0/m_s.gain_step);
                            total_IOPS = base_IOPS;              // set new starting IOPS to be at midpoint of swing.
                            error_integral = total_IOPS / I;     // adjusts the cumulative error total to according to new I, new IOPS.
                            have_reduced_gain = true;
                            m_s.last_gain_adjustment_subinterval = m_s.running_subinterval; // warmup extends until at least the last gain adjustmenty
                            {std::ostringstream o; o << "PID loop - saw inflection " << (on_way_up? "upwards" : "downwards") << ", IOPS swing ("
                                << std::fixed << std::setprecision(2) << (100.*ripple_fraction)
                                << "%) too big, ended this adaptive PID cycle by reducing gain at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                            adaptive_PID_subinterval_count = 0;  // other things get reset from this
                        }
                        else
                        {
                            // The size of the swing was OK, keep going in the same adaptive PID cycle.

                            // This becomes the starting point for a new swing in the other direction.
                            previous_inflection = this_inflection;
                            {std::ostringstream o; o << "PID loop - saw inflection " << (on_way_up ? "upwards" : "downwards") << ", size of swing is OK (" << std::fixed << std::setprecision(2) << (100.*ripple_fraction) << "%), starting new swing at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                        }
                    }
                    else // this is the first inflection point this adaptive PID cycle, start the first swing.
                    {
                        previous_inflection = this_inflection;
                        have_previous_inflection = true;
                        {std::ostringstream o; o << "PID loop - saw first inflection (" << (on_way_up ? "upwards" : "downwards") << ") this adaptive PID cycle at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                    }
                }
                else
                {
                    // we have kept going in the same direction.

                    if ( (!have_reduced_gain) && (!have_previous_inflection) && (adaptive_PID_subinterval_count > m_s.max_monotone) )
                    {
                        // The idea is to keep increasing the gain if we haven't reached / crossed the target this adaptive PID cycle

                        I *= max(m_s.gain_step, 1.0/m_s.gain_step);
                        error_integral = total_IOPS / I;       // adjusts the cumulative error total to according to new I, new IOPS.
                        adaptive_PID_subinterval_count = 0;    // other things get reset from this
                        m_s.last_gain_adjustment_subinterval = m_s.running_subinterval; // warmup extends until at least the last gain adjustmenty
                        {std::ostringstream o; o << "PID loop - kept going same direction for too many subintervals this adaptive PID cycle and have never reduced gain so increasing gain at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str()); }
                    }
                    else
                    {
                        std::ostringstream o;
                        o << "PID loop - IOPS kept going " << (on_way_up ? "upwards" : "downwards") << " (" << std::fixed << std::setprecision(2) << delta_percent << "%) at subinterval " << adaptive_PID_subinterval_count << " of the current adaptive PID cycle." << std::endl << std::endl; std::cout << o.str(); if (routine_logging) log(m_s.masterlogfile,o.str());
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
        std::pair<bool,std::string> rc = m_s.edit_rollup(s, o.str());
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

//unsigned int RollupInstance::most_subintervals_without_a_zero_crossing_starting(unsigned int starting)
//{
//    unsigned int ending = error_zero_crossing_subinterval.size()-1;
//
//    if (starting > ending)
//    {
//        std::ostringstream o;
//        o << "<Error> internal programming error RollupInstance::most_subintervals_without_a_zero_crossing_starting(unsigned int starting = "
//        << starting << ") - unfortunately the ending subinterval number is " << ending << std::endl;
//        std::cout << o.str();
//        log(m_s.masterlogfile, o.str());
//        m_s.kill_subthreads_and_exit();
//    }
//
//    unsigned int max_consecutive=0;
//    unsigned int consecutive=0;
//    for (unsigned int i = starting; i <= ending; i++)
//    {
//        if (error_zero_crossing_subinterval[i])
//        {
//            consecutive = 0;
//        }
//        else
//        {
//            consecutive++;
//            if (consecutive > max_consecutive) max_consecutive = consecutive;
//        }
//    }
//
//    return max_consecutive;
//}

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
        o << "<Error> in RollupInstance::isValidMeasurementStartingFrom( n = " << n << " ).  Last subinterval is " << (focus_metric_vector.size()-1) << "." << std::endl
            << "Average over sample set is zero, so we can\'t perform the measurement validation calculation which divides by the average over the sample set.  Number of samples in set is " << sample.count() << "." << std::endl;
        log (m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
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
    error_zero_crossing_subinterval.clear();


    if (m_s.have_pid)
    {
        error_signal = 0.0;

        error_derivative = 0.0;

        ivy_float initial_error = -1.0 * m_s.target_value - m_s.low_target;

        error_integral = 0.5 * initial_error * (m_s.ballpark_seconds/m_s.subinterval_seconds);

        ivy_float fraction_through_range = (m_s.target_value - m_s.low_target)/(m_s.high_target - m_s.low_target);

        total_IOPS = m_s.low_IOPS + fraction_through_range * (m_s.high_IOPS - m_s.low_IOPS);  // straight line is just a rough approximation

        auto n = pRollupType->instances.size(); // The calibration IOPS values are for the all=all rollup, so we divide by the number of instances in the focus rollup.

        total_IOPS /= (ivy_float) n;

        total_IOPS_last_time = total_IOPS;

        P = 0;
        I = total_IOPS / error_integral;
        D = 0;

        adaptive_PID_subinterval_count = 0;
    }
    else
    {
        total_IOPS = 0.;
        total_IOPS_last_time = 0.;
    }

    this_measurement = 0.0;
    prev_measurement = 0.0;

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

    ivytime t; t.setToNow();
    timestamp = t.format_as_datetime_with_ns();

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

    best_first_last_valid = false;

}

void RollupInstance::print_common_columns(std::ostream& os)
{
    os << "," << timestamp;
    os << "," << m_s.focus_metric_ID();
    os << "," << m_s.target_value;

    std::string P_string; {std::ostringstream o; o << P; P_string = o.str();}
    std::string I_string; {std::ostringstream o; o << I; I_string = o.str();}
    std::string D_string; {std::ostringstream o; o << D; D_string = o.str();}

    os << "," << quote_wrap_except_number( std::string("p=") + P_string + std::string("/i=") + I_string + std::string("/d=") + D_string );
    os << "," << quote_wrap_except_number(P_string);
    os << "," << quote_wrap_except_number(I_string);
    os << "," << quote_wrap_except_number(D_string);
    os << "," << quote_wrap_except_number(attributeNameComboID);
    os << "," << quote_wrap_except_number(rollupInstanceID);
    os << "," << quote_wrap_except_number(m_s.testName);
    os << "," << quote_wrap_except_number(m_s.stepName);

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
}

void RollupInstance::print_pid_csv_files()
{
    // we print the  header line and one set of the csv file lines in a file in the step folder.
    // but we also print the header line if it's not already there in the overall test folder,
    // and then print one copy of the csv lines there too.

    // gets called once when the RollupInstance declares success or failure with have_pid set.

    const std::string headers {"item,timestamp,focus metric,target value,pid combo token,p,i,d,rollup type,rollup instance,test name,step name,measurements ...\n"};

    std::string test_level_filename, step_level_filename;

    test_level_filename = m_s.testFolder + "/" + m_s.testName + ".PID.csv";
    step_level_filename = m_s.stepFolder + "/" + m_s.testName + "." + m_s.stepNNNN + "." + m_s.stepName + "." + attributeNameComboID + "." + rollupInstanceID + ".PID.csv";

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

        fileappend(step_level_filename,headers);
        fileappend(step_level_filename,o.str());
    }

    return;
}














