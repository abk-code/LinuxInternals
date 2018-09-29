*********************
Linux Internals Notes
*********************

- How do we write kernel space services?
- How do they differ from user space code?

1. Statically appending a service and part of bZimage.
2. Dynamically creating a modules. - module programming

    Most of service devs, FS , protocols etc prefer this interface of adding
    kmods (dynamically).  Without knowing much existing of source layout and
    using specific set of rules , we can insert a new service into the kernel.

    kmod goes into ring 0. So no APIs, no syscalls, no library invocations.
    Include files of particular modules should be from kernel headers.
    Userspace are typically from /usr/include.

    The files being included are in the following directory:
    /lib/modules/4.12.0-041200-generic/build/include/linux/module.h 

    kmod subsystem is responsible for cleanup and maintain the modules.  Using
    comments is not mandatory. Module is just mechanism (pkg) to insert code
    into kernel space.

    Makefile is not standalone Makefile.
    Take mod.c (mod.o) and make a mod out of it.

    EXPORT_SYMBOL_GPL - can't be used by proprietary modules.

    sysfs - logical file system and you will find called "module".
    We are interested in actually module body.

Background
==========

    Insert the kernel services instead of a function.
    Drivers are virtualized services where applications interact with driver.

    DD in GPFS kernel is basically 2 pieces of code.

        - Interface to the application (kernel specific) Need to be aware of kernel architecture.
        - Hardware specific (device/bus specific). Need to be aware of how physically the h/w is connected to cpu. What protocol to use, directly connected ? via bus?
    
    Kernel specific driver can be implemented in 3 different ways
    These are 3 different models / ways of writing code.

    1. Character driver approach (Sync devices).
    2. Block driver approach (Storage devices, flash, usb).
    3. Network driver (Network, WiFi etc).

Char drivers
############### 

Implemeted on all the devices where data exchange happens
synchronously. For eg mouse, keyboard. Not used for bulk data transfer.

Create a device file and allow the application to interact with the driver
using file operations. Kernel is going to view the driver like FS (vfs_ops).
Common API can be used using file* operations.

    Device files - 2 types

    1. Character devices
    2. Block devices

Check /proc/devices - for the available major,minor.

sudo mknod /dev/veda_cdrv c 300 0
crw-r--r-- 1 root root 300, 0 Aug 26 13:49 /dev/veda_cdrv

Aquiring major number should be done dynamically. Otherwise while porting may fail. Probe /proc/devices list and whichever maj,min is free should be used to create a device file. This will be a requirement in driver portability.

Kernel APIs 
###########

 alloc_chrdev_region() is used to dynamically reserver <maj,min> touple
 availability and return. This is used instead of register_chardev_region().

 Kernel allows 'n' number of devices to share same major number.  For eg: hard
 drive can have logical partitions. So we will have one device file for each
 logical partition. /dev/sda1, /dev/sda2 etc - they refer to multiple logical
 partitions with same major number but different minor numbers. Minor numbers
 are identifications file files/inodes of device.

 Depending on the type of devices, Maj,Min can be categorized.
 There is a way to create and delete device node can be done using 'devfs'. 
 Udev is generic framework (suite of scripts) that provide creation of device
 files. These are basedon events from hotplug subsystems.

 #. class_create() and device_create() 

 #. device_destroy() and class_destroy()

 These are 4 udev apis that are used to dynamically create the device files.

 This will change the device tree and hotplug will generate an event.  udev
 will pickup this event generated from hotplug and creates a device file.  In
 the exit routine, device tree updated (deletion of virtual device) and hotplug
 subsystem will generate an event. udev will pickup this event and device file
 deletion happens.
 Check /sys/class/VIRTUAL - for device tree.
  
 hald - hald daemon maintains list of devices.  lshal - will list of devices
 that are currently plugged in.

What is kernel?
#################
    Kernel is set of routines that service the 2 generated events.
    Kernel code can be grouped in 2.
    1. Process events/context.
    2. Device/Interrupt events/context. Higher priority and cpu scheduler
    is disabled.

    Process context routine is of low priority and can be pre-empted.
    The example chr_drv_skel.c - the open is in process context.
    Single threaded driver.

Deep drive of char dd open.
###########################

open->sys_open->driver_open->char_dev_open()
(normal file) > vfs open > driver specific open.
When drivers are implemented, identify if it's concurrent i.e. available
to 'n' number of applications parallely. Policy has to be coded that way.

What driver open should consider during the implementation of service.

1. Verify the caller request and enforce driver policy (concurrent or not).
2. Verify if the target device (status) is ready or not.
    (Push to wakeup if needed).
3. Check for need of allocation of resoureces within the driver.
    (Data structures, Interrupt service routines, memory  etc)
4. Use-count to track the # of driver invocations if needed. May be useful
    during debugging drivers. "current" is pointer to current pcb. So use that
    to print pid of the caller etc. 

Deep drive of char dd close.
#############################

close->sys_close->driver_close->char_dev_release()
(normal file close) > vfs close > driver specific close.

1. Release of the resources
2. Reduce the use-count etc.
3. Undo basically all the operations done in open.

Devices can be accessed via ports or memory (mapped, graphics card etc).

Deep drive of char dd read.
###########################

read->sys_read->driver_read->char_dev_read() (normal file read) > vfs read> driver specific read.

