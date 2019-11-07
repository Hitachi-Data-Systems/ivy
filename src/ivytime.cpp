//Copyright (c) 2016, 2017, 2018, 2019 Hitachi Vantara Corporation
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

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string.h>

using namespace std;

#include "ivytime.h"

ivytime ivytime_reference_delta (0);

ivytime ivytime_zero (0);

// NOTE:
//
// ivytime uses the Linux-specific call clock_gettime(CLOCK_MONOTONIC_RAW,&t)
// which records the number of hardware clock cycles since boot.
//
// This is because we were having issues with NTP changing the clock,
// which caused ivy to think that the interlocks were timing out.
//
// Now all the internal timings are in CLOCK_MONOTONIC_RAW times,
// but when we print the times out for humans to read, we adjust
// by a delta time difference "ivytime_reference_delta" which is
// what needs to be added to an internal time to make it appropriate
// for humands.
//
// This value's constructor initializes it to zero, meaning that
// nobody has attempted to print out a time yet.
//
// The first time somebody tries to print out a time, we get two times,
// one the CLOCK_MONOTONIC_RAW, and one CLOCK_REALTIME, and the
// difference between the two is set into ivytime_reference_delta.
//
// Thus if NTP or something/somebody else changes the clock while
// ivy is running, that difference won't be picked up by ivy.
// In other words, the timestamps in logs will all be consistent
// with each other, but they may be out by, for example, a minute
// from the times other applications or the OS show.

bool ivytime::isValid() const
{
    // The signs of both tv_sec and tv_nsec must be the same;

    if ( (t.tv_sec < 0) && (t.tv_nsec > 0) ) { return false; }

    if ( (t.tv_sec > 0) && (t.tv_nsec < 0) ) { return false; }

    // The nanosecnd value can't be bigger than a second.
    if ( t.tv_nsec > 1000000000 || t.tv_nsec < -1000000000) { return false; }

    return true;
}

bool ivytime::isZero() const
{
	return t.tv_sec == 0 && t.tv_nsec == 0;
}

void ivytime::normalize()
{
    while (!isValid()) { normalize_round(); }
    return;
}

void ivytime::normalize_round() // you just keep calling this until isValid()
{
    // fix what may be an invalid representation after an operation such as plus or minus.

    if ( t.tv_sec < 0 )
    {
        // work with negative seconds value
        if (t.tv_nsec <= -1000000000)
        {
            t.tv_sec  -= 1;
            t.tv_nsec += 1000000000;
            return;
        }
        else if (t.tv_nsec > 0)
        {
            t.tv_sec  += 1;
            t.tv_nsec -= 1000000000;
            return;
        }
    }
    else
    {
        // work with zero or positive seconds value
        if (t.tv_nsec >= 1000000000)
        {
            t.tv_sec  += 1;
            t.tv_nsec -= 1000000000;
            return;
        }
        else if (t.tv_nsec < 0)
        {
            t.tv_sec  -= 1;
            t.tv_nsec += 1000000000;
            return;
        }
    }
}

ivytime::ivytime() {
	t.tv_sec=0;
	t.tv_nsec=0;
	return;
}

std::string ivytime::toString() const
{
	ostringstream o;
	o << "<" << t.tv_sec << ',' << t.tv_nsec << '>';
	return o.str();
}

bool ivytime::durationFromString(std::string s)
{
	istringstream is(s);
	char c;
	is >> c;
	if (is.fail() || c!='<') {t.tv_sec=0;t.tv_nsec=0; return false;}
	is >> t.tv_sec;
	if (is.fail()) {t.tv_sec=0;t.tv_nsec=0; return false;}
	is >> c;
	if (is.fail() || c!=',') {t.tv_sec=0;t.tv_nsec=0; return false;}
	is >> t.tv_nsec;
	if (is.fail()) {t.tv_sec=0;t.tv_nsec=0; return false;}
	is >> c;
	if (is.fail() || c!='>' ) {t.tv_sec=0;t.tv_nsec=0; return false;}
	is >> c;
	if (!is.eof()) {t.tv_sec=0;t.tv_nsec=0; return false;}
	return true;
}

