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

        - Interface to the application (kernel specific) Need to be aware of
          kernel architecture.

        - Hardware specific (device/bus specific). Need to be aware of how
          physically the h/w is connected to cpu. What protocol to use,
          directly connected ? via bus?
    
    Kernel specific driver can be implemented in 3 different ways These are 3
    different models / ways of writing code.

    1. Character driver approach (Sync devices).
    2. Block driver approach (Storage devices, flash, usb).
    3. Network driver (Network, WiFi etc).

Char drivers
############### 

Implemeted on all the devices where data exchange happens synchronously. For eg
mouse, keyboard. Not used for bulk data transfer.

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

mmap, munmap
############

    Map IO cache buffer (from filesystem) into memory. 
    If application uses MAP_ANONYMOUS, then filedes is not considered and
    memory is mapped. Can be used on device files. 
    open(/dev/file1) and then mmap().

    (low memory zone, normal zone, highmem zone)
    Implementing mmap callback in a character driver.
    Anything above 896M is called high memory zone.
    Process address space is also in normal zone (< 896M)

    1. Each process in the user space aquires set of pages into which process
    code, data and stack segments are mapped.

    2. Process pcb in the kernel space carries details about pages allocated to
    the process and segments to wihch they are mapped.
    (Refer mm18)

    For each process: (application)
        - code segments go into few pages and need not be contiguous. Each vm_area_struct correspond to each contiguous segment.

        - stack segments go into few pages

        - data segments go into few pages. Each vm_area_struct correspond to each contiguous segment.

    /proc/pid/maps will give each of these vm_area_structs (block).
    block = set of pages

How linux tracks mappings?
****************************

    1. mm_struct_instance contains reference to a list of virtual memory blocks
    (vm_area_struct) that are mapped to application's code/data/stack segment.

    2. Each instance of vm_area_struct represents one block of the process
    address space.

    3. mm_struct also carries reference to the process page table with valid
    page table entries (PTEs). For protection reasons, we are mapping into
    different segments.

When mmap is called, what happens?
************************************

    1. Application's mmap request on a file invokes do_mmap kernel routine via
    sys_mmap.

    2. do_mmap() allocates new instance of struct vm_area_struct & fills it with
    details of the new block attributes based on the arguments passed to mmap()
    routine by the application. malloc() calls do_mmap(). This is a key routine.

    3.do_mmap() invokes appropriate mmap support routine assigned to the file
    operations (fops) instance for eg: fops_mmap().

How the driver's mmap works?
*****************************

    1. Identify physical memory region (frames) that is required to be mapped.

    2. Map physical memory region to kernel's logical address space which is
    page+offset. [Physical memory == frame+offset].

    3. Set page reservation indicating that I/O activity should be disabled.

    4. Map physical address region to VMA instance.

Direct Memory Access (DMA) - memory allocation and management.
##############################################################

    Bus specific mode etc - they require dma allocations.

    Address translation using page tables. Newer Intel PAE extentions provide 36 bit addresses.  There are 3 patches    availble which breakup the virtual address space.

    1. Each process carries it's own page table allocated by kernel at the
    process load time.

    2. Page tables contain entries mapping page to valid physical frame.

        .. code:: bash

            Valid   virtual-page    modified    protection page-frame 
            1   140     1            RW     31
            1   20      0            R X        38

    
         Each entry is called page table entry (PTE).

    3. Processor's MMU (memory mgmt unit) at runtime looks up into page-table
    to translate a logical address to physical address. And reference of
    page-table is loaded into processor's register on every context switch.  

Multi level paging: where it's used?
####################################

Linux uses 3-level paging on desktop/server arch and 4-level paging on
NUMA/Enterprise architectures.

#. Overhead / Limitation with page tables.
    1. As the number of processes increase kernel needs to set aside around 3MB
    of physical memory for each process to hold PTEs.

    2. Page TAble Indirection is one approach where there is no wastage of
    memory. Swap them out to disk when not required and swap in when required.
    This approach is implemented in many ways. 2-level, 3-level paging etc are
    different ways to manage page tables. The objective here is to enable page
    tables dynamically extensible. 

    3. Processor needs to spend 'n' amount of cycles looking up page-tables
    before the actual operations on the memory can be executed.

