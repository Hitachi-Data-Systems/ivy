import ivyrest
import requests
import time

ivy = ivyrest.IvyObj("localhost")

ivy.set_output_folder_root(".")
ivy.set_test_name("demo8_pattern")

ivy.hosts_luns(hosts = "sun159", select = "serial_number : 83011441")

ivy.create_workload(name = "random", select = "LDEV : 00:10", iosequencer = "random_steady", parameters = "fractionRead=80%, blocksize=4KiB, IOPS=10, maxtags=32, dedupe=2, pattern=random")

ivy.create_workload(name = "ascii", select = "LDEV : 00:10", iosequencer = "random_steady", parameters = "fractionRead=80%, blocksize=4KiB, IOPS=10, maxtags=32, dedupe=2, pattern=ascii")

ivy.create_workload(name = "trailing_zeros", select = "LDEV : 00:10", iosequencer = "random_steady", parameters = "fractionRead=80%, blocksize=4KiB, IOPS=10, maxtags=32, dedupe=2, pattern=trailing_zeros, compressibility=.5")

ivy.create_workload(name = "gobbledegook", select = "LDEV : 00:10", iosequencer = "random_steady", parameters = "fractionRead=80%, blocksize=4KiB, IOPS=10, maxtags=32, dedupe=2, pattern=gobbledegook")

for blocksizeKiB in [ 1 ]:
    ivy.edit_rollup(name = "all=all", parameters = "blocksize = " + str(blocksizeKiB) + "KiB")
    ivy.go(stepname="blocksize " + str(blocksizeKiB) + " KiB", warmup_seconds = 5, measure_seconds = 5)
