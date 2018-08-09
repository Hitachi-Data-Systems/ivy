import ivyrest

ivy = ivyrest.IvyObj()

ivy.set_output_folder_root(".")
ivy.set_test_name("demo1_fixed_DF")

ivy.hosts_luns(hosts = ["sun159", "172.17.19.159"], select = [ {"serial_number" : "83011441"} ])

ivy.create_workload(name="steady",
	select= "",                      ## A blank [select] field means "select all", i.e. unfiltered.
	iosequencer = "random_steady",
	parameters = "fractionRead=100%, IOPS=max, maxtags=32")  ## With 7 luns on one AMS port, maxtags=32 keeps the total under 512 with two host aliases.

for blocksizeKiB in  [ 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 ]: 
    ivy.edit_rollup(name="all=all",     #  // There is always an "all=all" rollup instance with all workload threads                         
		parameters = "blocksize = " + str(blocksizeKiB) + "KiB")
		
    ivy.go(stepname="blocksize " + str(blocksizeKiB) + " KiB", warmup_seconds = 5, measure_seconds = 10)
