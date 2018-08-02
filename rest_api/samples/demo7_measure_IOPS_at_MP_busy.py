import ivyrest

ivy = ivyrest.IvyRestClient("localhost")

ivy.set_output_folder_root(".")
ivy.set_test_name("demo7_measure_IOPS_at_MP_busy")

ivy.hosts_luns(Hosts = "sun159", Select = "serial_number : 83011441")

ivy.create_workload(workload = "steady", select = [{'LDEV' : '00:10'}], iosequencer = "random_steady", parameters = "fraction_read=100%, blocksize = 1 KiB, maxTags=128")

ivy.create_rollup(name = "MPU")

################################################################
# In step 0, we measure IOPS with IOPS=max

ivy.edit_rollup(name = "all=all", parameters = "IOPS=max")

ivy.go(stepname = "IOPS = max", measure="IOPS", accuracy_plus_minus = "2%", measure_seconds = 120)

# Now we retrieve the "Overall IOPS" value from the step 0 row of the summary.csv file for the all=all rollup.
 
summary_filename = ivy.test_folder() + "/all/" + ivy.test_name() + ".all=all.summary.csv"

max_IOPS = ivy.csv_cell_value(filename = summary_filename, row = 0, col = "Overall IOPS")

s = "\n********** step 0 result - max_IOPS = " + str(max_IOPS) + " **********\n\n";

print (s); # log(masterlogfile(),s);

################################################################
# In step 1, we run at high_IOPS = 98% of max IOPS measuring "high_target", the upper bound PID loop target metric

#double high_IOPS = 98% * max_IOPS;
high_IOPS = 0.98 * max_IOPS

ivy.edit_rollup(name = "all=all", parameters = "total_IOPS=" + str(high_IOPS))

ivy.go(stepname="high_target", measure="MP_core_busy_percent", accuracy_plus_minus="5%", measure_seconds = 120, timeout_seconds = "1:00:00")

if not ivy.last_result():
    print("Failed to obtain measurement running at 98% of max IOPS.\n")
    ivy.exit()

high_target = ivy.csv_cell_value(filename = summary_filename, row = 1, col = "subsystem avg MP_core busy_percent")
	## lets you access a csv file like a spreadsheet.
		## access individual cells, or get a row slice or a column slice
		## select column by column number or by column header text

s= "\n********** step 1 result - high_target MP_core % busy is " + str(round(100.0 * high_target, 2)) + "% at " + str(round(high_IOPS,2)) + " IOPS **********\n\n";

print (s); #log(masterlogfile(),s);


################################################################
# In step 2, we run at low_IOPS = 1% of the max IOPS to measure "low_target", the lower bound PID loop target metric

## We measure at low_IOPS second, after measuring at the high IOPS point so that this step 2 at 1% of max could serve as the first line in a chart at increasing measurement values.
## Then after doing the PID loop steps, we can repeat the 95% of max IOPS step to put the last line in the graph in place.

#double low_IOPS = 1% * max_IOPS;
low_IOPS = .01 * max_IOPS

ivy.edit_rollup(name = "all=all", parameters = "total_IOPS=" + str(low_IOPS))

ivy.go(stepname="low_target", measure="MP_core_busy_percent", accuracy_plus_minus="7.5%", measure_seconds = 120, timeout_seconds = "1:00:00")

if not ivy.last_result():
    print("Failed to obtain measurement running at 1% of max IOPS.\n")
    ivy.exit()

low_target = ivy.csv_cell_value(filename = summary_filename, row = 2, col = "subsystem avg MP_core busy_percent")
	## lets you access a csv file like a spreadsheet.
		## access individual cells, or get a row slice or a column slice
		## select column by column number or by column header text

s= "\n********** step 2 result - MP_core " + str(round(100* low_target, 2)) + "% busy at 1% of max IOPs = " + str(round(low_IOPS,2)) + " IOPS **********\n\n";

print (s); #log(masterlogfile(),s);

################################################################
# next we loop through the target values

for target_value in [ .1, .2, .3, .4, .5, .6 ]:
    ivy.go(stepname = "IOPS at " + str(round(100.0 * target_value, 1)) + "% MP_core busy",
                measure = MP_core_busy_percent, warmup_seconds = 30, measure_seconds=300,
                focus_rollup = "MPU",
                accuracy_plus_minus="5%", timeout_seconds = "1:00:00",
                dfc = pid,
                target_value = str(target_value),
                low_target   = str(low_target),
                high_target  = str(high_target),
                low_IOPS     = str(low_IOPS),
                high_IOPS    = str(high_IOPS))

################################################################
# Now repeat the high_IOPS step to give a line you can optionally add to the end of graphs by PG% 

ivy.edit_rollup(name = "all=all", parameters = "total_IOPS=" + str(high_IOPS))

ivy.go(stepname="high_target", measure="MP_core_busy_percent", accuracy_plus_minus="10%", measure_seconds = 300, timeout_seconds = "1:00:00")

################################################################
# It's very interesting to see a huge jump from "high IOPS", to hitting the wall at IOPS=max
# these last two lines may be useful in an Excel chart.

ivy.edit_rollup(name = "all=all", parameters = "IOPS=max")

ivy.go(stepname="IOPS = max", measure="MP_core_busy_percent", accuracy_plus_minus="10%", measure_seconds = 300, timeout_seconds = "1:00:00")