#. How to optimize
    1. To optimize translation time, cpus provide specific cpu local buffers
    called Translation Lookaside Buffer (TLBs).  

    2. Processor < L1 < L2 < L3 < Memory  - 
    Processor is fastest access.

    3. TLBs can be managed in 2 ways
    
              - Kernel / software managed: In s/w managed mode each TLB missed
                event triggers an exception which inturn is handled by kernel
                by updating TLB entries from page-tables.
    
              - Hardware managed: In h/w managed mode each TLB miss event makes
                processor jump into physical page table in memory and load
                appropriate  entries into TLB.

    4. Processors also provide high speed data instructions caches to optimize
    program execution by mirroring program's data/instructions to cache.

Memory Mapped IO (MMIO) Vs Port Mapped IO (PIO)
###############################################

    - PPIO:x86 They have port mapped IO.Separate set of instructions to read. In PC platform have 16 bit addressing.

    - MMIO:risk (ARM etc) have memory mapped IO. While memory bus is used for IO as well.

/proc/iomem - Details of memory mapped IO. Those devices that are memory mapped.
/proc/ioports - Details of port mapped IO.

Accessing port mapped devices from user space.
**********************************************

    1. Port mapped addresses can be accessed by applications of Linux and kernel space services (modules). 

    2. Apps can access port mapped devices using either of the following ways.
        2.1] RUnning as root application using ioperm() routines.
        2.2] Using special device file using /dev/<..> - Will be disabled in
        future. 


Kernel space port mapped device access.
**********************************************

    1. From kernel - you do request_region() and seek permission. Modules
    invoke request_region() routine to check for port access permissions and
    aquire port resource.

    2. Use kernel mode in/out family of functions for reads and writes on the
    IO port.

        3. Kernel APIs are* in[bwl], out[bwl] and string variants ins[bwl] and outs[bwl]

    linux/ioport.h and kernel/resource.c - Audit files.

What is a bus.
******************

    Bus = DAta path from transferring data between cpu and devices.

    3 kinds of buses

    1. Address bus  = Used to generate addresses.

    2. Data bus (parallel lines to carry data) = Used to transfer data.

    3. Control bus. = Used to transfer control signals. 

    IO bus is connection between IO devices and CPU.

Summary of PPIO
***************

PPIO can be done in 2 ways
    From user space : 
        ioperm/in/out family of functions
        /dev/port - using driver's read write operations

    From kernel space:
        Using in/out family with request_region()
        Using ioport_map() - New way.

    No way to do memory mapped IO from userspace.

Memory alignment.
*****************

    Refer Documentation/unaligned-memory-access.txt in kernel source.  
    Memory alignment is storing data value at the address which is evenly
    divisible by it's size. Unaligned data causes exceptions on some
    architectures and is not supported on some archs. Intel x86 throws an
    exception when un-aligned data operation is initiated.

Block Device Drivers
######################

    Bulk and mass storage - they use block driver model for asynchronous mode.

    Buffer/page cache - cache requested file's data is found. I/O request from
    the application will be synchronous if the data is found in buffer cache.
    If the I/O request doesn't find the data in the buffer/page cache, the
    application is put to wait (block) and a request is made to block layer.

    Block driver is per physical disk.
    There are 2 types of block driver.

            1. Logical block drivers - RAMDISK, emulation without physical device.

        2. Physical block drivers with I/O scheduler. REquest queue will be
        present between block layer and physical block driver that actually
        talks to the disk.
        
    - vfs identifies block device as an instance of gendisk for block driver.
    - vfs identifies char device as an instance of cdev for char device driver.

    Number of bios involved in a request == number of contiguous sectors If
    whole file falls physically in continuous sectors, then there will be just
    one bio and num of bio_vec will be N number physically contiguous sectors.

PCI and network drivers.
#########################

PCI is typically found in PCs but not found in SoCs or embedded.
2 types of bridges exist.

    - PCI to PCI bridge
    - PCI to ISA bridge (PCI2eISA, PCI2PCMCIA etc).
    
    Each PCI device carries 256 bytes of configuration information with some
    registers as part of an EEPROM/FIRMWARE. Inside device structure, configuration data is stored.

        First 64 bytes region carries a general header (structure) that
        provides details about device, class, vendor, status register, command
        register, port and memory information and interrupt line number (IRQ
        number). For all devices this header is common/required. REst will be
        device specific registers.

    - For each PCI bus, there will be a struct called pci_bus
    - For each PCI device, there will be struct called pci_device

        .. code:: bash

         lspci -v will show the details of the pci.
         00:02.0 VGA compatible controller
         bus-id:device-id.function-id. Each PCI device can provide upto 7 functions.

