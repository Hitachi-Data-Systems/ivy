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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdlib.h> // atoi
#include <list>
#include <algorithm> // for ivyhelpers.h find_if()

#include "ivyhelpers.h"
#include "ivydefines.h"
#include "IosequencerInput.h"

std::pair<bool,std::string> IosequencerInput::setParameter(std::string parameterNameEqualsValue) {
    // This is ancient code from when regexes were still broken in libstdc++.
	if (parameterNameEqualsValue.length()<3)
	{
        std::ostringstream o;
        o << "not name=value - \"" << parameterNameEqualsValue << "\".";
		return std::make_pair(false,o.str());
	}

	unsigned int equalsPosition=1;

	while ( (equalsPosition<(parameterNameEqualsValue.length()-2)) && (parameterNameEqualsValue[equalsPosition]!='='))
		equalsPosition++;

	if (parameterNameEqualsValue[equalsPosition]!='=')
	{
        std::ostringstream o;
        o << "not name=value - \"" << parameterNameEqualsValue << "\".";
		return std::make_pair(false,o.str());
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

	if (parameterName=="")
	{
        std::ostringstream o;
        o << "looking for name=value - parameter name missing in \"" << parameterNameEqualsValue << "\".";
		return std::make_pair(false,o.str());
	}

	if (parameterValue=="")
	{
        std::ostringstream o;
        o << "looking for name=value - parameter value missing in \"" << parameterNameEqualsValue << "\".";
		return std::make_pair(false,o.str());
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("iosequencer")) ) {
		if (normalized_identifier_equality(parameterValue,std::string("random_steady"))) {
			if (iosequencerIsSet && ! normalized_identifier_equality(iosequencer_type, std::string("random_steady")))
            {
                std::ostringstream o;
                o << "when setting \"" << parameterNameEqualsValue
                    << "\", iosequencer was already set to " << iosequencer_type
                    << "\".  iosequencer type may not be changed once set.";
                return std::make_pair(false,o.str());
            }
			iosequencer_type=parameterValue;
		} else if (normalized_identifier_equality(parameterValue,std::string("random_independent"))) {
			if (iosequencerIsSet && ! normalized_identifier_equality(iosequencer_type,std::string("random_independent")))
            {
                std::ostringstream o;
                o << "when setting \"" << parameterNameEqualsValue
                    << "\", iosequencer was already set to " << iosequencer_type
                    << "\".  iosequencer type may not be changed once set.";
                return std::make_pair(false,o.str());
            }
			iosequencer_type=parameterValue;
		} else if (stringCaseInsensitiveEquality(parameterValue,std::string("sequential"))) {
			if (iosequencerIsSet && 0 != iosequencer_type.compare(std::string("sequential")))
            {
                std::ostringstream o;
                o << "when setting \"" << parameterNameEqualsValue
                    << "\", iosequencer was already set to " << iosequencer_type
                    << "\".  iosequencer type may not be changed once set.";
                return std::make_pair(false,o.str());
            }
			iosequencer_type=parameterValue;
		}
		else
		{
            std::ostringstream o;
            o << "when setting \"" << parameterNameEqualsValue
                << "\", invalid iosequencer type \""  << parameterValue << "\".";
            return std::make_pair(false,o.str());
		}
		iosequencerIsSet=true;
        return std::make_pair(true,"");
	}

	if (!iosequencerIsSet)
    {
        std::ostringstream o;
        o << "when setting \"" << parameterNameEqualsValue
            << "\", \"iosequencer\" was not already set.  \"iosequencer\" must be set to a valid iosequencer type before setting any other parameters.";
        return std::make_pair(false,o.str());
    }

//	        "For iosequencer = { random_steady, random_independent } additional parameters are \n"
//        "                  hot_zone_size_bytes - default 0 (zero), accepts KiB, MiB, GiB, TiB suffixes,\n"
//        "                  hot_zone_fraction - default 0 (zero) fraction of all I/Os that should go to the hit_area.  Accepts % suffix.\n"
//        "                  hot_zone_read_fraction - default 0 (zero) fraction of all read I/Os that should go to the hit_area.\n"
//        "                  hot_zone_write_fraction - default 0 (zero) fraction of all write I/Os that should go to the hit_area.\n"
//        "                  If either of hot_zone_read_fraction or hot_zone_write_fraction are non-zero, hot_zone_fraction is ignored.\n\n"

	if ( normalized_identifier_equality(parameterName, std::string("hot_zone_size_bytes")) )
	{
        if ( (!normalized_identifier_equality(iosequencer_type,std::string("random_steady")))
          && (!normalized_identifier_equality(iosequencer_type,std::string("random_independent"))) )
        {
            std::ostringstream o;
            o << "IosequencerInput::setParameter( parameter name \"" << parameterName << "\""
                << ", parameter value \"" << parameterValue << "\")"
                << " - hot_zone_size_bytes is only valid for random_steady and random_independent iosequencers, not " << iosequencer_type  << "." << std::endl;
            return std::make_pair(false,o.str());
        }
        try
        {
            hot_zone_size_bytes = number_optional_trailing_KiB_MiB_GiB_TiB(parameterValue);
        }
        catch (const std::exception &e)
        {
            std::ostringstream o;
            o << "IosequencerInput::setParameter( parameter name \"" << parameterName << "\""
                << ", parameter value \"" << parameterValue << "\")"
                << " - invalid parameter value.  Must be an unsigned integer (digits) optionally followed by KiB, MiB, GiB, or TiB." << std::endl
                << "Error when trying to parse the value was " << e.what() << std::endl;
            return std::make_pair(false,o.str());
        }

        return std::make_pair(true,"");
    }

	if ( normalized_identifier_equality(parameterName, std::string("hot_zone_IOPS_fraction")) )
	{
        if ( (!normalized_identifier_equality(iosequencer_type,std::string("random_steady")))
          && (!normalized_identifier_equality(iosequencer_type,std::string("random_independent"))) )
        {
            std::ostringstream o;
            o << "IosequencerInput::setParameter( parameter name \"" << parameterName << "\""
                << ", parameter value \"" << parameterValue << "\")"
                << " - hot_zone_IOPS_fraction is only valid for random_steady and random_independent iosequencers, not " << iosequencer_type  << "." << std::endl;
            return std::make_pair(false,o.str());
        }

        try
        {
            hot_zone_IOPS_fraction = number_optional_trailing_percent(parameterValue,"");
        }
        catch (const std::exception &e)
        {
            std::ostringstream o;
            o << "IosequencerInput::setParameter( parameter name \"" << parameterName << "\""
                << ", parameter value \"" << parameterValue << "\")"
                << " - invalid parameter value.  Must be a number optionally followed by a percent sign \'%\'." << std::endl
                << "Error when trying to parse the value was " << e.what() << std::endl;
            return std::make_pair(false,o.str());
        }

        if (hot_zone_IOPS_fraction > 1.0 || hot_zone_IOPS_fraction < 0.0)
        {
            std::ostringstream o;
            o << "IosequencerInput::setParameter( parameter name \"" << parameterName << "\""
                << ", parameter value \"" << parameterValue << "\") - value must be from 0.0 to 1.0 (from 0% to 100%).";
            return std::make_pair(false,o.str());
        }
        return std::make_pair(true,"");
    }

	if ( normalized_identifier_equality(parameterName, std::string("hot_zone_read_fraction")) )
	{
        if ( (!normalized_identifier_equality(iosequencer_type,std::string("random_steady")))
          && (!normalized_identifier_equality(iosequencer_type,std::string("random_independent"))) )
        {
            std::ostringstream o;
            o << "IosequencerInput::setParameter( parameter name \"" << parameterName << "\""
                << ", parameter value \"" << parameterValue << "\")"
                << " - hot_zone_read_fraction is only valid for random_steady and random_independent iosequencers, not " << iosequencer_type  << "." << std::endl;
            return std::make_pair(false,o.str());
        }

        try
        {
            hot_zone_read_fraction = number_optional_trailing_percent(parameterValue,"");
        }
        catch (const std::exception &e)
        {
            std::ostringstream o;
            o << "IosequencerInput::setParameter( parameter name \"" << parameterName << "\""
                << ", parameter value \"" << parameterValue << "\")"
                << " - invalid parameter value.  Must be a number optionally followed by a percent sign \'%\'." << std::endl
                << "Error when trying to parse the value was " << e.what() << std::endl;
            return std::make_pair(false,o.str());
        }
        if (hot_zone_read_fraction > 1.0 || hot_zone_read_fraction < 0.0)
        {
            std::ostringstream o;
            o << "IosequencerInput::setParameter( parameter name \"" << parameterName << "\""
                << ", parameter value \"" << parameterValue << "\") - value must be from 0.0 to 1.0 (from 0% to 100%).";
            return std::make_pair(false,o.str());
        }

        return std::make_pair(true,"");
    }


	if ( normalized_identifier_equality(parameterName, std::string("hot_zone_write_fraction")) )
	{
        if ( (!normalized_identifier_equality(iosequencer_type,std::string("random_steady")))
          && (!normalized_identifier_equality(iosequencer_type,std::string("random_independent"))) )
        {
            std::ostringstream o;
            o << "IosequencerInput::setParameter( parameter name \"" << parameterName << "\""
                << ", parameter value \"" << parameterValue << "\")"
                << " - hot_zone_write_fraction is only valid for random_steady and random_independent iosequencers, not " << iosequencer_type  << "." << std::endl;
            return std::make_pair(false,o.str());
        }

        try
        {
            hot_zone_write_fraction = number_optional_trailing_percent(parameterValue,"");
        }
        catch (const std::exception &e)
        {
            std::ostringstream o;
            o << "IosequencerInput::setParameter( parameter name \"" << parameterName << "\""
                << ", parameter value \"" << parameterValue << "\")"
                << " - invalid parameter value.  Must be a number optionally followed by a percent sign \'%\'." << std::endl
                << "Error when trying to parse the value was " << e.what() << std::endl;
            return std::make_pair(false,o.str());
        }
        if (hot_zone_write_fraction > 1.0 || hot_zone_write_fraction < 0.0)
        {
            std::ostringstream o;
            o << "IosequencerInput::setParameter( parameter name \"" << parameterName << "\""
                << ", parameter value \"" << parameterValue << "\") - value must be from 0.0 to 1.0 (from 0% to 100%).";
            return std::make_pair(false,o.str());
        }

        return std::make_pair(true,"");
    }


	if ( normalized_identifier_equality(parameterName, std::string("blocksize")) ) {

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
		if ( is.fail() || (!is.eof()) )
		{
            std::ostringstream o;
            o << "invalid blocksize \"" << parameterValue;
			if (KiB) o << " KiB";
			if (MiB) o << " MiB";
			o << "\".";
            return std::make_pair(false,o.str());
		}
		if (KiB) b*=1024;
		if (MiB) b*=(1024*1024);
		if (0 != b%512)
		{

            std::ostringstream o;
            o << "invalid blocksize \"" << parameterValue;
			if (KiB) o << " KiB";
			if (MiB) o << " MiB";
			o << "\".  Blocksize must be a multiple of 512.";  /// NEED 4K SECTOR SUPPORT HERE
            return std::make_pair(false,o.str());
		}
		blocksize_bytes=b;
		return std::make_pair(true,"");
	}

	if ( normalized_identifier_equality(parameterName, std::string("maxTags")) ) {
		std::istringstream is(parameterValue);
		int mt;
		if ( (!(is >> mt)) || (!is.eof()) || (mt<1) || (mt > MAX_MAXTAGS))
		{
			return std::make_pair(false, std::string("invalid maxTags parameter value \"")+parameterValue +std::string("\"."));
		}
		maxTags=mt;
		return std::make_pair(true,"");
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("IOPS")) ) {

		if ( 0 == parameterValue.length() ) { return std::make_pair(false, "IOPS may not be set to the null string."); }

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
                std::ostringstream o;
                o << "invalid IOPS parameter value \"" << parameterValue << "\".  OK: IOPS=max, IOPS=100, iops = + 10 (adds 10 to IOPS), iops = *1.25 (increases IOPS by 25%)";
                return std::make_pair(false,o.str());
			}

			if (relativeAdd)           IOPS += ld;
			else if (relativeMultiply) IOPS *= ld;
			else                       IOPS  = ld;
		}
		return std::make_pair(true,"");
	}



	if ( normalized_identifier_equality(parameterName, std::string("fractionRead")) ) {

		if (0 == parameterValue.length()) { return std::make_pair(false,"fractionRead may not be set to the empty string."); }

		bool hadPercent {false};
		if ('%' == parameterValue[-1 + parameterValue.length()])
		{
			hadPercent = true;
			parameterValue.erase(-1 + parameterValue.length(),1); // hopefully erase last character
			trim(parameterValue);
		}

		std::istringstream is(parameterValue);
		ivy_float ld;
		if ( (!(is >> ld)) || (!is.eof()) || (ld<0.) || (ld>(hadPercent ? 100.0 : 1.0) ) )
		{
			std::ostringstream o;
			o << "invalid fractionRead parameter value \"" << parameterValue;
			if (hadPercent) o << " %";
			o << "\".";

			return std::make_pair(false,o.str());
		}
		if (normalized_identifier_equality(iosequencer_type, std::string("sequential")) && ld != 0. && ld != (hadPercent ? 100.0 : 1.0) )
		{
			std::ostringstream o;
			o << "The sequential iosequencer only accepts fractionRead = 0% or 100% or 0.0 or 1.0, not \"" << parameterValue << "\".";
			return std::make_pair(false,o.str());
		}

		fractionRead=ld;  if (hadPercent) fractionRead = fractionRead / 100.0;

		return std::make_pair(true,"");
	}

	if ( normalized_identifier_equality(parameterName, std::string("VolumeCoverageFractionStart")) )
	{
		if (0 == parameterValue.length()) { return std::make_pair(false, "VolumeCoverageFractionStart may not be set to the empty string."); }

		bool hadPercent {false};
		if ('%' == parameterValue[-1 + parameterValue.length()])
		{
			hadPercent = true;
			parameterValue.erase(-1 + parameterValue.length(),1); // hopefully erase last character
			trim(parameterValue);
		}

		std::istringstream is(parameterValue);
		ivy_float ld;
		if ( (!(is >> ld)) || (!is.eof()) || (ld<0.) || (ld>(hadPercent ? 100.0 : 1.0) ) )
		{
            std::ostringstream o;
			o << "invalid VolumeCoverageFractionStart parameter setting \"" << parameterValue;
			if (hadPercent) o << " %";
			o << "\".";
			return std::make_pair(false,o.str());
		}

		volCoverageFractionStart=ld;

		if (hadPercent) volCoverageFractionStart = volCoverageFractionStart / 100.0;

		if (volCoverageFractionStart >= volCoverageFractionEnd)
		{
			std::ostringstream o;
			o << "invalid VolumeCoverageFractionStart parameter setting \"" << parameterValue;
			if (hadPercent) o << "%";
            o << "\".   Volume coverage start must be before volume coverage end.";
			return std::make_pair(false,o.str());
		}

		return std::make_pair(true,"");
	}

	if ( normalized_identifier_equality(parameterName, std::string("VolumeCoverageFractionEnd")) )
	{
		if (0 == parameterValue.length()) { return std::make_pair(false, "VolumeCoverageFractionEnd may not be set to the empty string."); }

		bool hadPercent {false};
		if ('%' == parameterValue[-1 + parameterValue.length()])
		{
			hadPercent = true;
			parameterValue.erase(-1 + parameterValue.length(),1); // hopefully erase last character
			trim(parameterValue);
		}


		std::istringstream is(parameterValue);
		ivy_float ld;
		if ( (!(is >> ld)) || (!is.eof()) || (ld<0.) || (ld>(hadPercent ? 100.0 : 1.0) ) )
		{
			std::ostringstream o;
			o << "invalid VolumeCoverageFractionEnd parameter setting \"" << parameterValue;
			if (hadPercent) o << " %";
			o << "\".";
			return std::make_pair(false,o.str());
		}

		if (hadPercent) ld /= 100.0;

		volCoverageFractionEnd=ld;

		if (volCoverageFractionEnd<=volCoverageFractionStart)
		{
            std::ostringstream o;
            o << "invalid VolumeCoverageFractionEnd parameter setting \"" << parameterValue;
            if (hadPercent) o << "%";
            o << "\".  Volume coverage start must be before volume coverage end.";
			return std::make_pair(false,o.str());
		}


		return std::make_pair(true,"");
	}

	if ( normalized_identifier_equality(parameterName, std::string("SeqStartFractionOfCoverage")) ) {

		if (0 == parameterValue.length()) { return std::make_pair(false, "SeqStartFractionOfCoverage may not be set to the empty string."); }

		bool hadPercent {false};
		if ('%' == parameterValue[-1 + parameterValue.length()])
		{
			hadPercent = true;
			parameterValue.erase(-1 + parameterValue.length(),1); // hopefully erase last character
			trim(parameterValue);
		}


		std::istringstream is(parameterValue);
		ivy_float ld;
		if ( (!(is >> ld)) || (!is.eof()) || (ld<0.) || (ld>(hadPercent ? 100.0 : 1.0) ) )
		{
			std::ostringstream o;
			o << "invalid SeqStartFractionOfCoverage parameter setting \"" << parameterValue;
			if (hadPercent) o << " %";
			o << "\".";
			return std::make_pair(false,o.str());
		}
		seqStartFractionOfCoverage=ld;
		if (hadPercent) seqStartFractionOfCoverage = seqStartFractionOfCoverage / 100.0;

		return std::make_pair(true,"");
	}

	if ( normalized_identifier_equality(parameterName, std::string("dedupe")) ) {

		if (0 == parameterValue.length()) { return std::make_pair(false, "dedupe may not be set to the empty string."); }

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
			std::ostringstream o;
			o << "invalid dedupe parameter value \"" << parameterValue;
			if (hadPercent) o << " %";
			o << "\".  Must be a number greater than or equal to 1.0.";
			return std::make_pair(false,o.str());
		}

		if (hadPercent) ld /= 100.0;

		if ( ld < 1.0 )
		{
			std::ostringstream o;
			o << "invalid dedupe parameter value \"" << parameterValue;
			if (hadPercent) o << " %";
			o << "\".  Must be a number greater than or equal to 1.0.";
			return std::make_pair(false,o.str());
		}

        dedupe = ld;
		return std::make_pair(true,"");
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("pattern")) ) {

		pat = parse_pattern(parameterValue);

		if (pat==pattern::invalid)
		{
            std::ostringstream o;
            o << "invalid pattern parameter value \"" << parameterValue << "\".  " << valid_patterns();
			return std::make_pair(false,o.str());
		}

		return std::make_pair(true,"");
	}

	if ( stringCaseInsensitiveEquality(parameterName, std::string("compressibility")) ) {

		if (0 == parameterValue.length()) { return std::make_pair(false, "compressibility may not be set to the empty string."); }

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
			std::ostringstream o;
			o << "invalid compressibility parameter value \"" << parameterValue;
			if (hadPercent) o << " %";
			o << "\".  Must be a number greater than or equal to 0.0 (0%), and less than 1.0 (100%).";

			return std::make_pair(false,o.str());
		}

		if (hadPercent) ld /= 100.0;

		if ( ld < 0.0 || ld >= 1.0 )
		{
			std::ostringstream o;
			o << "invalid compressibility parameter value \"" << parameterValue;
			if (hadPercent) o << " %";
			o << "\".  Must be a number greater than or equal to 0.0 (0%), and less than 1.0 (100%).";

			return std::make_pair(false,o.str());
		}

        compressibility = ld;
		return std::make_pair(true,"");
	}

	if ( normalized_identifier_equality(parameterName, std::string("threads_in_workload_name")) ) {
		std::istringstream is(parameterValue);
		unsigned int eye;
		if ( (!(is >> eye)) || (!is.eof()) || eye==0)
		{
			std::ostringstream o;
			o << "invalid threads_in_workload_name parameter value \"" << parameterValue << "\".";
			return std::make_pair(false,o.str());
		}
		threads_in_workload_name=eye;
		return std::make_pair(true,"");
	}

	if ( normalized_identifier_equality(parameterName, std::string("this_thread_in_workload")) ) {
		std::istringstream is(parameterValue);
		unsigned int eye;
		if ( (!(is >> eye)) || (!is.eof()))
		{
			std::ostringstream o;
			o << "invalid this_thread_in_workload parameter value \"" << parameterValue << "\".";
			return std::make_pair(false,o.str());
		}
		this_thread_in_workload=eye;
		return std::make_pair(true,"");
	}

	if ( normalized_identifier_equality(parameterName, std::string("pattern_seed")) ) {
		std::istringstream is(parameterValue);
		uint64_t ui64;
		if ( (!(is >> ui64)) || (!is.eof()) || ui64 == 0)
		{
			std::ostringstream o;
			o << "invalid pattern_seed parameter value \"" << parameterValue << "\".";
			return std::make_pair(false,o.str());
		}
		pattern_seed=ui64;
		return std::make_pair(true,"");
	}

	{
        std::ostringstream o;
        o << "Invalid parameter name \"" << parameterName << "\".";
        return std::make_pair(false,o.str());
    }
}


