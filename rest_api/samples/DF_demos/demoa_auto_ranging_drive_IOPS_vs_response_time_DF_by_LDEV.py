import ivyrest

o = ivyrest.IvyObj()

# [outputFolderRoot] "/scripts/ivy/ivyoutput/sample_output";

o.set_output_folder_root(".")
o.set_test_name("demoa_auto_ranging_drive_IOPS_vs_response_time_DF_by_LDEV")

"""
// Demo "a" - auto-ranging drive IOPS vs. response time graph.

// In step 0, we measure max IOPS.
//      -This yields the "high_IOPS" and the "high_target" response time for the PID loop later

// In step 1, we run at 1% of the max IOPS to measure "baseline service time"
//      -This yields the "low_IOPS" and the "high_target" response time.

// Then we run a loop of steps using dynamic feedback control of IOPS to hit a series of
// host workload response time targets expressed as a multiple of the baseline service time.
//
// Most users operate with response time degraded up to at most 4x the baseline service time
// therefore plotting a curve for 1.125x, 1.5x, 1.75x, 2x, 3x, 4x, 5x the baseline service time
// along with the baseline and max IOPS points gives you a graph showing the full range of 
// capabilities of the storage LUN.
// 
// To demonstrate fine-grained pid-loop Dynamic Feedback Control (DFC), we say "[CreateWorkload] LDEV;"
// and then for pid=dfc, we say "focus_rollup=LDEV".  The pid=loop mechanism then operates 
// independently on each LDEV which in this case means each Parity Group.
//    The subsystem is an AMS2100, for which SCSI Inquiry pretty much just gives us LDEV and Port.
//    If we had used a RAID Subsystem, we could have made a rollup by PG, or by MPU, by Pool_ID, etc.


// ==================================================
// startup
"""

o.hosts_luns(hosts = "sun159",
	select = "serial_number : 83011441")
"""
		// [select] filter is applied to all LUNs discovered on the hosts to build
		// "available test LUNs".  Also discovers / connects to command devices, if available.

		// With a command device, for the associated LUNs, RMLIB API LDEV configuration attributes
		// such as "drivetype" are merged into the SCSI Inquiry LUN attributes and
		// become available for [select] filtering and become available to [CreateRollup] on.
"""

o.create_workload (name = "steady",
	select =   "LDEV : 0008",               # Filters "available test LUNs" by any attribute.
	iosequencer = "random_steady",           # Other [iogenerator] types are random_independent and sequential.
	parameters = "fractionRead=100%, blocksize=4KiB, IOPS=max, maxtags=128")

o.create_workload( name ="steady",  
	select =   "LDEV : 0009",   
	iosequencer = "random_steady", 
	parameters = "fractionRead=100%, blocksize=4KiB, IOPS=max, maxtags=128")

o.create_rollup(name="LDEV")

"""
// ==================================================
// In step 0, we run at iops=max to measure high_IOPS and high_target for the PID loop later

// The default [Go!] statement uses subinterval_seconds=5, warmup_seconds=5, measure_seconds=60
"""

o.go(stepname="iops_max")

""" Now we retrieve the "Overall IOPS" value from the step 0 row of the summary.csv file for the all=all rollup. """
 
summary_filename = o.test_folder() + "/all/" + o.test_name() + ".all=all.summary.csv";

high_IOPS   = float(o.csv_cell_value(filename = summary_filename, row = 0, col ="Overall IOPS"))
high_target = float(o.csv_cell_value(filename = summary_filename, row = 0, col = "Overall Average Service Time (ms)")) / 1000

""" divide by 1000 because pid target is in seconds, not ms. """

s = "\n********** step 0 result - high_IOPS = " + str(high_IOPS), " high_target = " + str(high_target) + " seconds **********\n\n";

print (s); #log(masterlogfile(),s);


"""
// ==================================================
// In step 1, we run at 1% of the max IOPS to measure "baseline service time"
"""

o.edit_rollup(name="all=all", parameters = "total_IOPS=" + str(.01 * high_IOPS))

o.create_rollup(name="LDEV")
""" this is to get a response time histogram by LDEV as a 1-1 mapping to parity group in this case.  Later in the next step, we will use this rollup to do fine-grained dynamic feedback control."""

"""  // because we need to see enough anomalously long I/Os for them to average out  """
o.go(stepname="baseline_service_time", measure_seconds=1000,
		 measure="response_time_seconds", accuracy_plus_minus="2.5%", timeout_seconds = 28800)

"""
set_csvfile(summary_filename); 
	// lets you access a csv file like a spreadsheet.
		// access individual cells, or get a row slice or a column slice
		// select column by column number or by column header text
"""

if not o.last_result():
    print("Failed to obtain measurement running at 1% of max IOPS.\n")
    o.exit()

low_IOPS   = float(o.csv_cell_value(filename=summary_filename, row = 1, col = "Overall IOPS"))
low_target = float(o.csv_cell_value(filename=summary_filename, row = 1, col = "Overall Average Service Time (ms)")) / 1000


s = "\n********** step 0 result - low_IOPS = " + str(low_IOPS)  + ", low_target = " + str(low_target) + " seconds **********\n\n";


print (s); #log(masterlogfile(),s);


"""
// ==================================================
// In step 2, we use dynamic feedback control of IOPS on host workload service time to plot the IOPS
// for a series of multiples of the baseline service time to plot the graph.

// To illustrate individual & independent pid control by focus rollup we focus on each PG separately.

// Need to come back and fix for the general case where actually we need to pick the
// max IOPS value from the corresponding rollup instance csv file, not the total IOPS.
"""

for multiplier in [ 1.125, 1.25, 1.5, 1.75, 2, 2.5, 3, 4, 5 ]:
    target = low_target * multiplier

    s= "\n********** target_service_time = " + str(target) + " seconds **********\n\n";
    print (s)

    o.go(stepname="" + str(round(multiplier,3)) + "_x_baseline" + "focus_rollup = LDEV",

		warmup_seconds=600, measure_seconds = 600, timeout_seconds = 14400,
		measure="response_time_seconds", accuracy_plus_minus="2.5%",

		dfc="PID",
		low_IOPS    = str(low_IOPS),
		low_target  = str(low_target),
		high_IOPS   = str(high_IOPS),
		high_target = str(high_target),
		target_value= str(target))

"""
// ==================================================
// In step 3, we do iops=max again to give us a final point for our graph.
"""

o.edit_rollup( name = "all=all", parameters ="IOPS=max")

o.go(stepname="iops_max_reprise", measure_seconds=180)