1. Verify read arguments. Do validation on the read request.
2. Read data from the device.(can be from port, device memory, bus etc).
3. Whatever data device returns may need to be processed and then return to the application.
4. Transfer the data to app buffer. copy_to_user will copy transfer the data from kernel to user address space.
5. Return # of bytes transferred.

Deep drive of char dd write.
############################

write->sys_write->driver_write->char_dev_write()
(normal file write) > vfs write> driver specific write.

1. Verify / validate the write request.
2. Copy data from app/userspace to driver specific buffer.
3. Process data (may be format?)
4. Write data to hardware.
5. Increment ppos
6. Return nbytes (num of bytes that were written).

Many frame buffer drivers (char) - they implement seek/lseek. It's not possible to move beyond the device space.

Concurrency
##############

When drivers are concurrent, there is a question of parallel access.  Take
care of saftey of shared data/resources. So need to make driver routines
re-entrant. There will be locking functions, etc.

    When drivers and kernel services - when multiple applications request
    request same driver functions paralelly. When concurrent function
    guarantees saftey of shared  data, functions are called re-entrant.

    Even n++ is not guarateed to be atomic on all processors. Atomic
    instructions take memory lock and they are CPU specific. The atomic macros
    expand into CPU specific instructions. 

    2 main types of locking mechanism

    1. Wait type which inturn has 2 - Semaphore and Mutex
        If the lock is held and there is a contention on the lock by another
        thread, that thread is put to wait state.
    2. Poll type - Spinlocks.
        They keep spinning or keep polling without any wait. 
    
    mutex is more lightweight than semaphore.

    If there is a contention on a share resource between process context code
    of the driver and interrupt context code of the driver (for eg: Interrupt
    handler), use a special variant of spin_lock calls spin_lock_irqsave,
    spin_lock_irq, spin_lock_bh.

    Normal spin_lock need to be used only during contention between driver code
    within process context.


What is an *ioctl* 
###################

Used for special operations on device file.
Special operations are device and vendor specific. So can't pre-declare
the function pointers for such special operations.
Up to 255 different cases/functionalities for single IOCTL.

ioctl_list - will list all the available ioctls

For implementing ioctl on the device - do the following

1. Identify the special operations on the device.
2. For each special ops, create a request command. Also create a header file so that applications can use ioctl.
3. Implement all the cases (for request).

Use ioctl encoding macros to create unique ioctl-request,command touple.

There are 4 encoding macros that can be used.
To arrive at unique numbers easily we use the following macros

1. _IO(type, nr) 2. _IOW(type, nr, dataitem) the fourth item calculated using sizeof
3._IOR(type, nr, dataitem)
4. _IOWR(type, nr, dataitem)

#. inparam - application writes, driver reads. (_IOW,application writes)
#. outparm - application reads, driver writes. (_IOR,application reads)

aquire big_kernel_lock()
ioctl > sys_ioctl > do_ioctl ------------------> 
fops > ioctl > chr_drv_ioctl    unlock()

Because do_ioctl() aquires a big_kernel_lock(), ioctl is rendered single
threaded. There is no way 2 apps/threads will enter ioctl function paralelly.

    unlocked_ioctl = this is function pointer in the file_operations is used
    to make ioctl concurrent.

    /usr/src/linux-headers-4.12.0-041200/include/linux/capability.h - this
    defines various priviledge levels. They are called CAP* constants.
    capable() kernel macro - within ioctl implementation can check for the
    privilege levels being allowed or not.


Semaphores.
###########

    Semaphore value=0 means semaphore is locked.
        value = 1, means semaphore is unlocked.

    down_interruptible - will try to aquire the lock and if it doesn't get the
    lock,it will go into interruptible wait state. It will create a wait-queue
    and pcb for the calling context will be enqued to the wait-queue.

    up() - in the write will make the semaphore available. (Unlock call of
    semaphore).

    The wait-queues that are part of semaphore are FIFO.  This type of locking
    using semaphores is discouraged because can lead to confusion.

completion locks: 
Another way to let the callers block if the resource is not available.

Wait-queue
wait_event_interruptible - Will push the caller into interruptible wait state. Calling process's pcb is enqueued into wait-queue. wake_up_interruptible() is the routine to wake up. This will send a signal to all the PCBs that are in the wait-queue.

async notifications
#####################

Linux provides a way that driver delivers messages to applications asynchronously. 

#. Poll
#. select

    are 2 ways that applications can get the status of the device.

    async is not used in all applications. When IO is not high priority, then
    this method is used. This will require applicatons to register with driver
    for async messages to be delivered.

    Applications get SIGIO from the driver and they can handle with special
    signal handler for SIGIO.

    kill_fasync() of sending SIGIO can be put in Interrupt Servie Routine (ISR).

    poll_wait() is a kernel routine that puts calling application into wait
    state (in the wait queue).
 

Introducing delays in kernel routines.
#######################################

For kernel debugging, we may need to introduce delays.

#. Busy wait - Infinite loop and cpu is busy.

#. Yield cpu time - schedule() to relinquish the cpu time and is often used. this doesn't waste the cpu cycles.

#. Timeouts - How long we want to be in delay by providing expiry timer. Process resumes after timer expires.


Interrupts
###########

    Hardware devices are connected to interrupt controller via special line
    called IRQ. These devices are triggering interrupts using that IRQ line.
    X86 provides 16 lines. You can have up to 256 lines. 

    IRQ line assigned to particular to device need to be known to driver writer.
    IRQ descriptor table is linked list of IRQ descriptor.

