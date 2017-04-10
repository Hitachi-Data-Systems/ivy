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
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdlib.h> // atoi
#include <algorithm>

#include "ivytime.h"
#include "ivydefines.h"
#include "ivyhelpers.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "IosequencerInput.h"
#include "SubintervalOutput.h"
#include "SubintervalRollup.h"


void SubintervalOutput::clear() {
	for (unsigned int i=0; i<SubintervalOutput::RunningStatCount(); i++ ) {
		u.accumulator_array[i].clear();
	}
	return;
}

bool SubintervalOutput::fromIstream(std::istream& is) {
	char c;
	if ((!(is >> c)) || (c!='<'))  {
		clear();
		return false;
	}
	for (unsigned int i=0; i<SubintervalOutput::RunningStatCount(); i++ ) {
		if (!u.accumulator_array[i].fromIstream(is)) {
			clear();
			return false;
		}
	}
	if ((!(is >> c)) || (c!='>'))  {
		clear();
		return false;
	}
	return true;
}


bool SubintervalOutput::fromString(std::string& s) {
	std::istringstream is(s);
	if (!fromIstream(is)) return false;
	char c;
	is >> c;
	if (!is.fail()) return false;
	return true;
}

std::string SubintervalOutput::toString() {
	std::ostringstream o;
	o << '<';
	for (unsigned int i=0; i<SubintervalOutput::RunningStatCount(); i++ ) {
		o<< u.accumulator_array[i].toString();
	}
	o << '>';
	return o.str();
}

void SubintervalOutput::add(const SubintervalOutput& s_o) {
	for (unsigned int i=0; i<SubintervalOutput::RunningStatCount(); i++ ) {
		u.accumulator_array[i] += s_o.u.accumulator_array[i];
	}
	return;
}


/*static*/ std::string SubintervalOutput::csvTitles(bool measurement_columns)
{
	std::ostringstream o;
	for (int i=0; i <= Accumulators_by_io_type::max_category_index(); i++)
	{
/*1*/		o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " IOPS";
        if (measurement_columns)
        {
/*2*/            o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " IOPS +/- estimate";
        }
/*3*/		o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " MB/s";
/*3a*/		o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " MiB/s";
/*4*/		o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " Average Blocksize (KiB)";
/*5*/		o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " Little\'s Law Avg Q";
/*6*/		o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " Average Service Time (ms)";
        if (measurement_columns)
        {
/*7*/           o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " service time +/- estimate";
        }
/*8*/		o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " Min Service Time (ms)";
/*9*/		o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " Max Service Time (ms)";
/*10*/		o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " Service Time Standard Deviation (ms)";
/*11*/		o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " Average Response Time (ms)";
/*12*/		o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " Min Response Time (ms)";
/*13*/		o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " Max Response Time (ms)";
/*14*/		o << "," << Accumulators_by_io_type::getRunningStatTitleByCategory(i) << " Response Time Standard Deviation (ms)";
	}
	return o.str();
}


bool lookupDoubleSidedStudentsTMultiplier(std::string& /*callers_error_message*/, ivy_float& /*return_value*/, const int /*sample_count*/, const std::string& /*confidence*/);