std::pair<bool,std::string> IosequencerInput::setMultipleParameters(std::string commaSeparatedList)
{
	// non-empty comma-separated list of parameterName=value settings

	unsigned int i=0, start,len;
	std::string nameEqualsValue;
	bool sawGoodOne=false;
	bool sawBadOne=false;

	std::string composite_error_message {};

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
                auto rv = setParameter(nameEqualsValue);
				if (rv.first) {
					sawGoodOne=true;
				} else {
                    if (sawBadOne) composite_error_message.push_back(' ');
					sawBadOne=true;
					composite_error_message += std::string("name=value \"") + nameEqualsValue + std::string("\" - " + rv.second);
				}
			}
		}

		i++;
	}

	return std::make_pair(sawGoodOne && (!sawBadOne),composite_error_message);
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

	hot_zone_size_bytes = 0;
	hot_zone_IOPS_fraction = 0.0;
	hot_zone_read_fraction = 0.0;
	hot_zone_write_fraction = 0.0;

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

	auto rc = setMultipleParameters(s.substr(t.length(),s.length()-n.length()));

	if (rc.first)
	{
		return true;
	}
	else
	{
		fileappend(logfilename,std::string("IosequencerInput::fromString() - setting parameters failed - ")+ rc.second);
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
    if(normalized_identifier_equality(iosequencer_type,std::string("sequential")))
    {
		o << ",seqStartFractionOfCoverage=" << seqStartFractionOfCoverage;
	}
	o << ",pattern=" << pattern_to_string(pat);
	o << ",dedupe=" << dedupe;
	o << ",compressibility=" << compressibility;

    o << ",threads_in_workload_name=" << threads_in_workload_name;
    o << ",this_thread_in_workload=" << this_thread_in_workload;
    o << ",pattern_seed=" << pattern_seed;

    if( normalized_identifier_equality(iosequencer_type,std::string("random_steady"))
     || normalized_identifier_equality(iosequencer_type,std::string("random_independent")) )
    {
        o << ",hot_zone_size_bytes=" << put_on_KiB_etc_suffix(hot_zone_size_bytes);

        if (hot_zone_read_fraction != 0.0 || hot_zone_write_fraction != 0.0)
        {
            o << ",hot_zone_read_fraction=\"" << (hot_zone_read_fraction*100.0) << "%\"";
            o << ",hot_zone_write_fraction=\"" << (hot_zone_write_fraction*100.0) << "%\"";
        }
        else
        {
            o << ",hot_zone_IOPS_fraction=\"" << (hot_zone_IOPS_fraction*100.0) << "%\"";
        }
    }

	return o.str();
}

