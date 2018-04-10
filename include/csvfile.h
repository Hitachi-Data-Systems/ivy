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
#pragma once

#include <vector>
#include <unordered_map>

// If your csv file doesn't have a header row, just load as normal
// but when you are accessing the data, be aware that your first row is number -1.

// A side effect of this, he said chuckling, is that you can look up data
// in later rows, using the value in the first row as a column title.

// If the same value occurs in more than one column in the first (header) row,
// then the column for that column title value becomes the last column the header value was seen in.

class csvfile
{
//variables
    private:
        std::vector< std::vector<std::string> > raw_values {};
        std::unordered_map<std::string,int> column_by_header_identifier_value {};
            // This map is private because we don't want the user to refer to the map directly.
            // Using the lookup methods provided, we first apply
            //     column_header_to_identifier(UnwrapCSVcolumn(s)))
            // both when we load the map, as well as against
            // the column header value the user gives you.
            // So for lookups " 50% busy", 50__busy, or "50% BUSY" are synonymous.


//methods
    public:
        csvfile();

        csvfile(const std::string& filename) { load(filename); }

        virtual ~csvfile();

        void clear() { raw_values.clear(); column_by_header_identifier_value.clear(); }

        friend std::ostream& operator<<(std::ostream&, const csvfile&);

        std::string details();

        std::pair<bool,std::string> load(const std::string& /*filename*/);

        int rows();
            // returns the number of rows following the header row.
            // returns 0 if there is only a header row.
            // returns -1 if file loaded didn't exist or was empty.

        int header_columns();
            // returns -1 if file loaded didn't exist or was empty.

        int columns_in_row(int row /* from -1 */);
            // row number -1 means header row
            // returns -1 for an invalid row

        std::string column_header(int column /* from 0*/ );

        std::string raw_cell_value(int /* row from -1 */, int                 /* column number from 0 */ );
        std::string raw_cell_value(int /* row from -1 */, const std::string&  /* column header */        );
            // returns the exact characters found between commas in the source file
            // returns the empty string on invalid row or column

        std::string cell_value(int /* row from -1 */, int                /* column number from 0 */ );
        std::string cell_value(int /* row from -1 */, const std::string& /* column header */ );
            // This "unwraps" the raw cell value.

            // First if it starts with =" and ends with ", we just clip those off and return the rest.

            // Otherwise, an unwrapped CSV column value first has leading/trailing whitespace removed and then if what remains is a single quoted string,
            // the quotes are removed and any internal "escaped" quotes have their escaping backslashes removed, i.e. \" -> ".

        std::string row(int); // -1 gives you the header row
            // e.g. nsome,56.2,="bork"

        std::string column(int                /* column number */);
        std::string column(const std::string& /* column header */);
            // This returns a column slice, e.g. "IOPS,56,67,88"

        int lookup_column(const std::string& /* header name */);

        void remove_empty_columns();

};
