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

#include <unistd.h>

#include "ivytypes.h"
#include "ivyhelpers.h"
#include "discover_luns.h"

// This was very early ivy code from before C++11 and before std::regex had been fixed.

// So the parsers are hand-written old fashioned 1970s style.


discovered_LUNs::discovered_LUNs() {
	//showluns_output="";
}

std::string get_ivydriver_path_part()
{
#define MAX_FULLY_QUALIFIED_PATHNAME 511
    const size_t pathname_max_length_with_null = MAX_FULLY_QUALIFIED_PATHNAME + 1;
    char pathname_char[pathname_max_length_with_null];

    // Read the symbolic link '/proc/self/exe'.
    const char *proc_self_exe = "/proc/self/exe";
    const int readlink_rc = int(readlink(proc_self_exe, pathname_char, MAX_FULLY_QUALIFIED_PATHNAME));

    std::string fully_qualified {};

    if (readlink_rc <= 0) { return ""; }

    fully_qualified = pathname_char;

    std::string path_part_regex_string { R"ivy((.*/)(ivydriver[^/]*))ivy" };
    std::regex path_part_regex( path_part_regex_string );

    std::smatch entire_match;
    std::ssub_match path_part;

    if (std::regex_match(fully_qualified, entire_match, path_part_regex))
    {
        path_part = entire_match[1];
        return path_part.str();
    }

    return "";
}


void discovered_LUNs::discover() {
	showluns_output=GetStdoutFromCommand(get_ivydriver_path_part() + SHOWLUNS_CMD);
}

discovered_LUNs::discovered_LUNs(std::string showluns_output) {
	this->showluns_output=showluns_output;
}

void discovered_LUNs::add_in(discovered_LUNs& dL) {

	if (isEmpty()) {
		showluns_output=dL.get_showluns_output();
		return;
	} else {
		if (dL.isEmpty())
			return;
	}

	std::string s;

	if (!dL.get_header_line(s)) return;
        // sorry older programming style, this sets s and returns false if there wasn't a first line in dL.
        // This is just to advance the cursor in dL past the header line.

	std::ostringstream o;

	while (dL.get_next_line(s))
		o << std::endl << s;

	showluns_output += o.str();
}

bool discovered_LUNs::get_csv_line(int row, std::string& line, std::string logfilename) {
//{
//ostringstream o;
//o << "discovered_LUNs::get_csv_line(int row=" << row << ",,)" << std::endl;
//fileappend(logfilename,o.str());
//}
	// row and column start with 0

	if (row<0) {
		std::ostringstream o;
		o << "discovered_LUNs::get_csv_line(" << row << ") - row may not be negative. Row starts from zero." << std::endl; line="";
		fileappend(logfilename,o.str());
		return false;
	}

	int bytes_remaining=showluns_output.length();
	int cursor=0;
	int cur_row=0;
	int line_start=0;
	int line_len=0;
//{
//ostringstream o;
//o << "before stepping over previous rows, cur_row=" << cur_row << ", cursor=" << cursor << ", bytes_remaining=" << bytes_remaining << ", row=" << row << std::endl;
//fileappend(logfilename,o.str());
//}

	while ((cur_row < row) && (bytes_remaining>0)) {
		// step over preceding lines
		while ((showluns_output[cursor]!='\r') && (showluns_output[cursor]!='\n') && (bytes_remaining>0)) {
//{
//ostringstream o;
//o << "   about to step over \'" << showluns_output[cursor] << "\' & before incrementing cursor and decrementing bytes_remaining, cur_row=" << cur_row << ", cursor=" << cursor << ", bytes_remaining=" << bytes_remaining << ", row=" << row << std::endl;
//fileappend(logfilename,o.str());
//}

			cursor++; bytes_remaining--;
		}
		if ((bytes_remaining>1) && (showluns_output[cursor]=='\r') && (showluns_output[cursor+1]=='\n')) {cursor++;bytes_remaining--;} // supposed to handle Windows \r\n case.
		if (bytes_remaining<=0) return false;
		cur_row++; cursor++; bytes_remaining--;
//fileappend(logfilename,std::string("get_csv_line() - skipped over a row.\n"));
	}
//{
//ostringstream o;
//o << "after stepping over previous rows, cur_row=" << cur_row << ", cursor=" << cursor << ", bytes_remaining=" << bytes_remaining << ", row=" << row << std::endl;
//fileappend(logfilename,o.str());
//}
	if (bytes_remaining==0 && cur_row<row) {
		line="";
//fileappend(logfilename,std::string("get_csv_line() - we don't have that many rows.\n"));
		return false;
	}
//fileappend(logfilename,std::string("get_csv_line() - do have that many rows.\n"));

	if (bytes_remaining==0 && cur_row==row) {line=""; return true;}
	line_start=cursor;
	while ((showluns_output[cursor]!='\r') && (showluns_output[cursor]!='\n') && (bytes_remaining>0)) {
		cursor++; bytes_remaining--; line_len++;
	}
	if (line_len==0) {line=""; return true;}
	line=showluns_output.substr(line_start,line_len);
	return true;
}

