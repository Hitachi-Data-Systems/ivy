//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//

#include "Matcher.h"

//#define IVY_MATCHER_TRACE_ON
//#define IVY_MATCHER_COMPILE_TRACE_ON

void MatcherColumnHeader::compile()
{
	// The compile is successful if the attribute name provided by the user was found
	// as an attribute for at least one discovered LUN.  In other words, this is done
	// so that the user can easily tell the difference between "no such attribute"
	// and "No LUNs with matching attribute values".

	// The way this works is that the LUN class has a copyOntoMe(LUN* other) method.
	// Every time we create a LUN using a header line and a data line, we copy that
	// newly created on on top of "the sample LUN" in that host controller thread.

	// So if you have several vendors that provide separate proprietary tools to decode
	// proprietary information about that vendor's SCSI Inquiry data, every time
	// you create a new LUN object from a LUN-lister csv data line, you copy
	// the newly created LUN on top of the sample LUN.  Then once all host
	// controller threads have finished parsing all their LUN-lister records,
	// we copy the HostSampleLUN from each host on TheSampleLUN.

	// And that's what we use to determine if a select statement compile is
	// successful.  The compile fails only in the case where the attribute
	// name provided by the user was not observed on any LUN Lister header line.

	if (p_SelectClause->p_sample_LUN->contains_attribute_name(p_SelectClause->attribute_name_token()))
	{
		compileSuccessful = true;
	}
	else
	{
		std::ostringstream o;
		o << "MatcherColumnHeader::compile() - No LUN column header was found that corresponds to the specified attribute name =\""
		  << p_SelectClause->attribute_name_token() << "\"." << std::endl;
		fileappend(logfilename, o.str());
	}
}

bool MatcherColumnHeader::matches(LUN* pLUN)
{

#ifdef IVY_MATCHER_TRACE_ON
	{
		std::ostringstream o;
		o << "MatcherColumnHeader::matches() entered for LUN \"" << pLUN->toString() << "\"" << std::endl
			<< "and SelectClause \"" << p_SelectClause->toString() << "\"." << std::endl;
		std::cout << o.str();
	}
#endif//IVY_MATCHER_TRACE_ON

	if (!compileSuccessful) return false;
	for (auto& s : p_SelectClause->attribute_value_tokens())
	{
		if (pLUN->attribute_value_matches(p_SelectClause->attribute_name_token(), s))
		{
#ifdef IVY_MATCHER_TRACE_ON
			std::ostringstream o;
			o << "MatcherColumnHeader::matches() - matched token value \"" << s << "\"." << std::endl;
			std::cout << o.str();
#endif//IVY_MATCHER_TRACE_ON

			return true;
		}
#ifdef IVY_MATCHER_TRACE_ON
		else
		{
			std::ostringstream o;
			o << "MatcherColumnHeader::matches() - did not match token value \"" << s << "\"." << std::endl;
			std::cout << o.str();
		}
#endif//IVY_MATCHER_TRACE_ON
	}
	return false;
}

void Matcher_ivyscript_hostname::compile()
{
	std::string x;
	std::list<std::string> thisTokenHostnames;

	for (auto& s : p_SelectClause->attribute_value_tokens())
	{
		thisTokenHostnames.clear();
		x = load_hosts(s, thisTokenHostnames);
		if (0!=x.length())
		{
			std::ostringstream o;
			o << "Matcher_ivyscript_hostname::compile() - load_hosts(\"" << s << "\") returned  \"" << x << "\"." << std::endl;
			std::cout << o.str();
			compileSuccessful=false;
			return;
		}
		for (auto& s : thisTokenHostnames)
		{
			hostname_set.insert(s);
		}
	}
	compileSuccessful = true;
}

