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

#include "ivy_engine.h"

extern std::set<std::string> valid_IosequencerInput_parameters;

std::pair<bool,std::string> ParameterValueLookupTable::fromString(std::string s)
{
	contents.clear();
	workload_loopy.clear();

	return addString(s);
}

std::pair<bool,std::string> ParameterValueLookupTable::addString(std::string s)
{
	unsigned int i {0};

	unsigned int identifier_start, value_start, identifier_length, value_length;
	char starting_quote;

	while (true) {
		while (i < s.length() && (isspace(s[i]) || ',' == s[i]))
		{ // whitespace or commas before identifier
			i++;
		}

		if ( i >= s.length() ) return std::make_pair(true,std::string(""));

		if (!isalpha(s[i])) // start of identifier
		{
            std::ostringstream o;
			o << "ParameterValueLookupTable::fromString() invalid input string:" << std::endl << s << std::endl;
			for (unsigned int j=0; j<i; j++) o << ' ';
			o << '^' << std::endl << "Key must start with alphabetic, or value needs to be quoted." << std::endl;
			contents.clear();
			std::cout << o.str();
			log(m_s.masterlogfile, o.str());
			return std::make_pair(false,o.str());
		}

		identifier_start=i;
		identifier_length=1;
		i++;

		while ( i<s.length() && (isalnum(s[i]) || '_' == s[i]) )
		{ // identifier
			i++;
			identifier_length++;
		}

		while (i<s.length() && isspace(s[i]))
		{ // step over whitespace following identifier
			i++;
		}

		if ( (i>=s.length()) || ( ('=' != s[i]) && ('(' != s[i])) )
		{
            std::ostringstream o;
			o << "ParameterValueLookupTable::fromString() invalid input string:" << std::endl << s << std::endl;
			for (unsigned int j=0; j<i; j++) o << ' ';
			o << '^' << std::endl << "No '=' following identifier." << std::endl;
			contents.clear();
			std::cout << o.str();
			log(m_s.masterlogfile,o.str());
			return std::make_pair(false,o.str());
		}

		if (s[i] == '=') // if s[i] is '(', the '=' is optional before the parenthesized list.
		{
		    i++; // step over '='

            while (i<s.length() && isspace(s[i])) // step over whitespace following '='
            {
                i++;
            }
        }


		if (i>=s.length())
		{
            std::ostringstream o;
			o << "ParameterValueLookupTable::fromString() invalid input string:" << std::endl << s << std::endl;
			for (unsigned int j=0; j<i; j++) o << ' ';
			o << '^' << std::endl << "No value following '='." << std::endl
				<< "To specify that a value is the null string, say identifier=\"\"" << std::endl;
			contents.clear();
			std::cout << o.str();
			log(m_s.masterlogfile,o.str());
			return std::make_pair(false,o.str());
		}

        if (s[i] == '(')
        {
            // we have the ( in something like:     blocksize_bytes = ( "2 KiB", 4096 )

            // The attribute name is not a "go" parameter, rather it's an IosequencerInput parameter name,
            // and we're going to loop over run_subinterval_sequence using edit rollup to set the values.

            std::string normalized_parameter_name = normalize_identifier(s.substr(identifier_start,identifier_length));

            bool processing_IOPS_curve;

            if (normalized_parameter_name == normalize_identifier("IOPS_curve"))
            {
                processing_IOPS_curve = true;
            }
            else
            {
                processing_IOPS_curve = false;

                auto nit = valid_IosequencerInput_parameters.find(normalized_parameter_name);
                if (nit == valid_IosequencerInput_parameters.end())
                {
                    std::ostringstream o;
                    o << "<Error> in go parameters - \"" << s.substr(identifier_start,identifier_length) << "\" was not found as a valid workload parameter name."
                        << "  Invalid input string:" << std::endl << s << std::endl;
                    contents.clear();
                    workload_loopy.clear();
                    std::cout << o.str();
                    log(m_s.masterlogfile,o.str());
                    return std::make_pair(false,o.str());
                }
            }

            i++; // step over '('

            workload_loopy.loop_levels.emplace_back();
            {
                loop_level& ll = workload_loopy.loop_levels.back();

                if (processing_IOPS_curve)
                {
                    ll.attribute = "IOPS_curve";
                    ll.values.push_back("IOPS=max");
                }
                else
                {
                    ll.attribute = s.substr(identifier_start,identifier_length);
                }

                while (true)
                {

                    while (i<s.length() && (isspace(s[i]) || s[i] == ',' ))
                    { // step over whitespace and commas
                        i++;
                    }

                    if (i >= s.length())
                    {
                        std::ostringstream o;
                        o << "ParameterValueLookupTable::fromString() invalid input string:" << std::endl << s << std::endl;
                        o << "For list of values for \"" << ll.attribute << " - ran off the end of the string looking for closing \")\"" << std::endl;
                        contents.clear();
                        workload_loopy.clear();
                        std::cout << o.str();
                        log(m_s.masterlogfile,o.str());
                        return std::make_pair(false,o.str());
                    }

                    if (s[i] == ')') { i++; break; }

                    // start of value
                    if ('\'' == s[i] || '\"' == s[i])
                    { // value is quoted string
                        starting_quote=s[i];
                        i++;
                        value_start=i;
                        value_length=0;
                        while (i<s.length() && starting_quote != s[i]) {i++; value_length++;}
                        if (i>=s.length())
                        {
                            std::ostringstream o;
                            o << "ParameterValueLookupTable::fromString() invalid input string:" << std::endl << s << std::endl;
                            for (unsigned int j=0; j<i; j++) o << ' ';
                            o << '^' << std::endl << "No ending quote." << std::endl;
                            contents.clear();
                            std::cout << o.str();
                            log(m_s.masterlogfile,o.str());
                            return std::make_pair(false,o.str());
                        }
                        i++; // step over ending quote
                    }
                    else
                    { // value is unquoted alphanumerics and underscores, periods, percent signs, and colons
                        if (!( isalnum(s[i]) || '_' == s[i] || '.' == s[i] || '%' == s[i] || ':' == s[i] ))
                        {
                            std::ostringstream o;
                            o << "ParameterValueLookupTable::fromString() invalid input string:" << std::endl << s << std::endl;
                            for (unsigned int j=0; j<i; j++) o << ' ';
                            o << '^' << std::endl << "Unquoted value must be all alphanumerics, underscores \'_\', periods \'.\', percent signs \'%', and colons \':\'." << std::endl;
                            contents.clear();
                            std::cout << o.str();
                            log(m_s.masterlogfile,o.str());
                            return std::make_pair(false,o.str());
                        }
                        value_start=i;
                        value_length=1;
                        i++;
                        while (i<s.length() && (isalnum(s[i]) || '_' == s[i] || '.' == s[i] || '%' == s[i] || ':' == s[i]))
                        {
                            i++;
                            value_length++;
                        }
                    }

                    std::string value_text = s.substr(value_start,value_length);

                    if (processing_IOPS_curve)
                    {
                        if (!std::regex_match(value_text,float_number_optional_trailing_percent_regex))
                        {
                                std::ostringstream o;
                                o << "For IOPS_curve, invalid value \"" << value_text << "\".  IOPS_curve values must be numbers with optional trailing percent sign % and must be greater than zero and less than or equal to 200%." << std::endl;
                                contents.clear();
                                std::cout << o.str();
                                log(m_s.masterlogfile,o.str());
                                return std::make_pair(false,o.str());
                        }

                        ivy_float v = number_optional_trailing_percent(value_text,"parsing IOPS_curve value");

                        if (v <= 0.0 || v > 2.0)
                        {
                                std::ostringstream o;
                                o << "For IOPS_curve, invalid value \"" << value_text << "\".  IOPS_curve values must be numbers with optional trailing percent sign % and must be greater than zero and less than or equal to 200%." << std::endl;
                                contents.clear();
                                std::cout << o.str();
                                log(m_s.masterlogfile,o.str());
                                return std::make_pair(false,o.str());
                        }
                    }

                    ll.values.push_back(value_text);
                }
            }
        }
        else // not a list of values enclosed in parenthesis
        {
            // start of value
            if ('\'' == s[i] || '\"' == s[i])
            { // value is quoted string
                starting_quote=s[i];
                i++;
                value_start=i;
                value_length=0;
                while (i<s.length() && starting_quote != s[i]) {i++; value_length++;}
                if (i>=s.length())
                {
                    std::ostringstream o;
                    o << "ParameterValueLookupTable::fromString() invalid input string:" << std::endl << s << std::endl;
                    for (unsigned int j=0; j<i; j++) o << ' ';
                    o << '^' << std::endl << "No ending quote." << std::endl;
                    contents.clear();
                    std::cout << o.str();
                    log(m_s.masterlogfile,o.str());
                    return std::make_pair(false,o.str());
                }
                i++; // step over ending quote
            }
            else
            { // value is unquoted alphanumerics and underscores, periods, percent signs, and colons
                if (!( isalnum(s[i]) || '_' == s[i] || '.' == s[i] || '%' == s[i] || s[i] == ':'))
                {
                    std::ostringstream o;
                    o << "ParameterValueLookupTable::fromString() invalid input string:" << std::endl << s << std::endl;
                    for (unsigned int j=0; j<i; j++) o << ' ';
                    o << '^' << std::endl << "Unquoted value must be all alphanumerics, underscores \'_\', periods \'.\', percent signs \'%', and colons \':\'." << std::endl;
                    contents.clear();
                    std::cout << o.str();
                    log(m_s.masterlogfile,o.str());
                    return std::make_pair(false,o.str());
                }
                value_start=i;
                value_length=1;
                i++;
                while (i<s.length() && (isalnum(s[i]) || '_' == s[i] || '.' == s[i] || '%' == s[i] || ':' == s[i]))
                {
                    i++;
                    value_length++;
                }
            }

            contents[normalize_identifier(s.substr(identifier_start,identifier_length))]=s.substr(value_start,value_length);
        }
	}

    return std::make_pair(true,std::string(""));
}

