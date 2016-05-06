//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <set>
#include <cctype>
#include <list>
#include <algorithm>

using namespace std;

#include "ivytime.h"
#include "ivyhelpers.h"

#include "LDEVset.h"

std::string LDEVset::toString() const
{
	if (seen.empty()) return "";
	unsigned int firstInSubrange{0},lastInSubrange{0};
	std::ostringstream o;
	bool needs_leading_blank {false};
	unsigned int high,low;
	// The way this works is that we don't output a contiguous subrange like 02:00-02:FF
	// until we know for sure we have seen the end of it, so to speak.
	// The firstInSubrange and lastInSubrange variables are the accumulator of what we've
	// seen so far as a contiguous subrange and that we have not yet printed anything about.
	bool first_ldev = true;
  	for (unsigned int ldev : seen)
  	{
		if (first_ldev)
		{
            first_ldev = false;
			firstInSubrange = lastInSubrange = ldev;
			// on the first time through we just prime the accumulator with one item.
		} else {
			if ((1+lastInSubrange) == ldev)
			{
				lastInSubrange = ldev;
				// what we saw extends the contiguous range seen so far
			} else {
				// output previous subrange first
				if (needs_leading_blank) o << ' ';
				needs_leading_blank=true;
				high = firstInSubrange >> 8;
				low = firstInSubrange & 0xFF;
				o << std::hex << std::setw(2) << std::setfill('0') << high << ':' << std::hex << std::setw(2) << std::setfill('0') << low;
				if (firstInSubrange!=lastInSubrange) {
					high = lastInSubrange >> 8;
					low = lastInSubrange & 0xFF;
					o << '-'<< std::hex << std::setw(2) << std::setfill('0') << high << ':' << std::hex << std::setw(2) << std::setfill('0') << low;
				}
				// then prime the accumulator with the one new item.
				firstInSubrange = lastInSubrange = ldev;
			}
		}
	}
	// output last subrange as yet unprinted
	if (needs_leading_blank) {o << ' ';}
	high = firstInSubrange >> 8;
	low = firstInSubrange & 0xFF;
	o << std::hex << std::setw(2) << std::setfill('0') << high << ':' << std::hex << std::setw(2) << std::setfill('0') << low;
	if (firstInSubrange!=lastInSubrange) {
		high = lastInSubrange >> 8;
		low = lastInSubrange & 0xFF;
		o << '-'<< std::hex << std::setw(2) << std::setfill('0') << high << ':' << std::hex << std::setw(2) << std::setfill('0') << low;
	}

	return toUpper(o.str());
}