// This was commented out when ivytime_reference_delta & CLOCK_MONOTONIC_RAW
// were added.
//
//bool ivytime::fromString(std::string s)
//{
//	istringstream is(s);
//	char c;
//	is >> c;
//	if (is.fail() || c!='<') {t.tv_sec=0;t.tv_nsec=0; return false;}
//	is >> t.tv_sec;
//	if (is.fail()) {t.tv_sec=0;t.tv_nsec=0; return false;}
//	is >> c;
//	if (is.fail() || c!=',') {t.tv_sec=0;t.tv_nsec=0; return false;}
//	is >> t.tv_nsec;
//	if (is.fail()) {t.tv_sec=0;t.tv_nsec=0; return false;}
//	is >> c;
//	if (is.fail() || c!='>' ) {t.tv_sec=0;t.tv_nsec=0; return false;}
//	is >> c;
//	if (!is.eof()) {t.tv_sec=0;t.tv_nsec=0; return false;}
//
// If you put it back into service, check to make sure this line
// does the trick to convert to a CLOCK_MONOTONIC_RAW time:
//  (*this) = (*this) - get_ivytime_reference_delta();
//	return true;
//}


ivytime::~ivytime() {
	return;
}


ivytime::ivytime(const ivytime & rhs) {
	t.tv_sec=rhs.t.tv_sec;
	t.tv_nsec=rhs.t.tv_nsec;
}


ivytime::ivytime(const ivytime* rhs) {
	t.tv_sec=rhs->t.tv_sec;
	t.tv_nsec=rhs->t.tv_nsec;
}


ivytime::ivytime(const struct timespec& source) {
	t.tv_sec=source.tv_sec;
	t.tv_nsec=source.tv_nsec;
	return;
}


ivytime::ivytime(const int Seconds) {
	t.tv_sec=Seconds;
	t.tv_nsec=0;
	return;
}
ivytime::ivytime(const unsigned int Seconds) {
	t.tv_sec=Seconds;
	t.tv_nsec=0;
	return;
}

ivytime::ivytime(const double Seconds) {
	t.tv_sec= ((long double) Seconds);
	t.tv_nsec=(((long double) Seconds /* this weirdness is to add a constructor for a double without changing how existing code works. */ ) -((long double) t.tv_sec))*1000000000.0;
	return;
}

ivytime::ivytime(const long double Seconds) {
	t.tv_sec=Seconds;
	t.tv_nsec=(Seconds-((long double) t.tv_sec))*1000000000.0;
	return;
}


ivytime::ivytime(uint64_t seconds, uint64_t nanoseconds){
	t.tv_sec=seconds;
	t.tv_nsec=nanoseconds;
	return;
}


ivytime& ivytime::operator=(const ivytime& rhs) {
	if(this == &rhs) return *this;
	this->t.tv_sec=rhs.t.tv_sec;
	this->t.tv_nsec=rhs.t.tv_nsec;
	return *this;
}


bool ivytime::operator==(const ivytime& rhs) const {
	if ((t.tv_sec==rhs.t.tv_sec) && (t.tv_nsec==rhs.t.tv_nsec)) {return true;}
	return false;
}


bool ivytime::operator!=(const ivytime& rhs) const {
	if ((t.tv_sec!=rhs.t.tv_sec) || (t.tv_nsec!=rhs.t.tv_nsec)) {return true;}
	return false;
}


bool ivytime::operator<(const ivytime& rhs) const {
	if (t.tv_sec < rhs.t.tv_sec) {return true;}
	if ((t.tv_sec == rhs.t.tv_sec) && (t.tv_nsec<rhs.t.tv_nsec)) {return true;}
	return false;
}


bool ivytime::operator>(const ivytime& rhs) const {
	if (t.tv_sec > rhs.t.tv_sec) {return true;}
	if ((t.tv_sec == rhs.t.tv_sec) && (t.tv_nsec>rhs.t.tv_nsec)) {return true;}
	return false;
}


bool ivytime::operator<=(const ivytime& rhs) const {
	if (t.tv_sec < rhs.t.tv_sec) return true;
	if ((t.tv_sec==rhs.t.tv_sec) && (t.tv_nsec <= rhs.t.tv_nsec)) return true;
	return false;
}


bool ivytime::operator>=(const ivytime& rhs) const {
	if (t.tv_sec > rhs.t.tv_sec) return true;
	if ((t.tv_sec==rhs.t.tv_sec) && (t.tv_nsec >= rhs.t.tv_nsec)) return true;
	return false;
}


const ivytime ivytime::operator+ (const ivytime &rhs) const
{
	ivytime result(this);

	result.t.tv_sec += rhs.t.tv_sec;
	result.t.tv_nsec += rhs.t.tv_nsec;

	result.normalize();

	return result;
}