std::string ParameterValueLookupTable::toString()
{
	std::ostringstream o;
	bool first=true;
	std::string value;
	bool needs_quoting;
	char quote_char;

	for (auto& pear : contents)
	{
		needs_quoting=false;
		quote_char='\"';
		if (!first) o << ", ";
		first=false;
		o << rehydrate_parameter_name(pear.first) << " = ";
		value=pear.second;
		for (unsigned int i=0; i<value.length(); i++)
		{
			if (!( isalnum(value[i]) || '_' == value[i] || '.' == value[i] || '%' == value[i] || ':' == value[i] ))
			{
				needs_quoting=true;
				if ('\"' == value[i]) quote_char = '\'';
			}
		}
		if (needs_quoting) o << quote_char;
		o << value;
		if (needs_quoting) o << quote_char;
	}

	for (const loop_level& ll : workload_loopy.loop_levels)
	{
	    if (o.str().size() > 0) o << ", ";

	    bool first=true;

	    o << ll.attribute << " = (";

	    for (const auto& v : ll.values)
	    {
	        if (! first) o << ", ";
	        first=false;
	        o << "\"" << v << "\"";
	    }
	    o << ")";
	}

	return o.str();
}

bool ParameterValueLookupTable::contains(std::string key)
{
	auto m = contents.find(normalize_identifier(key));
	if (contents.end()==m) return false;
	return true;
}

