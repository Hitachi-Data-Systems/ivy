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
#include "IogeneratorInput.h"

bool IogeneratorInput::setParameter(std::string& callers_error_message, std::string parameterNameEqualsValue) {

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

	if ( stringCaseInsensitiveEquality(parameterName, std::string("iogenerator")) ) {
		if (stringCaseInsensitiveEquality(parameterValue,std::string("random_steady"))) {
			if (iogeneratorIsSet && 0 != iogenerator_type.compare(std::string("random_steady"))) {
				callers_error_message = std::string("when trying to set iogenerator=random_steady it was already set to ")
					+ iogenerator_type + std::string("\".  \"iogenerator\" type may not be changed once set.");
				return false;
			}
			iogenerator_type=parameterValue;
		} else if (stringCaseInsensitiveEquality(parameterValue,std::string("random_independent"))) {
			if (iogeneratorIsSet && 0 != iogenerator_type.compare(std::string("random_independent"))) {
				callers_error_message = std::string("when trying to set iogenerator=random_independent it was already set to ")
					+ iogenerator_type + std::string("\".  \"iogenerator\" type may not be changed once set.");
				return false;
			}
			iogenerator_type=parameterValue;
		} else if (stringCaseInsensitiveEquality(parameterValue,std::string("sequential"))) {
			if (iogeneratorIsSet && 0 != iogenerator_type.compare(std::string("sequential"))) {
				callers_error_message = std::string("when trying to set iogenerator=sequential it was already set to ")
					+ iogenerator_type + std::string("\".  \"iogenerator\" type may not be changed once set.");
				return false;
			}
			iogenerator_type=parameterValue;
		} else {
			callers_error_message = std::string("invalid iogenerator type \"")+parameterNameEqualsValue
				+std::string("\".");
			return false;
		}
		iogeneratorIsSet=true;
		return true;
	}

	if (!iogeneratorIsSet) {
		callers_error_message = std::string("IogeneratorInput::setParameter(\"")+parameterNameEqualsValue
			+std::string("\",): \"iogenerator\" must be set to a valid iogenerator type before setting any other parameters.\n");
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
		if (0 == iogenerator_type.compare(std::string("sequential")) && ld != 0. && ld != (hadPercent ? 100.0 : 1.0) )
		{
			callers_error_message = std::string("The sequential iogenerator only accepts fractionRead = 0% or 100% or 0.0 or 1.0, not \"")+parameterValue
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

	callers_error_message = std::string("name = value (\"")+parameterNameEqualsValue + std::string("\".  Invalid parameter name \"") + parameterName + std::string("\".\n");

	return false;
}


bool IogeneratorInput::setMultipleParameters(std::string& callers_error_message, std::string commaSeparatedList)
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

std::string IogeneratorInput::toStringFull() {  // we might need to use this form to make iogenerator input rollups correctly track all instances even of default values
	if (iogeneratorIsSet)
		return std::string("IogeneratorInput<") + getParameterNameEqualsTextValueCommaSeparatedList() + std::string(">");
	else
		return std::string("IogeneratorInput<>");
}

std::string IogeneratorInput::toString() {
	if (iogeneratorIsSet)
		return std::string("IogeneratorInput<") + getNonDefaultParameterNameEqualsTextValueCommaSeparatedList() + std::string(">");
	else
		return std::string("IogeneratorInput<>");
}

void IogeneratorInput::reset() {
        iogenerator_type=std::string("INVALID");
        iogeneratorIsSet=false;
	blocksize_bytes=4096;
	maxTags=1;
	IOPS=1.; // -1.0 means "drive I/Os as fast as possible"
	fractionRead=1.0;
	volCoverageFractionStart=0.0;
	volCoverageFractionEnd=1.0;
	seqStartFractionOfCoverage=0.0;
}


bool IogeneratorInput::fromString(std::string s, std::string logfilename) {
	reset();
	std::string n {"IogeneratorInput<>"};
	if (s==n) {
		return true;
	}
	if ( s.length() < n.length() ) {
		return false;
	}

	std::string t{"IogeneratorInput<"};
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
		fileappend(logfilename,std::string("IogeneratorInput::fromString() - setting parameters failed - ")+ my_error_message);
		return false;
	}

}

bool IogeneratorInput::fromIstream(std::istream& i, std::string logfile)
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

std::string IogeneratorInput::getParameterNameEqualsTextValueCommaSeparatedList() {
	std::ostringstream o;
	o << "iogenerator=" << iogenerator_type;
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
    if(0==iogenerator_type.compare(std::string("sequential")))
    {
		o << ",seqStartFractionOfCoverage=" << seqStartFractionOfCoverage;
	}
	return o.str();
}

std::string IogeneratorInput::getNonDefaultParameterNameEqualsTextValueCommaSeparatedList() {
	std::ostringstream o;
	o << "iogenerator=" << iogenerator_type;

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
	if ( ! defaultMaxTags() )
	{
		o << ",maxTags=" <<  maxTags;
	}

	if ( ! defaultIOPS() )
	{
		if (IOPS==-1) o << ",IOPS=max";
		else          o << ",IOPS=" << IOPS;
	}

	if ( ! defaultFractionRead() )
	{
		o << ",fractionRead=" << fractionRead;
	}

	if ( ! defaultVolCoverageFractionStart())
	{
		o << ",VolumeCoverageFractionStart=" <<  volCoverageFractionStart;
	}

	if( ! defaultVolCoverageFractionEnd() )
	{
		o << ",VolumeCoverageFractionEnd=" << volCoverageFractionEnd;
	}


    if( ! defaultSeqStartFractionOfCoverage() )
	{
		o << ",seqStartFractionOfCoverage=" << seqStartFractionOfCoverage;

	}
	return o.str();
}

void IogeneratorInput::copy(const IogeneratorInput& source)
{
	iogenerator_type=source.iogenerator_type;
	iogeneratorIsSet=source.iogeneratorIsSet;
	blocksize_bytes=source.blocksize_bytes;
	maxTags=source.maxTags;
	IOPS=source.IOPS;
	fractionRead=source.fractionRead;
	volCoverageFractionStart=source.volCoverageFractionStart;
	volCoverageFractionEnd=source.volCoverageFractionEnd;
	seqStartFractionOfCoverage=source.seqStartFractionOfCoverage;
}


