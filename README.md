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

