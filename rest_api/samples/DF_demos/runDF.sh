!/bin/bash

#OPTIONS=
OPTIONS="-log "

echo "ivy $OPTIONS demo0_all_discovered_LUNs_DF.py >demo0_all_discovered_LUNs_DF.console.txt 2>&1"
      ivy $OPTIONS demo0_all_discovered_LUNs_DF.py >demo0_all_discovered_LUNs_DF.console.txt 2>&1

echo "ivy $OPTIONS demo1_fixed_DF.py >demo1_fixed_DF.console.txt 2>&1"
      ivy $OPTIONS demo1_fixed_DF.py >demo1_fixed_DF.console.txt 2>&1

echo "ivy $OPTIONS demo2_create_rollup_DF.py >demo2_create_rollup_DF.console.txt 2>&1"
      ivy $OPTIONS demo2_create_rollup_DF.py >demo2_create_rollup_DF.console.txt 2>&1

echo "ivy $OPTIONS demo3_layered_workloads_DF.py >demo3_layered_workloads_DF.console.txt 2>&1"
      ivy $OPTIONS demo3_layered_workloads_DF.py >demo3_layered_workloads_DF.console.txt 2>&1

echo "ivy $OPTIONS demo4_edit_rollup_DF.py >demo4_edit_rollup_DF.console.txt 2>&1"
      ivy $OPTIONS demo4_edit_rollup_DF.py >demo4_edit_rollup_DF.console.txt 2>&1

echo "ivy $OPTIONS demo5_measure_read_vs_write_DF.py >demo5_measure_read_vs_write_DF.console.txt 2>&1"
      ivy $OPTIONS demo5_measure_read_vs_write_DF.py >demo5_measure_read_vs_write_DF.console.txt 2>&1

echo "ivy $OPTIONS demo9_builtin.py >demo9_builtin.console.txt 2>&1"
      ivy $OPTIONS demo9_builtin.py >demo9_builtin.console.txt 2>&1

echo "ivy $OPTIONS demoa_adaptive_PID_drive_IOPS_by_target_service_time.py >demoa_adaptive_PID_drive_IOPS_by_target_service_time.console.txt 2>&1"
      ivy $OPTIONS demoa_adaptive_PID_drive_IOPS_by_target_service_time.py >demoa_adaptive_PID_drive_IOPS_by_target_service_time.console.txt 2>&1

echo "ivy $OPTIONS demoa_auto_ranging_drive_IOPS_vs_response_time_DF_by_LDEV.py >demoa_auto_ranging_drive_IOPS_vs_response_time_DF_by_LDEV.console.txt 2>&1"
     ivy $OPTIONS demoa_auto_ranging_drive_IOPS_vs_response_time_DF_by_LDEV.py >demoa_auto_ranging_drive_IOPS_vs_response_time_DF_by_LDEV.console.txt 2>&1

echo "ivy $OPTIONS demob_sequential.py >demob_sequential.console.txt 2>&1"
      ivy $OPTIONS demob_sequential.py >demob_sequential.console.txt 2>&1

echo "done."