bool LDEVset::add(std::string ldev_set, std::string logfilename) {

	trim(ldev_set);  // removes leading/trailing whitespace
	if (stringCaseInsensitiveEquality(std::string("All"),ldev_set)) {
		allLDEVs=true;
		seen.clear();
	}

	// 01:00-01:1F 03:45
	// returns false on malformed input and complains to log file

	// Sadly, "regex" has not yet been implemented with c++std11.

	// We use a hand-built parser.  Good thing I'm an old fart so I can do this in my sleep.
	int cursor=0;
	int bytes_remaining=ldev_set.length();

	unsigned int first,last;
	unsigned int high,low;

	if (ldev_set.length()==0) return false; // OK to add empty string.
	while (bytes_remaining>0) {
		// at start of subexpression

		// step over leading whitespace
		while (bytes_remaining && isspace(ldev_set[cursor])) {
			cursor++; bytes_remaining--;
		}
		if (0==bytes_remaining) return false; // empty string or at end of expression

		// start of subrange - may be a single LDEV 00:00 or may be a contiguous range of LDEVs 00:00-01:ff

		// read in first or only LDEV ID in the form 001a or 00:1A
		if (bytes_remaining<4) {
			ostringstream o;
			o << "LDEVset::add(\"" << ldev_set << "\") - ldev too short" << std::endl;
			fileappend(logfilename,o.str());
			return false;
		} // ldev must be at least 4 hex digits long
		if ((!isxdigit(ldev_set[cursor])) || (!isxdigit(ldev_set[cursor+1]))) {
			ostringstream o; o << "LDEVset::add(\"" << ldev_set << "\") - first 2 chars of ldev not hex digits" << std::endl;
			fileappend(logfilename,o.str());
			return false;
		}
		if (':'==ldev_set[cursor+2]) {
			if ((bytes_remaining<5) || (!isxdigit(ldev_set[cursor+3])) || (!isxdigit(ldev_set[cursor+4]))){
				ostringstream o; o << "LDEVset::add(\"" << ldev_set << "\") - did not find 2 hex digits after \':\'" << std::endl;
				fileappend(logfilename,o.str());
				return false;
			}
        	std::istringstream h(ldev_set.substr(cursor,2));
       	 	std::istringstream l(ldev_set.substr(cursor+3,2));
        	h >> std::hex >> high;
        	l >> std::hex >> low;
			bytes_remaining-=5; cursor+=5;
		} else {
			if ((!isxdigit(ldev_set[cursor+2])) || (!isxdigit(ldev_set[cursor+3]))) {
				ostringstream o; o << "LDEVset::add(\"" << ldev_set << "\") - last 2 chars of ldev not hex digits" << std::endl;
				fileappend(logfilename,o.str());
				return false;
			}
        	std::istringstream h(ldev_set.substr(cursor,2));
       	 	std::istringstream l(ldev_set.substr(cursor+2,2));
        	h >> std::hex >> high;
        	l >> std::hex >> low;
			bytes_remaining-=4; cursor+=4;
		}
        	first = (high << 8) + low;
		if (0==bytes_remaining || ldev_set[cursor]!='-') {
			// singleton LDEV
			seen.insert(first);
		} else {
			// hyphenated range
			cursor++; bytes_remaining--;
			if (0==bytes_remaining) {
				ostringstream o; o << "LDEVset::add(\"" << ldev_set << "\") - missing second LDEV after hyphen (\'-\')." << std::endl;
				fileappend(logfilename,o.str());
				return false;
			}

			// read in ending LDEV ID in the form 001a or 00:1A
			if (bytes_remaining<4) {
				ostringstream o; o << "LDEVset::add(\"" << ldev_set << "\") - 2nd ldev too short" << std::endl;
				fileappend(logfilename,o.str());
				return false;
			} // ldev must be at least 4 hex digits long
			if ((!isxdigit(ldev_set[cursor])) || (!isxdigit(ldev_set[cursor+1]))) {
				ostringstream o; o << "LDEVset::add(\"" << ldev_set << "\") - LDEV must have 4 hex digits not including colon if present." << std::endl;
				fileappend(logfilename,o.str());
				return false;
			}
			if (':'==ldev_set[cursor+2]) {
				if ((bytes_remaining<5) || (!isxdigit(ldev_set[cursor+3])) || (!isxdigit(ldev_set[cursor+4]))){
					ostringstream o; o << "LDEVset::add(\"" << ldev_set << "\") - did not find 2 hex digits after \':\' in 2nd LDEV in range" << std::endl;
					fileappend(logfilename,o.str());
					return false;
				}
	        	std::istringstream h(ldev_set.substr(cursor,2));
	       	 	std::istringstream l(ldev_set.substr(cursor+3,2));
	        	h >> std::hex >> high;
	        	l >> std::hex >> low;
				bytes_remaining-=5; cursor+=5;
			} else {
				if ((!isxdigit(ldev_set[cursor+2])) || (!isxdigit(ldev_set[cursor+3]))) {
					ostringstream o; o << "LDEVset::add(\"" << ldev_set << "\") - last 2 chars of ldev not hex digits in 2nd LDEV in range" << std::endl;
					fileappend(logfilename,o.str());
					return false;
				}
	        	std::istringstream h(ldev_set.substr(cursor,2));
	       	 	std::istringstream l(ldev_set.substr(cursor+2,2));
	        	h >> std::hex >> high;
	        	l >> std::hex >> low;
				bytes_remaining-=4; cursor+=4;
			}
	        last = (high << 8) + low;
			if  (last<first) {
				ostringstream o; o << "LDEVset.add(): ldev range is backwards, lowest LDEV ID must come first." << std::endl;
				fileappend(logfilename,o.str());
				return false;
			}
			// insert range
			for (unsigned int i=first;i<=last;i++) seen.insert(i);
		}
	}
	// now that we have parsed the input string and added the values into the set THEN we test to see if the set is already "all LDEVs"
	if (allLDEVs) seen.clear();
	return true; // no errors encountered
}

std::string LDEVset::toStringWithSemicolonSeparators() {
	std::string s = toString();
	if (s.length()>0) {
		for (unsigned int i=0; i<s.length(); i++) if (s[i]==' ') s[i]=';';
	}
	return s;
}

bool LDEVset::addWithSemicolonSeparators(std::string ldev_set, std::string logfilename) {
	std::string s=ldev_set;
	if (s.length()>0) {
		for (unsigned int i=0; i<s.length(); i++) if (s[i]==';') s[i]=' ';
	}
	return add(s,logfilename);
}

int LDEVset::LDEVtoInt(std::string s) {
	// returns -1 on invalid input
	// valid input is 4 hex digits or 2 hex digits, a ':', and 2 more hex digits
	std::string four;
	if (s.length()==5 && s[2]==':')
		four=s.substr(0,2)+s.substr(3,2);
	else
		four=s;
	if (four.length()!=4) return -1;
	for (int i=0;i<4;i++) if (!isxdigit(four[i])) return -1;
	int high,low;
	std::istringstream h(four.substr(0,2));
	std::istringstream l(four.substr(2,2));
	h >> std::hex >> high;
	l >> std::hex >> low;
	return (high << 8) + low;
}

void LDEVset::merge(const LDEVset& other)
{
	for (auto& LDEV : other.seen)
	{
		seen.insert(LDEV);
	}

	return;
}