Steps involved in interaction with PCI devices
***********************************************
    1. Register with PCI bios.
    2. Enable the device. (Push to wakeup state).
    3. Probe device configuration. (IRQs, IO ports/memory regions).
    4. Allocate resources.
    5. Communicate with the device.

Implementation of PCI NIC drivers (PCI and network code is new).
*****************************************************************

1. Register the driver with PCI subsystem (PCI bios).

.. code:: bash
             
         pci_register_driver() - This function registers driver with PCI bios.
    
         2. static struct pci_driver nic_driver {
        .name = "nicdriver",
        .id_table = nic_idtable,
        .probe = nic_probe,
        .remove = nic_remove,
    };

    static int __init nic_init(void)
    {
        return pci_register_driver(&nic_driver);
    }

    static int __exit nic_cleanup(void)
    {
        pci_unregister_driver(&nic_driver);
    }

PCI bios invokes the probe routine of the registered driver when the physical device specified in found. Probe is callback() and called when device is physically there.
What we do in the probe function?

Major steps in NIC driver.
*****************************

    1. Carry out device initialization operations.
        -  Enable the device using pci_enable_device()
        -  Enable bus master ring (if available).
            Bus master ring means, presence of DMA functionality. 
            pci_set_master()
        -  Extract IO/Memory information. 
            - pci_resource_start()
            - pci_resource_len()
            - pci_request_regions()
        
        Verify if device's IO/Memory region is in use. 2 drivers should not use
        same registers.  
        Perform memory / IO mapping
        pci_iomap, pci_map() etc.

    2. Register the driver with appropriate subsystem. 

        Incase of network driver register it with common device layer.

        *Char driver register with VFS
        Block driver registers with block layer and VFS (Optional)
        Network driver registers with Common Netdevice Layer. Int n/w
        terminology is called DLL. this comprises code for Mac layer and
        physical layer.*

   
    3. Network drivers are not enumerated as device files.
    (no VFS registration, no maj,min numbers).

    4. Net drivers register as an instance of net_device with common net device
    layer (CNDL)

    5. ifconfig is a userspace tool that shows all registered net_device
    objects.


Steps of network driver registration.
*************************************

    Physical layer protocols define how frame needs to be created and
    transmitted. (Eth, Wireless, FDDI etc)

    1.  Wireless lan device driver, Eth driver etc  will register as net_device
    irrespective of the MAC Protocol it's supporting. The network driver is
    independent of physical layer protocol.

        - Create an instance of type struct net_device. (alloc_netdev)
          Initialize the fields with device specific configuration. net_device
          instance uses a void pointer to refer to driver specific private
          structure which usually contains driver configuration info. (includes
          device usage statistics)

        - Fill config data of network device to net_device fields.      (pdev,
          iobase, reqs_len, irq etc)

        - Initialize driver interface functions to function pointers of
          net_device instance. Register net_device with CNDL. (Mandatory step).

        ifconfig up eth0 << open will get from netdev_ops (structure that
        contains function pointers).

    2. Register ISR 

    3. Allocate transmission and reception DMA buffers within nic_open().

Important notes on addresses, DMA issues (bus,cpu,io)
*****************************************************
    
    1. p = kmalloc() - which is page + offset (logical address).  CPU can
       translate this into frame+offset (physical address) using TLB,
       translation. But such allocation within NIC driver,  can't translate
       this to physical address.

    2. Whenever we need DMA, we need 2 addresses for the memory. One is
       physical address and one is logical address. In X86 architecture bus
       address, IO address and cpu addresses are same. Some archs like ARM etc,
       such addresses do not match. Hence there is another chip called I/O MMU.

    In PCI, DMA controller (bus master) is built-in. Pre-PCI, like ISA there
    used to be another DMA controller. DMA is capability of device to take over
    the address/data bus on the memory controller and drive transactions on it.

DMA allocations
****************
    1. PHysical and logical addresses of the buffer.
    2. Cache save buffers.

    Devices can't use logical addresses which usually are return types of
    kernel memory allocation routines (kmalloc).

    - __pa(page+offset) will return  __va(frame+offset)
    - __va(frame+offset) will return __pa(page+offset) 
    
        These can only be used in kernel space.

    Logical addresses of DMA buffer need to be translated to physical
    equivalents and configure the device controller with physical address.  In
    few cases there could be probable side effects when DMA memory is cached in
    CPU caches

