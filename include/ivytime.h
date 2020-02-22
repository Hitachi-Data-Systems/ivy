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
#pragma once

// This dates back to an early version of ivy before I had std=c++11 - Ian.  At some point, switch this over to using std::chrono ...e

#include <stdint.h>
#include <time.h>

#include "ivytypes.h"

//  You can change what clocks you use or what time functions you use by editing the ivytime class

class ivytime {

public:

	// for now, the internal variable t is public.   Take a look at taking this private later.

	struct timespec t;

	ivytime();
	virtual ~ivytime();
	ivytime(const struct timespec& ts);
	ivytime(const int Seconds);
	ivytime(const unsigned int Seconds);
	ivytime(const ivytime& rhs);
	ivytime(const ivytime* rhs);
	ivytime(uint64_t seconds, uint64_t nanoseconds);
	ivytime(const double seconds);
	ivytime(const long double seconds);

	ivytime& operator=(const ivytime& rhs);

	bool operator==(const ivytime& rhs) const;
	bool operator!=(const ivytime& rhs) const;
	bool operator<(const ivytime& rhs) const;
	bool operator>(const ivytime& rhs) const;
	bool operator<=(const ivytime& rhs) const;
	bool operator>=(const ivytime& rhs) const;
	const ivytime operator+ (const ivytime &rhs) const;
	const ivytime operator- (const ivytime &rhs) const;
	operator std::string();
	operator double();
	operator long double();
	operator time_t();

	std::string format_as_duration_HMMSSns() const;
	std::string format_as_duration_HMMSS() const;
	std::string format_as_datetime() const;
	std::string format_as_datetime_with_ns() const;
    std::string format_with_ms(int decimal_places=6) const;
	std::string format_as_hex_dot_hex() const;

	void setToNow();
	inline void setToZero() { t.tv_sec = 0; t.tv_nsec = 0; }
	void waitUntilThisTime() const;
	void wait_for_this_long() const;
	uint64_t Milliseconds() const;
	void setFromNanoseconds(uint64_t Nanoseconds);
	uint64_t getAsNanoseconds() const;
	long double getlongdoubleseconds() const;
	long double seconds_from_now(); // positive in future, negative in past
	long double seconds_from(const ivytime& t);
	    // positive value means starting from "t", we move towards the future to reach my own value.
	    // negative value means starting from "t", we move towards the past to reach my own value.
	std::string duration_from_now();
	std::string duration_from(const ivytime& t);

	std::string toString() const;
	bool durationFromString(std::string);
	//bool fromString(std::string);
	bool isValid() const;
	bool isZero() const;
	void normalize(); // fix what may be an invalid representation after an operation such as plus or minus.
    void normalize_round(); // you just keep calling this until isValid()

private:
    ivytime& get_ivytime_reference_delta() const;
};

extern ivytime ivytime_reference_delta; // DO NOT refer to this yourself.  See notes at top of ivytime.cpp.

extern const ivytime ivytime_zero; // Feel free to reference this. It is set to "ivytime(0)" at startup.

