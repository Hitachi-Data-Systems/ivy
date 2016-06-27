# ivy

"ivy" is a Linux-based block storage synthetic workload generator and benchmarking system.

## Features

* **Low CPU overhead** - Written in C++ using Linux native Asynchronous I/O interface to issue multiple I/Os in parallel from one workload thread.
* **Vendor independent design** - LUN attributes decoded by external vendor specific SCSI Inquiry-based LUN discovery tool become selectable in ivy.
  * The Hitachi LUN_discovery toolset is at https://github.com/Hitachi-Data-Systems/LUN_discovery
* **Dynamic feedback control** - PID loop feature enables for example "find IOPS at 1 ms response time".
* **Test in minimum time** - Will run a test as long as necessary to measure to specified plus/minus accuracy.
* **Easy test setup** - Select by vendor LUN attribute such as subsystem logical device, port, parity group, pool ID, etc. instead of by Linux /dev/sdxxx name.
* **Sliced and diced data** - `[CreateRollup] "port";` will print csv files with data rolled up by port.
* **Configuration validation** - `[CreateRollup "port" [quantity] 16 [MaxDroop] "20%";` will invalid results unless 16 ports are reporting and the IOPS for the slowest port is at most 20% below the IOPS for the fastest port.
* **Workflow automation** - `.ivyscript` programming language with "C/C++"-like syntax.

## Vendor proprietary "connectors"

Authorized Hitachi internal users in our labs with the code and license key can use the "command device connector", which retrieves configuration and real-time performance monitor data from Hitachi subsystems.

Other vendors are encouraged to develop their own "connectors" for ivy.

With the Hitachi command device connector, ivy

* Augments the SCSI Inquiry attributes with logical device (LDEV) attributes obtained from the subsystem through the command device.
* Retrieves subsystem performance information synchronized and aligned with ivy host data.
* Filters real-time subsystem data by ivy "rollup instance" enabling fine-grained dynamic feedback control of IOPS to hit a target utilization of a component inside the subsystem.

## ivy home web site

Documentation, training, videos are in the ["ivy" project on the HDS community](https://community.hds.com/groups/ivy)

## Contributors

Just me so far - ian.vogelesang@hds.com - but I'm looking forward to others diving in.

## License

[Apache 2.0](http://www.apache.org/licenses/LICENSE-2.0)

## videos

* [Introduction to ivy](https://www.youtube.com/watch?v=--h_tdnRkkE&amp;list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&amp;index=1)
* [Testing in minimum time with ivy](https://www.youtube.com/watch?v=2rrwpY4ySwQ&amp;list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&amp;index=2)
* [Installing ivy](https://www.youtube.com/watch?v=0AqzXsEbCJM&amp;list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&amp;index=3)
* [Introduction to ivy output csv files](https://www.youtube.com/watch?v=WNVJccfrhrg&amp;list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&amp;index=4)
* [demo0 DF - LUN discovery](https://www.youtube.com/watch?v=75Z3hwDI42A&amp;list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&amp;index=5)
* [demo0 RAID - LUN discovery with command device connector](https://www.youtube.com/watch?v=ZQDe6nHBPV8&amp;list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&amp;index=6)
* [demo1 fixed DF - testing for a fixed duration](https://www.youtube.com/watch?v=l-Lpj4h-9iI&amp;list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&amp;index=7)
* [demo1 fixed RAID - real time subsystem data with command device connector](https://www.youtube.com/watch?v=Gk7DDY0JI04&amp;list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&amp;index=8)
* [demo2 \[CreateRollup\]](https://www.youtube.com/watch?v=TOQzbdRm8do&amp;list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&amp;index=9)
* [demo3 layered workloads](https://www.youtube.com/watch?v=gOIYZ81m-Bo&amp;list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&amp;index=10)
* [demo5 \[EditRollup\] - read vs. write example](https://www.youtube.com/watch?v=hzF2MKhhd0k&amp;list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&amp;index=11)
* [\[Go\] statement 1 of 3](https://www.youtube.com/watch?v=3bAn5pFKS4I&amp;list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&amp;index=12)
* [\[Go\] statement 2 of 3 - the focus rollup](https://www.youtube.com/watch?v=_nT25ieZWzI&amp;list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&amp;index=13)
* [\[Go\] statement 3 of 3 - measure=on and the PID loop feature](https://www.youtube.com/watch?v=QZ6aqLtKPEg&amp;list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&amp;index=14)

