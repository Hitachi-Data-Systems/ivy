#!/bin/bash

#OPTIONS=
OPTIONS="-log "

echo "ivy $OPTIONS demo0_all_discovered_LUNs_RAID.py   >demo0_all_discovered_LUNs_RAID.console.txt   2>&1"
      ivy $OPTIONS demo0_all_discovered_LUNs_RAID.py   >demo0_all_discovered_LUNs_RAID.console.txt   2>&1

echo "ivy $OPTIONS demo1_fixed_RAID.py                 >demo1_fixed_RAID.console.txt                 2>&1"
      ivy $OPTIONS demo1_fixed_RAID.py                 >demo1_fixed_RAID.console.txt                 2>&1

echo "ivy $OPTIONS demo2_create_rollup_RAID.py         >demo2_create_rollup_RAID.console.txt         2>&1"
      ivy $OPTIONS demo2_create_rollup_RAID.py         >demo2_create_rollup_RAID.console.txt         2>&1

echo "ivy $OPTIONS demo3_layered_workloads_RAID.py     >demo3_layered_workloads_RAID.console.txt     2>&1"
      ivy $OPTIONS demo3_layered_workloads_RAID.py     >demo3_layered_workloads_RAID.console.txt     2>&1

echo "ivy $OPTIONS demo4_edit_rollup_RAID.py           >demo4_edit_rollup_RAID.console.txt           2>&1"
      ivy $OPTIONS demo4_edit_rollup_RAID.py           >demo4_edit_rollup_RAID.console.txt           2>&1

echo "ivy $OPTIONS demo5_measure_read_vs_write_RAID.py >demo5_measure_read_vs_write_RAID.console.txt 2>&1"
      ivy $OPTIONS demo5_measure_read_vs_write_RAID.py >demo5_measure_read_vs_write_RAID.console.txt 2>&1

echo "ivy $OPTIONS demo6_measure_IOPS_at_PG_busy.py    >demo6_measure_IOPS_at_PG_busy.console.txt    2>&1"
      ivy $OPTIONS demo6_measure_IOPS_at_PG_busy.py    >demo6_measure_IOPS_at_PG_busy.console.txt    2>&1

echo "ivy $OPTIONS demo7_measure_IOPS_at_MP_busy.py    >demo7_measure_IOPS_at_MP_busy.console.txt    2>&1"
      ivy $OPTIONS demo7_measure_IOPS_at_MP_busy.py    >demo7_measure_IOPS_at_MP_busy.console.txt    2>&1

echo "ivy $OPTIONS demo8_pattern.py                    >demo8_pattern.console.txt                    2>&1"
      ivy $OPTIONS demo8_pattern.py                    >demo8_pattern.console.txt                    2>&1



