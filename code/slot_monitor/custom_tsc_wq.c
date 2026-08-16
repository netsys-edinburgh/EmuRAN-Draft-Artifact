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
#include <linux/workqueue.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ujjwal Pawar");
MODULE_DESCRIPTION("Chronos Timer Module");

#define MAX_CORES 8
#define TIMER_INTERVAL_NS 50000

static struct hrtimer timers[MAX_CORES];
static int core_list[MAX_CORES] = {0, 1, 2, 3};
static int num_cores = 4; // Number of cores in the list
atomic64_t global_tsc = ATOMIC_INIT(0);
static int increment_value = 250; // Value to increment global_tsc by

static atomic64_t core_rounds[MAX_CORES];
static atomic64_t completed_round = ATOMIC_INIT(0);

// Export the global_tsc variable
EXPORT_SYMBOL(global_tsc);

// /proc entry
static struct proc_dir_entry *proc_entry;

struct timer_work {
    struct work_struct work;
    int cpu;
    atomic_t pending;
};

static struct timer_work __percpu *timer_works;
static struct workqueue_struct __percpu **cpu_workqueues;

static void timer_work_func(struct work_struct *work)
{
    struct timer_work *tw;
    int cpu, core_index = -1;
    int i;
    uint64_t this_round, last_completed;

    if (!work) {
        pr_err("Work structure is NULL\n");
        return;
    }

    tw = container_of(work, struct timer_work, work);
    if (!tw) {
        pr_err("Failed to get timer_work structure\n");
        return;
    }

    cpu = tw->cpu;

    // Reset the pending flag
    atomic_set(&tw->pending, 0);

    // Find the core_index for this CPU
    for (i = 0; i < num_cores; i++) {
        if (core_list[i] == cpu) {
            core_index = i;
            break;
        }
    }

    if (core_index == -1) {
        pr_err("Work function called on unexpected CPU: %d\n", cpu);
        return;
    }

    this_round = atomic64_read(&core_rounds[core_index]);
    last_completed = atomic64_read(&completed_round);
    if(this_round == last_completed){
        this_round = atomic64_inc_return(&core_rounds[core_index]);
        //printk(KERN_INFO "[CPU %d] Round %lld completed last completed %lld\n", cpu, this_round, last_completed);

        bool all_completed = true;
        for (i = 0; i < num_cores; i++) {
            smp_rmb(); 
            if (atomic64_read(&core_rounds[i]) < this_round) {
                all_completed = false;
                break;
            }
        }
        if (all_completed) {
            smp_rmb(); 
            if (atomic64_cmpxchg(&completed_round, last_completed, this_round) == last_completed) {
                last_completed = atomic64_read(&completed_round);   
                atomic64_add(increment_value, &global_tsc);
                smp_wmb(); // Ensure global_tsc is updated before resetting core_rounds
                //printk(KERN_INFO "[CPU %d]Increment by : Round %lld completed last completed %lld\n", cpu, this_round, last_completed);
            }
        }
    }
    else{
        //printk(KERN_INFO "[CPU %d] skip\n", cpu);
    }
}

