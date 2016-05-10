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
#include <sstream>
#include <sys/stat.h>

#include "ivytime.h"
#include "ivydefines.h"
#include "ivyhelpers.h"
#include "LUN.h"
#include "AttributeNameCombo.h"
#include "IogeneratorInputRollup.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "IogeneratorInput.h"
#include "SubintervalOutput.h"
#include "SubintervalRollup.h"
#include "SequenceOfSubintervalRollup.h"
#include "WorkloadID.h"
#include "ListOfWorkloadIDs.h"
#include "RollupInstance.h"
#include "RollupType.h"
#include "master_stuff.h"


const std::vector<ivy_float> single_sided_fraction_confidence { 0.7500, 0.8000, 0.8500, 0.9000, 0.9500, 0.9750, 0.9900, 0.9950, 0.9975, 0.9990, 0.9995 };

const std::vector<ivy_float> double_sided_fraction_confidence { 0.5000, 0.6000, 0.7000, 0.8000, 0.9000, 0.9500, 0.9800, 0.9900, 0.9950, 0.9980, 0.9990 };

struct degree_of_freedom
{
	int n_minus_one;
	std::vector<ivy_float> multipliers;
};

const std::vector<degree_of_freedom> multiplier_table
// taken from http://en.wikipedia.org/wiki/Student%27s_t-distribution
{
	// { degrees_of_freedom = number of samples - 1, {multipliers for above confidence values} }
	{ 1, { 1.000, 1.376, 1.963, 3.078, 6.314, 12.710, 31.820, 63.660, 127.300, 318.300, 636.600 } }
	,{ 2, { 0.816, 1.080, 1.386, 1.886, 2.920, 4.303, 6.965, 9.925, 14.090, 22.330, 31.600 } }
	,{ 3, { 0.765, 0.978, 1.250, 1.638, 2.353, 3.182, 4.541, 5.841, 7.453, 10.210, 12.920 } }
	,{ 4, { 0.741, 0.941, 1.190, 1.533, 2.132, 2.776, 3.747, 4.604, 5.598, 7.173, 8.610 } }
	,{ 5, { 0.727, 0.920, 1.156, 1.476, 2.015, 2.571, 3.365, 4.032, 4.773, 5.893, 6.869 } }
	,{ 6, { 0.718, 0.906, 1.134, 1.440, 1.943, 2.447, 3.143, 3.707, 4.317, 5.208, 5.959 } }
	,{ 7, { 0.711, 0.896, 1.119, 1.415, 1.895, 2.365, 2.998, 3.499, 4.029, 4.785, 5.408 } }
	,{ 8, { 0.706, 0.889, 1.108, 1.397, 1.860, 2.306, 2.896, 3.355, 3.833, 4.501, 5.041 } }
	,{ 9, { 0.703, 0.883, 1.100, 1.383, 1.833, 2.262, 2.821, 3.250, 3.690, 4.297, 4.781 } }
	,{ 10, { 0.700, 0.879, 1.093, 1.372, 1.812, 2.228, 2.764, 3.169, 3.581, 4.144, 4.587 } }
	,{ 11, { 0.697, 0.876, 1.088, 1.363, 1.796, 2.201, 2.718, 3.106, 3.497, 4.025, 4.437 } }
	,{ 12, { 0.695, 0.873, 1.083, 1.356, 1.782, 2.179, 2.681, 3.055, 3.428, 3.930, 4.318 } }
	,{ 13, { 0.694, 0.870, 1.079, 1.350, 1.771, 2.160, 2.650, 3.012, 3.372, 3.852, 4.221 } }
	,{ 14, { 0.692, 0.868, 1.076, 1.345, 1.761, 2.145, 2.624, 2.977, 3.326, 3.787, 4.140 } }
	,{ 15, { 0.691, 0.866, 1.074, 1.341, 1.753, 2.131, 2.602, 2.947, 3.286, 3.733, 4.073 } }
	,{ 16, { 0.690, 0.865, 1.071, 1.337, 1.746, 2.120, 2.583, 2.921, 3.252, 3.686, 4.015 } }
	,{ 17, { 0.689, 0.863, 1.069, 1.333, 1.740, 2.110, 2.567, 2.898, 3.222, 3.646, 3.965 } }
	,{ 18, { 0.688, 0.862, 1.067, 1.330, 1.734, 2.101, 2.552, 2.878, 3.197, 3.610, 3.922 } }
	,{ 19, { 0.688, 0.861, 1.066, 1.328, 1.729, 2.093, 2.539, 2.861, 3.174, 3.579, 3.883 } }
	,{ 20, { 0.687, 0.860, 1.064, 1.325, 1.725, 2.086, 2.528, 2.845, 3.153, 3.552, 3.850 } }
	,{ 21, { 0.686, 0.859, 1.063, 1.323, 1.721, 2.080, 2.518, 2.831, 3.135, 3.527, 3.819 } }
	,{ 22, { 0.686, 0.858, 1.061, 1.321, 1.717, 2.074, 2.508, 2.819, 3.119, 3.505, 3.792 } }
	,{ 23, { 0.685, 0.858, 1.060, 1.319, 1.714, 2.069, 2.500, 2.807, 3.104, 3.485, 3.767 } }
	,{ 24, { 0.685, 0.857, 1.059, 1.318, 1.711, 2.064, 2.492, 2.797, 3.091, 3.467, 3.745 } }
	,{ 25, { 0.684, 0.856, 1.058, 1.316, 1.708, 2.060, 2.485, 2.787, 3.078, 3.450, 3.725 } }
	,{ 26, { 0.684, 0.856, 1.058, 1.315, 1.706, 2.056, 2.479, 2.779, 3.067, 3.435, 3.707 } }
	,{ 27, { 0.684, 0.855, 1.057, 1.314, 1.703, 2.052, 2.473, 2.771, 3.057, 3.421, 3.690 } }
	,{ 28, { 0.683, 0.855, 1.056, 1.313, 1.701, 2.048, 2.467, 2.763, 3.047, 3.408, 3.674 } }
	,{ 29, { 0.683, 0.854, 1.055, 1.311, 1.699, 2.045, 2.462, 2.756, 3.038, 3.396, 3.659 } }
	,{ 30, /* index 29 */ { 0.683, 0.854, 1.055, 1.310, 1.697, 2.042, 2.457, 2.750, 3.030, 3.385, 3.646 } }
	,{ 40, /* index 30 */ { 0.681, 0.851, 1.050, 1.303, 1.684, 2.021, 2.423, 2.704, 2.971, 3.307, 3.551 } }
	,{ 50, /* index 31 */ { 0.679, 0.849, 1.047, 1.299, 1.676, 2.009, 2.403, 2.678, 2.937, 3.261, 3.496 } }
	,{ 60, /* index 32 */ { 0.679, 0.848, 1.045, 1.296, 1.671, 2.000, 2.390, 2.660, 2.915, 3.232, 3.460 } }
	,{ 80, /* index 33 */ { 0.678, 0.846, 1.043, 1.292, 1.664, 1.990, 2.374, 2.639, 2.887, 3.195, 3.416 } }
	,{ 100, /* index 34 */ { 0.677, 0.845, 1.042, 1.290, 1.660, 1.984, 2.364, 2.626, 2.871, 3.174, 3.390 } }
	,{ 120, /* index 35 */ { 0.677, 0.845, 1.041, 1.289, 1.658, 1.980, 2.358, 2.617, 2.860, 3.160, 3.373 } }

	//{ infinity { 0.674, 0.842, 1.036, 1.282, 1.645, 1.960, 2.326, 2.576, 2.807, 3.090, 3.291 } }
};