std::string ParameterValueLookupTable::retrieve(std::string key)
{
	auto m = contents.find(normalize_identifier(key));
	if (contents.end()==m) return std::string("");
	return m->second;
}

std::pair<bool,string> ParameterValueLookupTable::containsOnlyValidParameterNames(std::string s) // s = listOfValidParameterNames
{
	// Input is list of valid go parameter names.
	// Comma separators are optional.
	// Returns false on malformed input list of valid parameter names, or if "contents" contains any invalid keys.

	unsigned int i=0;

	int identifier_start, identifier_length;
	std::set<std::string> valid_parameter_names;

	std::ostringstream bad_names;

	while (true) {
		while (i<s.length() && (isspace(s[i]) || ',' == s[i]))
		{ // whitespace or commas before identifier
			i++;
		}

		if (i>=s.length()) break;

		if (!isalpha(s[i])) // start of identifier
		{
            std::ostringstream o;
			o << "<Error> Internal programming error - ParameterValueLookupTable::containsOnlyValidParameterNames() invalid list of valid parameter names:" << std::endl << s << std::endl;
			for (unsigned int j=0; j<i; j++) o << ' ';
			o << '^' << std::endl << "Parameter names must start with alphabetic and consist entirely of alphanumerics and underscores." << std::endl;
			std::cout << o.str(); log(m_s.masterlogfile,o.str());
			return std::make_pair(false,o.str());
		}

		identifier_start=i;
		identifier_length=1;
		i++;

		while ( i<s.length() && (isalnum(s[i]) || '_' == s[i]) )
		{ // identifier
			i++;
			identifier_length++;
		}
		valid_parameter_names.insert(normalize_identifier(s.substr(identifier_start,identifier_length)));
	}

    std::ostringstream o;

	for (auto& pear : contents)
	{
		if (valid_parameter_names.end() == valid_parameter_names.find(normalize_identifier(pear.first)))
		{
			if (bad_names.str().size() > 0) bad_names << ", ";
			bad_names << pear.first;
		}
	}

	return std::make_pair(bad_names.str().size() == 0, bad_names.str());
}


