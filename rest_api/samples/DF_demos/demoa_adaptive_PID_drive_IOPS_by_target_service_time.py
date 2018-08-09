import ivyrest

z= ivyrest.IvyObj("localhost")

z.set_output_folder_root(".")
z.set_test_name("demoa_adaptive_PID_drive_IOPS_by_target_service_time")

# [outputFolderRoot] "/scripts/ivy/ivyoutput/sample_output";

"""
// Demo "a" - adaptive gain Dynamic Feedback Control (DFC) finding the IOPS for a specified service time.

// The idea is to make the summary csv file ready to make an Excel chart showing IOPS by service time
// where the service time target is a specified multiple of the baseline service time at 1% of max IOPS.

// In step 0, we measure max IOPS with queue depth of 256.

// In step 1, we run at 1% of the max IOPS ("low_IOPS") to measure the PID loop "low_value" parameter.

// In step 2, we run at 95% of the max IOPS ("high_IOPS") to measure the PID loop "high_value" parameter.

// Then we run a loop of steps using Dynamic Feedback Control of IOPS to hit a series of
// host workload response time targets expressed as a multiple of the baseline service time.
//
// Most users operate with response time degraded up to at most 4x the baseline service time
// therefore plotting a curve for 1.0625x, 1.125x, 1.5x, 1.75x, 2x, 3x, 4x, 5x the baseline service time
// along with the baseline and max IOPS points gives you a graph showing the full range of 
// capabilities of the storage LUN.
 
// We will be using two LUNs (LDEVs), each comprising a PG, and we perform dynamic feedback control
// at the granularity of the "LDEV" rollup to illustrate fine-grained DFC.
"""

## ==================================================
## startup

z.hosts_luns(hosts = "sun159",
	select = "serial_number : 83011441")

z.create_workload(name="steady",
	select =   "LDEV : [ 0008, 0009 ]",      
	iosequencer = "random_steady",          
	parameters = "fractionRead=100%, blocksize=4KiB, IOPS=max, maxtags=128")

z.create_rollup(name="LDEV")

## ==================================================
## In step 0, we run at iops=max

z.go(stepname="iops_max", measure="IOPS", accuracy_plus_minus = "2.5%", measure_seconds = 120)

## Now we retrieve the "Overall IOPS" value from the step 0 row of the summary.csv file for the all=all rollup.
 
summary_filename = z.test_folder() + "/all/" + z.test_name() + ".all=all.summary.csv"

max_IOPS = float(z.csv_cell_value(filename=summary_filename, row = 0, col = "Overall IOPS"))

s = "\n********** step 0 result - max_IOPS = " + str(max_IOPS) + " **********\n\n";

print (s); log(masterlogfile(),s);

## ==================================================
## In step 1, we run at 95% of the max IOPS to measure the PID loop "high_value" parameter

#double high_IOPS = 95% * max_IOPS;
high_IOPS = .95 * max_IOPS

z.edit_rollup(name="all=all", parameters = "total_IOPS=" + str(high_IOPS))

z.go(stepname="95% of max IOPS", measure="service_time_seconds", accuracy_plus_minus="2.5%", measure_seconds = 120, timeout_seconds = "30:00")

if not z.last_result():
    print("Failed to obtain measurement running at 95% of max IOPS.\n")
    z.exit()

high_service_time_ms = float(z.csv_cell_value(filename=summary_filename,row=1,col="Overall Average Service Time (ms)"))

"""
	// lets you access a csv file like a spreadsheet.
		// access individual cells, or get a row slice or a column slice
		// select column by column number or by column header text
"""

high_value = high_service_time_ms / 1000.0;

s= "\n********** step 2 result - high service time in ms is " + str(high_service_time_ms) + " at " + str(high_IOPS) + " IOPS **********\n\n";

print (s); log(z.masterlogfile(),s);


## ==================================================
## In step 2, we run at 1% of the max IOPS to measure "baseline service time" or "low_target"

#low_IOPS = 1% * max_IOPS;
low_IOPS = 0.01 * max_IOPS

z.edit_rollup(name = "all=all", parameters =  "total_IOPS=" + str(low_IOPS))

z.go(stepname="1% of max IOPS", measure="service_time_seconds", accuracy_plus_minus="2.5%", measure_seconds = 120, timeout_seconds = "30:00")

result = z.last_result()

if not result:
    print("Failed to obtain measurement running at 1% of max IOPS.\n")
    z.exit()

baseline_service_time_ms = float(z.csv_cell_value(filename=summary_filename,row=2,col="Overall Average Service Time (ms)"))
"""
	// lets you access a csv file like a spreadsheet.
		// access individual cells, or get a row slice or a column slice
		// select column by column number or by column header text
"""

low_value = baseline_service_time_ms / 1000.0

s= "\n********** step 1 result - baseline service time in ms is " + str(baseline_service_time_ms) + " at " + str(low_IOPS) + " IOPS **********\n\n";

print (s); log(z.masterlogfile(),s);


"""
// ==================================================
// Then we use dynamic feedback control of IOPS on host workload service time to plot the IOPS
// for a series of multiples of the baseline service time to plot the graph.

// To illustrate individual & independent pid control by focus rollup we focus on each LDEV (and thus each PG) separately.
"""