bool lookupDoubleSidedStudentsTMultiplier(std::string& callers_error_message, ivy_float& return_value, const int sample_count, const std::string& confidence)
{
	std::ostringstream o;

	if (sample_count <2)
	{
		std::ostringstream o;
		o << "lookupDoubleSidedStudentsTMultiplier(,,sample_count=" << sample_count << ", confidence=\"" << confidence << "\") - sample count must be at least 2.";
		callers_error_message = o.str();
		return false;
	}

	ivy_float confidence_value;

	try
	{
		confidence_value = number_optional_trailing_percent(confidence);

	}
	catch (std::invalid_argument& iaex)
	{
		std::ostringstream o;
		o << "lookupDoubleSidedStudentsTMultiplier(,,sample_count=" << sample_count
			<< ", confidence=\"" << confidence
			<< "\") - confidence value must be a string representation of a number with optional decimal point and optionally ending in \'%\'" << std::endl;

/*debug*/		o << "Parsing subroutine failed saying \"" << iaex.what() << "\"." << std::endl;
		callers_error_message = o.str();
		return false;
	}

	int index{-1};

	for (unsigned int i=0; i< double_sided_fraction_confidence.size(); i++)
	{
		if ( (abs(confidence_value - double_sided_fraction_confidence[i]) / double_sided_fraction_confidence[i]) < .0000001)
		{
			index = i;
			break;
		}
	}

	if (-1 == index)
	{
		o << "lookupDoubleSidedStudentsTMultiplier(,,sample_count=" << sample_count << ", confidence=\"" << confidence << "\") - confidence value is not supported.";
		o << "  Supported values are:";
		for (auto& fraction : double_sided_fraction_confidence)
		{
			o << ' ' << std::fixed << std::setprecision(12) << (100.*fraction) << '%';
		}
		std::cout << std::endl << "<Error> " << o.str() << std::endl << std::endl;
		callers_error_message = o.str();
		return false;
	}

	int table_index;

	if (sample_count <= 31) table_index = sample_count-1;
	else if (sample_count <41) table_index = 29;
	else if (sample_count <51) table_index = 30;
	else if (sample_count <61) table_index = 31;
	else if (sample_count <81) table_index = 32;
	else if (sample_count <101) table_index = 33;
	else if (sample_count <121) table_index = 34;
	else table_index = 35;

	return_value = multiplier_table[table_index].multipliers[index];

	return true;
}

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



