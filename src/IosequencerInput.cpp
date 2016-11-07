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
#include <list>
#include <algorithm> // for ivyhelpers.h find_if()

#include "ivyhelpers.h"
#include "ivydefines.h"
#include "IosequencerInput.h"

bool IosequencerInput::setParameter(std::string& callers_error_message, std::string parameterNameEqualsValue) {

	callers_error_message.clear();

	if (parameterNameEqualsValue.length()<3)
	{
		callers_error_message= std::string("not name=value - \"")+parameterNameEqualsValue + std::string("\".");
		return false;
	}

	unsigned int equalsPosition=1;

	while ( (equalsPosition<(parameterNameEqualsValue.length()-2)) && (parameterNameEqualsValue[equalsPosition]!='='))
		equalsPosition++;

	if (parameterNameEqualsValue[equalsPosition]!='=') {
		callers_error_message = std::string("not name=value - \"")+parameterNameEqualsValue + std::string("\".");
		return false;
	}

	std::string parameterName, parameterValue;

	parameterName=parameterNameEqualsValue.substr(0,equalsPosition);
	parameterValue=parameterNameEqualsValue.substr(equalsPosition+1,parameterNameEqualsValue.length()-(equalsPosition+1));
	trim(parameterName);
	trim(parameterValue);

	// strip off enclosing single or double quotes.
	if (parameterValue.length()>=2 && (
		( parameterValue[0]=='\"' && parameterValue[parameterValue.length()-1]=='\"' ) ||
		( parameterValue[0]=='\'' && parameterValue[parameterValue.length()-1]=='\'' )
	))
	{
		parameterValue.erase(parameterValue.length()-1,1);
		parameterValue.erase(0,1);
	}

	if (parameterName=="") {
		callers_error_message = std::string("looking for name=value - parameter name missing in \"")+parameterNameEqualsValue
			+std::string("\".");
		return false;
	}

	if (parameterValue=="") {
		callers_error_message = std::string("looking for name=value - parameter value missing in \"")+parameterNameEqualsValue
			+std::string("\".");
		return false;
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("iosequencer")) ) {
		if (stringCaseInsensitiveEquality(parameterValue,std::string("random_steady"))) {
			if (iosequencerIsSet && 0 != iosequencer_type.compare(std::string("random_steady"))) {
				callers_error_message = std::string("when trying to set iosequencer=random_steady it was already set to ")
					+ iosequencer_type + std::string("\".  \"iosequencer\" type may not be changed once set.");
				return false;
			}
			iosequencer_type=parameterValue;
		} else if (stringCaseInsensitiveEquality(parameterValue,std::string("random_independent"))) {
			if (iosequencerIsSet && 0 != iosequencer_type.compare(std::string("random_independent"))) {
				callers_error_message = std::string("when trying to set iosequencer=random_independent it was already set to ")
					+ iosequencer_type + std::string("\".  \"iosequencer\" type may not be changed once set.");
				return false;
			}
			iosequencer_type=parameterValue;
		} else if (stringCaseInsensitiveEquality(parameterValue,std::string("sequential"))) {
			if (iosequencerIsSet && 0 != iosequencer_type.compare(std::string("sequential"))) {
				callers_error_message = std::string("when trying to set iosequencer=sequential it was already set to ")
					+ iosequencer_type + std::string("\".  \"iosequencer\" type may not be changed once set.");
				return false;
			}
			iosequencer_type=parameterValue;
		} else {
			callers_error_message = std::string("invalid iosequencer type \"")+parameterNameEqualsValue
				+std::string("\".");
			return false;
		}
		iosequencerIsSet=true;
		return true;
	}

	if (!iosequencerIsSet) {
		callers_error_message = std::string("IosequencerInput::setParameter(\"")+parameterNameEqualsValue
			+std::string("\",): \"iosequencer\" must be set to a valid iosequencer type before setting any other parameters.\n");
		return false;
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("blocksize")) ) {

		bool KiB{false}, MiB{false};
		if (parameterValue.length()>3 && stringCaseInsensitiveEquality(std::string("KiB"),parameterValue.substr(parameterValue.length()-3,3)) ) {
			KiB=true;
			parameterValue=parameterValue.substr(0,parameterValue.length()-3);
			trim(parameterValue);
		} else if (parameterValue.length()>3 && stringCaseInsensitiveEquality(std::string("MiB"),parameterValue.substr(parameterValue.length()-3,3)) ) {
			MiB=true;
			parameterValue=parameterValue.substr(0,parameterValue.length()-3);
			trim(parameterValue);
		}
		std::istringstream is(parameterValue);
		int b;
		is >> b;
		if ( is.fail() || (!is.eof()) ) {
			callers_error_message = std::string("invalid blocksize \"") + parameterValue;
			if (KiB) callers_error_message += std::string(" KiB");
			if (MiB) callers_error_message += std::string(" MiB");
			callers_error_message += std::string("\".");
			return false;
		}
		if (KiB) b*=1024;
		if (MiB) b*=(1024*1024);
		if (0 != b%512) {
			callers_error_message = std::string("invalid blocksize \"") + parameterValue;
			if (KiB) callers_error_message += std::string(" KiB");
			if (MiB) callers_error_message += std::string(" MiB");
			callers_error_message += std::string("\".  Blocksize must be a multiple of 512.");  /// NEED 4K SECTOR SUPPORT HERE
			return false;
		}
		blocksize_bytes=b;
		return true;
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("maxTags")) ) {
		std::istringstream is(parameterValue);
		int mt;
		if ( (!(is >> mt)) || (!is.eof()) || (mt<1) || (mt > MAX_MAXTAGS)) {
			callers_error_message = std::string("invalid maxTags parameter value \"")+parameterValue +std::string("\".");
			return false;
		}
		maxTags=mt;
		return true;
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("IOPS")) ) {

		if ( 0 == parameterValue.length() ) {callers_error_message = "IOPS may not be set to the null string."; return false;}

		if ( stringCaseInsensitiveEquality(parameterValue, std::string("max")) )
		{
			IOPS=-1;
		} else {
			std::string stripped_value = parameterValue;

			bool relativeAdd {false};
			bool relativeMultiply {false};

			if ('+' == parameterValue[0])
			{
				relativeAdd = true;
				parameterValue.erase(0,1);  // hopefully this erases the first character
				trim(parameterValue);
			}
			else if ('*' == parameterValue[0])
			{
				relativeMultiply = true;
				parameterValue.erase(0,1);  // hopefully this erases the first character
				trim(parameterValue);
			}

			std::istringstream is(parameterValue);
			ivy_float ld;

			if ( (!(is >> ld)) || (!is.eof()) || (ld<0.) )
			{
				callers_error_message = std::string("invalid IOPS parameter value \"")+parameterValue
					+ std::string("\".  OK: IOPS=max, IOPS=100, iops = + 10 (adds 10 to IOPS), iops = *1.25 (increases IOPS by 25%)");

				return false;
			}

			if (relativeAdd)           IOPS += ld;
			else if (relativeMultiply) IOPS *= ld;
			else                       IOPS  = ld;
		}
		return true;
	}



	if ( stringCaseInsensitiveEquality(parameterName, std::string("fractionRead")) ) {

		if (0 == parameterValue.length()) {callers_error_message = "fractionRead may not be set to the empty string."; return false;}

		bool hadPercent {false};
		if ('%' == parameterValue[-1 + parameterValue.length()])
		{
			hadPercent = true;
			parameterValue.erase(-1 + parameterValue.length(),1); // hopefully erase last character
			trim(parameterValue);
		}

		std::istringstream is(parameterValue);
		ivy_float ld;
		if ( (!(is >> ld)) || (!is.eof()) || (ld<0.) || (ld>(hadPercent ? 100.0 : 1.0) ) ) {
			callers_error_message = std::string("invalid fractionRead parameter value \"")+parameterValue;
			if (hadPercent) callers_error_message += " %";
			callers_error_message += std::string("\".");

			return false;
		}
		if (0 == iosequencer_type.compare(std::string("sequential")) && ld != 0. && ld != (hadPercent ? 100.0 : 1.0) )
		{
			callers_error_message = std::string("The sequential iosequencer only accepts fractionRead = 0% or 100% or 0.0 or 1.0, not \"")+parameterValue
				+ std::string("\".");
			return false;
		}

		fractionRead=ld;  if (hadPercent) fractionRead = fractionRead / 100.0;

		return true;
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("VolumeCoverageFractionStart")) )
	{
		if (0 == parameterValue.length()) {callers_error_message = "VolumeCoverageFractionStart may not be set to the empty string."; return false;}

		bool hadPercent {false};
		if ('%' == parameterValue[-1 + parameterValue.length()])
		{
			hadPercent = true;
			parameterValue.erase(-1 + parameterValue.length(),1); // hopefully erase last character
			trim(parameterValue);
		}

		std::istringstream is(parameterValue);
		ivy_float ld;
		if ( (!(is >> ld)) || (!is.eof()) || (ld<0.) || (ld>(hadPercent ? 100.0 : 1.0) ) ){
			callers_error_message = std::string("invalid VolumeCoverageFractionStart parameter setting \"")+parameterValue;
			if (hadPercent) callers_error_message += std::string(" %");
			callers_error_message += std::string("\".");
			return false;
		}

		volCoverageFractionStart=ld;  if (hadPercent) volCoverageFractionStart = volCoverageFractionStart / 100.0;

		if (ld>=volCoverageFractionEnd) {
			callers_error_message = std::string("invalid VolumeCoverageFractionStart parameter setting \"")+parameterValue
				+ std::string("\".   Volume coverage start must be before volume coverage end.\n");
			return false;
		}
		return true;
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("VolumeCoverageFractionEnd")) )
	{
		if (0 == parameterValue.length()) {callers_error_message = "VolumeCoverageFractionEnd may not be set to the empty string."; return false;}

		bool hadPercent {false};
		if ('%' == parameterValue[-1 + parameterValue.length()])
		{
			hadPercent = true;
			parameterValue.erase(-1 + parameterValue.length(),1); // hopefully erase last character
			trim(parameterValue);
		}


		std::istringstream is(parameterValue);
		ivy_float ld;
		if ( (!(is >> ld)) || (!is.eof()) || (ld<0.) || (ld>(hadPercent ? 100.0 : 1.0) ) ){
			callers_error_message = std::string("invalid VolumeCoverageFractionEnd parameter setting \"")+parameterValue;
			if (hadPercent) callers_error_message += std::string(" %");
			callers_error_message += std::string("\".");
			return false;
		}
		if (ld<=volCoverageFractionStart) {
			callers_error_message = std::string("invalid VolumeCoverageFractionEnd parameter setting \"")+parameterValue
				+ std::string("\".  Volume coverage start must be before volume coverage end.");
			return false;
		}

		volCoverageFractionEnd=ld;  if (hadPercent) volCoverageFractionEnd = volCoverageFractionEnd / 100.0;

		return true;
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("SeqStartFractionOfCoverage")) ) {

		if (0 == parameterValue.length()) {callers_error_message = "SeqStartFractionOfCoverage may not be set to the empty string."; return false;}

		bool hadPercent {false};
		if ('%' == parameterValue[-1 + parameterValue.length()])
		{
			hadPercent = true;
			parameterValue.erase(-1 + parameterValue.length(),1); // hopefully erase last character
			trim(parameterValue);
		}


		std::istringstream is(parameterValue);
		ivy_float ld;
		if ( (!(is >> ld)) || (!is.eof()) || (ld<0.) || (ld>(hadPercent ? 100.0 : 1.0) ) ){
			callers_error_message = std::string("invalid SeqStartFractionOfCoverage parameter setting \"")+parameterValue;
			if (hadPercent) callers_error_message += std::string(" %");
			callers_error_message += std::string("\".");
			return false;
		}
		seqStartFractionOfCoverage=ld;  if (hadPercent) seqStartFractionOfCoverage = seqStartFractionOfCoverage / 100.0;

		return true;
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("dedupe")) ) {

		if (0 == parameterValue.length()) {callers_error_message = "dedupe may not be set to the empty string."; return false;}

		bool hadPercent {false};
		if ('%' == parameterValue[-1 + parameterValue.length()])
		{
			hadPercent = true;
			parameterValue.erase(-1 + parameterValue.length(),1); // hopefully erase last character
			trim(parameterValue);
		}

		std::istringstream is(parameterValue);
		ivy_float ld;
		if ( (!(is >> ld)) || (!is.eof()) )
		{
			callers_error_message = std::string("invalid dedupe parameter value \"")+parameterValue;
			if (hadPercent) callers_error_message += " %";
			callers_error_message += std::string("\".  Must be a number greater than or equal to 1.0.");

			return false;
		}

		if (hadPercent) ld /= 100.0;

		if ( ld < 1.0 )
		{
			callers_error_message = std::string("invalid dedupe parameter value \"")+parameterValue;
			if (hadPercent) callers_error_message += " %";
			callers_error_message += std::string("\".  Must be a number greater than or equal to 1.0.");

			return false;
		}

        dedupe = ld;
		return true;
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("pattern")) ) {

		pat = parse_pattern(parameterValue);

		if (pat==pattern::invalid)
		{
            std::ostringstream o;
            o << "invalid pattern parameter value \"" << parameterValue << "\".  " << valid_patterns();
			callers_error_message = o.str();

			return false;
		}
		return true;
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("compressibility")) ) {

		if (0 == parameterValue.length()) {callers_error_message = "compressibility may not be set to the empty string."; return false;}

		bool hadPercent {false};
		if ('%' == parameterValue[-1 + parameterValue.length()])
		{
			hadPercent = true;
			parameterValue.erase(-1 + parameterValue.length(),1); // hopefully erase last character
			trim(parameterValue);
		}

		std::istringstream is(parameterValue);
		ivy_float ld;
		if ( (!(is >> ld)) || (!is.eof()) )
		{
			callers_error_message = std::string("invalid compressibility parameter value \"")+parameterValue;
			if (hadPercent) callers_error_message += " %";
			callers_error_message += std::string("\".  Must be a number greater than or equal to 0.0 (0%), and less than 1.0 (100%).");

			return false;
		}

		if (hadPercent) ld /= 100.0;

		if ( ld < 0.0 || ld >= 1.0 )
		{
			callers_error_message = std::string("invalid compressibility parameter value \"")+parameterValue;
			if (hadPercent) callers_error_message += " %";
			callers_error_message += std::string("\".  Must be a number greater than or equal to 0.0 (0%), and less than 1.0 (100%).");

			return false;
		}

        compressibility = ld;
		return true;
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("threads_in_workload_name")) ) {
		std::istringstream is(parameterValue);
		unsigned int eye;
		if ( (!(is >> eye)) || (!is.eof()) || eye==0) {
			callers_error_message = std::string("invalid threads_in_workload_name parameter value \"")+parameterValue +std::string("\".");
			return false;
		}
		threads_in_workload_name=eye;
		return true;
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("this_thread_in_workload")) ) {
		std::istringstream is(parameterValue);
		unsigned int eye;
		if ( (!(is >> eye)) || (!is.eof())) {
			callers_error_message = std::string("invalid this_thread_in_workload parameter value \"")+parameterValue +std::string("\".");
			return false;
		}
		this_thread_in_workload=eye;
		return true;
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("pattern_seed")) ) {
		std::istringstream is(parameterValue);
		uint64_t ui64;
		if ( (!(is >> ui64)) || (!is.eof()) || ui64 == 0) {
			callers_error_message = std::string("invalid pattern_seed parameter value \"")+parameterValue +std::string("\".");
			return false;
		}
		pattern_seed=ui64;
		return true;
	}

	callers_error_message = std::string("Invalid parameter name \"") + parameterName + std::string("\".\n");

	return false;
}