static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
    int cpu;
    ktime_t now;
    struct timer_work *tw;
    struct workqueue_struct *cpu_wq;

    if (!timer) {
        pr_err("Timer is NULL in timer_callback\n");
        return HRTIMER_NORESTART;
    }

    cpu = smp_processor_id();
    now = ktime_get();

    if (!timer_works || !cpu_workqueues) {
        pr_err("timer_works or cpu_workqueues is NULL in timer_callback\n");
        return HRTIMER_NORESTART;
    }

    tw = per_cpu_ptr(timer_works, cpu);
    if (!tw) {
        pr_err("Failed to get per-CPU timer_work for CPU %d\n", cpu);
        return HRTIMER_NORESTART;
    }

    cpu_wq = *per_cpu_ptr(cpu_workqueues, cpu);
    if (!cpu_wq) {
        pr_err("Failed to get per-CPU workqueue for CPU %d\n", cpu);
        return HRTIMER_NORESTART;
    }

    // Only queue work if there's no pending work
    if (atomic_cmpxchg(&tw->pending, 0, 1) == 0) {
        if (!queue_work(cpu_wq, &tw->work)) {
            atomic_set(&tw->pending, 0);  // Reset if queueing failed
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
    int i, ret = 0;
    struct timespec64 start_time, end_time;
    uint64_t start_tsc, end_tsc;
    const unsigned int measurement_period_us = 1000000;  // 1 second
    ktime_t ktime = ns_to_ktime(TIMER_INTERVAL_NS);

    timer_works = alloc_percpu(struct timer_work);
    if (!timer_works) {
        pr_err("Failed to allocate per-CPU timer_work structures\n");
        return -ENOMEM;
    }

    cpu_workqueues = alloc_percpu(struct workqueue_struct *);
    if (!cpu_workqueues) {
        pr_err("Failed to allocate per-CPU workqueues\n");
        free_percpu(timer_works);
        return -ENOMEM;
    }

    proc_entry = proc_create("global_tsc", 0, NULL, &proc_fops);
    if (!proc_entry) {
        pr_err("Error creating /proc/global_tsc entry\n");
        free_percpu(cpu_workqueues);
        free_percpu(timer_works);
        return -ENOMEM;
    }

    ktime_get_real_ts64(&start_time);
    start_tsc = rdtsc();
    msleep(measurement_period_us / 1000);
    ktime_get_real_ts64(&end_time);
    end_tsc = rdtsc();

    uint64_t elapsed_ns = (end_time.tv_sec - start_time.tv_sec) * 1000000000LL +
                          (end_time.tv_nsec - start_time.tv_nsec);
    uint64_t tsc_frequency = (end_tsc - start_tsc) * 1000000000LL / elapsed_ns;

    increment_value = tsc_frequency / 20000;  // Increment per 2 us
  //  increment_value += increment_value; // inflate the increment value
    printk(KERN_INFO "TSC frequency: %lld Hz, increment value: %d\n",
           tsc_frequency, increment_value);

    for (i = 0; i < num_cores; i++) {
        int cpu = core_list[i];
        struct timer_work *tw;
        struct workqueue_struct *cpu_wq;

        if (cpu < 0 || cpu >= num_possible_cpus()) {
            pr_err("Invalid CPU number: %d\n", cpu);
            ret = -EINVAL;
            goto err_cleanup;
        }

        atomic64_set(&core_rounds[i], 0);
        hrtimer_init(&timers[i], CLOCK_MONOTONIC, HRTIMER_MODE_ABS_PINNED_HARD );
        timers[i].function = &timer_callback;

        tw = per_cpu_ptr(timer_works, cpu);
        if (!tw) {
            pr_err("Failed to get per-CPU timer_work for CPU %d\n", cpu);
            ret = -EINVAL;
            goto err_cleanup;
        }

        tw->cpu = cpu;
        INIT_WORK(&tw->work, timer_work_func);
        atomic_set(&tw->pending, 0);

        cpu_wq = alloc_workqueue("timer_wq_cpu%d", WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1, cpu);
        if (!cpu_wq) {
            pr_err("Failed to create workqueue for CPU %d\n", cpu);
            ret = -ENOMEM;
            goto err_cleanup;
        }
        *per_cpu_ptr(cpu_workqueues, cpu) = cpu_wq;

        if (!set_cpus_allowed_ptr(current, cpumask_of(cpu))) {
            pr_warn("Failed to set CPU affinity for CPU %d\n", cpu);
        }

        hrtimer_start(&timers[i], ktime, HRTIMER_MODE_ABS_PINNED_HARD );
    }

    pr_info("Timer module initialized on specified cores\n");
    return 0;

err_cleanup:
    for (i = i - 1; i >= 0; i--) {
        int cpu = core_list[i];
        struct workqueue_struct *cpu_wq = *per_cpu_ptr(cpu_workqueues, cpu);
        if (cpu_wq) {
            destroy_workqueue(cpu_wq);
        }
        hrtimer_cancel(&timers[i]);
    }
    if (proc_entry) {
        proc_remove(proc_entry);
    }
    free_percpu(cpu_workqueues);
    free_percpu(timer_works);
    return ret;
}

static void __exit timer_module_exit(void)
{
    int i;

    if (timer_works && cpu_workqueues) {
        for (i = 0; i < num_cores; i++) {
            int cpu = core_list[i];
            struct timer_work *tw = per_cpu_ptr(timer_works, cpu);
            struct workqueue_struct *cpu_wq = *per_cpu_ptr(cpu_workqueues, cpu);
            
            hrtimer_cancel(&timers[i]);
            
            if (tw && cpu_wq) {
                cancel_work_sync(&tw->work);
                destroy_workqueue(cpu_wq);
            }
        }

        free_percpu(cpu_workqueues);
        free_percpu(timer_works);
    }

    if (proc_entry) {
        proc_remove(proc_entry);
    }

    pr_info("Timer module unloaded. Final global_tsc value: %lld\n",
            atomic64_read(&global_tsc));
}

module_init(timer_module_init);
module_exit(timer_module_exit);
