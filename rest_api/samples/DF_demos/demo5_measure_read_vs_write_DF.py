import ivyrest

z = ivyrest.IvyObj()
# [outputfolderroot] "/scripts/ivy/ivyoutput/sample_output";

z.set_output_folder_root(".")
z.set_test_name("demo5_measure_read_vs_write_DF")

z.hosts_luns(hosts = "sun159", select = "serial_number : 83011441, ldev : 000A")

z.create_workload( name =  "r_steady", iosequencer = "random_steady", parameters = "IOPS=5, blocksize=4KiB, maxtags=32")

for fractionR in [1., 0.75, 0.5, 0.25, 0.]:     ## Traditional C-style loops work too.
                                              ## Same as:  for ( fractionR = 1.; fractionR >= 0.; fractionR = fractionR - 0.25 )  
    z.edit_rollup(name="all=all",  parameters = "IOPS=max, fractionread=" + str(fractionR))

    z.go(stepname = "max IOPS " + str(round(100.*fractionR,0)) + "% read",
                measure="IOPS", warmup_seconds=30, measure_seconds = 120,
                accuracy_plus_minus="1%", confidence = "95%", timeout_seconds = 14400)

## Now we re-run the same thing, but this time with IOPS set to 90% of what was measured with IOPS=max.

## In particular we want to look at the random write I/O service time histogram with WP below max

## In the previous step, the measurement was of IOPS, but this time we know the IOPS, so we will measure service_time
## so that we will measure after service time stabilizes.


summary_filename = z.test_folder() + "/all/" + z.test_name() + ".all=all.summary.csv"

print("summary_filename = " + summary_filename + "\n")


print("csv file has " + str(z.csv_rows(summary_filename)) + " rows of data following the header line.\n");

index = -1

for fractionR in [1., 0.75, 0.5, 0.25, 0.]:
    max_IOPS = float(z.csv_cell_value(filename=summary_filename, row = index, col = "Overall IOPS"))

    z.edit_rollup(name= "all=all", parameters = "total_IOPS=" + str(0.9 * max_IOPS) + ", fractionread=" + str(fractionR))

    z.go(stepname = "90% of max IOPS " + str(round(100.*fractionR,0)) + "% read",
                     measure="service_time_seconds", warmup_seconds=30, measure_seconds = 120,
                     accuracy_plus_minus="2.5%", confidence = "95%", timeout_seconds = 14400)
    index=index+1

##    measure = MB_per_second           is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=bytes_transferred, accessor=sum
##    measure = IOPS                    is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=bytes_transferred, accessor=count
##    measure = service_time_seconds    is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=service_time,      accessor=avg
##    measure = response_time_seconds   is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=response_time,     accessor=avg
##    measure = MP_core_busy_percent    is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=MP_core, element_metric=busy_percent
##    measure = PG_busy_percent         is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=PG,      element_metric=busy_percent
##    measure = CLPR_WP_percent         is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=CLPR,    element_metric=WP_percent
