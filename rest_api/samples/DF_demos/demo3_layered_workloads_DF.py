import ivyrest as iv

#[outputfolderroot] "/scripts/ivy/ivyoutput/sample_output";

c = iv.IvyObj()

c.set_output_folder_root(".")
c.set_test_name("demo3_layered_workloads_DF")

c.hosts_luns(hosts = ["sun159", "172.17.19.159"], select = "serial_number : 83011441")

## When creating a set of workloads that have minor variations on a base set of parameter settings,
## you can set the default parameter settings for each iosequencer =  type, being random_steady, random_independent, and sequential.

## Setting defaults like this is for the workloads you have yet to create.

## (To set parameters in existing workloads, use [EditRollup].)

c.set_io_sequencer_template(iosequencer = "random_steady",
	parameters = "IOPS=20, blocksize=4KiB, maxtags=1, fractionread=0.5, VolumeCoverageFractionStart=0.0, VolumeCoverageFractionEnd=0.5")

c.set_io_sequencer_template(iosequencer = "sequential", parameters = "blocksize=64KiB, maxtags=2, iops=25, fractionread=0.0, VolumeCoverageFractionStart=0.5, VolumeCoverageFractionEnd=1.0")

## LUN coverage - you can specify the beginning/ending points from 0% to 100% across the LUN.

## Note that the random)_steady template targets the first half of each LUN, and the sequential template covers the 2nd half of the LUNs.

c.create_workload( name = "r_steady",        select = [{ "host" :  "sun159"}, {"LDEV" : "0000" }],     ## proper JSON syntax
	 iosequencer =  "random_steady"   )

c.create_workload( name = "r_independent"  , select =  [{ "host" : "172.17.19.159"}, {"LDEV" : "0008" }],  
	iosequencer =  "random_independent"     , parameters = "blocksize=2048, IOPS=0.5")

## Note that we used one ivyscript_hostname alias to drive DF LDEV 4, and the other host to drive DF LDEV 5.

c.create_workload( name = "sw1" , select =  "LDEV : 00:00-00:ff", iosequencer =  "sequential" , parameters = "FractionRead = 0, SeqStartFractionOfCoverage=.2")
c.create_workload( name = "sw2" , select =  "LDEV : 00:00-00:ff", iosequencer =  "sequential" , parameters = "FractionRead = 0, SeqStartFractionOfCoverage=.2")
c.create_workload( name = "sr3" , select =  "LDEV : 00:00-00:ff", iosequencer =  "sequential" , parameters = "FractionRead = 1, SeqStartFractionOfCoverage=.4")
c.create_workload( name = "sr4" , select =  "LDEV : 00:00-00:ff", iosequencer =  "sequential" , parameters = "FractionRead = 1, SeqStartFractionOfCoverage=.6")
c.create_workload( name = "sr5" , select =  "LDEV : 00:00-00:ff", iosequencer =  "sequential" , parameters = "FractionRead = 1, SeqStartFractionOfCoverage=.8")

c.create_rollup(name="workload") ## Here we create a rollup by workload, so we will get one summary line for each workload totalled over all the hosts & LUNs.

c.go(stepname="step_eh", warmup_seconds = 5, measure_seconds = 10)
