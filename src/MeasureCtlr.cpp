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
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "ivyhelpers.h"
#include "MeasureCtlr.h"
#include "ivy_engine.h"
#include "RollupInstance.h"

extern bool routine_logging;

void MeasureCtlr::reset()
{
	current = -1;
	seq_fill_extending = false;
}

std::string wurst_of_the_best();

int MeasureCtlr::evaluateSubinterval()
{
	current++;

    if (m_s.have_pid && m_s.last_gain_adjustment_subinterval > 0 && current < ( m_s.last_gain_adjustment_subinterval + m_s.min_measure_count - 1))
    {
        std::ostringstream o;
        o << "Last PID loop gain adjustment was at subinterval " << m_s.last_gain_adjustment_subinterval
            << " and we need run for " <<  (( m_s.last_gain_adjustment_subinterval + m_s.min_measure_count - 1) - current)
            << " more subintervals from now to have at least " << m_s.measure_seconds
            << " seconds worth of measurement data. " << std::endl << std::endl;
        std::cout << o.str();
        if (routine_logging) log(m_s.masterlogfile, o.str());
        m_s.have_timeout_rollup = false;
        return  EVALUATE_SUBINTERVAL_CONTINUE;
    }

    if (current < ( m_s.min_warmup_count + m_s.min_measure_count - 1)) /* if we say success at subinterval min_warmup_subintervals + min_sample_size, there's one more running that will be a cooldown subinterval */
    {
        std::ostringstream o;
        o << "This is subinterval number " << current << " (numbered from zero) of at least " << ( m_s.min_warmup_count + m_s.min_measure_count )
            << " subintervals to accommodate " << m_s.warmup_seconds << " warmup_seconds and " << m_s.measure_seconds << " measure_seconds with subintervals " << m_s.subinterval_seconds << " seconds long." << std::endl << std::endl;
        std::cout << o.str();
        log(m_s.masterlogfile, o.str());
        m_s.have_timeout_rollup = false;
        return  EVALUATE_SUBINTERVAL_CONTINUE;
    }

    if (!m_s.have_measure)  // fixed number of subintervals
    {
        if (m_s.have_pid && (m_s.last_gain_adjustment_subinterval > 0) )
        {
            measureFirstIndex = m_s.last_gain_adjustment_subinterval + 1;
        }
        else
        {
            measureFirstIndex = m_s.min_warmup_count;
        }
        measureLastIndex = current;
        if (m_s.keep_filling)
        {
            std::ostringstream o;
            o   << "Fixed duration measurement being extended until all sequential write workloads have completely wrapped around to where they started and have written to all blocks."
                << std::endl << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile, o.str());
            return  EVALUATE_SUBINTERVAL_CONTINUE;
        }
        else
        {
            return EVALUATE_SUBINTERVAL_SUCCESS;
        }
    }

    // "measure" is on from here

    if (seq_fill_extending)
    {
        measureLastIndex = current;

        if (m_s.keep_filling)
        {
            std::ostringstream o;
            o   << "Successful measurement from subinterval " << measureFirstIndex << " to " << measurement_end
                << " is being extended from now at subinterval " << current << " until sequential fill is complete." << std::endl << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile, o.str());

            return EVALUATE_SUBINTERVAL_CONTINUE;
        }
        else
        {
            std::ostringstream o;
            o   << "Successful measurement from subinterval " << measureFirstIndex << " to " << measurement_end
                << " has been extended until sequential fill was complete at " << current << "." << std::endl << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile, o.str());

            return EVALUATE_SUBINTERVAL_SUCCESS;
        }
    }

    if (current > (m_s.timeout_seconds / m_s.subinterval_seconds) && (!m_s.keep_filling))
    {
        std::ostringstream o;
        o << "<Error> Timed out - timeout_seconds = " << m_s.timeout_seconds << ".  Failed to get a valid measurement with accuracy_plus_minus = \"" << m_s.accuracy_plus_minus_parameter
            << "\" and confidence = \"" << m_s.confidence_parameter << "\"." <<  std::endl;
        o << std::endl << "====================================================" << std::endl << std::endl;
        log (m_s.masterlogfile,o.str());
        std::cout << o.str();

        m_s.MeasureCtlr_failure_point = current;

        return EVALUATE_SUBINTERVAL_FAILURE;
    }

    bool first_pass {true};
        // This first pass refers to times through when evaluating candidates for "measure".
        // We don't get here without the "measure" feature and without having already run the minimum number of warmup and measure subintervals.

    // This next bit does a search for possible subsequences ending in the current subinterval which
    // represent a valid measurement of "the focus metric" within each instance of the focus rollup,
    // meeting minimum warmup and measurement length and meeting accuracy_plus_minus and confidence requirements.

    // The test is done by each RollupInstance in the "focus rollup" used for dfc=pid or measure=on,
    // looking at the values in the RollupInstance's focus_metric_vector.

    // The shortest possible solution constrained by measure_seconds is examined first, and
    // if that was inconclusive, the next longer candidate is checked all the way to
    // the longest possible subsequence which is constrained by warmup_seconds.

    // Inconclusive means "So far for all the canididate subintervals we complied with
    // min_wp, max_sp, and max_wp_change (max wp excursion), but we didn't see a valid measurement yet.
    // Go ahead and try one more if there is a longer candiate."

    // If we fail the Write Pending criteria, looking for longer subsequences would be a waste of time,
    // so we say CONTINUE right away.

    // All this iterating over all substring lengths ending in the current subinterval
    // takes place in real time, ending in us saying CONTINUE if we hit the longest possible candiate
    // measurement subsequence without meeting the criteria, and of course, saying
    // SUCCESS as soon as we get a match.  On the other hand, each rollup instance keeps a vector
    // of the focus metric value for each subinterval, so this examination really is reasonable
    // with the processor power these younger whippersnappers are used to nowadays.