const ivytime ivytime::operator- (const ivytime &rhs) const{

	ivytime result(this);

	result.t.tv_sec -= rhs.t.tv_sec;
	result.t.tv_nsec -= rhs.t.tv_nsec;

    result.normalize();

    return result;
}


void ivytime::waitUntilThisTime() const
{
	ivytime now;
	now.setToNow();
	ivytime wait_period;
	if (now<(*this))
	{
		wait_period=(*this)-now;
		while (true)
		{
			int rc=nanosleep(&wait_period.t, &wait_period.t);
			if (EINTR==rc) continue;
			else break; // not checking for EINVAL invalid input
		}
	}
	return;
}


void ivytime::wait_for_this_long() const
{
	ivytime now;
	now.setToNow();

    ivytime wait_until = now + *this;

    wait_until.waitUntilThisTime();

	return;
}


long double ivytime::seconds_from_now()
{
	ivytime now;
	now.setToNow();

    ivytime delta = *this - now;   // positive deltas are in the future

	return delta.getlongdoubleseconds();
}

long double ivytime::seconds_from(const ivytime& t)
{
    ivytime delta = *this - t;   // positive deltas are in the future

	return delta.getlongdoubleseconds();
}


std::string ivytime::duration_from_now()
{
	ivytime now;
	now.setToNow();

    ivytime delta = *this - now;   // positive deltas are in the future

	return delta.format_as_duration_HMMSSns();
}


std::string ivytime::duration_from(const ivytime& t)
{
    ivytime delta = *this - t;   // positive deltas are in the future

	return delta.format_as_duration_HMMSSns();
}


std::string ivytime::format_as_datetime() const {
	// 2012-04-15 HH:MM:SS - 19 characters plus terminating null makes buffer size 20

	ivytime adjusted = (*this) + get_ivytime_reference_delta();

	char timebuffer[20];
	strftime(timebuffer,20,"%Y-%m-%d %H:%M:%S",localtime(&adjusted.t.tv_sec));
	std::ostringstream os;
	os << timebuffer;
	return os.str();
}


std::string ivytime::format_as_hex_dot_hex() const
{
    std::ostringstream o;

    o << "0x";

	o << std::hex << std::setw(2*sizeof(t.tv_sec)) << std::setfill('0') << t.tv_sec;

    o << '.';

	o << std::hex << std::setw(2*sizeof(t.tv_nsec)) << std::setfill('0') << t.tv_nsec;

	return o.str();
}


std::string ivytime::format_as_datetime_with_ns() const {
	// 2012-04-15 HH:MM:SS - 19 characters plus terminating null makes buffer size 20
	ivytime adjusted = (*this) + get_ivytime_reference_delta();

	char timebuffer[20];
	strftime(timebuffer,20,"%Y-%m-%d %H:%M:%S",localtime(&adjusted.t.tv_sec));
	std::ostringstream os;
	os << timebuffer << '.' << std::setw(9) << std::setfill ('0') << adjusted.t.tv_nsec;
	return os.str();
}

std::string ivytime::format_with_ms(int decimal_places) const
{
    long double ld_ms = 1000.0 * getlongdoubleseconds();
    std::ostringstream o;
    o << std::fixed << std::setprecision(decimal_places) << ld_ms << " ms";
    return o.str();
}



ivytime::operator std::string() { return format_as_datetime_with_ns(); }


std::string ivytime::format_as_duration_HMMSSns() const
{
    if (!isValid())
    {
        return std::string("<Error> In ivytime::format_as_duration_HMMSSns().")
            + std::string("  Invalid ivytime value ")
            + (*this).toString()
            + std::string("\nivytime seconds and nanoseonds must not disagree in sign (same or zero).")
            + std::string("\nivytime nanoseconds value may not exceed plus or minus one second.")
            ;
    }

    std::ostringstream o;

    ivytime it;

    if (t.tv_sec < 0 || t.tv_nsec < 0)
    {
        o << "-";
        it.t.tv_sec = 0 - t.tv_sec;
        it.t.tv_nsec = 0 - t.tv_nsec;
    }
    else
    {
//        o << "+";
        it = (*this);
    }

	uint64_t hours, minutes, seconds;

	seconds = it.t.tv_sec % 60;
	minutes = ((it.t.tv_sec-seconds)/60) % 60;
	hours=(it.t.tv_sec-seconds-(minutes*60))/3600;

	o << hours
	    << ':' << std::setw(2) << std::setfill('0') << minutes
	    << ':' << std::setw(2) << std::setfill('0') << seconds
	    << '.' << std::setw(9) << std::setfill('0') << it.t.tv_nsec;

	return o.str();
}