std::string SubintervalOutput::csvValues
(
    ivy_float seconds
    , SubintervalRollup* p_SubintervalRollup
    , ivy_float non_random_sample_correction_factor
)
{

	if ( 0 >= seconds)
	{
		std:: ostringstream o;
		o << "SubintervalOutput::csvValues( ivy_float seconds = " << seconds << " ) - \"seconds\" must be greater than zero.";
		return o.str();
	}

	RunningStat<ivy_float, ivy_int> bytes_transferred, service_time, response_time;

	std::ostringstream o;
	for (int i=0; i <= Accumulators_by_io_type::max_category_index(); i++)
	{
		bytes_transferred.clear();
		service_time.clear();
		response_time.clear();
		bytes_transferred += u.a.bytes_transferred.getRunningStatByCategory(i);  // not a reference because some categories are generated upon demand by summing more detailed categories.
		service_time += u.a.service_time.getRunningStatByCategory(i);
		response_time += u.a.response_time.getRunningStatByCategory(i);

		if (0 == bytes_transferred.count())
		{
            unsigned int columns = 13;

            if (nullptr != p_SubintervalRollup) { columns += 2; }

			for (unsigned int i=0; i < columns; i++) o << ',';
		}
		else
		{
			// should really throw an exception if this happens - but for now print the error message in the csv file.
			if (bytes_transferred.count() != service_time.count())
			{
				o << "SubintervalOutput::csvValues( ivy_float seconds = " << seconds << " ) -	dreaded internal programming error.  "
					<< "bytes_transferred.count() = " << bytes_transferred.count() << " differs from service_time.count() = " << service_time.count();
				for (int i=0; i<11; i++) o << ',';
			}
			else
			{
				/* IOPS */
				o << ',' << ( ((ivy_float) bytes_transferred.count()) / seconds );

				if (nullptr != p_SubintervalRollup)
				{
                    RunningStat<ivy_float, ivy_int>& samples = p_SubintervalRollup->IOPS_series[i];

//                    o << ',';
//
//                    if (samples.count() > 0) { o << samples.count(); }

                    o << ',';

                    if ( samples.count() >= 2 && samples.avg() != 0.0)
                    {
                        ivy_float students_t_multiplier;

                        {
                            std::string my_error_message;
                            if (lookupDoubleSidedStudentsTMultiplier(my_error_message, students_t_multiplier, samples.count(), plus_minus_series_confidence_default))
                            {
                                ivy_float plusminusfraction =  (  students_t_multiplier * samples.standardDeviation() / sqrt((ivy_float) samples.count())   ) / samples.avg();

                                plusminusfraction *= non_random_sample_correction_factor;
                                    // This adjusts the plus-minus magnitude to compensate for the fact that there is a correlation between the measurements in successive subintervals
                                    // making the excursions locally smaller than if you had run infinitely many subintervals and you selected subintervals at random from that population,
                                    // which is what the math (student's T distribution) is based on.

                                o << (100.0 * plusminusfraction) << '%';
                            }
                        }
                    }
				}

				/* MB/s */
				o << ',' << ( (bytes_transferred.sum() / 1000000.0) / seconds );

				/* MiB/s */
				o << ',' << ( (bytes_transferred.sum() / (1024.0 * 1024.0) ) / seconds );

				/* Average Blocksize (KiB) */
				o << ',' << ( bytes_transferred.avg() / 1024.0      );

				/* Little's law average queue depth */
				o << ',' <<	( 	( ((ivy_float) bytes_transferred.count()) / seconds)   /*IOPS*/ *  	service_time.avg() 	);

				/* Average Service Time (ms) */
				o << ',' << (service_time.avg() * 1000.0);

				if (nullptr != p_SubintervalRollup)
				{
                    RunningStat<ivy_float, ivy_int>& samples = p_SubintervalRollup->service_time_series[i];

                    o << ',';

                    if ( samples.count() >= 2 && samples.avg() != 0.0)
                    {
                        ivy_float students_t_multiplier;

                        {
                            std::string my_error_message;
                            if (lookupDoubleSidedStudentsTMultiplier(my_error_message, students_t_multiplier, samples.count(), "95%"))
                            {
                                ivy_float plusminusfraction =  (  students_t_multiplier * samples.standardDeviation() / sqrt((ivy_float) samples.count())   ) / samples.avg();

                                plusminusfraction *= non_random_sample_correction_factor;
                                    // This adjusts the plus-minus magnitude to compensate for the fact that there is a correlation between the measurements in successive subintervals
                                    // making the excursions locally smaller than if you had run infinitely many subintervals and you selected subintervals at random from that population,
                                    // which is what the math (student's T distribution) is based on.

                                o << (100.0 * plusminusfraction) << '%';
                            }
                        }
                    }
				}

				/* Min Service Time (ms) */
				o << ',' << (service_time.min() * 1000.0);

				/* Max Service Time (ms) */
				o << ',' << (service_time.max() * 1000.0);

				/* Service Time Standard Deviation (ms) */
				o << ',' << (service_time.standardDeviation() * 1000.0);

				if (0 == response_time.count())
				{
					o << ",,,,";
				}
				else
				{
					/* Average Response Time (ms) */
					o << ',' << (response_time.avg() * 1000.0);

					/* Min Response Time (ms) */
					o << ',' << (response_time.min() * 1000.0);

					/* Max Response Time (ms) */
					o << ',' << (response_time.max() * 1000.0);

					/* Response Time Standard Deviation (ms) */
					o << ',' << (response_time.standardDeviation() * 1000.0);
				}
			}
		}
	}

	return o.str();
}