bool IosequencerInput::setMultipleParameters(std::string& callers_error_message, std::string commaSeparatedList)
{
	callers_error_message.clear();

	// non-empty comma-separated list of parameterName=value settings

	unsigned int i=0, start,len;
	std::string nameEqualsValue;
	bool sawGoodOne=false;
	bool sawBadOne=false;

	std::string my_error_message;

	while (i<commaSeparatedList.length()) {
		start=i;
		len=0;
		while (i<commaSeparatedList.length() && commaSeparatedList[i]!=',') {
			i++;
			len++;
		}
		if (len>0) {
			nameEqualsValue=commaSeparatedList.substr(start,len);
			trim(nameEqualsValue);
			if (nameEqualsValue.length()>0) {
				if (setParameter(my_error_message, nameEqualsValue)) {
					sawGoodOne=true;
				} else {
					sawBadOne=true;
					callers_error_message += std::string(" name=value \"") + nameEqualsValue + std::string("\" - " + my_error_message);
				}
			}
		}

		i++;
	}
	return (sawGoodOne && (!sawBadOne));
}

std::string IosequencerInput::toStringFull() {  // we might need to use this form to make iosequencer input rollups correctly track all instances even of default values
	if (iosequencerIsSet)
		return std::string("IosequencerInput<") + getParameterNameEqualsTextValueCommaSeparatedList() + std::string(">");
	else
		return std::string("IosequencerInput<>");
}

