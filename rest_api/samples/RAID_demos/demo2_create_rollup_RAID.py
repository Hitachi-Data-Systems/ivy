import ivyrest

ivy = ivyrest.IvyObj("localhost")

ivy.set_output_folder_root(".")
ivy.set_test_name("demo2_create_rollup_RAID")

ivy.hosts_luns(hosts = "sun159", select = "serial_number : 83011441")

ivy.create_workload (name = "steady", select = "", iosequencer = "random_steady", parameters = "fractionRead=100%, blocksize=4KiB, IOPS=100, maxtags=32")

ivy.create_rollup(name="Port")

# If you have more than one subsystem in the test config, add serial_number to your rollup to distinguish

ivy.create_rollup(name="Serial_Number+Port")

# host means the "ivyscript_hostname" you used on the [hosts] statement.
# hostname is the canonical hostname.

ivy.create_rollup(name="MPU")  # you can only do this if you have a command device
	# Look in the csv files for instances of this rollup, and you will see only host workload / RMLIB subsystem data for under-test LDEVs mapped to that MPU.
	# For G600, you will see 4 MP_cores reporting in each MPU rollup instance.
	
ivy.create_rollup(name="host")
	# "host" means the name you used for the host on the [hosts] statement, the "ivyscript_hostname",
	# which may be an alias or an IPV4 dotted quad.  To get the canonical hostname at the test host, say "hostname".

# Either of the two following is how you specify you want a by-subinterval csv file for each individual workload thread.

ivy.create_rollup(name="host+LUN_name+workload")
ivy.create_rollup(name="workloadID")

#mystr = str(ivy.show_rollup_structure())
#print("-------------\nRollup structure:\n" + mystr + "\n");
print("-------------\nRollup structure:\n" + ivy.show_rollup_structure() + "\n");

ivy.go(stepname="step_eh", warmup_seconds = 5, measure_seconds = 5)