std::string ivytime::format_as_duration_HMMSS() const
{
    if (!isValid())
    {
        return std::string("<Error> In ivytime::format_as_duration_HMMSS().")
            + std::string("  Invalid ivytime value ")
            + (*this).toString()
            + std::string("\nivytime seconds and nanoseonds must not disagree in sign (same or zero).")
            + std::string("\nivytime nanoseconds value may not exceed plus or minus one second.")
            ;
    }

    std::ostringstream o;

    ivytime it;

    if (t.tv_sec < 0 || t.tv_nsec < 0)
    {
        o << "-";
        it.t.tv_sec = 0 - t.tv_sec;
        it.t.tv_nsec = 0 - t.tv_nsec;
    }
    else
    {
//        o << "+";
        it = (*this);
    }

	uint64_t hours, minutes, seconds;

	seconds = it.t.tv_sec % 60;
	minutes = ((it.t.tv_sec-seconds)/60) % 60;
	hours=(it.t.tv_sec-seconds-(minutes*60))/3600;

	o << hours << ':' << std::setw(2) << std::setfill('0') << minutes << ':' << std::setw(2) << std::setfill('0') << seconds;

	return o.str();
}

ivytime::operator double() {
	return ((double)t.tv_sec) + (((double) t.tv_nsec)/1E9);
}

ivytime::operator long double() {
	return ((long double)t.tv_sec) + (((long double) t.tv_nsec)/(long double)1E9);
}

ivytime::operator time_t() { return t.tv_sec;}

void ivytime::setToNow()
{
	int rc;

	if ((rc=clock_gettime(CLOCK_MONOTONIC_RAW,&t))!= 0)
	{
	    std::ostringstream o;
		o <<"<Error> internal programming error - clock_gettime(CLOCK_MONOTONIC_RAW,) failed with return code " << rc << " - errno = " << errno << " - " << strerror(errno) << std::endl;
		t.tv_sec=0;
		t.tv_nsec=0;
		std::cout << o.str();
		throw std::runtime_error(o.str());
	}

	return;
}


ivytime& ivytime::get_ivytime_reference_delta() const
{
    if (ivytime_reference_delta == ivytime_zero)
    {
        int rc;

        ivytime monotonic, realtime;

        if ((rc=clock_gettime(CLOCK_MONOTONIC_RAW,&monotonic.t))!= 0)
        {
            std::ostringstream o;
            o <<"<Error> internal programming error - clock_gettime(CLOCK_MONOTONIC_RAW,) failed with return code " << rc << " - errno = " << errno << " - " << strerror(errno) << std::endl;
            std::cout << o.str();
            throw std::runtime_error(o.str());
        }

        if ((rc=clock_gettime(CLOCK_REALTIME,&realtime.t))!= 0)
        {
            std::ostringstream o;
            o <<"<Error> internal programming error - clock_gettime(CLOCK_MONOTONIC_RAW,) failed with return code " << rc << " - errno = " << errno << " - " << strerror(errno) << std::endl;
            std::cout << o.str();
            throw std::runtime_error(o.str());
        }

        ivytime_reference_delta = realtime - monotonic;
    }

    return ivytime_reference_delta;
}


uint64_t ivytime::Milliseconds() const {
	return (((uint64_t)t.tv_sec)*1000) + (((uint64_t)t.tv_nsec)/1000000);
}


void ivytime::setFromNanoseconds(uint64_t Nanoseconds) {
	uint64_t seconds;
	uint64_t ns;
	seconds = Nanoseconds / 1000000000LL;
	ns = Nanoseconds - (seconds*1000000000LL);
	t.tv_sec=(time_t) seconds;
	t.tv_nsec= ns;
	return;
}

uint64_t ivytime::getAsNanoseconds() const {
	return (1000000000LL*((uint64_t)t.tv_sec)) + ((uint64_t)t.tv_nsec);
}

long double ivytime::getlongdoubleseconds() const {
	return ((long double)t.tv_sec) + (((long double)t.tv_nsec)/((long double)1000000000));
}

