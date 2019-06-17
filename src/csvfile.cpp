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
#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <sstream>

#include "csvfile.h"
#include "ivyhelpers.h"

extern std::string masterlogfile();

csvfile::csvfile()
{
    //ctor
}

csvfile::~csvfile()
{
    //dtor
}

void csvfile::clear()
{
    raw_values.clear();
    column_by_header_identifier_value.clear();
    is_loaded=false;
    encountered_error=false;
    error_message.clear();
}

std::ostream& operator<<(std::ostream& os, const csvfile& c)
{
    for (const auto& row : c.raw_values)
    {
        bool first_col;

        first_col = true;

        for (const auto& v : row)
        {
            if (!first_col) { os << ','; }

            first_col=false;

            os << v;
        }
        os << std::endl;
    }
   return os;
}


std::string csvfile::details()
{
    if (encountered_error) {return error_message;}
    if (!is_loaded) {return "<Error> csvfile has not been loaded";}

   std::ostringstream os;

   os << "csvfile<" << (((int)raw_values.size())-1) << " rows after header row.>" << std::endl;
   for (int row=-1; row < (((int)raw_values.size())-1); row++)
   {
        os << "<csvfile row " << row << ">";
        const std::vector<std::string>& r = raw_values[row+1];
        os << " " << r.size() << " columns ";
        for (auto& s : r) os << "  \"" << s << "\"";
        os << std::endl;
   }

   for (auto& pear : column_by_header_identifier_value)
   {
        os << "\"" << pear.first << "\" is column " << pear.second << std::endl;
   }

   return os.str();
}

std::pair<bool,std::string> csvfile::load(const std::string& filename)
{
    clear();

    std::istream* p_istream = &std::cin;

    std::ifstream input_stream;

    if (!filename.empty())
    {
        struct stat struct_stat;
        if(stat(filename.c_str(),&struct_stat))
        {
            std::ostringstream o;
            o << "<Error> csvfile::load() - csv filename \"" + filename + "\" does not exist." << std::endl;
            encountered_error = true;
            error_message = o.str();
            return std::make_pair(false,o.str());
        }

        if(!S_ISREG(struct_stat.st_mode))
        {
            std::ostringstream o;
            o << "<Error> csvfile::load() - csv filename \"" + filename + "\" is not a regular file." << std::endl;
            encountered_error = true;
            error_message = o.str();
            return std::make_pair(false,o.str());
        }

        input_stream.open(filename);

        if (!input_stream.is_open())
        {
            std::ostringstream o;
            o << "<Error> csvfile::load() - csv filename \"" + filename + "\" failed to open for input." << std::endl;
            encountered_error = true;
            error_message = o.str();
            return std::make_pair(false,o.str());
        }
        p_istream = &input_stream;
    }

    std::string csvline;
    std::string token;

    while (std::getline(*p_istream,csvline))
    {
        raw_values.push_back(std::vector<std::string>());

        int columns = countCSVlineUnquotedCommas(csvline)+1;

        for ( int i = 0; i < columns; i++ )
        {
            token = retrieveRawCSVcolumn(csvline, i);
            raw_values.back().push_back(token);
        }
    }

    // p_istream->close();

    if (raw_values.size() > 0) for ( auto& s: raw_values[0] )
    {
        std::string key = column_header_to_identifier(s);
        int col = column_by_header_identifier_value.size();
        column_by_header_identifier_value[key] = col;
    }

    // baseline_service_time_secondsstd::cout << "After csvfile::load: " << std::endl << *this << std::endl;
    is_loaded = true;
    return std::make_pair(true,"");
}

int csvfile::lookup_column(const std::string& header_name)
{
    if ((!is_loaded) || encountered_error) return -2;

    auto it = column_by_header_identifier_value.find(column_header_to_identifier(header_name));

    if (it == column_by_header_identifier_value.end()) return -1;

    return it->second;
}

int csvfile::rows()
{
    if ((!is_loaded) || encountered_error) return -2;

    // not including header row.
    // returns -1 if file loaded didn't exist or was empty.

    return ((int) raw_values.size()) - 1;
}

int csvfile::header_columns()
{
    if ((!is_loaded) || encountered_error) return -2;

    // returns -1 if file loaded didn't exist or was empty.

    if ( raw_values.size() == 0 ) return -1;

    return raw_values[0].size();
}

int csvfile::columns_in_row(int row)
{
    if ((!is_loaded) || encountered_error) return -2;
    // row number -1 means header row

    // returns -1 for an invalid row

    if (row > ((int)(raw_values.size()-2)) ) return -1;

    if (row < -1) return -1;

    return raw_values[row+1].size();
}

