import ivyrest

ivy = ivyrest.IvyObj()

ivy.set_output_folder_root(".")
ivy.set_test_name("demo1_fixed_RAID")

ivy.hosts_luns(hosts = "sun159", select = "serial_number : 83011441")

ivy.create_workload (name = "steady", select = "", iosequencer = "random_steady", parameters = "fractionRead=100%, blocksize=4KiB, IOPS=max, maxtags=32")


for blocksizeKiB in [ 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 ]:
    ivy.edit_rollup(name = "all=all", parameters = "blocksize = " + str(blocksizeKiB) + "KiB")
    ivy.go(stepname="blocksize " + str(blocksizeKiB) + " KiB", warmup_seconds = 10, measure_seconds = 30)

#################################################
"""
//
//   Note that in the test setup used to run this demo, the LUNs and the command device
//   were on the same subsystem port.  At the 128 KiB block size, the port becomes
//   so saturated that communication with the command device runs so slow that
//   it doesn't collect in time.
//
"""