bool RollupInstance::makeMeasurementRollup(std::string& callers_error_message, unsigned int firstMeasurementIndex, unsigned int lastMeasurementIndex)
{
		if (subintervals.sequence.size() < 3)
		{
			std::ostringstream o;
			o << "RollupInstance::makeMeasurementRollup() - The total number of subintervals in the sequence is " << subintervals.sequence.size()
			<< std::endl << "and there must be at least one warmup subinterval, one measurement subinterval, and one cooldown subinterval, "
			<< std::endl << "due to TCP/IP network time jitter when each test host hears the \"Go\" command.  This means we don't depend on NTP or clock synchronization.";
			callers_error_message = o.str();
			return false;
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
			callers_error_message = o.str();
			return false;
		}

		measurementRollup.clear();
        measurement_subsystem_data.data.clear();
        measurement_subsystem_data.repetition_factor = 0.0;

		for (unsigned int i = firstMeasurementIndex; i <= lastMeasurementIndex; i++)
		{
			measurementRollup.addIn(subintervals.sequence[i]);
            if (subsystem_data_by_subinterval.size() > i) measurement_subsystem_data.addIn(subsystem_data_by_subinterval[i]);
		}

		return true;
}


bool RollupInstance::add_workload_detail_line(std::string& callers_error_message, WorkloadID& wID, IogeneratorInput& iI, SubintervalOutput& sO)
{
	std::string my_error_message;

	int index = -1 + subintervals.sequence.size();

	if (-1 == index)
	{
		callers_error_message = "RollupInstance::add_workload_detail_line() failed because the RollupInstance subinterval sequence was empty.";
		return false;
	}

	SubintervalRollup& subintervalRollup = subintervals.sequence[index];

	if ( ! subintervalRollup.inputRollup.add(my_error_message,iI.getParameterNameEqualsTextValueCommaSeparatedList()) )
	{
		std::ostringstream o;
		o << "RollupInstance::add_workload_detail_line() - failed adding in IogeneratorInput.getParameterNameEqualsTextValueCommaSeparatedList()=\""
		  << iI.getParameterNameEqualsTextValueCommaSeparatedList() << "\" to the last SubintervalRollup in the sequence of " << subintervals.sequence.size() << ".";
		callers_error_message = o.str();
		return false;
	}

	subintervalRollup.outputRollup.add(sO);  // for two existing SubintervalOutput objects, this call doesn't fail.

	return true;
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
        error_integral = 0.0;
        total_IOPS = m_s.starting_total_IOPS;
        total_IOPS_last_time = total_IOPS;
        error_zero_crossing_subinterval.push_back(false);

        // Here, we are using the target IOPS value from the last subinterval as "IOPS last time".

        // A note here to take a look at adding an option of instead of using the target IOPS from
        // last time as "IOPS last time", use instead the measured IOPS from the last subinterval.

        // The question was would this help PID loop stability or are we doomed anyway
        // because the I/O sequencer would keep getting further and further ahead.

        // Maybe we could add a filter onto the PID loop mechanism where it maintains a
        // second total_IOPS calculation which would be triggered when the I/O sequencer
        // gets, say, one second in the future, which would drift us back into "reasonable host latency".

        // We define the "host latency" of an I/O as the amount of time from the scheduled time
        // until when the I/O has been accepted by the AIO interface.

            // On the to-do list:
                // Change the definition of "service time" to be from when the I/O has been accepted
                // by the AIO interface, not the time from just before you tried to launch the I/O,
                // because right now we are recording time the AIO call to start an I/O blocks
                // because the device driver doesn't have enough free tags, we record this time as
                // service time even though the I/O hasn't been launched yet.)

        // Maybe we could use a mechanism to over-ride the [EditRollup] mechanism
        // and if host latency is getting too long, we adjust total_IOPS to make host latency
        // drift towards the "OK" zone.

        // Another idea is to design the feature into the I/O sequencer itself, as
        // for example, if a bursty workload is being generated, you may want to
        // generate fewer bursts, rather than changing what a burst looks like.


    // Background:

        // The first time I ran the code, I was plotting a service_time - IOPS chart,
        // going at 1.125, 1.25, 1.5, 1.75, 2, 3, 4, 5 times
        // the baseline service time measured at 1% of max IOPS.

        // At 4 x, it was on the edge, and at 5 x, the target service time
        // was over the service time we saw using IOPS=max for maxTags=32,
        // so the 5x baseline service time was unreachable.

        // In this case it's easy - do the PID loop by response_time, and
        // then you can measure right out to 5x and beyond.


    }
    else
    {
        total_IOPS_last_time = total_IOPS;

        this_measurement = focus_metric_vector[subinterval_count-1];
        prev_measurement = focus_metric_vector[subinterval_count-2];

        error_signal = this_measurement - m_s.target_value;
        error_integral += error_signal;
        error_derivative = this_measurement - prev_measurement;

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

        p_times_error = m_s.p_multiplier * error_signal;
        i_times_integral = m_s.i_multiplier * error_integral;
        d_times_derivative = m_s.d_multiplier * error_derivative;

        total_IOPS = p_times_error + i_times_integral + d_times_derivative;
    }

    if (total_IOPS < m_s.min_IOPS) total_IOPS = m_s.min_IOPS;

    //*debug*/ print_console_info_line();

    // Finished calculation of new total_IOPS value, now send it out

    {
        std::ostringstream o;
        o << "total_IOPS=" << std::fixed << std::setprecision(6) << total_IOPS;
        std::string my_msg;
        if (!m_s.editRollup(my_msg, attributeNameComboID+std::string("=")+rollupInstanceID, o.str()))
        {
            std::ostringstream o2;
            o2 << "For subinterval " << (subinterval_count-1) << ", m_s.editRollup(\"" << (attributeNameComboID+std::string("=")+rollupInstanceID) << "\", \"" << o.str() << "\") failed - " << my_msg << std::endl;
            std::cout << o2.str();
            log(m_s.masterlogfile, o2.str());
            m_s.kill_subthreads_and_exit();
        }
    }

    print_subinterval_column();

}

