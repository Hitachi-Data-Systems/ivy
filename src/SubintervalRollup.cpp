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
#include "ivytime.h"
#include "ivyhelpers.h"
#include "ivydefines.h"
#include "IosequencerInput.h"
#include "IosequencerInputRollup.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "SubintervalOutput.h"
#include "SubintervalRollup.h"
#include "SequenceOfSubintervalRollup.h"
#include "WorkloadID.h"
#include "ListOfWorkloadIDs.h"
#include "RollupInstance.h"
#include "LUN.h"
#include "AttributeNameCombo.h"
#include "RollupType.h"
#include "ivy_engine.h"

void SubintervalRollup::clear()
{
    inputRollup.clear();
    outputRollup.clear();
    startIvytime = ivytime(0);
    endIvytime   = ivytime(0);
    for (auto& rs : IOPS_series        ) { rs.clear(); }
    for (auto& rs : service_time_series) { rs.clear(); }
}

void SubintervalRollup::addIn(const SubintervalRollup& other)
{
	if (ivytime(0) == startIvytime) startIvytime=other.startIvytime;
	else if ( (!(ivytime(0) == other.startIvytime)) && other.startIvytime < startIvytime) startIvytime = other.startIvytime;

	if (ivytime(0) == endIvytime) endIvytime=other.endIvytime;
	else if ( (!(ivytime(0) == other.endIvytime)) && other.endIvytime > endIvytime) endIvytime = other.endIvytime;

	inputRollup.merge(other.inputRollup);
	outputRollup.add(other.outputRollup);

	for (unsigned int i = 0; i <= Accumulators_by_io_type::max_category_index(); i++)
	{
        // Here, we treat each subinterval's average IOPS (and average service time, if IOPS > 0) as if they were "observations"
        // or the measured values of a metric obtained for samples selected at random out of a large population.

            // Because these observations are not selected at random from a large population, but rather form a time series
            // where behaviours may exhibit patterns with durations of seconds or minutes, a "non-random sample correction factor"
            // exaggeration multiplier is applied to the observed +/- variations for the purposes of correcting
            // actual plus/minus variation observed by running ivy multiple times.

            // Just playing with the "measure" feature a few times when it was first written
            // indicated that if you multiply the observed variation by 2.0 times this would be roughly right.

            // This needs to be explored for different circumstances to see if different correction factors
            // need to be applied under different circumstances.

            // This will be time well-spent, as then we won't need to spend any extra time testing
            // but still we will be reasonably confident that our +/- accuracy specifications are
            // met if we use ivy to measure something over and over.

        // Every subinterval has an IOPS measurement, which may be zero.
        // Only subintervals that have IOPS > 0.0 have a service time measurement (observation).
        // Because these observation counts may be different, each is shown in its own csv column.

        // Note also that as a time series of observations, the average value of the observed "subinterval average service time"
        // for the purposes of indicating +/- variation of service time from subinterval to subinterval is never shown to the user.

        // The csv file average service time column shows you the average service time over all the I/Os in the measurement period
        // without respect to how those I/Os were distributed across subintervals.
            // For example, suppose that the measurement period consisted of 5 subintervals
                // 1 I/O, 100 ms
                // 1 I/O, 100 ms
                // 1,000,000 I/Os, 1 ms
                // 1 I/O, 100 ms
                // 1 I/O, 100 ms
            // then looking at the average service time as a time series to estimate service time +/- variation,
            // the average over the 5 subintervals is ((4/5) x 100 ms) + ((1/5) x 1 ms) or 80.2 ms and the plus/minus distance is from this value.
            // Over the same 5 subintervals, the average service time is
            //    (   (1 * 100 ms) + (1 * 100 ms) + (1000000 * 1 ms) + (1 * 100 ms) + (1 * 100 ms)   )   /   ( 1 + 1 + 1000000 + 1 + 1)
            // = 1.000396 ms.

        RunningStat<ivy_float,ivy_int> st = other.outputRollup.u.a.service_time.getRunningStatByCategory(i);

        ivy_float IOPS = ( static_cast<ivy_float>(st.count()) ) / m_s.subinterval_seconds;

        IOPS_series[i].push(IOPS);

        if (IOPS > 0.0)
        {
            service_time_series[i].push(st.avg());
        }
	}
}

