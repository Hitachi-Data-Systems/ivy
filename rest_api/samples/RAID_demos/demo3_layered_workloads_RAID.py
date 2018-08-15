import ivyrest

ivy = ivyrest.IvyObj("localhost")

host_list = ["sun159"]
select_list = [ {'serial_number' : '83011441' } ]

ivy.set_output_folder_root(".")
ivy.set_test_name("demo3_layered_workloads_RAID")

ivy.hosts_luns(hosts = host_list, select = select_list)

# When creating a set of workloads that have minor variations on a base set of parameter settings,
# you can set the default parameter settings for each [iosequencer] type, being random_steady, random_independent, and sequential.

ivy.set_io_sequencer_template(iosequencer = "random_steady", parameters ="IOPS=100, blocksize=4KiB, maxtags=32, fractionRead = 70%, VolumeCoverageFractionStart=0.0, VolumeCoverageFractionEnd=0.5")

ivy.set_io_sequencer_template(iosequencer =  "sequential", parameters = "iops=25, blocksize=64KiB, maxtags=2, VolumeCoverageFractionStart=0.5, VolumeCoverageFractionEnd=1.0")

# LUN coverage - you can specify the beginning/ending points from 0% to 100% across the LUN.
# Note that the random)_steady template targets the first half of each LUN, and the sequential template covers the 2nd half of the LUNs.

# parameters were set earlier using [SetIosequencerTemplate]
ivy.create_workload(name = "steady", iosequencer = "random_steady", parameters="")

ivy.create_workload(name = "sw1", select = "", iosequencer = "sequential", parameters =  "FractionRead = 0, SeqStartFractionOfCoverage=.0")
ivy.create_workload(name = "sw2", select =  "", iosequencer = "sequential", parameters = "FractionRead = 0, SeqStartFractionOfCoverage=.2")
ivy.create_workload(name = "sr3", select =  "", iosequencer =  "sequential", parameters = "FractionRead = 1, SeqStartFractionOfCoverage=.4")
ivy.create_workload(name = "sr4", select =  "", iosequencer = "sequential",  parameters = "FractionRead = 1, SeqStartFractionOfCoverage=.6")

# Here we create a rollup by workload, so we will get one summary line for each workload totalled over all the hosts & LUNs.
ivy.create_rollup(name="workload")

ivy.go(stepname="step_eh", warmup_seconds = 5, measure_seconds = 10)