bool Matcher_ivyscript_hostname::matches(LUN* pLUN)
{
	if (NULL == pLUN)
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "Matcher_ivyscript_hostname::matches() called with NULL pointer." << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return false;
	}
	std::string LUN_ivyscript_hostname = pLUN->attribute_value(std::string("ivyscript_hostname"));
	trim(LUN_ivyscript_hostname);  // paranoia

	if (0 == LUN_ivyscript_hostname.length())
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "Matcher_ivyscript_hostname::matches() - doesn't match because this LUN doesn\'t have an ivyscript_hostname attribute." << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return false;
	}

	auto h = hostname_set.find(LUN_ivyscript_hostname);

	if (h != hostname_set.end())
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "Matcher_ivyscript_hostname::matches() - specified ivyscript_hostname matches. \"" << LUN_ivyscript_hostname << "\"" << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return true;
	}
	else
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "Matcher_ivyscript_hostname::matches() - specified ivyscript_hostname does not match. \"" << LUN_ivyscript_hostname << "\"" << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return false;
	}
}

void MatcherLDEV::compile()
{
	for (auto& s : p_SelectClause->attribute_value_tokens())
	{
		thisTokenLDEVset.clear();
		if (!thisTokenLDEVset.add(s, logfilename))
		{
			compileSuccessful = false;
			return;
		}
		overallLDEVset.merge(thisTokenLDEVset);
	}
	compileSuccessful = true;
}

bool MatcherLDEV::matches(LUN* pLUN)
{
	if (NULL == pLUN)
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherLDEV::matches() called with NULL pointer." << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return false;
	}
	std::string LDEV_ID = pLUN->attribute_value(std::string("LDEV"));
	trim(LDEV_ID);  // paranoia

	if (0 == LDEV_ID.length())
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherLDEV::matches() - doesn't match because this LUN doesn\'t have an LDEV attribute." << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return false;
	}

	int LDEVasInt = LDEVset::LDEVtoInt(LDEV_ID);
	if (-1 == LDEVasInt)
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherLDEV::matches() - LDEV ID did not parse as an LDEVset. \"" << LDEV_ID << "\"" << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return false;
	}

	auto seenit = overallLDEVset.seen.find(LDEVasInt);

	if (seenit != overallLDEVset.seen.end())
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherLDEV::matches() - specified LDEV matches. \"" << LDEV_ID << "\"" << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return true;
	}
	else
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherLDEV::matches() - specified LDEV does not match. \"" << LDEV_ID << "\"" << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return false;
	}
}

