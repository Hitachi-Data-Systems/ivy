import ivyrest

ivy = ivyrest.IvyObj("localhost")

host_list = ["sun159"]
select_list = [ {'serial_number' : '83011441' } ]

ivy.set_output_folder_root(".")
ivy.set_test_name("demo4_edit_rollup_RAID")

ivy.hosts_luns(hosts = host_list, select = select_list)

# The [EditRollup] statement gives you access to the ivy Dynamic Feedback Controller engine's ability to deliver
# parameter updates to workload threads.

# The DFC engine C++ code that issues real=time parameter updates uses exactly the same interface, 
# providing a rollup instance set, like "all=all", and parameter settings, such as "IOPS=100, fractionRead = "75%

# [EditRollup] "serial_number+LDEV = 83011441+0004"  [parameters] "IOPS = 289";
# [EditRollup] "port = { 1A, 3A, 5A, 7A }"          [parameters] "fractionRead = 100%";

ivy.create_workload(name="steady", select = "", iosequencer = "random_steady", parameters = "fractionRead=100%, IOPS=max, maxTags=32")

for blocksize in [4, 8, 64]:
    ivy.edit_rollup(name = "all=all", parameters = "blocksize = " + str(blocksize) + "KiB")
    ivy.go(stepname="iops_max_" + str(blocksize) + "KiB_random_read", measure_seconds = 60)