bool discovered_LUNs::get_csv_field(int row, int col, std::string& field,std::string logfilename) {

	// row and column start with 0

	if (row<0 || col<0)
	{
		std::ostringstream o;
		o << "discovered_LUNs::get_csv_field(" << row << ',' << col << ") - row & col may not be negative. They start from zero." << std::endl; field="";
		fileappend (logfilename,o.str());
		return false;
	}
	std::string csvline;
	if (!get_csv_line(row,csvline,logfilename)) {field=""; return false;}

	int bytes_remaining=csvline.length();
	int cursor=0;
	int cur_col=0;
	int start_posn=0;
	int field_len=0;

	while (cur_col< col && bytes_remaining>0) {
		while(csvline[cursor]!=',' && bytes_remaining>0) {cursor++;bytes_remaining--;}
		if (bytes_remaining==0) {field=""; return false;}
		cur_col++; cursor++; bytes_remaining--;
	}
	if (bytes_remaining==0) {field=""; return true;}
	start_posn=cursor;
	while(csvline[cursor]!=',' && bytes_remaining>0) {cursor++;bytes_remaining--;field_len++;}
	if (field_len==0) {field=""; return true;}
	field=csvline.substr(start_posn,field_len);
	return true;
}

int discovered_LUNs::get_column_by_header(std::string header, std::string logfilename) {
	if (header.length()==0) return -1;
	int col=0;
	std::string field;
	while (get_csv_field(0,col,field,logfilename) && field!=header) {
		col++;
	}
	if (field==header) return col;
	return -1;
}

bool discovered_LUNs::get_attribute_value_by_header(std::string header, int lun_index, std::string& attribute_value, std::string logfilename) {
//	{
//		std::ostringstream o;
//		o <<"discovered_LUNs::get_attribute_value_by_header(header=\"" << header << "\", int lun_index=" << lun_index << "." << std::endl;
//		fileappend(logfilename,o.str());
//	}
	if (lun_index<0) {
		std::ostringstream o;
		o <<"discovered_LUNs::get_attribute_value_by_header(header=\"" << header << "\", int lun_index=" << lun_index << ",) - lun_index may not be negative." << std::endl;
		fileappend(logfilename,o.str());
		return false;
	}
	int col = get_column_by_header(header,logfilename);
	if (col==-1) {
		std::ostringstream o;
		o<<"discovered_LUNs::get_attribute_value_by_header(header=\"" << header << "\", int lun_index=" << lun_index << ",) - column header string not found" << std::endl;
		fileappend(logfilename,o.str());
		return false;
	}
	return get_csv_field(lun_index+1,col,attribute_value,logfilename);
}

bool discovered_LUNs::get_header_line(std::string& header_line) {
	int cursor=0;
	int bytes_remaining=showluns_output.length();
	int line_length=0;
	if (bytes_remaining==0) {
		header_line="";
		return false;
	}
	while (bytes_remaining>0 && showluns_output[cursor]!='\r' && showluns_output[cursor]!='\n') {
		cursor++; bytes_remaining--; line_length++;
	}

	if (line_length==0) header_line="";
	else header_line=showluns_output.substr(0,line_length);

	if (bytes_remaining>1 && showluns_output[cursor]=='\r' && showluns_output[cursor+1]=='\n') {
		if (bytes_remaining==2) next_line_starting_cursor=-1;
		else next_line_starting_cursor=cursor+2;
	} else if (bytes_remaining>1) {
		next_line_starting_cursor=cursor+1;
	} else {
		next_line_starting_cursor=-1;
	}
	return true;
}


bool discovered_LUNs::get_next_line(std::string& next_line) {
	int cursor=next_line_starting_cursor;
	if (-1==cursor) {next_line=""; return false;}
        int bytes_remaining=showluns_output.length()-cursor;
        int line_length=0;
        if (bytes_remaining==0) {  // this is pure paranoia as this is not supposed to be able to happen
                next_line="";
                return false;
        }
        while (bytes_remaining>0 && showluns_output[cursor]!='\r' && showluns_output[cursor]!='\n') {
                cursor++; bytes_remaining--; line_length++;
        }

        if (line_length==0) next_line="";
        else next_line=showluns_output.substr(next_line_starting_cursor,line_length);

        if (bytes_remaining>1 && showluns_output[cursor]=='\r' && showluns_output[cursor+1]=='\n') {
                if (bytes_remaining==2) next_line_starting_cursor=-1;
                else next_line_starting_cursor=cursor+2;
        } else if (bytes_remaining>1) {
                next_line_starting_cursor=cursor+1;
        } else {
                next_line_starting_cursor=-1;
        }
        return true;
}