Matcher_PG_compiled_match_token::Matcher_PG_compiled_match_token(std::string token, std::string logfilename) : text(token)
{
	// OK, it was late at night and I couldn't sleep so I thought I would put in handy shorthand for PG expressions.  Got a bit out of hand.

#ifdef IVY_MATCHER_COMPILE_TRACE_ON
	std::cout << "Matcher_PG_compiled_match_token constructor starting on text \"" << text << "\" and logfilename \"" << logfilename << "\"." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON

	trim(text);  // takes off leading / trailing whitespace

	if (0 == text.length())
	{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
		std::cout << "Matcher_PG_compiled_match_token construction failed as text is empty." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
		fileappend(logfilename,std::string("Matcher_PG_compiled_match_token() constructor called with null string token"));
		compiledWellFormedToken = false;  // To make it explicit.
		return;
	}
	cursor=0;
	next_char=text[0];

	if ('*' == next_char)
	{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
		std::cout << "Consuming \'*\' indicating first number wildcard." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
		// First number is wildcard
		consume();
		firstNumberIsWildcard = true;
	}
	else
	{
		firstNumberIsWildcard = false;

		integer_text.clear();

		if (!isdigit(next_char))
		{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
			std::cout << "Invalid first number did not begin with * or a digit." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
			fileappend(logfilename,std::string("Matcher_PG_compiled_match_token() - constructor must start with * or digit."));
			compiledWellFormedToken = false;  // To make it explicit.
			return;
		}
		while (next_char_is_available() && isdigit(next_char))
		{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
			std::cout << "Consuming numeric digit \'" << next_char << "\'." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
			integer_text.push_back(next_char);
			consume();
		}
		{
			std::istringstream is(integer_text);
			is >> firstNumberRangeStart;
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
			std::cout << "firstNumberRangeStart = " << firstNumberRangeStart << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
		}

		// OK we have the first starting number, do we have a range?

		if (next_char_is_available() && ':' == next_char)
		{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
			std::cout << "Consuming the \':\' indicating a first number range." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
			consume();  // consume the ':'

			integer_text.clear();

			if ( (!next_char_is_available()) || (!isdigit(next_char)) )
			{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
				std::cout << "No digit followed the \':\' in the first number range." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
				fileappend(logfilename,std::string("Matcher_PG_compiled_match_token() - range ending integer value not found after colon."));
				compiledWellFormedToken = false;  // To make it explicit.
				return;
			}
			while (next_char_is_available() && isdigit(next_char))
			{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
				std::cout << "Consuming numeric digit \'" << next_char << "\'." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
				integer_text.push_back(next_char);
				consume();
			}
			{
				std::istringstream is(integer_text);
				is >> firstNumberRangeEnd;
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
				std::cout << "firstNumberRangeEnd = " << firstNumberRangeEnd << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
			}
			if (firstNumberRangeStart>firstNumberRangeEnd)
			{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
				std::cout << "firstNumberRangeStart > firstNumberRangeEnd." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
				fileappend(logfilename,std::string("Matcher_PG_compiled_match_token() - first number range starting number greater than ending value."));
				compiledWellFormedToken = false;  // To make it explicit.
				return;
			}
		}
		else
		{
			// we did not get a range
			firstNumberRangeEnd = firstNumberRangeStart;
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
			std::cout << "Have auto-set range end to be same as range beginning .  firstNumberRangeEnd = " << firstNumberRangeEnd << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
		}
	} // have consumed the first number spec, either wildcard, a single value, or a range of values

	if ( (!next_char_is_available()) || (next_char != '-') )
	{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
		std::cout << "Matcher_PG_compiled_match_token() - did not find hyphen after first number [range]." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
		fileappend(logfilename,std::string("Matcher_PG_compiled_match_token() - did not find hyphen after first number [range]."));
		compiledWellFormedToken = false;  // To make it explicit.
		return;
	}
	consume();  // consume the hyphen
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
		std::cout << "Matcher_PG_compiled_match_token() - consumed hyphen \'-\'." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON

	if ( ! next_char_is_available() )
	{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
		std::cout << "Matcher_PG_compiled_match_token() - did not find second number after hyphen." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
		fileappend(logfilename,std::string("Matcher_PG_compiled_match_token() - did not find second number after hyphen."));
		compiledWellFormedToken = false;  // To make it explicit.
		return;
	}

	if ('*' == next_char)
	{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
		std::cout << "Matcher_PG_compiled_match_token() - consumed asterisk \'*\' indicating second number wildcard." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
		// second number is wildcard
		consume();
		secondNumberIsWildcard = true;
	}
	else
	{
		secondNumberIsWildcard = false;

		integer_text.clear();

		if (!isdigit(next_char))
		{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
			std::cout << "Matcher_PG_compiled_match_token() - second number immediately following the hyphen must start with * or digit." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
			fileappend(logfilename,std::string("Matcher_PG_compiled_match_token() - second number immediately following the hyphen must start with * or digit."));
			compiledWellFormedToken = false;  // To make it explicit.
			return;
		}
		while (next_char_is_available() && isdigit(next_char))
		{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
			std::cout << "Consuming numeric digit \'" << next_char << "\'." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
			integer_text.push_back(next_char);
			consume();
		}
		{
			std::istringstream is(integer_text);
			is >> secondNumberRangeStart;
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
			std::cout << "secondNumberRangeStart = " << secondNumberRangeStart << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
		}

		// OK we have the second starting number, do we have a range?

		if (next_char_is_available() && ':' == next_char)
		{

			consume();  // consume the ':'
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
			std::cout << "Matcher_PG_compiled_match_token() - consumed \':\' indicating second number range." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON

			integer_text.clear();

			if ( (!next_char_is_available()) || (!isdigit(next_char)) )
			{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
				std::cout << "Matcher_PG_compiled_match_token() - range ending integer value not found after colon." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
				fileappend(logfilename,std::string("Matcher_PG_compiled_match_token() - range ending integer value not found after colon."));
				compiledWellFormedToken = false;  // To make it explicit.
				return;
			}
			while (next_char_is_available() && isdigit(next_char))
			{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
				std::cout << "Consuming numeric digit \'" << next_char << "\'." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
				integer_text.push_back(next_char);
				consume();
			}
			{
				std::istringstream is(integer_text);
				is >> secondNumberRangeEnd;
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
				std::cout << "secondNumberRangeEnd = " << secondNumberRangeEnd << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
			}
			if (secondNumberRangeStart>secondNumberRangeEnd)
			{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
				std::cout << "secondNumberRangeStart > secondNumberRangeEnd." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
				fileappend(logfilename,std::string("Matcher_PG_compiled_match_token() - second number range starting value greater than ending value."));
				compiledWellFormedToken = false;  // To make it explicit.
				return;
			}

		}
		else
		{
			// we did not get a range
			secondNumberRangeEnd = secondNumberRangeStart;
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
			std::cout << "Have auto-set range end to be same as range beginning .  secondNumberRangeEnd = " << secondNumberRangeEnd << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
		}
	}

	// OK, we got everything we were looking for, but is there more we didn't eat?
	if (next_char_is_available())
	{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
		std::cout << "Unrecognized / unconsumed characters remaining after what looked like a valid PG match token." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
		fileappend(logfilename,std::string("Matcher_PG_compiled_match_token() - unrecognized / unconsumed characters remaining after what looked like a valid PG match token."));
		compiledWellFormedToken = false;  // To make it explicit.
		return;
	}
	compiledWellFormedToken = true;
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
	std::cout << "We have compiledWellFormedToken." << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
	return;

}

