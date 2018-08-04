import ivyrest

ivy = ivyrest.IvyRestClient("localhost")

ivy.set_output_folder_root(".")
ivy.set_test_name("demo5_measure_read_vs_write_RAID")

ivy.hosts_luns(Hosts = "sun159", Select = "serial_number : 83011441")

ivy.create_workload(workload = "steady", select = [{'LDEV' : '00:10'}], iosequencer = "random_steady", parameters = "")

ivy.edit_rollup(name = "all=all", parameters = "blocksize = 4KiB, maxTags=32")

for fractionR in  [ 1., 0.75, 0.5, 0.25, 0. ]:
    ivy.edit_rollup(name="all=all", parameters = "IOPS=max, fractionRead=" + str(fractionR))

    # At the end of the subinterval sequence, the sequencer sets IOPS to zero at cooldown,
    # So for every pass we set IOPS=max.

    ivy.go(stepname = "max IOPS " + str(round(100.*fractionR, 0)) + "% read", measure="IOPS", accuracy_plus_minus="2.5", timeout_seconds = "14400", max_wp_change="20%")

    if not ivy.last_result():
        ivy.exit()

# Now we re-run the same thing, but this time with IOPS set to 90% of what was measured with IOPS=max.

# In particular we want to look at the random write I/O service time histogram with WP below max

# In the previous step, the measurement was of IOPS, but this time we know the IOPS, so we will measure service_time
# so that we will measure after service time stabilizes.

summary_filename = ivy.test_folder() + "/all/" + ivy.test_name() + ".all=all.summary.csv"

print("summary_filename = " + summary_filename + "\n")

print("csv file has " + str(ivy.csv_rows(summary_filename)) + " rows of data following the header line.\n")

index=-1

for fractionR in [1., 0.75, 0.5, 0.25, 0.]:
    index=index+1
    max_IOPS = float(ivy.csv_cell_value(filename = summary_filename, row = index, col = "Overall IOPS"))	
    ivy.edit_rollup(name="all=all", parameters = "total_IOPS=" + str(0.9 * max_IOPS) + ", fractionread=" + str(fractionR))
    ivy.go(stepname = "90% of max IOPS " + str(round(100.*fractionR, 0)) + "% read", measure="service_time_seconds", accuracy_plus_minus="2.5%", confidence ="95%", timeout_seconds = "14400", max_wp_change="20%") # we need to average over several WP excursions to get the accuracy at 50% writes