std::string csvfile::raw_cell_value(int row, int col )
{
    if (encountered_error) {return error_message;}
    if (!is_loaded) {return "<Error> csvfile has not been loaded";}

    // returns the exact characters found between commas in the source file

    // returns the empty string on invalid row or column

    // first row is number -1 (header)

    if (row < -1 || row > ((int)(raw_values.size()-2)) ) return "";

    const std::vector<std::string>& row_vector = raw_values[row+1];

    if ( col < 0 || col > ((int)(row_vector.size()-1)) ) return "";

    return row_vector[col];
}

std::string csvfile::raw_cell_value(int row, const std::string& column_header )
{
    if (encountered_error) {return error_message;}
    if (!is_loaded) {return "<Error> csvfile has not been loaded";}

    return raw_cell_value(row,lookup_column(column_header));
}

std::string csvfile::column_header(int col)
{
    if (encountered_error) {return error_message;}
    if (!is_loaded) {return "<Error> csvfile has not been loaded";}

    return raw_cell_value(-1,col);
}

std::string csvfile::cell_value(int row, int col)
{
    if (encountered_error) {return error_message;}
    if (!is_loaded) {return "<Error> csvfile has not been loaded";}

    // This "unwraps" the raw cell value.

    // First if it starts with =" and ends with ", we just clip those off and return the rest.

    // Otherwise, an unwrapped CSV column value first has leading/trailing whitespace removed and then if what remains is a single quoted string,
    // the quotes are removed and any internal "escaped" quotes have their escaping backslashes removed, i.e. \" -> ".

    return UnwrapCSVcolumn(raw_cell_value(row,col));
}

std::string csvfile::cell_value(int row, const std::string& column_header)
{
    if (encountered_error) {return error_message;}
    if (!is_loaded) {return "<Error> csvfile has not been loaded";}

    return UnwrapCSVcolumn(raw_cell_value(row,lookup_column(column_header)));
}


std::string csvfile::row(int r)
{
    if (encountered_error) {return error_message;}
    if (!is_loaded) {return "<Error> csvfile has not been loaded";}

    // e.g. true,56.2,="bork"

    // -1 gives you the header row

    if (r < -1 || r > ((int)(raw_values.size() - 2)) ) return "";

    std::ostringstream o;

    for (unsigned int i = 0; i < raw_values[r+1].size(); i++)
    {
        if (i>0) o << ',';
        o << raw_values[r+1][i];
    }

    return o.str();
}

std::string csvfile::column(int col)
{
    if (encountered_error) {return error_message;}
    if (!is_loaded) {return "<Error> csvfile has not been loaded";}

    // This returns a column slice, e.g. "IOPS,56,67,88"
    std::ostringstream o;

    for (unsigned int row = 0; row < raw_values.size(); row++)
    {
        if ( row > 0 ) o << ',';
        o << raw_cell_value(row-1,col);
    }
    return o.str();
}

std::string csvfile::column( const std::string& column_header )
{
    if (encountered_error) {return error_message;}
    if (!is_loaded) {return "<Error> csvfile has not been loaded";}

      int col = lookup_column(column_header);

      return column(col);
}


void csvfile::remove_empty_columns()
{
    if ( raw_values.size() <= 1 ) return;  // no data rows

    unsigned int columns = raw_values[0].size(); // i.e. the number of header columns.
        // Thus we only delete empty columns that have a header column.

    if (0 == columns) return; // no header columns

    for (unsigned int col = columns - 1; col >= 0; col-- )
    // we do this in descending order so that when we erase columns,
    // earlier as-yet-unprocessed columns don't get renumbered.
    {
        bool leeg;

        leeg = true;
//*debug*/        {
//*debug*/            const std::string& header = raw_values[0][col];
//*debug*/            if (stringCaseInsensitiveEquality(header,std::string("Parity Group"))) continue;
//*debug*/            if (stringCaseInsensitiveEquality(header,std::string("Parity_Group"))) continue;
//*debug*/            if (stringCaseInsensitiveEquality(header,std::string("PG"))) continue;
//*debug*/        }

        for (unsigned int row = 1; row < (raw_values.size()); row++)
        {

            if (col >= raw_values[row].size())
            {
                continue;
            }

            if (!(raw_values[row][col].empty()))
            {
                leeg = false;
                break;
            }
        }

        if (leeg)
        {
            for (auto& csv_row : raw_values)
            {
                if (csv_row.size() <= col) continue;

                csv_row.erase(csv_row.begin()+col);
            }
        }
        if (col == 0) break;
    }
}