void Matcher_PG_compiled_match_token::consume()
{
	cursor++;
	if (next_char_is_available()) next_char = text[cursor];
}

bool  Matcher_PG_compiled_match_token::matches(int firstNumber, int secondNumber)
{
	if (!firstNumberIsWildcard)
	{
		if ( (firstNumber < firstNumberRangeStart) || (firstNumber > firstNumberRangeEnd) ) return false;
	}

	if (!secondNumberIsWildcard)
	{
		if ( (secondNumber < secondNumberRangeStart) || (secondNumber > secondNumberRangeEnd) ) return false;
	}

	return true;
}

void MatcherPG::compile()
{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
	std::cout << "MatcherPG::compile() entered" << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
	Matcher_PG_compiled_match_token* p_compiledToken;

	for (auto& s : p_SelectClause->attribute_value_tokens())
	{
		p_compiledToken = new Matcher_PG_compiled_match_token(s,logfilename);
		if (!p_compiledToken->compiledOK())
		{
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
			std::cout << "!p_compiledToken->compiledOK()" << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
			delete p_compiledToken;
			compileSuccessful = false;  // making it explicit
			return;
		}
		compiled_token_list.push_back(p_compiledToken);
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
		std::cout << "pushed back p_compiledToken->toString() = \"" << p_compiledToken->toString() << "\"."<< std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
	}
	compileSuccessful=true;
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
	std::cout << "MatcherPG::compile() exit with compiledOK=true" << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON	return;
}

void MatcherPG::consume()
{
	cursor++;

	if (next_char_available())
	{
		next_char = text[cursor];
#ifdef IVY_MATCHER_COMPILE_TRACE_ON
		std::cout << "text = \"" << text << "\"."<< std::endl;
		std::cout << "        ";
		for (int i=0; i < cursor; i++)
		{
			std::cout << ' ';
		}
		std::cout << '|' << std::endl;
#endif//IVY_MATCHER_COMPILE_TRACE_ON
	}
}


bool MatcherPG::matches(LUN* pLUN)
{
	if (NULL == pLUN)
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherPG::matches() called with NULL pointer." << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return false;
	}

	text = pLUN->attribute_value(std::string("Parity Group"));
	trim(text);  // remove leading / trailing whitespace - paranoia

	if (0 == text.length())
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherPG::matches() - doesn't match because this LUN doesn\'t have a \"Parity Group\" attribute." << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return false;
	}