for multiplier in [ 1.0625, 1.125, 1.25, 1.5, 1.75, 2, 2.5, 3, 4, 5, 6, 7, 8 ]:
    target_value = multiplier * low_value;

    if target_value < high_value:
        s= "\n********** target service time is " + str(1000.0 * target_value) + " ms **********\n\n";
        print (s); log(z.masterlogfile(),s);

        z.go(stepname="" + str(round(multiplier,4)) + "_x_baseline" 
				+ "focus_rollup = LDEV",
			measure="service_time_seconds",
			accuracy_plus_minus="2.5%", measure_seconds = 120, timeout_seconds = "1:00:00",
			 dfc="PID",
			 max_ripple = "0.5%",
			 target_value = str(target_value),
			 low_IOPS = str(low_IOPS),
			 low_target = str(low_value),
			 high_IOPS = str(high_IOPS),
			 high_target = str(high_value))
    else :
        s= "\n********** target service time is " + str(1000.0 * target_value) + " ms but this is above the upper limit, so not running this iteration. **********\n\n";
        print (s); log(z.masterlogfile(),s);


## ==================================================
## Finally, we repeat the 95% measurement simply so that the summary csv file has a nice graph from 1% of max IOPS, through the intermediate PID target values, to the repeated 95% of max IOPS step
z.edit_rollup(name="all=all", parameters = "total_IOPS=" + str(high_IOPS))

z.go(stepname="95% of max IOPS", measure="service_time_seconds", accuracy_plus_minus="2.5%", measure_seconds = 120, timeout_seconds = "30:00")



#################################################################################################################
#                                                                                                               #
#    What follows is the comment section from the source code for the RollupInstance::perform_PID() method:     #
#                                                                                                               #
#################################################################################################################



"""
    // The next bit is how we run an adaptive algorithm to reduce the "I" gain if we are seeing
    // excessive swings in IOPS, or to increase the gain if we haven't reached the target yet.

    // The mechanism works on the basis of successive "adaptive PID subinterval cycles" where
    // every time we make a gain adjustment (up or down), we start a new subinterval cycle.

    // A "swing" is the cumulative distance that IOPS moves in one or more steps
    // in the same direction up or down ("monotonically") before changing direction
    // at an "inflection point".

    // If we see IOPS swinging up and down repeatedly (at least two up-swings and two down-swings
    // within the current adaptive PID cycle) and the average swing size in each direction
    // both up and down is "excessive ", then we reduce the gain and start a new adaptive PID cycle.

    // "Excessive" means that the average up swing expressed as a fraction of the midpoint of
    // the swing is bigger than "max_ripple" (default 1% at time of writing), and likewise
    // the average down swing is also bigger than max_ripple.

    // Looking for excessive swings in both directions to identify instability is used to reject
    // noise in the measurement value from subinterval to subinterval that might cause small swings
    // in the opposite direction while we are making big movements in one direction because
    // we haven't reached the target value yet and we are steering IOPS up or down.

    // If we see average size of swing over multiple swings in both directions is too high
    // we reduce the "I" gain by "gain_step", e.g. 2.0.

    // The gain reduction multiplicative factor we use is min( gain_step, 1 / gain_step ).
    // In other words, you can either say 4.0 or 0.25, it works exactly the same either way.

    // When for the first time we see these excessive swings both up and down,
    // this is taken as evidence that we have "arrived on station".

        // (At time of writing this, donn't know if this will be sufficient
        //  and maybe we'll need to do something else to know that we have arrived
        //  at the target either by further analysis of the changes in IOPS
        //  or by actually looking at the error signal.)

    // Before we have arrived on station as evidenced by repeated excessive IOPS
    // swings in both directions during at least one adaptive PID cycle,
    // we keep increasing the gain and starting new adaptive PID cycles triggered
    // two ways:

    // 1) Within the same adaptive PID subinterval cycle, if the IOPS is either flat
    //    or moves exclusively in one direction up or down for more than "max_monotone" subintervals,
    //    this triggers a gain increase and the start of a new adaptive PID cycle.

    // 2) Within the same adaptive PID subinterval cycle, if the number of subintervals
    //    where IOPS is adjusted "up" is less than 1/3 of the number of subintervals where an IOPS
    //    change was made, or if the number of "down" steps is less than 1/3 the total steps
    //    over a period of "balanced_step_direction_by" (at time of writing defaulting to 12)
    //    total either flat or moves exclusively in one direction up or down for more
    //    than "max_monotone" subintervals, this triggers a gain increase and
    //    the start of a new adaptive PID cycle.

    //    The reason we have this as well as max_monotone is that there may be enough
    //    noise in the measurement from subinterval to subinterval to cause some
    //    erratic movement of IOPS over and above the drift towards the target as
    //    we are actively steering in one direction most of the time.

    //    So the idea is that if we are on target, we will be making roughly the
    //    same number of "up" individual IOPS adjustments as individual "down" adjustments
    //    regardless of their packaging into "swings" which are the monotonic
    //    subsequences joined by "inflection points" where the IOPS adjustment
    //    changes direction.  Thus if over two thirds of the adjustments are
    //    the same direction we increase the gain.

    // The default value of "balanced_step_direction_by" (12) is longer than
    // the default value of "max_monotone" (5) and this is consistent with it taking
    // longer in the presence of noise to decode the signal.
"""