std::string IosequencerInput::getNonDefaultParameterNameEqualsTextValueCommaSeparatedList() {
	std::ostringstream o;
	o << "iosequencer=" << iosequencer_type;

	if (!defaultBlocksize())
	{
		o<< ",blocksize=";
		if (0==(blocksize_bytes%(1024*1024))) {
			o << (blocksize_bytes/(1024*1024)) << " MiB";
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

    if( normalized_identifier_equality(iosequencer_type,std::string("random_steady"))
     || normalized_identifier_equality(iosequencer_type,std::string("random_independent")) )
    {
        if (hot_zone_size_bytes > 0)
        {
            o << ",hot_zone_size_bytes=" << put_on_KiB_etc_suffix(hot_zone_size_bytes);

            if (hot_zone_read_fraction != 0.0 || hot_zone_write_fraction != 0.0)
            {
                o << ",hot_zone_read_fraction=\"" << (hot_zone_read_fraction*100.0) << "%\"";
                o << ",hot_zone_write_fraction=\"" << (hot_zone_write_fraction*100.0) << "%\"";
            }
            else
            {
                o << ",hot_zone_IOPS_fraction=\"" << (hot_zone_IOPS_fraction*100.0) << "%\"";
            }
        }
    }

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

	hot_zone_size_bytes     = source.hot_zone_size_bytes;
	hot_zone_IOPS_fraction  = source.hot_zone_IOPS_fraction;
	hot_zone_read_fraction  = source.hot_zone_read_fraction;
	hot_zone_write_fraction = source.hot_zone_write_fraction;

}


