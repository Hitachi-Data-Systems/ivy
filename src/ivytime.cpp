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
#define _LARGE_FILE_API
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <chrono>
#include <ctime>
#include <random>
#include <functional>
#include <cstdlib>
#include <unistd.h>
#include <algorithm>
#include <malloc.h>
#include <string.h>
#include <scsi/sg.h>

using namespace std;

#include "ivytime.h"

ivytime::ivytime() {
	t.tv_sec=0;
	t.tv_nsec=0;
	return;
}

std::string ivytime::toString()
{
	ostringstream o;
	o << "<" << t.tv_sec << ',' << t.tv_nsec << '>';
	return o.str();
}

bool ivytime::fromString(std::string s)
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

const ivytime ivytime::operator+ (const ivytime &rhs) const {
	ivytime result(this);
	result.t.tv_sec += rhs.t.tv_sec;
	result.t.tv_nsec += rhs.t.tv_nsec;
	if (result.t.tv_nsec > 1000000000) {
		result.t.tv_sec++;
		result.t.tv_nsec-=1000000000;
	}
	return result;
}

const ivytime ivytime::operator- (const ivytime &rhs) const{
	ivytime result(this);
	result.t.tv_sec -= rhs.t.tv_sec;
	result.t.tv_nsec -= rhs.t.tv_nsec;
	if (result.t.tv_nsec < 0) {
		result.t.tv_sec--;
		result.t.tv_nsec+=1000000000;
	}
	return result;
}


void ivytime::waitUntilThisTime()
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


std::string ivytime::format_as_datetime() {
	// 2012-04-15 HH:MM:SS - 19 characters plus terminating null makes buffer size 20
	char timebuffer[20];
	strftime(timebuffer,20,"%Y-%m-%d %H:%M:%S",localtime(&t.tv_sec));
	std::ostringstream os;
	os << timebuffer;
	return os.str();
}

std::string ivytime::format_as_datetime_with_ns() {
	// 2012-04-15 HH:MM:SS - 19 characters plus terminating null makes buffer size 20
	char timebuffer[20];
	strftime(timebuffer,20,"%Y-%m-%d %H:%M:%S",localtime(&t.tv_sec));
	std::ostringstream os;
	os << timebuffer << '.' << std::setw(9) << std::setfill ('0') << t.tv_nsec;
	return os.str();
}

ivytime::operator std::string() { return format_as_datetime_with_ns(); }

std::string ivytime::format_as_duration_HMMSSns() {
	uint64_t hours, minutes, seconds;
	seconds = t.tv_sec % 60;
	minutes = ((t.tv_sec-seconds)/60) % 60;
	hours=(t.tv_sec-seconds-(minutes*60))/3600;
	std::ostringstream os;
	os << hours << ':' << std::setw(2) << std::setfill('0') << minutes << ':' << std::setw(2) << std::setfill('0') << seconds << '.' << std::setw(9) << std::setfill('0') << t.tv_nsec;
	return os.str();
}


std::string ivytime::format_as_duration_HMMSS() {
	uint64_t hours, minutes, seconds;
	seconds = t.tv_sec % 60;
	minutes = ((t.tv_sec-seconds)/60) % 60;
	hours=(t.tv_sec-seconds-(minutes*60))/3600;
	std::ostringstream os;
	os << hours << ':' << std::setw(2) << std::setfill('0') << minutes << ':' << std::setw(2) << std::setfill('0') << seconds;
	return os.str();
}

ivytime::operator double() {
	return ((double)t.tv_sec) + (((double) t.tv_nsec)/1E9);
}

ivytime::operator long double() {
	return ((long double)t.tv_sec) + (((long double) t.tv_nsec)/(long double)1E9);
}

ivytime::operator time_t() { return t.tv_sec;}

void ivytime::setToNow() {
	int rc;
	if ((rc=clock_gettime(CLOCK_REALTIME,&t))!= 0) {
		std::cerr<<"clock_gettime(CLOCK_REALTIME,) failed with return code " << rc << std::endl;
		t.tv_sec=0;
		t.tv_nsec=0;
	}
	return;
}
uint64_t ivytime::Milliseconds() {
	return (((uint64_t)t.tv_sec)*1000) + (((uint64_t)t.tv_nsec)/1000);
}

void ivytime::roundUpToStartOfMinute() {
	if (t.tv_nsec > 0) {
		t.tv_sec++;
		t.tv_nsec=0;
	}
	t.tv_sec = (t.tv_sec+59) / 60;
	t.tv_sec = 60 * t.tv_sec;
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

uint64_t ivytime::getAsNanoseconds() {
	return (1000000000LL*((uint64_t)t.tv_sec)) + ((uint64_t)t.tv_nsec);
}

long double ivytime::getlongdoubleseconds() {
	return ((long double)t.tv_sec) + (((long double)t.tv_nsec)/((long double)1000000000));
}
