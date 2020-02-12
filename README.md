# ivy

"ivy" is a Linux-based block storage synthetic workload generator and benchmarking system.

## 2020-02-10 News - ivy now supports io_uring
* This requires a 5.5 or newer Linux Kernel.
* Executables for ivy 3 using "aio" are also available.

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

## Getting started
Look in the doc subfolder and read the "Introduction to ivy" PowerPoint or .pdf presentation material, or else watch the "Introduction to ivy" video (link below).

## Source code
See src subfolder.  ivy is built using the CodeBlocks IDE.

## Binary Linux executables
Download the latest version version of the ivy & LUN_discovery binaries from https://hcpanywhere.hitachivantara.com/u/tERhdz1EYf-mRGF2/ivy.bin.zip?l
* When you unzip the ivy.bin.zip file, you will get a folder whose name includes the version number, so you won't overwrite any earlier downloads, and you can easily see if it's a new version.

If you are running a Linux kernel version lower than 5.5, here is the download link for ivy 3 executables: https://hcpanywhere.hitachivantara.com/u/oKCIrSmgiD2yoIis/ivy3.bin.zip?l 

## The LUN_discovery tool set is required to run ivy
[LUN_discovery on github](https://github.com/Hitachi-Data-Systems/LUN_discovery)

## Sample ivy output
Download from https://hcpanywhere.hitachivantara.com/u/9K41Z32uNePmrjVm/ivy_sample_output.zip?l

## Documentation
See the doc subfolder.

## Videos

(These videos are now a bit dated, sorry.  Please take a look at the sample output - link just above - to see what these demos look like with the current ivy version.  The PID loop feature has in particular been greatly simplified.)

Executive summary
* [Standalone 6-minute executive summary clips from "introduction to ivy"](https://youtu.be/l5-lSqleP4s)

Video series
* [Introduction to ivy](https://www.youtube.com/watch?v=VsBsz83Pl28&list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&index=1)
* [Installing ivy](https://www.youtube.com/watch?v=0AqzXsEbCJM&list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&index=2)
* [Introduction to ivy output csv files](https://www.youtube.com/watch?v=WNVJccfrhrg&list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&index=3)
* [demo0 DF - LUN discovery](https://www.youtube.com/watch?v=75Z3hwDI42A&list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&index=4)
* [demo0 RAID - LUN discovery with command device connector](https://www.youtube.com/watch?v=ZQDe6nHBPV8&list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&index=5)
* [demo1 fixed DF - testing for a fixed duration](https://www.youtube.com/watch?v=l-Lpj4h-9iI&list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&index=6)
* [demo1 fixed RAID - real time subsystem data with command device connector](https://www.youtube.com/watch?v=Gk7DDY0JI04&list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&index=7)
* [demo2 \[CreateRollup\]](https://www.youtube.com/watch?v=TOQzbdRm8do&list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&index=8)
* [demo3 layered workloads](https://www.youtube.com/watch?v=gOIYZ81m-Bo&list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&index=9)
* [demo5 \[EditRollup\] - read vs. write example](https://www.youtube.com/watch?v=hzF2MKhhd0k&list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&index=10)
* [\[Go\] statement 1 of 3](https://www.youtube.com/watch?v=3bAn5pFKS4I&list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&index=11)
* [\[Go\] statement 2 of 3 - the focus rollup](https://www.youtube.com/watch?v=_nT25ieZWzI&list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&index=12)
* [\[Go\] statement 3 of 3 - measure=on and the PID loop feature](https://www.youtube.com/watch?v=QZ6aqLtKPEg&list=PLHmnN_gEh0ZzK8KqOXfWqdVsEjuaqjpu8&index=13)

## Demos
Sample output from the demos is in the sample_output folder.
* Some of the demos also have a video - see list of videos above.
* Some of the demos come in two flavours, "DF", and "RAID".
* The "DF" version was done on an HDS AMS2100 subsystem.
 * The "DF" demos show the ivy functionality that is available on any vendor's equipment.
 * To adapt for another vendor's equipment, to specifically select some but not all LUNs we would need an equivalent to the the [LUN_discovery](https://github.com/Hitachi-Data-Systems/LUN_discovery) tool to decode the attributes of that vendor's LUNs, such as the equivalent of Hitachi LDEVs (logical devices).
* The "RAID" demos were performed on a Hitachi RAID family subsystem with a command device configured to one of the test hosts, and where the Hitachi proprietary "ivy command device connector" software was installed on the test host together with a subsystem license key.
  * The command device connector software is proprietary and is not part of the ivy open source project.  It is available only for authorized internal Hitachi lab use.
  * These RAID demos have more detailed subsystem configuration information retrieved from the command device, as well as subinterval-by-subinterval real-time performance data retrieved from the command device.
* The RAID demos are provided for two reasons
  1. To illustrate the full functionality of ivy.
  2. To show other vendors the functionality they can implement by developing their own connectors to their own subsystem products.
* Where there are both DF and RAID versions of the videos / demos, it is recommended to watch the DF version first and then the RAID version, as the RAID version generally builds on the DF version.
* Some demos which depend on having a command device connector such demos for performing dynamic feedback control on subsystem internal MP % busy or subsystem Parity Group % busy come only in one flavour.  Other vendors by developing their own connectors could develop similar functionality.

## Contributors

ian.vogelesang@hitachivantara.com, Kumaran Subramaniam, stephen.morgan@hitachivantara.com

## License

[Apache 2.0](http://www.apache.org/licenses/LICENSE-2.0)