std::string IosequencerInput::toString() {
	if (iosequencerIsSet)
		return std::string("IosequencerInput<") + getNonDefaultParameterNameEqualsTextValueCommaSeparatedList() + std::string(">");
	else
		return std::string("IosequencerInput<>");
}

void IosequencerInput::reset() {
        iosequencer_type=std::string("INVALID");
        iosequencerIsSet=false;
	blocksize_bytes=blocksize_bytes_default;
	maxTags=maxTags_default;
	IOPS=IOPS_default; // -1.0 means "drive I/Os as fast as possible"
	fractionRead=fractionRead_default;
	volCoverageFractionStart=volCoverageFractionStart_default;
	volCoverageFractionEnd=volCoverageFractionEnd_default;
	seqStartFractionOfCoverage=seqStartFractionOfCoverage_default;

	dedupe = dedupe_default;
	pat = pattern_default;
	compressibility = compressibility_default;

	threads_in_workload_name = threads_in_workload_name_default;
	this_thread_in_workload = this_thread_in_workload_default;
	pattern_seed = pattern_seed_default;
}


bool IosequencerInput::fromString(std::string s, std::string logfilename) {
	reset();
	std::string n {"IosequencerInput<>"};
	if (s==n) {
		return true;
	}
	if ( s.length() < n.length() ) {
		return false;
	}

	std::string t{"IosequencerInput<"};
	if (t != s.substr(0,t.length())) {
		return false;
	}

	if ( '>' != s[s.length()-1] ) {
		return false;
	}

	std::string my_error_message;

	if ( setMultipleParameters(my_error_message, s.substr(t.length(),s.length()-n.length())))
	{
		return true;
	}
	else
	{
		fileappend(logfilename,std::string("IosequencerInput::fromString() - setting parameters failed - ")+ my_error_message);
		return false;
	}

}