std::pair<bool,std::string> ParameterValueLookupTable::contains_these_parameter_names(const std::set<std::string>& set_of_names)
{
    // returns true with a string like "IOPS, blocksize" if any of the set of names is present.

    bool got_one {false};

    std::string return_value {};

	for (auto& pear : contents)
	{
	    std::string normalized = normalize_identifier(pear.first);

		if (set_of_names.end() != set_of_names.find(normalized))
		{
			if (got_one) return_value += ", "s;

			got_one = true;

			return_value += rehydrate_parameter_name(normalized);
		}
	}

    return std::make_pair(got_one, return_value);
}



/*static*/ std::map<std::string,std::string> ParameterValueLookupTable::parameter_name_rehydration_table
{
    {"accumulatortype",         "accumulator_type"},
    {"accuracyplusminus",       "accuracy_plus_minus"},
    {"balancedstepdirectionby", "balanced_step_direction_by"},
    {"ballparkseconds",         "ballpark_seconds"},
    {"checkfailedcomponent",    "check_failed_component"},
    {"cooldownbympbusy",        "cooldown_by_MP_busy"},
    {"cooldownbympbusystaydowntimeseconds",        "cooldown_by_MP_busy_stay_down_time_seconds"},
    {"cooldownbywp",            "cooldown_by_WP"},
    {"dfc",                     "DFC"},
    {"element_metric",          "element_metric"},
    {"endingiops",              "ending_IOPS"},
    {"focusrollup",             "focus_rollup"},
    {"gainstep",                "gain_step"},
    {"highiops",                "high_IOPS"},
    {"hightarget",              "high_target"},
    {"lowiops",                 "low_IOPS"},
    {"lowtarget",               "low_target"},
    {"maxiops",                 "max_IOPS"},
    {"maxmonotone",             "max_monotone"},
    {"maxripple",               "max_ripple"},
    {"maxwpchange",             "max_wp_change"},
    {"maxwp",                   "max_WP"},
    {"measureseconds",          "measure_seconds"},
    {"minwp",                   "min_WP"},
    {"pgpercentbusy",           "PG_percent_busy"},
    {"raidsubsystem",           "RAID_subsystem"},
    {"sequentialfill",          "sequential_fill"},
    {"servicetimeseconds",      "service_time_seconds"},
    {"skipldev",                "skip_LDEV"},
    {"startingiops",            "starting_IOPS"},
    {"subintervalseconds",      "subinterval_seconds"},
    {"subsystembusythreshold",  "subsystem_busy_threshold"},
    {"subsystemelement",        "subsystem_element"},
    {"subsystemwpthreshold",    "subsystem_WP_threshold"},
    {"suppressperf",            "suppress_perf"},
    {"targetvalue",             "target_value"},
    {"timeoutseconds",          "timeout_seconds"},
    {"warmupseconds",           "warmup_seconds"},
    {"maxtags",                 "maxTags"},
    {"iops",                    "IOPS"},
    {"skewweight",              "skew_weight"},
    {"fractionread",            "fractionRead"},
    {"rangestart",              "RangeStart"},
    {"rangeend",                "RangeEnd"},
    {"seqstartpoint",           "SeqStartPoint"},
    {"dedupemethod",            "dedupe_method"},
    {"dedupeunitbytes",         "dedupe_unit_bytes"},
    {"threadsinworkloadname",   "threads_in_workload_name"},
    {"thisthreadinworkload",    "this_thread_in_workload"},
    {"patternseed",             "pattern_seed"},
    {"hotzonesizebytes",        "hot_zone_size_bytes"},
    {"hotzonereadfraction",     "hot_zone_read_fraction"},
    {"hotzonewritefraction",    "hot_zone_write_fraction"},
    {"hotzoneiopsfraction",     "hot_zone_IOPS_fraction"},
    {"totaliops",               "total_IOPS"}
};

std::string ParameterValueLookupTable::rehydrate_parameter_name(const std::string& n)
{
    auto it = parameter_name_rehydration_table.find(n);

    if (it == parameter_name_rehydration_table.end()) return n;

    return it->second;
}
