# A tiny educational kernel for Raspberry Pi 3
# test
A tiny kernel *incrementally built* for OS education.
Start with minimal, baremetal code. Then add kernel features in small doses.
Each experiment is self-contained and can run on both QEMU and real Raspberry Pi 3 hardware.

## Helpful documentation and tutorials
* [AArch64 cheatsheet](aarch64-cheatsheet.md)
  * [ARMv8 Instruction Set Overview](https://www.cs.princeton.edu/courses/archive/spr21/cos217/reading/ArmInstructionSetOverview.pdf)
  * [AArch64 Full Beginner's Assembly Tutorial](https://mariokartwii.com/armv8/)
  * [Official AArch64 ISA doc](https://developer.arm.com/documentation/ddi0602/latest/)
  * [Official AArch64 registers doc](https://developer.arm.com/documentation/ddi0601/latest/)
  * [Official AArch64/Aarch32 address translation doc](https://developer.arm.com/documentation/100940/latest/)
* [QEMU cheetsheet](qemu.md)
* [Using GDB to debug kernel](gdb.md)
* [Inspect kernel binary](inspect-kernel-binary.md)
* [Official Raspberry Pi boot sequence documentation](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#boot-sequence)

## Experiments
0. **Getting started**
      * [Sharpen your tools!](lesson00/rpi-os.md)
1. **Helloworld from baremetal**
      * [Power on + UART bring up](lesson01/rpi-os.md)
2. **Exception elevated**
      * [CPU initialization, exception levels](lesson02/rpi-os.md)
3. **Heartbeats on**
      * [Interrupt handling](lesson03/rpi-os.md)
4. **Process scheduler**
      * [A. Cooperative](lesson04a/rpi-os.md)
      * [B. Preemptive](lesson04b/rpi-os.md)
5. **A world of two lands**
      * [User processes and system calls](lesson05/rpi-os.md)
6. **Into virtual**
      * [Virtual memory management](lesson06/rpi-os.md)

## Acknowledgements
1. Sergey Matyukevich, Alexandre Venito et al. made the educational kernel and most of the resources in https://github.com/s-matyukevich/raspberry-pi-os
2. Felix Xiaozhu Lin et al. adapted the codebase to make it work on QEMU and improved the content in https://github.com/fxlin/p1-kernel
3. Millian Poquet adapted the codebase for his operating system design course in https://github.com/mpoquet/raspberry-pi-os-qemu