//*debug*/ {std::ostringstream o; o << ""<< std::endl;std::cout<<o.str();log(m_s.masterlogfile,o.str());}
	std::map<std::pair<std::string, Subsystem*>, RunningStat<ivy_float, ivy_int>> wp_by_CLPR;

	for (int fromLast=0; fromLast <= (current - max(m_s.last_gain_adjustment_subinterval,m_s.min_warmup_count)) /* drop at least one subinterval */; fromLast++)
	{
		unsigned int now_processing;
		now_processing = current - fromLast;

//*debug*/ {std::ostringstream o; o << "current=" << current << ", fromLast=" << fromLast << ", now_processing " << now_processing << std::endl;std::cout<<o.str();log(m_s.masterlogfile,o.str());}

		// we see if Write Pending was within the slew (plus or minus) limit.  We only consider subinterval subsequences if WP is "stable", or at least not slewing too much over the potential measurement period
		if (!m_s.suppress_subsystem_perf) for (auto& pear : m_s.cooldown_WP_watch_set)
		{
			ivy_float wp;

			try
			{
				wp = ((Hitachi_RAID_subsystem*)pear.second)->get_wp(pear.first,now_processing);
				wp_by_CLPR[pear].push(wp);
			}
			catch(std::invalid_argument& iaex)
			{
				std::ostringstream o; o << "MeasureCtlr::evaluateSubinterval() - failed retrieving Write Pending value - " << iaex.what() << std::endl;
				log (m_s.masterlogfile,o.str());
				std::cout << o.str();
				m_s.kill_subthreads_and_exit();
			}

			if ( wp < (m_s.min_wp - 1e-6))
			{
				std::ostringstream o;

				ivytime ouch_point;
				ivy_float temp = ((ivy_float) (now_processing+1) ) * m_s.subinterval_seconds;
				ouch_point = ivytime(temp);

				ivytime now_point;
				temp = ((ivy_float) (current+1) ) * m_s.subinterval_seconds;
				now_point = ivytime(temp);

				o << "From now at " << now_point.format_as_duration_HMMSS() << " into the test looking backwards further and further for a valid measurement subsequence," << std::endl
                    << "stopped at " << ouch_point.format_as_duration_HMMSS() << " into the test, where Write Pending at " << (100.*wp) << "% on " << pear.second->serial_number << " " << pear.first
                    << " was below the min_wp parameter setting of " << (100.*m_s.min_wp) << "%" << std::endl;
                o << std::endl;

                if (!first_pass) o << wurst_of_the_best() << std::endl;

				log (m_s.masterlogfile,o.str());
				std::cout << o.str();

				return EVALUATE_SUBINTERVAL_CONTINUE;
			}

			if ( wp > (m_s.max_wp + 1e-6))
			{
				std::ostringstream o;

				ivytime ouch_point;
				ivy_float temp =  ((ivy_float) (now_processing+1) ) * m_s.subinterval_seconds;
				ouch_point = ivytime(temp);

				ivytime now_point;
				temp = ((ivy_float) (current+1) ) * m_s.subinterval_seconds;
				now_point = ivytime(temp);

				o << "From now at " << now_point.format_as_duration_HMMSS() << " into the test looking backwards further and further for a valid measurement subsequence," << std::endl
                    << "stopped at " << ouch_point.format_as_duration_HMMSS() << " into the test, where Write Pending at " << (100.*wp) << "% on " << pear.second->serial_number << " " << pear.first
                    << " was above the max_wp parameter setting of " << (100.*m_s.max_wp) << "%" << std::endl;
                o << std::endl;

                if (!first_pass) o << wurst_of_the_best() << std::endl;

				log (m_s.masterlogfile,o.str());
				std::cout << o.str();

				return EVALUATE_SUBINTERVAL_CONTINUE;
			}

			if ( ( wp_by_CLPR[pear].max() - wp_by_CLPR[pear].min() ) > (m_s.max_wp_change + 1e-6) ) // artifact of using a floating point RunningStat for this
			{
				std::ostringstream o;

				ivytime ouch_point;
				ivy_float temp = ((ivy_float) (now_processing+1) ) * m_s.subinterval_seconds;
				ouch_point = ivytime(temp);

				ivytime now_point;
				temp = ((ivy_float) (current+1) ) * m_s.subinterval_seconds;
				now_point = ivytime(temp);

				o << "From now at " << now_point.format_as_duration_HMMSS() << " into the test looking backwards further and further for a valid measurement subsequence," << std::endl
                    << "stopped at " << ouch_point.format_as_duration_HMMSS()
                    << " into the test, as by then Write Pending had changed over a range of " << (100.*( wp_by_CLPR[pear].max() - wp_by_CLPR[pear].min() )) << "%"
                    << " on " << pear.second->serial_number << " " << pear.first
                    << " and max_wp_change = " << (100.*m_s.max_wp_change) << "%" << std::endl;
                o << std::endl;

                if (!first_pass) o << wurst_of_the_best() << std::endl;

				log (m_s.masterlogfile,o.str());
				std::cout << o.str();

				return EVALUATE_SUBINTERVAL_CONTINUE;
			}
		}


        if (m_s.p_focus_rollup == nullptr) throw std::runtime_error("<Error> internal programming error - MeasureCtlr::evaluateSubinterval() - m_s.p_focus_rollup == nullptr.");

        if (fromLast == 0) continue; // we need to have a minimum of two subintervals to do the "is it valid" math.

        if (fromLast < (m_s.min_measure_count-1)) continue; // examine sequences of at least measure_seconds

        bool clean {true};

        for (auto& pear : m_s.p_focus_rollup->instances)
        {
            RollupInstance* pRollupInstance = pear.second;

//            if (m_s.have_max_above_or_below)
//            {
//                if ( m_s.max_above_or_below < pRollupInstance->most_subintervals_without_a_zero_crossing_starting(now_processing) )
//                {
//                    std::ostringstream o;
//                    o << "At subinterval " << now_processing << " " << (m_s.subinterval_seconds * (current-now_processing))  << " seconds back from the most recent subinterval, for rollup instance " << m_s.p_focus_rollup->attributeNameCombo.attributeNameComboID
//                        << "=" << pRollupInstance->rollupInstanceID << "," << std::endl
//                        << "Number of consecutive subintervals without an error signal zero crossing " << pRollupInstance->most_subintervals_without_a_zero_crossing_starting(now_processing)
//                        << " exceeds have_max_above_or_below = " << m_s.max_above_or_below << "." << std::endl << std::endl;
//
//                    if (!first_pass) o << wurst_of_the_best() << std::endl;
//
//                    log (m_s.masterlogfile,o.str());
//                    std::cout << o.str();
//
//                    return EVALUATE_SUBINTERVAL_CONTINUE;
//                }
//            }

            std::pair<bool,ivy_float> m = pRollupInstance -> isValidMeasurementStartingFrom(now_processing);

            const bool& matched = m.first;
//            const ivy_float& measured_value = m.second;

            ivy_float x = pRollupInstance -> plusminusfraction_percent_above_below_target;

            if (first_pass || (x < pRollupInstance->best))
            {
                pRollupInstance->best = x;
            }

            pRollupInstance->best_first = now_processing;
            pRollupInstance->best_last = current;
            pRollupInstance->best_first_last_valid = true;

            if ( !matched)
            {
                clean=false;
                break;
            }

//            if ( m_s.have_within && abs((measured_value-m_s.target_value)/m_s.target_value) > m_s.within )
//            {
//                std::ostringstream o;
//                o << "For rollup instance " << pRollupInstance->attributeNameComboID << "=" << pRollupInstance->rollupInstanceID
//                    << " an otherwise valid subsequence average measured value "  << measured_value
//                    << " was more than within = " << std::fixed <<std::setprecision(2) << (100.*m_s.within) << "%"
//                    << " away from the target_value = " << m_s.target_value << "." << std::endl;
//                o << "Otherwise valid means meeting accuracy_plus_minus=" << m_s.accuracy_plus_minus_parameter
//                    << ", confidence=" << m_s.confidence_parameter
//                    << ", warmup_seconds=" << std::fixed << std::setprecision(0) << m_s.warmup_seconds
//                    << ", measure_seconds=" << std::fixed << std::setprecision(0) << m_s.measure_seconds
//                    << "." << std::endl;
//
//                std::cout << o.str();
//                log(m_s.masterlogfile, o.str());
//
//                return EVALUATE_SUBINTERVAL_CONTINUE;
//            }

        }

        first_pass=false;  // the first pass we made it far enough down to examine a candidate,
                           // and we have populated the "best" variable for each focus rollup instance
        if (clean)
        {
            measureFirstIndex = now_processing;
            measureLastIndex = current;

            if (m_s.keep_filling)
            {
                seq_fill_extending = true;

                measurement_end = measureLastIndex;  // saving this for use in a status message

                std::ostringstream o; o << "Successful measurement from subinterval " << measureFirstIndex << " to " << measurement_end
                    << " will be extended until sequential fill is complete." << std::endl << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile, o.str());

                return EVALUATE_SUBINTERVAL_CONTINUE;
            }

            std::ostringstream o; o << "Successful measurement from subinterval " << measureFirstIndex << " to " << measureLastIndex << "." << std::endl;

            std::cout << o.str();
            log(m_s.masterlogfile, o.str());

            return EVALUATE_SUBINTERVAL_SUCCESS;
        }
	}

    if (!first_pass)
    {
        std::ostringstream o;

//      o << "DEBUG at the wurst." << std::endl;
        o << wurst_of_the_best() << std::endl;

        log(m_s.masterlogfile, o.str());
        std::cout << o.str();
    }
    else
    {
        std::ostringstream o;
        o << "Do not have \"best so far\" accuracy data." << std::endl;

        log(m_s.masterlogfile, o.str());
        std::cout << o.str();
    }

	return EVALUATE_SUBINTERVAL_CONTINUE;

}

std::string wurst_of_the_best()
{
    ivy_float wurst {-1.};

    std::string s;

    bool none_yet {true};

    for (auto& pear : m_s.p_focus_rollup->instances)
    {
        if (none_yet)
        {
            none_yet=false;
            wurst = pear.second->best;
            s = pear.first;
            m_s.p_focus_rollup->p_wurst_RollupInstance = pear.second;
        }
        else
        {
            if (wurst < pear.second->best)
            {
                wurst = pear.second->best;
                s = pear.first;
                m_s.p_focus_rollup->p_wurst_RollupInstance = pear.second;
            }
        }
    }

    std::ostringstream o;

    o << "The best subsequence plus/minus variation of the worst instance \"" << s << "\" of the focus rollup is " << wurst << " % above the target +/- "<< std::fixed << std::setprecision (2) << (100. * m_s.accuracy_plus_minus_fraction) << "% accuracy." << std::endl;

    if (wurst < 0)
    {
        o << "Negative values mean that for each rollup individually, there is a subsequence that is better than the specification," << std::endl
            << "but there is no subsequence for which all the rollup instances meet the specification." << std::endl;
    }

    return o.str();
}


