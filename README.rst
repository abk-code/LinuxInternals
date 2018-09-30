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

How interrupts work in Linux?
#############################

    do_irq() is function of process 0. Process 0 (kernel) will respond to
    interrupt. Process 1 (init) is responsible for creation and management of
    processes.

    When interrupts occur, do_irq() queries the interrupt controller and
    finds which IRQ line is triggered. Linux kernel configures do_irq() routine
    as a default response function for all external interrupts. 

    do_irq() is routine of process 0, which is responsible for allocation of
    interrupt stack and invoking appropriate ISR routines. This interrupt stack
    is of 2 types. If kernel is configured to use 8k stack, then there is no
    separate stack. If kernel is configured with 4K stack, then interrupt stack
    is allocated. With 4k, there will be performance hit. By default 8K is the
    size of kernel stack. In Embedded linux and if there is a memory constraint
    4K may be needed.


#. Find interrupt request line on which interrupt signal was triggered (by querying interrupt controller).

#. Lookup IRQ descriptor table for addresses of registered interrupt service routines.

#. Invoke registered ISRs.

#. Enable IRQ line.

#. Execute other priority work. 
    
#. Invoke process scheduler. Until step6, process scheduler is disabled.

    Interrupt latency is total time spent by the system in response to
    interrupt. If interrupt latency is high, applications performance may be
    impacted. High priority will starve since system is spending more time in
    interrupts. One device may block other devices. When timer interrupt goes
    off, other interrupts are disabled.

Interrupt latency == Total amount of time system spends on the interrupt.

Factors contributing to interrupt latency.
******************************************

#. H/W latency: Amount of time a processor is taking ack interrupt and invoke ISR.
#. Kernel latency:  In Linux/Windows/or when process 0 is responding, how much time process 0 takes to start an ISR. This is called kernel latency.
#.  ISR Latency: When ISR routine invoked, how much time it takes. ISR are usually referred as INterrupt handlers.
#. Soft interrupt latency (bottom half).
#. Scheduler latency.
    - Check if any high pri tasks waiting in queue.
    - Signal handlers for the signals pending.
    - Giving cpu to high pri task.

RTOS has fixed time latency for interrupts. GPOS do not have fixed time latency.


For NIC, both reception and transmission of pkt will trigger an interrupt.

pseudo-code for interrupt handler for NIC 
###########################################

#. reception of pkt (on network device).
#. Allocate buffer to hold the packet.
#. Copy from NIC buffer to kernel or vice-versa.
#. Process the packet, specially phsyical header.
#. Queue pkt handing over to upper protocol layers.

While designing ISRs the following issues are to be considered.
###############################################################

Don't while writing ISR routine.
********************************

    #. Avoid calling dynamic memory allocation routines.
    #. Avoid transferring data (synchronous) between 2 buffer blocks.
    #. Avoid contending for access on global data structures because you need   to go through locks.
    #. Avoid operations on user space addresses.
    #. Avoid call to scheduler. While ISR is running scheduler is disabled. Hence a call to scheduler may result in deadlock and need to be avoided.
    #. Avoid calls to operations which are non-atomic.

Do's while writing ISR routine.
*******************************

    #. Use pre-allocated buffers. (skb buffer in network drivers).
    #. Consider using per CPU data wherever needed.
    #. Consider using DMA whenever data needs to be transferred between device and memory.
    #. Identify non-critical work and use appropriate deferred routines to  execute them when system is idle or other scheduled time.

If you are doing anything h/w specific within ISR is critical.
Anything other than this is non-critical from Interrupt perspective.

Bottom Halves
*************

Linux has bottom halves or RTOS call BH as "deferred functions". Basically step 3 and 4 (processing and enque of the packets to higher layers) can be deferred and run. It need not be run with interrupts disabled on the cpu.

.. code:: bash

   ISR 
   {
     schedule_bh(func);
   }

   func()                //Soft IRQ. 
   {
     body;
   }
    
Soft IRQ == Piece of code that runs with IRQ enabled but scheduler disabled. This is run in interrupt context. ISR terminates, IRQ enabled, scheduler enabled - Bottom half. Bottom half can be of 2 types. They can be running soft interrupts context.They are called Soft IRQs or Work queue.

SOFTIRQs, TASKLET & WORKQUEUE
#############################

Soft IRQ, tasklet and workqueue are 3 ways to do deffered execution of a routine while servicing an interrupt. Non critical work can be scheduled.