unsigned int RollupInstance::most_subintervals_without_a_zero_crossing_starting(unsigned int starting)
{
    unsigned int ending = error_zero_crossing_subinterval.size()-1;

    if (starting > ending)
    {
        std::ostringstream o;
        o << "<Error> internal programming error RollupInstance::most_subintervals_without_a_zero_crossing_starting(unsigned int starting = "
        << starting << ") - unfortunately the ending subinterval number is " << ending << std::endl;
        std::cout << o.str();
        log(m_s.masterlogfile, o.str());
        m_s.kill_subthreads_and_exit();
    }

    unsigned int max_consecutive=0;
    unsigned int consecutive=0;
    for (unsigned int i = starting; i <= ending; i++)
    {
        if (error_zero_crossing_subinterval[i])
        {
            consecutive = 0;
        }
        else
        {
            consecutive++;
            if (consecutive > max_consecutive) max_consecutive = consecutive;
        }
    }

    return max_consecutive;
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
        o << "<Error> in RollupInstance::isValidMeasurementStartingFrom( n = " << n << " ).  Last subinterval is " << (focus_metric_vector.size()-1) << "." << std::endl
            << "Average over sample set is zero, so we can\'t perform the measurement validation calculation which divides by the average over the sample set.  Number of samples in set is " << sample.count() << "." << std::endl;
        log (m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }


    plusminusfraction =  (  students_t_multiplier * sample.standardDeviation() / sqrt((ivy_float) sample.count())   ) / sample.avg();

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
    << "p = " << m_s.p_multiplier << ", i = " << m_s.i_multiplier << ", d = " << m_s.d_multiplier
    << ", new total IOPS = " << total_IOPS << std::endl << std::endl;

    std::cout << o.str();
    log(m_s.masterlogfile,o.str());
    return;
}


void RollupInstance::reset() // gets called by go statement, when it assigns the DFC
{
    focus_metric_vector.clear();
    error_zero_crossing_subinterval.clear();

    error_signal = 0.0;
    error_derivative = 0.0;
    error_integral = 0.0;

    this_measurement = 0.0;
    prev_measurement = 0.0;

    total_IOPS = 0.;
    total_IOPS_last_time = 0.;

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

}

void RollupInstance::print_common_columns(std::ostream& os)
{
    os << "," << timestamp;
    os << "," << m_s.focus_metric_ID();
    os << "," << m_s.target_value;
    os << "," << quote_wrap_except_number( std::string("p=") + m_s.p_parameter + std::string("/i=") + m_s.i_parameter + std::string("/d=") + m_s.d_parameter );
    os << "," << quote_wrap_except_number(m_s.p_parameter);
    os << "," << quote_wrap_except_number(m_s.i_parameter);
    os << "," << quote_wrap_except_number(m_s.d_parameter);
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














