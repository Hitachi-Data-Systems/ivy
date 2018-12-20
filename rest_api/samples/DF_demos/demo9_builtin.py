import ivyrest
import re
import subprocess

# first we will run some test sequences to generate some csv files to look at.

hdl = ivyrest.IvyObj()

hdl.set_output_folder_root(".")
hdl.set_test_name("demo9_builtin")

# [OutputFolderRoot] "/scripts/ivy/ivyoutput/sample_output";

hdl.hosts_luns(hosts = "sun159", select = "serial_number :  83011441")

hdl.create_workload(name="steady", 
	select = "",                      # A blank [select] field means "select all", i.e. unfiltered.
	iosequencer = "random_steady",
	parameters = "fractionRead=100%, IOPS=max, blocksize = 4 KiB")

for tags in [ 1, 32 ]:
    hdl.edit_rollup(name="all=all", parameters = "maxTags="+str(tags))

    hdl.go(stepname="tags_" + str(tags), measure_seconds=10)

hdl.create_rollup(name =  "LDEV")

print ("Demonstrate ivy built-in functions\n\n");

print("outputFolderRoot() = \"" + hdl.output_folder_root() + "\"\n - defaults to /scripts/ivy/ivyoutput\n");

print("\ntestName() = \"" + hdl.test_name() + "\"\n - the part of the ivyscript filename before the \".ivyscript\".\n");

print("\ntestFolder() = \"" + hdl.test_folder() + "\"\n - the root folder for this ivy invocation.\n");

print("\nmasterlogfile() = \"" + hdl.masterlogfile() + "\"\n - You can log(masterlogfile(), \"something\\n\");\n");

print("\nstepName()=\"" + hdl.step_name() + "\"\n - from most recent [Go!]\n - Used to name folders, csv files, and label summary lines.\n");

print("\nstepNNNN()=\"" + hdl.step_NNNN() + "\"\n - from most recent [Go!]\n - right(stepNNNN(),4) gives you just the number.\n");

print("\nstepFolder()=\"" + hdl.step_folder() + "\"\n - This is where by-subinterval csv files go, both subsystem & host.\n");

print("\nshow_rollup_structure():\n" + hdl.show_rollup_structure() + "\n(end of show_rollup_structure()\n");

hdl.log(hdl.masterlogfile(),"\n\nHello, whirrled!\n\n");
## this adds a timestamp prefix to the string, and adds a "\n" on the end if it doesn't already have one,
## and writes the line(s) to the specified file.  Continuation lines won't have timestamps.

print( "\nIt is now " + subprocess.check_output("date") + " - note, shell_command() runs as root.\n");

print("\nThe idea is that you might want to grep in testFolder() or in stepFolder for csv files, and then\n"
      + "use the csvfile functions below to access an ivy output csv file as a kind of spreadsheet where\n"
      + "you can access by cells, rows, or columns, and where you can specify the column with either the\n"
      + "column header text or the column number.\n");

print("\nstring substring(string s, int begin_index_from_zero, int number_of_chars);\"");
s = "Johan";
print("\ns=\"" + s + "\", substring(s,1,2)=\""+substring(s,1,2)+"\".\n");

print("\nstring left (\"" + hdl.step_NNNN() + "\",4) = \"" + left (hdl.step_NNNN(),4) + "\"\n");
print("\nstring right(\"" + hdl.step_NNNN() + "\",4) = \"" + right(hdl.step_NNNN(),4) + "\"\n");
print(" - like in BASIC, give you leftmost / rightmost characters.\n");

print("\nint regex_match(std::string s, string regex);\n"
      + "int regex_sub_match_count(string s, string regex);\n"
      + "string regex_sub_match(string s, string regex, int n);\n"
      + " - ivy uses the default C++ std::regex, that may be, ECMAscript, I think ...\n");
      
s = "The quick red fox jumped over the lazy brown dog.";

r = ".*(red).*(fox.*dog).*";

print("\nFor string s = \"" + s + "\", regex r = \"" + r + "\":\n");

if re.match(s,r):
    print ("regex_match(s=\""+s+"\",r=\""+r+"\") - matched.\n");
else:
     print ("regex_match(s=\""+s+"\",r=\""+r+"\") - did not match.\n");
                      
print("\nregex_sub_match_count(s=\""+s+"\",r=\""+r+"\") = " + string(regex_sub_match_count(s,r)) + "\n");

n = 0
while  n < regex_sub_match_count(s,r):
    print("submatch " + string(n) + " is \"" + regex_sub_match(s,r,n) + "\".\n");
    n = n + 1

s1 = "Harry";
s2 = "HARRY";
s3 = "Harry!";

print ("\nstringCaseInsensitiveEquality(string s1 = \""+s1+"\", string s2 = \"" + s2 + "\") = ");
if stringCaseInsensitiveEquality(s1,s2):
    print("true\n")
else:
    print("false\n");

print ("\nstringCaseInsensitiveEquality(string s1 = \""+s1+"\", string s3 = \"" + s3 + "\") = ");
if stringCaseInsensitiveEquality(s1,s3):
    print("true\n")
else:
     print("false\n");