Linux 2.6 bottom halves.

Soft IRQ: 
*********

It's a way where routine can be scheduled for deffered execution. It's available for static drivers and not dynamic drivers.  This is a bottom half which is runs with interrupts enabled, but pre-emption disabled. Refer include/linux/interrupt.h. All softirqs are instances of softirq_action structure. Maximum of 32 soft irqs.

Execution of softirq
**********************

#. There are maximum 32 allowed softirqs. There is a per-cpu softirq list.  If ISR is running on cpu1, then softirq will run on cpu1 and so on.  When   raise_softirq() is called, specified softirq instance is enqued to the list of pending softirqs (per cpu). Both top-half and bottom-half need to be on same cpu otherwise there will be cache-line problem.

#. Pending softirq list is cleared (run one by one) by do_irq() routine immediately AFTER isr is terminated with interrupt lines enabled. softirq run with pre-emption disabled but IRQ enabled. If interrupt goes off during softirq, then do_irq() is run as re-entrant and softirq will get pre-empted. Softirq pre-emption will happen for nested interrupts.

#. Softirqs can be run in 3 different ways. 

 - softirq can run in the context of do_irq(). 
 - softirq can also run in the after spin_unlock_bh().
        do_irq() can't run softirq, if the spin_lock_bh() is held.

 - Softirq may execute in the context of ksoftirqd (Per cpu kernel
        thread). This is in process context of ksoftirqd - daemon. 

.. code:: bash

    static int __init() my_init()
    {
        open_softirq(mysoftirq, func)
        request_irq()
    }

    ISR {
        raise_softirq(mysoftirq);
    }

    func {
        ..
        ..
    } 

Another example of softirq.
***************************

Both need to run on same cpu.

.. code:: bash

    func()
    {
        /* write to a list */
        while(1)
            raise_softirq(mysoftirq); <<< This is legal, though bad.
    }

    read() {
        spin_lock_bh()
        /* read from list */
        spin_unlock_bh();
    }


Linux kernel allows softirq routine to reschedule itself within itself. It's possible to do raise_softirq() within softirq(). ksoftirqd - will also clear the pending softirqs when it gets the cpu slice.


Question1: Why do we allow to reschedule softirq?
**************************************************

Consider 2 cpus as below

.. code:: bash

 cpu1.                          cpu2:
 read()                         func() 
 {                              {
    spin_lock_bh()                  spin_lock_bh()
    /*read from list */             /*write to the list */
    spin_unlock_bh()                spin_unlock_bh()
 }                              }

Suppose cpu1 has aquired the lock and is reading from the list, and interrupt is delivered to cpu2. cpu2 has some bh (softirq) func() and it tries to aquire the lock. The spin_lock_bh() in func() is going to spin on the cpu2 for lock to become available. This cpu2 is in interrupt contextand going to waste cpu cycles which is not correct. Hence rescheduling of softirq is permitted from bh.

Rescheduling bh is required to relinquish cpu from within bottom half whenc ritical resource (like lock) is rquired for bottom half execution is not available. 

Question2: Will softirq (bh) can run on 2 cpus run paralely?
**************************************************************

Yes, because the interrupt can be delivered to different cpus.  Use appropriate mutual exclusion locks while writing softirqs. 


Limitations of softirqs.
*************************

#. Softirqs are concurrent, i.e. same softirq could run on 'n' cpus paralelly.
#. While implementing softirq code using mutual exclusion locks is mandatory wherever needed.
#. Using locks in interrupt context code will result in variable interrupt latency. 

These are the reasons why softirqs are not available for modules. Concurrent execution in bottom half, only then consider softirqs. 

Tasklets.
*********

    It's a softirq where tasklet is guarrateed to be serial. Because of serial
    execution,there is no need for locks. Tasklets are dyanmic softirqs that
    can be used from within module drivers without concurrency.  They are
    always executed serially.

    Tasklets = Dyanmic softirqs - concurrency. 
 

    #. Built on top of softirqs
    #. Represented by 2 softirqs, tasklet_struct structure
    #. Created both statically and dynamically
    #. Less restrictive sync routines.

Steps involved in tasklets.
***************************

#. Declare tasklet

    .. code:: bash

         DECLARE_TASKLET(name,func, data)

          OR

         struct tasklet_struct mytasklet;
         tasklet_init(mytasklet, func, data);

#. Implement BH routine

     .. code:: bash
          
             void func(unsigned long data);

