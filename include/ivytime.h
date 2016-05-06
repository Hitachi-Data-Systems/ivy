//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

// This dates back to an early version of ivy before I had std=c++11 - Ian.  At some point, switch this over to using std::chrono ...e

#include <stdint.h>
#include <time.h>
#include <string>

//  You can change what clocks you use or what time functions you use by editing the ivytime class

class ivytime {

public:

	// for now, the internal variable t is public.   Take a look at taking this private later.

	struct timespec t;

	ivytime();
	virtual ~ivytime();
	ivytime(const struct timespec& ts);
	ivytime(const int Seconds);
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

	std::string format_as_duration_HMMSSns();
	std::string format_as_duration_HMMSS();
	std::string format_as_datetime();
	std::string format_as_datetime_with_ns();
	void setToNow();
	void waitUntilThisTime();
	uint64_t Milliseconds();
	void roundUpToStartOfMinute();
	void setFromNanoseconds(uint64_t Nanoseconds);
	uint64_t getAsNanoseconds();
	long double getlongdoubleseconds();
	std::string toString();
	bool fromString(std::string);

};


