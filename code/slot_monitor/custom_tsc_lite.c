#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/smp.h>
#include <linux/atomic.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/percpu.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Timer module for multiple cores with robust round tracking");

#define MAX_CORES 1
#define TIMER_INTERVAL_NS 50000
#define SHM_PATH "/dev/shm/my-little-shared-memory"
#define SHARED_MEMORY_PATH "/dev/shm/my-little-shared-memory"
#define SHARED_MEMORY_SIZE 10000 // Define the size of shared memory segment
static struct hrtimer timers[MAX_CORES];
static int core_list[MAX_CORES] = {0, 1, 2, 3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29};
static int num_cores = 1; // Number of cores in the list
atomic64_t global_tsc = ATOMIC_INIT(0);
static int increment_value = 250; // Value to increment global_tsc by
static atomic64_t core_rounds[MAX_CORES];
static atomic64_t completed_round = ATOMIC_INIT(0);
static int slotstart = 0;
char buffer[100];
loff_t pos = 0;
// Export the global_tsc variable
EXPORT_SYMBOL(global_tsc);

// /proc entry
static struct proc_dir_entry *proc_entry;

static void *shm_addr;
static size_t shm_size;
static struct file *shm_file;

// Function to open the shared memory file
static int open_shared_memory(void)
{
    loff_t size;
    shm_file = filp_open(SHM_PATH, O_RDWR, 0);
    if (IS_ERR(shm_file)) {
        printk(KERN_ALERT "Failed to open shared memory file\n");
        return PTR_ERR(shm_file);
    }

    size = i_size_read(file_inode(shm_file));
    shm_size = (size_t)size;

    shm_addr = vmalloc(shm_size);
    if (!shm_addr) {
        filp_close(shm_file, NULL);
        printk(KERN_ALERT "Failed to allocate memory\n");
        return -ENOMEM;
    }
    return 0;
}

static void close_shared_memory(void)
{
    if (shm_addr) {
        vfree(shm_addr);
    }
    if (shm_file) {
        filp_close(shm_file, NULL);
    }
    printk(KERN_INFO "Closed shared memory file\n");
}

int read_shared_memory(void)
{
    if (!shm_addr) {
        return -EINVAL;
    }
    return kernel_read(shm_file, shm_addr, 1, 0);
}

int write_shared_memory(const void *buf, size_t count)
{
    if (!shm_addr) {
        return -EINVAL;
    }
    return kernel_write(shm_file, buf, count, 0);
}


static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
    int cpu = smp_processor_id();
    int core_index = cpu;
    int i;
    ktime_t now = ktime_get();

    if (core_index == -1) {
        pr_err("Timer callback called on unexpected CPU: %d\n", cpu);
        return HRTIMER_NORESTART;
    }

            //smp_rmb(); 
   read_shared_memory();
   buffer[0]=((char*)shm_addr)[0];
               // printk(KERN_INFO "Buffer: %s\n", buffer);
   switch (buffer[0]) {
      case 'S':
       write_shared_memory("P", 1);
                slotstart = 0;
                atomic64_add(increment_value, &global_tsc);
                break;

      case 'P':
         if (slotstart == 20) {
            write_shared_memory("F", 1);
            atomic64_add(increment_value / 100, &global_tsc);
            } else {
            atomic64_add(increment_value, &global_tsc);
            slotstart++;
              }
         break;

      case 'F':
           atomic64_add(increment_value / 100, &global_tsc);
           break;
      default:
          atomic64_add(increment_value, &global_tsc);
           break;
      }

    }
    hrtimer_forward(timer, now, ns_to_ktime(TIMER_INTERVAL_NS));
    return HRTIMER_RESTART;
}

static int global_tsc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%lld\n", atomic64_read(&global_tsc));
    return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, global_tsc_show, NULL);
}

static const struct proc_ops proc_fops = {
    .proc_open = proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
static int __init timer_module_init(void)
{
    int i;
    struct timespec64 start_time, end_time;
    uint64_t start_tsc, end_tsc;
    const unsigned int measurement_period_us = 1000000;  // 1 second
     int ret;   

    ktime_get_real_ts64(&start_time);
    start_tsc = rdtsc();
    msleep(measurement_period_us / 1000);
    ktime_get_real_ts64(&end_time);
    end_tsc = rdtsc();

    uint64_t elapsed_ns = (end_time.tv_sec - start_time.tv_sec) * 1000000000LL +
                          (end_time.tv_nsec - start_time.tv_nsec);
    uint64_t tsc_frequency = (end_tsc - start_tsc) * 1000000000LL / elapsed_ns;

    increment_value = (tsc_frequency - 200000000) / 20000;  // Increment per 100 ns
    //increment_value  = increment_value/4;
  //  increment_value += increment_value; // inflate the increment value
    printk(KERN_INFO "TSC frequency: %lld Hz, increment value: %d\n",
           tsc_frequency, increment_value);
        // Open shared memory
    ret = open_shared_memory();
    if (ret) {
        pr_err("Failed to initialize shared memory\n");
    }


    ktime_t ktime = ns_to_ktime(TIMER_INTERVAL_NS);

    proc_entry = proc_create("global_tsc", 0, NULL, &proc_fops);
    if (!proc_entry) {
        pr_err("Error creating /proc/global_tsc entry\n");
        return -ENOMEM;
    }

    for (i = 0; i < num_cores; i++) {
        int cpu = core_list[i];
        atomic64_set(&core_rounds[i], 0);  // Initialize to -1
        hrtimer_init(&timers[i], CLOCK_MONOTONIC, HRTIMER_MODE_ABS_HARD );
        timers[i].function = &timer_callback;
    //    set_cpus_allowed_ptr(current, cpumask_of(cpu));
        hrtimer_start(&timers[i], ktime, HRTIMER_MODE_ABS_HARD);
    }

    pr_info("Timer module initialized");
    return 0;
}

static void __exit timer_module_exit(void)
{
    int i;
    proc_remove(proc_entry);

    for (i = 0; i < num_cores; i++) {
        hrtimer_cancel(&timers[i]);
    }
    pr_info("Timer module unloaded. Final global_tsc value: %lld\n",
            atomic64_read(&global_tsc));
      // Close shared memory
    close_shared_memory();

}

module_init(timer_module_init);
module_exit(timer_module_exit);
