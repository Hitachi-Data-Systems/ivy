import ivyrest

# [outputfolderroot] "/scripts/ivy/ivyoutput/sample_output";

a = ivyrest.IvyObj("localhost")

a.set_output_folder_root(".")
a.set_test_name("demo4_edit_rollup_DF")

a.hosts_luns(hosts =  "sun159",  select = "serial_number : 83011441")

## The [EditRollup] statement gives you access to the ivy Dynamic Feedback Controller engine's ability to deliver
## parameter updates to workload threads.

## The DFC engine C++ code that issues real=time parameter updates uses exactly the same interface, 
## providing a rollup instance set, like "all=all", and parameter settings, such as "IOPS=100, fractionRead = "75%

## [EditRollup] "serial_number+LDEV = 83011441+0004"  [parameters] "IOPS = 289";
## [EditRollup] "port = { 1A, 3A, 5A, 7A }"           [parameters] "fractionRead = 100%";  // sorry, doesn't accept JSON syntax ... yet

a.create_workload(name = "r_steady", select = "",  iosequencer = "random_steady", parameters = "fractionread=100%,  maxtags=32, IOPS=max, blocksize = 512")

for blocksize in [4, 8, 64 ]:
    a.edit_rollup(name="all=all",  parameters = "blocksize = " + str(blocksize) + "kib")
    a.go(stepname="iops_max_" + str(blocksize) + "KiB_random_read", measure_seconds = 10)
