#!/bin/bash

csvfile=/home/ivogelesang/ivy/doc/test_csv.csv
binfolder=/home/ivogelesang/ivy/bin

echo "csvfile contents follows:"
echo "------------------------"
cat $csvfile
echo "------------------------"
cmd="$binfolder/csv_lookup_column $csvfile Fife_and_Drum"
echo "$cmd"
$cmd
echo "------------------------"
cmd="$binfolder/csv_lookup_column $csvfile Fore_or_5 Fife_and_Drum"
echo "$cmd"
$cmd
echo "------------------------"
cmd="$binfolder/csv_column_header $csvfile 2"
echo "$cmd"
$cmd
echo "------------------------"
cmd="$binfolder/csv_rows $csvfile"
echo "$cmd"
$cmd
echo "------------------------"
cmd="$binfolder/csv_rows $csvfile"
echo "$cmd"
$cmd
echo "------------------------"
cmd="$binfolder/csv_header_columns $csvfile"
echo "$cmd"
$cmd
echo "------------------------"
cmd="$binfolder/csv_columns_in_row $csvfile 2"
echo "$cmd"
$cmd
echo "------------------------"
cmd="$binfolder/csv_raw_cell_value $csvfile 0 Fife_and_Drum"
echo "$cmd"
$cmd
cmd="$binfolder/csv_raw_cell_value $csvfile 1 2"
echo "$cmd"
$cmd
cmd="$binfolder/csv_raw_cell_value $csvfile 3 1"
echo "$cmd"
$cmd
cmd="$binfolder/csv_raw_cell_value $csvfile 3 2"
echo "$cmd"
$cmd
echo "------------------------"
cmd="$binfolder/csv_cell_value $csvfile 0 Fife_and_Drum"
echo "$cmd"
$cmd
echo "------------------------"
cmd="$binfolder/csv_cell_value $csvfile 1 2"
echo "$cmd"
$cmd
echo "------------------------"
cmd="$binfolder/csv_cell_value $csvfile 3 1"
echo "$cmd"
$cmd
echo "------------------------"
cmd="$binfolder/csv_cell_value $csvfile 3 2"
echo "$cmd"
$cmd
echo "------------------------"
cmd="$binfolder/csv_row $csvfile -1"
echo "$cmd"
$cmd
echo ""
cmd="$binfolder/csv_row $csvfile 0"
echo "$cmd"
$cmd
echo ""
cmd="$binfolder/csv_row $csvfile 2"
echo "$cmd"
$cmd
echo ""
cmd="$binfolder/csv_row $csvfile 3"
echo "$cmd"
$cmd
echo "------------------------"
cmd="$binfolder/csv_column $csvfile 0"
echo "$cmd"
$cmd
echo ""
cmd="$binfolder/csv_column $csvfile 1"
echo "$cmd"
$cmd
echo ""
cmd="$binfolder/csv_column $csvfile 2"
echo "$cmd"
$cmd
echo ""
cmd="$binfolder/csv_column $csvfile 3"
echo "$cmd"
$cmd
echo ""
cmd="$binfolder/csv_column $csvfile 4"
echo "$cmd"
$cmd
echo "------------------------"
cmd="$binfolder/csv_column $csvfile Fife_and_Drum"
echo "$cmd"
$cmd
echo "------------------------"
