import ivyrest
import requests
import time

ivy = ivyrest.IvyRestClient("172.17.19.159")

host_list = ["sun159"]
select_list = [ {'serial_number' : '83011441' } ]

ivy.set_output_folder_root(".")
print(ivy.test_folder())
ivy.set_test_name("6D2P")

summary_filename = ivy.test_folder() + "/all/" + ivy.test_name() + ".all=all.summary.csv"
lun_filename = ivy.test_folder() + "/available_test_LUNs.csv"
print(summary_filename)
print(lun_filename)

ivy.hosts_luns(Hosts = host_list, Select = select_list)

ivy.create_rollup(name="Workload")
ivy.create_rollup(name="Workload+host")
ivy.create_rollup(name="Port")

#//////////Random Read Miss////////////////
ioparams = {
'iosequencer' : 'random_steady',
'IOPS' : 'max',
'fractionRead' : 1.0,
'VolumeCoverageFractionStart' : 0.0,
'VolumeCoverageFractionEnd' : 1.0,
'blocksize' : 8,
'maxtags' : 64
}

ivy.set_io_sequencer_template(**ioparams)
ivy.create_workload(workload = "RandomReadMiss", iosequencer = "random_steady", parameters="")

for i in [64,44,30,20,14,9,6,4,3,2,1]:
    ivy.edit_rollup(name = "all=all", parameters = " maxTags=" + str(i))
    ivy.go(stepname="RR_maxTags_" + str(i), subinterval_seconds=10, warmup_seconds=120, measure_seconds=180)

summary_filename = ivy.test_folder() + "/all/" + ivy.test_name() + ".all=all.summary.csv"
lun_filename = ivy.test_folder() + "/available_test_LUNs.csv"

max_IOPS = float(ivy.csv_cell_value(filename = summary_filename, row = int(ivy.csv_rows(filename = summary_filename)) - 1, col = "Overall IOPS"))/float(ivy.csv_rows(filename = lun_filename))

for j in [0.7,0.45,0.25,0.1,0.01]:
    ivy.edit_rollup(name = "all=all", parameters = "IOPS="+ str(max_IOPS * j) +", maxTags=1")
    ivy.go(stepname="RR_maxTags_1(" + str(j*100) + "%)", subinterval_seconds=10, warmup_seconds=120, measure_seconds=180)

ivy.delete_workload (workload = "RandomReadMiss")

#//////////Random Write Miss////////////////
ioparams = {
'iosequencer' : 'random_steady',
'IOPS' : 'max',
'fractionRead' : 0.0,
'VolumeCoverageFractionStart' : 0.0,
'VolumeCoverageFractionEnd' : 1.0,
'blocksize' : 8,
'maxtags' : 16
}

ivy.set_io_sequencer_template(**ioparams)
ivy.create_workload(workload = "RandomWriteMiss", iosequencer = "random_steady", parameters="")

for i in [16,13,11,9,7,6,5,4,3,2,1]:
    ivy.edit_rollup(name = "all=all", parameters = " maxTags=" + str(i))

    ivy.go(stepname="RR_maxTags_" + str(i), subinterval_seconds=10, warmup_seconds=120, measure_seconds=180)

summary_filename = ivy.test_folder() + "/all/" + ivy.test_name() + ".all=all.summary.csv"
lun_filename = ivy.test_folder() + "/available_test_LUNs.csv"

max_IOPS = float(ivy.csv_cell_value(filename = summary_filename, row = int(ivy.csv_rows(filename = summary_filename)) - 1, col = "Overall IOPS"))/float(ivy.csv_rows(filename = lun_filename))

for j in [0.7,0.45,0.25,0.1,0.01]:
    ivy.edit_rollup(name = "all=all", parameters = "IOPS="+ str(max_IOPS * j) +", maxTags=1")
    ivy.go(stepname="RR_maxTags_1(" + str(j*100) + "%)", subinterval_seconds=10, warmup_seconds=120, measure_seconds=180)

ivy.delete_workload (workload = "RandomWriteMiss")

#//////////Sequential Read////////////////
ioparams = {
'iosequencer' : 'sequential',
'IOPS' : 'max',
'fractionRead' : 1.0,
'VolumeCoverageFractionStart' : 0.0,
'VolumeCoverageFractionEnd' : 1.0,
'blocksize' : 256,
'maxtags' : 32
}

ivy.set_io_sequencer_template(**ioparams)
ivy.create_workload(workload = "SequentialRead", iosequencer = "sequential", parameters="")

for i in [32,24,17,13,9,7,5,4,3,2,1]:
    ivy.edit_rollup(name = "all=all", parameters = " maxTags=" + str(i))

    ivy.go(stepname="RR_maxTags_" + str(i), subinterval_seconds=10, warmup_seconds=120, measure_seconds=180)

summary_filename = ivy.test_folder() + "/all/" + ivy.test_name() + ".all=all.summary.csv"
lun_filename = ivy.test_folder() + "/available_test_LUNs.csv"

max_IOPS = float(ivy.csv_cell_value(filename = summary_filename, row = int(ivy.csv_rows(filename = summary_filename)) - 1, col = "Overall IOPS"))/float(ivy.csv_rows(filename = lun_filename))

for j in [0.7,0.45,0.25,0.1,0.01]:
    ivy.edit_rollup(name = "all=all", parameters = "IOPS="+ str(max_IOPS * j) +", maxTags=1")
    ivy.go(stepname="RR_maxTags_1(" + str(j*100) + "%)", subinterval_seconds=10, warmup_seconds=120, measure_seconds=180)

ivy.delete_workload (workload = "SequentialRead")

#//////////Sequential Write////////////////
ioparams = {
'iosequencer' : 'sequential',
'IOPS' : 'max',
'fractionRead' : 0.0,
'VolumeCoverageFractionStart' : 0.0,
'VolumeCoverageFractionEnd' : 1.0,
'blocksize' : 256,
'maxtags' : 32
}

ivy.set_io_sequencer_template(**ioparams)
ivy.create_workload(workload = "SequentialWrite", iosequencer = "sequential", parameters="")

for i in [32,24,17,13,9,7,5,4,3,2,1]:
    ivy.edit_rollup(name = "all=all", parameters = " maxTags=" + str(i))

    ivy.go(stepname="RR_maxTags_" + str(i), subinterval_seconds=10, warmup_seconds=120, measure_seconds=180)

summary_filename = ivy.test_folder() + "/all/" + ivy.test_name() + ".all=all.summary.csv"
lun_filename = ivy.test_folder() + "/available_test_LUNs.csv"

max_IOPS = float(ivy.csv_cell_value(filename = summary_filename, row = int(ivy.csv_rows(filename = summary_filename)) - 1, col = "Overall IOPS"))/float(ivy.csv_rows(filename = lun_filename))

for j in [0.7,0.45,0.25,0.1,0.01]:
    ivy.edit_rollup(name = "all=all", parameters = "IOPS="+ str(max_IOPS * j) +", maxTags=1")
    ivy.go(stepname="RR_maxTags_1(" + str(j*100) + "%)", subinterval_seconds=10, warmup_seconds=120, measure_seconds=180)

ivy.delete_workload (workload = "SequentialWrite")

ivy.exit()