"""
s = "   bork  ";

print("\ntrim(\"" + s + "\") = \"" + trim(s) + "\"   // removes leading / trailing whitespace\n");

print ("\nto_lower(\"Harry\") = \"" + to_lower("Harry") + "\".\n");
print ("to_upper(\"Harry\") = \"" + to_upper("Harry") + "\".\n");

s = "875387847";
print("\nmatches_digits(\"" + s + "\") = ");
if (matches_digits(s)) then print ("true\n"); else print("false\n");

s = "87g5387847";
print("\nmatches_digits(\"" + s + "\") = ");
if (matches_digits(s)) then print ("true\n"); else print("false\n");

s = "1";
print("\nmatches_float_number(\"" + s + "\") = ");
if (matches_float_number(s)) then print ("true\n"); else print("false\n");

s = "1.0";
print("\nmatches_float_number(\"" + s + "\") = ");
if (matches_float_number(s)) then print ("true\n"); else print("false\n");


s = "1.0E-03";
print("\nmatches_float_number(\"" + s + "\") = ");
if (matches_float_number(s)) then print ("true\n"); else print("false\n");

s = "1.0%";
print("\nmatches_float_number(\"" + s + "\") = ");
if (matches_float_number(s)) then print ("true\n"); else print("false\n");


s = "1.0%";
print("\nmatches_float_number_optional_trailing_percent(\"" + s + "\") = ");
if (matches_float_number_optional_trailing_percent(s)) then print ("true\n"); else print("false\n");
print("Some ivy parameters can be set to a number with an optional trailing percent.\n");

s = "bat_92";
print("\nmatches_identifier(\"" + s + "\") = ");
if (matches_identifier(s)) then print ("true\n"); else print("false\n");

s = "192.168.1.1";
print("\nmatches_identifier(\"" + s + "\") = ");
if (matches_identifier(s)) then print ("true\n"); else print("false\n");


s = "bat_92";
print("\nmatches_IPv4_dotted_quad(\"" + s + "\") = ");
if (matches_IPv4_dotted_quad(s)) then print ("true\n"); else print("false\n");

s = "192.168.1.1";
print("\nmatches_IPv4_dotted_quad(\"" + s + "\") = ");
if (matches_IPv4_dotted_quad(s)) then print ("true\n"); else print("false\n");


fileappend(masterlogfile(), "\nGoodbye, whirrled.\n\n");
"""

f = hdl.test_folder() + "/all/" + hdl.test_name() + ".all.data_validation.csv";

print("\nset_csvfile(\"" + f + "\");\n" +
"  // Loads csv file into a kind of spreadsheet object, if it's not already loaded into memory.\n" +
"  // You can load multiple csv files and switch back and forth using set_csvfile().\n" +
"  // All subsequent csvfile calls refer to the currently set csvfile.\n\n"                        ); 


print( "csv file has csvfile_rows() = " + str(hdl.csv_rows()) + " rows following the header row.\n" );
print("    (returns -1 if the file cannot be accessed or is empty.)\n");

if hdl.csv_rows() >=0:
    print("header row has " + str(hdl.csv_header_columns()) + " columns is \"" + hdl.csv_row(-1) + "\"\n");

#shell_command("rm -f " + f + ".transpose.csv"); // in case we are running the script multiple times.

print("\nColumn slices for each header column:\n");

n = 0
while n < hdl.csv_header_columns():
    print ("column " + str(n) + " has header text \"" + hdl.csvfile_column_header(n) 
+ "\" and is \"" + hdl.csvfile_column(n) + "\"\n");
	
    ## fileappend(f+".transpose.csv",csvfile_column(n)+"\n");
    ## things sticking out beyond the last header column are dropped in the transpose.
    n = n + 1

print("\n");

n = 0
while n < hdl.csv_rows():
    print ("row " + str(n) + " has " + str(hdl.csv_columns_in_row(n)) + " columns and is \"" + hdl.csv_row(n) + "\"\n");
    n = n + 1

qvf = "Quantity Validation Failure";

print("\nThe \"" + qvf + "\" column is \"" + csvfile_column(qvf) + "\"\n");

print ("\nYou can refer to a column using an int, the column index from zero.\n" +
       "You can refer to a column using a string, the column header text.\n");

print("\nWhen ivy writes csv files, for text strings it makes the value look like formula like =\"string\".\n"
+"This is so that excel will not try to interpret the contents as anything other than a text string.\n"
+"Here, the normal csv_cell_value(row,col) function \"unwraps\" csv column values, to treat ,horse,cow the same as ,\"horse\",=\"cow\".\n"
    + "The raw value is exactly what was between the commas in the csv file.\n");

print("\n\nrow -1 is the header row.  row 0 is the first row after the header row.\n");
print("csvfile_rows() returns -1 if bad or empty, 0 for only a header row, ...\n\n");

row =-1
while row < hdl.csv_rows():
    col = 0
    while col < hdl.csv_columns_in_row(row):
        print( "csvfile_cell_value( row = " + str(row) + ", col = " + string(col) + ") = \""
+ hdl.csv_cell_value(row,col) + "\"\n"	)
        print( "csvfile_raw_cell_value( row = " + str(row) + ", col = " + string(col) + ") = \"" + csvfile_raw_cell_value(row,col) + "\"\n"	)

        col = col + 1

    row = row + 1

"""
print( "csvfile_cell_value( row = " + string(row) + ", column_header = \"" + qvf + "\" ) = \""
		+ csvfile_cell_value(row,qvf) + "\"\n"	);
print( "csvfile_raw_cell_value( row = " + string(row) + ", column_header = \"" + qvf + "\" ) = \""
		+ csvfile_raw_cell_value(row,qvf) + "\"\n"	);

print ("\ndrop_csvfile(\"" + f + "\");\n - If you are done with it and you would like to release the space.\n" );

drop_csvfile(f);

print ("\nstring_with_decimal_places(pi(),3) = " + to_string_with_decimal_places(pi(),3) + "\n\n");

"""
