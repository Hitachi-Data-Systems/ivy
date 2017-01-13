#include <string>

std::string csv_usage { R"(
These command line utilities expose as standalone executables the ivyscript interpreter builtin csvfile functions.

These functions make it easy to treat a csv file as a kind of spreadsheet where you can
access individual cells, or retrieve entire rows or columns.

These utility functions can be used with any csv file that begins with a column title header row,
but the row and column numbering systems are designed to make accessing ivy output csv files easy.

The column title header row is considered to be row number minus-one (-1).  This means the data rows
are numbered starting from row 0, and thus in the measurement summary csv files that have one
row per step, the step number is also the csv row number.  Similarly, in the per-subinterval csv
files in each step subfolder, the subinterval number is also the csv file row number.

Columns may be specified either as a column number starting from zero (0), or a column may be
specified by the text value of the column header from the header row.
    - e.g. to retrieve the IOPS achieved in step 2, say:  csv_cell_value x.csv 2 "Overall IOPS"

In order to prevent Excel from interpreting a cell according to its value, for example
to prevent Excel from storing the cell value as the date January 1st when you say "1-1",
ivy uses the trick to show a character string as a formula, as in ="1-1".

In these utility functions, when you retrieve using "csv_cell_value", any ivy formula
wrapper is removed, and quoted strings are unpacked, meaning the quotes are removed
and escaped characters are rehydrated.  The "csv_raw_cell_value" executable gives
you the exact original text between the commas in the csv file.

If something goes wrong, each of these executables will say something starting with "<Error>".

csv_lookup_column filename column_header
    e.g.    csv_lookup_column x.csv "Overall IOPS"
    - returns the column number for the specified column title (column header).

csv_column_header filename column_number
    e.g.    csv_column_header x.csv 0
    - prints the column header for the specified column number

csv_rows filename
    e.g.    csv_rows x.csv
    - prints the number of data rows following the header row.
    - prints 0 if there is only a header row.

csv_header_columns filename
    e.g.    csv_header_columns /home/user/Desktop/a.csv"
    - prints the number of columns (number of commas plus one) in the header row

csv_columns_in_row filename row_number
    e.g.    csv_columns_in_row x.csv 3
    - prints the number of columns (number of commas plus one) in the specified row

csv_raw_cell_value filename row_number column_number
csv_raw_cell_value filename row_number column_header
    e.g.    csv_raw_cell_value x.csv 4 5
    e.g.    csv_raw_cell_value x.csv 4 "Overall IOPS"
    - prints the exact characters found between commas in the csv file

csv_cell_value filename row_number column_number
csv_cell_value filename row_number column_header
    e.g.    csv_cell_value x.csv 4 5
    e.g.    csv_cell_value x.csv 4 "Overall IOPS"
    - This "unwraps" the raw cell value.
        - First if it starts with =" and ends with ", we just clip those off and return the rest.
        - Otherwise, an unwrapped CSV column value first has leading/trailing whitespace removed and then if what remains is a single quoted string,
          the quotes are removed and any internal "escaped" quotes have their escaping backslashes removed, i.e. \" -> ".

csv_row filename row_number
    e.g     csv_row x.csv 2
    - prints the entire row

csv_column filename column_number
csv_column filename column_header
    e.g.    csv_column x.csv 4
    e.g.    csv_column x.csv "Overall IOPS"
    - prints column slice, e.g. "Overall IOPS,56,67,88"

)"
};