#. Schedule tasklet

     .. code:: bash

            tasklet_schedule(&mytasklet); <<< Low priority

            OR

            tasklet_hi_schedule(&mytasklet); <<< High priority - use if you want high.
    
#. When tasklets are run? - Execution policy.  Tasklets are executed using same policy that is applied for softirqs since interrupt subystem of kernel views a tasklet either instance of type high_softirq or tasklet_softirq.

    Interrupt subsystem guarrantees the following with regards to execution of
    tasklet. 

#. Tasklets --- multithreaded analogue of BHs. (From interrupt.h file).

   Main feature differing them of generic softirqs: tasklet is running only on one CPU simultaneously. Main feature differing them of BHs: different tasklets may be run simultaneously on different CPUs.

Properties:
************ 
   * If tasklet_schedule() is called, then tasklet is guaranteed
     to be executed on some cpu at least once after this.
   * If the tasklet is already scheduled, but its execution is still not
     started, it will be executed only once.
   * If this tasklet is already running on another CPU (or schedule is called
     from tasklet itself), it is rescheduled for later.
   * Tasklet is strictly serialized wrt itself, but not
     wrt another tasklets. If client needs some intertask synchronization,
     he makes it with spinlocks.
    
    2 different tasklets between 2 different drivers can be run parallel and
    there may be need for synchronization. Tasklets are strictly serialized but
    not wrt to other tasklets. Inside a tasklet if you are accessing global
    data structure, then locking is required. 

    *They can be run in 3 different ways.*

        3.1. softirq can run in the context of do_irq().

        3.2. softirq can also run in the after spin_unlock_bh().
        do_irq() can't run softirq, if the spin_lock_bh() is held.

        3.3. Softirq may execute in the context of ksoftirqd (Per cpu kernel
        thread). This is in process context of ksoftirqd - daemon. 

Workqueues
***********
    - Workqueues are 2.6 kernel only. Tasklets and Softirqs were there in 2.0.
    - Workqueue are instances of work structure. 

Timer bottomhalf 
******************
        - Executes whenever assigned timeout elapses.


Memory management in Linux
###########################

There are 2 source directories for memory management.

#. /usr/src/linux/mm - (Memory manager)
#. /usr/src/linux/arch/i386/mm - (Memory initializer, runs at boottime) ppc/mm, mpis/mm, alpha/mm - Architecture specific source code in the kernel.

Processor views memory in real-mode as single array. 
Processor views memory in protected-mode as set of arrays/vectors. Each frame size is 4K.  The view changes depending on the type of mode.

When does the shift from real to protected mode happens?

*Different types of memory allocation algorithms.*
**************************************************

1. Page allocator : THis is primary memory allocator and source of memory.

2. slab allocator : Odd size memory allocation and always returns physically contiguous memory.

 .. code:: bash

        kmalloc(),returns address and kfree(), frees addess.
        Can be called from driver or kernel service.
        /proc/slabinfo - can view the slab allocation details.

 Slab allocation allows private cache (per driver/kernel service). Default allocators 
 could be called from the cache list.

 .. code:: bash

        kmem_cache_create()
            kmem_cache_alloc() 
            kmem_cache_free()
        kmem_cache_destroy()

3. Cache allocator : Need to allocate data structures frequently. Reserver some pages as cache of objects so that drivers and FS can pre-create the  objs and use them. They are not available to application directly.

4. Fragmented memory allocator: Odd size requests and source of allocation is from various fragments. It's used when allocation request size is large and not called from applications directly.

5. Boot memory allocators: startup drivers, BSP drivers - they aquire memory using boot mem allocator.

 .. code:: bash

     include <linux/mempool.h>
     pcb_task_struct_t, cdev etc - most of them are pre-allocated in pools.
    
Cache is reserving pages which will be used for allocating memory later. Memory pool, create a cache and allocate instances. Mem pool is based on cache. Scsi, usb drivers, network drivers etc which are frequenty used structures that are used for data transfer are created using memory pool.

Slab layer allows kernel services to create memory pools that can be used for pre-allocation of specific objects.

    1. Create a memory pool.
    2. Aquiring an object from the pool.  (mempool_alloc)
    3. Release the object (mempool_free)
    4. Destroy the pool (mempool_destroy).
    5. Destroy the cache (AFTER the pool destruction) - kmem_cache_destroy

FS, protocol stack typically use this facility. Any request beyond > 128K - kmalloc() may fail. That is because 128K physically contigous block for kmalloc.  