bool IosequencerInput::fromIstream(std::istream& i, std::string logfile)
{
	reset();

	std::string s;
	char c;

	while (true)
	{
		i >> c;
		if (i.fail()) return false;
		s.push_back(c);
		if ('>' == c) return fromString(s,logfile);
	}
}

std::string IosequencerInput::getParameterNameEqualsTextValueCommaSeparatedList() {
	std::ostringstream o;
	o << "iosequencer=" << iosequencer_type;
	o<< ",blocksize=";
	if (0==(blocksize_bytes%(1024*1024))) {
		o << (blocksize_bytes/(1024*1024)) << "MiB";
	} else if (0==(blocksize_bytes%1024)) {
		o << (blocksize_bytes/1024) << " KiB";
	} else {
		o << blocksize_bytes;
	}
	o << ",maxTags=" <<  maxTags;
	if (IOPS==-1) o << ",IOPS=max";
	else          o << ",IOPS=" << IOPS;
	o << ",fractionRead=" << fractionRead;
	o << ",VolumeCoverageFractionStart=" <<  volCoverageFractionStart;
	o << ",VolumeCoverageFractionEnd=" << volCoverageFractionEnd;
    if(0==iosequencer_type.compare(std::string("sequential")))
    {
		o << ",seqStartFractionOfCoverage=" << seqStartFractionOfCoverage;
	}
	o << ",pattern=" << pattern_to_string(pat);
	o << ",dedupe=" << dedupe;
	o << ",compressibility=" << compressibility;

    o << ",threads_in_workload_name=" << threads_in_workload_name;
    o << ",this_thread_in_workload=" << this_thread_in_workload;
    o << ",pattern_seed=" << pattern_seed;

	return o.str();
}

