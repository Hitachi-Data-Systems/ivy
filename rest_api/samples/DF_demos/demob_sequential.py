import ivyrest

iv = ivyrest.IvyObj()

iv.set_output_folder_root(".")
iv.set_test_name("demob_sequential")

iv.hosts_luns(hosts = "sun159", select = "serial_number : 83011441")

zones = 10
for zone in range(0, zones):
    start = float(zone)/float(zones)
    end = float(zone+1)/float(zones)

    iv.create_workload(name = "zone" + str(zone),
		iosequencer = "sequential",
		parameters =  "VolumeCoverageFractionStart=" + str(start)
		          + ",VolumeCoverageFractionEnd=" + str(end) 
		          + ",IOPS=max, blocksize=64KiB, fractionRead=100%, maxTags=1")

iv.create_rollup(name = "workload")

iv.go(stepname="separate_zones", measure_seconds = 60)

iv.delete_workload(name="*")
##########################################
for zone in range(0,zones):
    start = float(zone)/float(zones)

    iv.create_workload(name = "zone" + str(zone),
		iosequencer = "sequential",
		parameters = "SeqStartFractionOfCoverage=" + str(start)
			+ ",IOPS=max, blocksize=64KiB, fractionRead=100%, maxTags=1")

iv.go(stepname="whole_LUN_staggered_start", measure_seconds = 60)

iv.delete_workload(name="*")

##########################################

zones = 12

#seq_percent_read = 75%;
seq_percent_read = 0.75

for zone in range(0, zones):
    start = float(zone)/float(zones)
    end = float(zone+1)/float(zones)
	
    if float(zone) / float(zones)  < seq_percent_read:
        print("zone " + str(zone) + " of " + str(zones) + " is a read zone for seq_percent_read = " + str(seq_percent_read) + "\n")
        rw = 1.0
        p = "read_"
    else :
        print("zone " + str(zone) + " of " + str(zones) + " is a write zone for seq_percent_read = " + str(seq_percent_read) + "\n");
        rw = 0.0
        p = "write_"

    iv.create_workload(name = p + "zone" + str(zone),
                        iosequencer = "sequential",
			parameters =  
				"VolumeCoverageFractionStart=" + str(start) 
				+ ",VolumeCoverageFractionEnd=" + str(end) 
				+ ",IOPS=max, blocksize=64KiB, maxTags=1"
				+ ", fractionRead=" + str(rw))

iv.go(stepname="read_and_write_zones", measure_seconds = 60)
