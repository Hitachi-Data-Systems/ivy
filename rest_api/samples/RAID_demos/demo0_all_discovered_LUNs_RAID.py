import ivyrest

ivy = ivyrest.IvyObj()

#ivy.set_output_folder_root(".")
#ivy.set_test_name("demo0_all_discovered_LUNs_RAID")

ivy.hosts_luns(hosts = "sun159", select = "serial_number : 83011441")
