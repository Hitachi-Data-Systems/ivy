import ivyrest
import requests
import time

# In step 0, we measure max IOPS.
#      -This yields the "high_IOPS" and the "high_target".
# In step 1, we run at 1% of the max IOPS.
#      -This yields the "low_IOPS" and the "high_target".

ivy = ivyrest.IvyRestClient("172.17.19.159", user_agent="Mozilla/5.0")

# Hosts + LUN discovery

host_list = ["sun159"]
port_list = ["1A", "2A"]
select_list = [ {'serial_number' : '83011441' }, {'Port' : port_list } ]

print(host_list)

ivy.set_output_folder_root(".")
ivy.set_test_name("MP_50")
ivy.hosts_luns(Hosts = host_list, Select = select_list)

# Create Workload
params = {
'workload' : 'steady',
'iosequencer' : 'random_steady',
'IOPS' : 'max',
'fractionRead' : 80,
'blocksize' : 4,
'maxtags' : 128
}

ioparams = {
'iosequencer' : 'random_steady',
'IOPS' : 'max',
'fractionRead' : 80,
'blocksize' : 4,
'maxtags' : 128
}

ivy.set_io_sequencer_template(**ioparams)

#ivy.SetIOsequencerTemplate(iosequencer = "random_steady", parameters = "fractionRead=80%, blocksize=4KiB, IOPS=max, maxtags=128")

ivy.create_workload(**params)
#ivy.CreateWorkload (workload = "steady", select = "", iosequencer = "random_steady", 
#                                  parameters = "fractionRead=80%, blocksize=4KiB, IOPS=max, maxtags=128")

# Step 0
goparams_step0 = {
'stepname' : 'high',
'warmup_seconds' : 10,
'measure_seconds' : 60,
'subinterval_seconds' : 5
}

ivy.go(**goparams_step0)

#string summary_filename = testFolder() + "/all/" + testName() + ".all=all.summary.csv";
summary_filename = "MP_50/all/MP_50.all=all.summary.csv"

high_IOPS = ivy.csv_cell_value(filename = summary_filename, row = 0, col = "Overall IOPS")
print(high_IOPS)

#double high_target = double(csv_cell_value(summary_filename,0,"subsystem avg MP_core busy_percent"));   
#high_target = ivy.csv_cell_value(filename = summary_filename, row = 0, col = "subsystem avg MP_core busy_percent")

high_target = '70%'

result0 = '\n********** step 0 high_IOPS = ' + high_IOPS + ", high_target = " + high_target + ' **********\n\n'
print(result0)

# TBD - log(masterlogfile(),s);

# ==================================================
# In step 1, we run at 1% of the max IOPS to measure "low_target"


ivy.edit_rollup(name = "all=all", parameters = "total_IOPS=" + str(.01 * float(high_IOPS)))

goparams_step1 = {
'stepname' : 'low',
'warmup_seconds' : 10,
'measure' : 'MP_core_busy_percent',
'accuracy_plus_minus' : "2.5%",
'timeout_seconds' : "30:00",
'subinterval_seconds' : 5
}

result = ivy.go(**goparams_step1)

# TODO -- check success/fail status
#if ("success" != result) { print("Failed to obtain measurement running at 1% of max IOPS.\n"); exit(); }

low_IOPS = ivy.csv_cell_value(filename = summary_filename, row = 1, col = "Overall IOPS")

print(low_IOPS)
#low_target = ivy.csv_cell_value(summary_filename,1,"subsystem avg MP_core busy_percent");

low_target = '10%'

result1 = '\n********** step 1 result - low_IOPS = ' + low_IOPS + ', low_target = ' + low_target + ' **********\n\n'
print(result1)

#/ ==================================================
#/ In step 2, we use dynamic feedback control of IOPS on host workload service time to plot the IOPS
#/ for a series of multiples of the baseline service time to plot the graph.

#/ To illustrate individual & independent pid control by focus rollup we focus on each PG separately.

target = '50%'

goparams_step2 = {
'stepname' : '50% MP_core',
'warmup_seconds' : 10,
'measure_seconds' : 60,
'timeout_seconds' : "30:00",
'measure' : 'MP_core_busy_percent',
'accuracy_plus_minus' : "1%"
}

dfc_params = {
'dfc' : 'PID',
'low_IOPS'    : str(low_IOPS),
'low_target'  : str(low_target),
'high_IOPS'   : str(high_IOPS),
'high_target' : str(high_target),
'target_value': str(target)
}
result = ivy.go(**goparams_step2, **dfc_params)

print(result)
#result = last_result();
#if ("success" != result) { print("Failed to obtain measurement at 50% MP_core busy.\n"); exit(); }

fifty_percent_IOPS =  ivy.csv_cell_value(filename = summary_filename, row = 2, col = 'Overall IOPS')
print(fifty_percent_IOPS)

result2 = '\n********** step 2 result - IOPS at 50% MP_core busy =  ' + fifty_percent_IOPS + '\n**********\n\n'
print(result2)

ivy.end_session()