RunningStat<ivy_float,ivy_int> SubintervalOutput::overall_service_time_RS()
{
    return u.a.service_time.getRunningStatByCategory(0);
}

RunningStat<ivy_float,ivy_int> SubintervalOutput::overall_bytes_transferred_RS()
{
	return u.a.bytes_transferred.getRunningStatByCategory(0);
}




std::string SubintervalOutput::thumbnail(ivy_float seconds)
{

	if ( 0 >= seconds)
	{
		std:: ostringstream o;
		o << "SubintervalOutput::thumbnail( ivy_float seconds = " << seconds << " ) - \"seconds\" must be greater than zero.";
		return o.str();
	}

	RunningStat<ivy_float, ivy_int>	bytes_transferred {u.a.bytes_transferred.getRunningStatByCategory(0)};
	RunningStat<ivy_float, ivy_int>	service_time {u.a.service_time.getRunningStatByCategory(0)};
	RunningStat<ivy_float, ivy_int>	read_service_time {u.a.service_time.getRunningStatByCategory(3)};
	RunningStat<ivy_float, ivy_int>	write_service_time {u.a.service_time.getRunningStatByCategory(4)};

	RunningStat<ivy_float, ivy_int>	read_bytes_transferred {u.a.bytes_transferred.getRunningStatByCategory(3)};
	RunningStat<ivy_float, ivy_int>	write_bytes_transferred {u.a.bytes_transferred.getRunningStatByCategory(4)};


	if (0 == bytes_transferred.count())
	{
		return "<no events>";
	}

	std::ostringstream o;

	if (abs(bytes_transferred.count() - service_time.count()) > 1)   // but why are they sometimes 1 different?
	{
		o << "SubintervalOutput::thumbnail( ivy_float seconds = " << seconds << " ) -	dreaded internal programming error.  "
			<< "bytes_transferred.count() = " << bytes_transferred.count() << " differs from service_time.count() = " << service_time.count() << "  ";
	}

    o << std::endl;

	o << "Host workload overall:  IOPS = " << ( ((ivy_float) bytes_transferred.count()) / seconds ) ;
	o << ", read IOPS = " << ( ((ivy_float) read_bytes_transferred.count()) / seconds ) ;

	o << ", write IOPS = " << ( ((ivy_float) write_bytes_transferred.count()) / seconds ) ;

	o << ", MB/s = " << ( (bytes_transferred.sum() / 1000000.0) / seconds ) ;
	o << ", MiB/s = " << ( (bytes_transferred.sum() / (1024.0*1024.0)) / seconds ) ;

	o << ", avg Blk (KiB) = " << ( bytes_transferred.avg() / 1024.0      );

	o << ", Little\'s law avg  Q = " << std::fixed << std::setprecision(1) << (    ( ((ivy_float) bytes_transferred.count()) / seconds )/*IOPS*/  *   service_time.avg()     );

    o << std::endl << std::endl;

	o << "                        avg Serv. Time (ms) = " <<  std::fixed << std::setprecision(3) <<  (service_time.avg() * 1000.0) ;
	o << ", avg Read Serv. Time (ms) = "; if (read_bytes_transferred.count()==0) o << "< no reads >"; else o <<  std::fixed << std::setprecision(3) << (read_service_time.avg() * 1000.0) ;
	o << ", avg Write Serv. Time (ms) = "; if (write_bytes_transferred.count()==0) o << "< no writes >"; else o <<  std::fixed << std::setprecision(3) << (write_service_time.avg() * 1000.0) ;

    o << std::endl;

	return o.str();
}
























