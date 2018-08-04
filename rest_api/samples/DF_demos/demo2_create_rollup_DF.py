import ivyrest

cl = ivyrest.IvyRestClient("localhost")

# [outputfolderroot] "/scripts/ivy/ivyoutput/sample_output";
cl.set_output_folder_root(".")
cl.set_test_name("demo2_create_rollup_DF")

cl.hosts_luns(Hosts = [ "sun159", "172.17.19.159"], Select = "serial_number : 83011441")

cl.create_workload(workload = "steady", select= "", iosequencer="random_steady", parameters = "fractionRead=100%, blocksize=4KiB, IOPS=max, maxtags=32")

cl.create_rollup(name = "Port")

## If you have more than one subsystem in the test config, add serial_number to your rollup to distinguish

cl.create_rollup(name = "Serial_Number+Port")

## host means the "ivyscript_hostname" you used on the [hosts] statement.
## hostname is the canonical hostname.

cl.create_rollup(name = "host")

## Either of the two following is how you specify you want a by-subinterval csv file for each individual workload thread.

cl.create_rollup(name = "host+LUN_name+workload")
cl.create_rollup(name = "workloadID")


print("-------------\nRollup structure:\n" + cl.show_rollup_structure() + "\n");

cl.go(stepname="step_eh", warmup_seconds = 5, measure_seconds = 10)

cl.exit()