#ifdef IVY_MATCHER_TRACE_ON
	std::cout << "MatcherPG::matches() - parsing LUN'a Parity Group attribute value = \"" << text << "\"" << std::endl;
#endif//IVY_MATCHER_TRACE_ON
	cursor=0;
	next_char = text[0];
	int firstNumber, secondNumber;

	if (!isdigit(next_char))
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherPG::matches() - \"" << text << "\" doesn\'t start with a digit.  Parse failure means match failure." << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return false;  // if the value doesn't look like 1-1 or 01-01, it doesn't match by definition
	}

	integer_text.clear();

	while (next_char_available() && isdigit(next_char))
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherPG::matches() - consuming digit \'" << next_char << "\'." << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		integer_text.push_back(next_char);
		consume();
	}
	{
		std::istringstream is(integer_text);
		is >> firstNumber;
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherPG::matches() - parsed firstNumber = " << firstNumber << std::endl;
#endif//IVY_MATCHER_TRACE_ON
	}


	if ( (!next_char_available()) || ('-' != next_char) )
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherPG::matches() - did not find hyphen \'-\'." << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return false;
	}
	consume();  // eat the hyphen
#ifdef IVY_MATCHER_TRACE_ON
	std::cout << "MatcherPG::matches() - ate the hyphen \'-\'." << std::endl;
#endif//IVY_MATCHER_TRACE_ON

	if ( (!next_char_available()) || (!isdigit(next_char)) )
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherPG::matches() - in \"" << text << "\", there is no digit after the hyphen.  Parse failure means match failure." << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return false;  // if the value doesn't look like 1-1 or 01-01, it doesn't match by definition
	}

	integer_text.clear();

	while (next_char_available() && isdigit(next_char))
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherPG::matches() - consuming digit \'" << next_char << "\'." << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		integer_text.push_back(next_char);
		consume();
	}
	{
		std::istringstream is(integer_text);
		is >> secondNumber;
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherPG::matches() - parsed secondNumber = " << secondNumber << std::endl;
#endif//IVY_MATCHER_TRACE_ON
	}
#ifdef IVY_MATCHER_TRACE_ON
	std::cout << "MatcherPG::matches() has retrieved \"Parity Group\" = " << firstNumber << '-' << secondNumber << " from the LUN." << std::endl;
#endif//IVY_MATCHER_TRACE_ON

	if (next_char_available())
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "MatcherPG::matches() extra uneaten trailing characters after successful parse to that point." << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		return false;  // extra uneaten trailing characters
	}

	for (auto& p_c_Token : compiled_token_list)
	{
#ifdef IVY_MATCHER_TRACE_ON
		std::cout << "... checking against compiled token rehydrated as " << p_c_Token->toString() << std::endl;
#endif//IVY_MATCHER_TRACE_ON
		if (p_c_Token->matches(firstNumber, secondNumber))
		{
#ifdef IVY_MATCHER_TRACE_ON
			std::cout << "... match successful." << std::endl;
#endif//IVY_MATCHER_TRACE_ON
			return true;
		}
#ifdef IVY_MATCHER_TRACE_ON
		else
		{
			std::cout << "... match not successful." << std::endl;
		}
#endif//IVY_MATCHER_TRACE_ON
	}

	return false;
}

std::string Matcher_PG_compiled_match_token::toString()
{
	if (!compiledWellFormedToken)
	{
		return std::string("Matcher_PG_compiled_match_token::toString() - original text token was malformed.");
	}

	std::ostringstream o;

	if (firstNumberIsWildcard)
	{
		o << '*';
	}
	else
	{
		o << firstNumberRangeStart;

		if ( firstNumberRangeStart != firstNumberRangeEnd)
		{
			o << ':' << firstNumberRangeEnd;
		}
	}

	o << '-';

	if (secondNumberIsWildcard)
	{
		o << '*';
	}
	else
	{
		o << secondNumberRangeStart;
		if ( secondNumberRangeStart != secondNumberRangeEnd)
		{
			o << ':' << secondNumberRangeEnd;
		}
	}

	return o.str();
}