std::string IosequencerInput::getNonDefaultParameterNameEqualsTextValueCommaSeparatedList() {
	std::ostringstream o;
	o << "iosequencer=" << iosequencer_type;

	if (!defaultBlocksize())
	{
		o<< ",blocksize=";
		if (0==(blocksize_bytes%(1024*1024))) {
			o << (blocksize_bytes/(1024*1024)) << "MiB";
		} else if (0==(blocksize_bytes%1024)) {
			o << (blocksize_bytes/1024) << " KiB";
		} else {
			o << blocksize_bytes;
		}
	}

	if ( ! defaultMaxTags() ) { o << ",maxTags=" <<  maxTags; }

	if ( ! defaultIOPS() )
	{
		if (IOPS==-1) o << ",IOPS=max";
		else          o << ",IOPS=" << IOPS;
	}

	if ( ! defaultFractionRead() )              { o << ",fractionRead=" << fractionRead; }

	if ( ! defaultVolCoverageFractionStart())   { o << ",VolumeCoverageFractionStart=" <<  volCoverageFractionStart; }

	if( ! defaultVolCoverageFractionEnd() )     { o << ",VolumeCoverageFractionEnd=" << volCoverageFractionEnd; }

    if( ! defaultSeqStartFractionOfCoverage() )	{ o << ",seqStartFractionOfCoverage=" << seqStartFractionOfCoverage; }

	if (!defaultPattern())                      { o << ",pattern=" << pattern_to_string(pat);}
	if (!defaultDedupe())                       { o << ",dedupe=" << dedupe;}
	if (!defaultCompressibility())              { o << ",compressibility=" << compressibility;}

    if (!defaultThreads_in_workload_name()) { o << ",threads_in_workload_name=" << threads_in_workload_name;}
    if (!defaultThis_thread_in_workload())  { o << ",this_thread_in_workload=" << this_thread_in_workload;}
    if (!defaultPattern_seed())             { o << ",pattern_seed=" << pattern_seed;}
	return o.str();
}

void IosequencerInput::copy(const IosequencerInput& source)
{
	iosequencer_type=source.iosequencer_type;
	iosequencerIsSet=source.iosequencerIsSet;
	blocksize_bytes=source.blocksize_bytes;
	maxTags=source.maxTags;
	IOPS=source.IOPS;
	fractionRead=source.fractionRead;
	volCoverageFractionStart=source.volCoverageFractionStart;
	volCoverageFractionEnd=source.volCoverageFractionEnd;
	seqStartFractionOfCoverage=source.seqStartFractionOfCoverage;

	dedupe=source.dedupe;
	pat=source.pat;
	compressibility=source.compressibility;

	threads_in_workload_name=source.threads_in_workload_name;
	this_thread_in_workload=source.this_thread_in_workload;
	pattern_seed=source.pattern_seed;
}


