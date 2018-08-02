import ivyrest
import requests
import time

ivy = ivyrest.IvyRestClient("localhost")

ivy.set_output_folder_root(".")
ivy.set_test_name("demo0_all_discovered_LUNs_RAID")

ivy.hosts_luns(Hosts = "sun159", Select = "serial_number : 83011441")

ivy.exit()
