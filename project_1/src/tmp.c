/*
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/printk.h>
#include <sys/__assert.h>
#include <string.h>
#include <timing/timing.h>
#include "task_model.h"

/* size of stack area used by each thread */
#define STACKSIZE 1024

K_THREAD_STACK_ARRAY_DEFINE(tstacks, NUM_THREADS, STACKSIZE);

K_MUTEX_DEFINE(cond_mutex);
K_CONDVAR_DEFINE(cond_var);

k_tid_t t_id_array[NUM_THREADS];

struct k_thread my_thread_data[NUM_THREADS];
struct k_mutex my_mutex[NUM_MUTEXES];
struct k_sem my_sem[NUM_THREADS];
struct k_timer my_timer[NUM_THREADS];
bool done_flag = false;

struct k_mutex deadline_mutex[NUM_MUTEXES];
bool deadline_flags[NUM_THREADS] = {false};
int t_id[NUM_THREADS];

bool activate_flag = false;

static void reset_counter_timer_handler(struct k_timer *dummy);
static void activate_threads(struct k_timer *dummy);

K_TIMER_DEFINE(reset_counter_timer, reset_counter_timer_handler, NULL);
K_TIMER_DEFINE(activate_thread_timer, activate_threads, NULL);

static void reset_counter_timer_handler(struct k_timer *dummy)
{
	printk("Timer has expired after %d seconds\r\n", TOTAL_TIME);
    printk("ending all threads\r\n");
    done_flag = true; 
    /*
    for (int i = 0; i < NUM_THREADS; i++)
    {
        k_thread_abort(t_id_array[i]);
    }*/
}

/*
 * On "activate" command from shell
 */
static void activate_threads(struct k_timer *dummy)
{
    printk("Activate the timer\r\n");
    k_mutex_lock(&cond_mutex, K_FOREVER);
    activate_flag = true;
    if (NUM_THREADS > 1)
        k_condvar_broadcast(&cond_var);
    else
        k_condvar_signal(&cond_var);
    k_mutex_unlock(&cond_mutex);
    k_timer_start(&reset_counter_timer, K_MSEC(4000), K_NO_WAIT);
}


void print_task_s(struct task_s t)
{
    printk("Thread name is %s\r\n", t.t_name);
    printk("Priority is %d\r\n", t.priority);
    printk("Period is %d\r\n", t.period);
    for (int i = 0; i < 3; i++)
    {
        printk("loop_iter[%d] is %d\r\n", i, t.loop_iter[i]);
    }
    printk("Mutex id is %d\r\n", t.mutex_m);
}

void compute(void *p1, void *p2, void *p3)
{
    struct task_s t = *(struct task_s *)p1;
    int i = *(int *)p2;
    int period = t.period;
    int mutex_id = t.mutex_m;
    int count = 0;

    printk("Here %s\r\n", t.t_name);
    k_mutex_lock(&cond_mutex, K_FOREVER);
    while (activate_flag == false)
        k_condvar_wait(&cond_var, &cond_mutex, K_FOREVER);
    k_mutex_unlock(&cond_mutex);
    printk("Here2%s\r\n", t.t_name);
    while (done_flag == false)
    {
        k_timer_start(&my_timer[i], K_MSEC(period), K_NO_WAIT);
        //timing_start();
        //timing_t prev = timing_counter_get();
        /*  Computer 1  */
        count = t.loop_iter[0];
        while(count > 0)
        {
            count--;
        }
        /*  Mutex lock  */
        k_mutex_lock(&my_mutex[mutex_id], K_FOREVER);
        /*  Computer 2  */
        count = t.loop_iter[1];
        while(count > 0)
        {
            count--;
        }
        
        /*  Computer 3  */
        count = t.loop_iter[2];
        while(count > 0)
        {
            count--;
        }
        /*  Mutex unlock    */
        k_mutex_unlock(&my_mutex[mutex_id]);
        k_mutex_lock(&deadline_mutex[i], K_FOREVER);
        deadline_flags[i] = false;
        k_mutex_unlock(&deadline_mutex[i]);
        k_sem_take(&my_sem[i], K_FOREVER);
        //k_msleep(period);
        //timing_t curr = timing_counter_get();
        //timing_stop();
        
        /*
        int32_t ms = (int32_t)timing_cycles_to_ns(curr - prev) / 1000000;
        printk("Thread %s: Exe time= %d\r\n", t.t_name, ms);
        if (ms > period)
        {
            printk("Deadline is missed for %s: Exe time= %d, period= %d\r\n", t.t_name, 
                    ms, period);
        }
        else
        {
            k_msleep(period - ms);
        }
        */
        /*
         * Or stop the timer and put thread to sleep(curr + period)
         */
    }
}

void print_task_h(void)
{

    printk("Num of mutexes are %d\r\n", NUM_MUTEXES);
    printk("Num of threads are %d\r\n", NUM_THREADS);
    printk("Total time is %d\r\n", TOTAL_TIME);

    for (int i = 0; i < NUM_THREADS; i++)
    {
        print_task_s(threads[i]);
    }
}
void init_threads()
{
    for (int i = 0; i < NUM_THREADS; i++)
    {
        printk("Thread %d is created\r\n", i+1);
        t_id[i] = i;
        t_id_array[i] = k_thread_create(&my_thread_data[i], tstacks[i], STACKSIZE,
                compute, (void *)&threads[i], (void *)&t_id[i],
                NULL, threads[i].priority, 0, K_NO_WAIT);
        (void)k_thread_name_set(t_id_array[i], threads[i].t_name);
    }
}

void init_semaphores()
{
    for (int i = 0; i < NUM_THREADS; i++)
    {
        k_sem_init(&my_sem[i], 0, 1);
    }
}

void init_mutexes()
{
    for (int i = 0; i < NUM_MUTEXES; i++)
    {
        k_mutex_init(&my_mutex[i]);
        k_mutex_init(&deadline_mutex[i]);
    }
}

void timer_handler(struct k_timer *dummy)
{
    //int id = *(int *)dummy->user_data;
    int id = (int)dummy->user_data;
   
    k_mutex_lock(&deadline_mutex[id], K_FOREVER);
    if (deadline_flags[id] == true)
    {
        printk("Deadline is missed for %s\r\n", threads[id].t_name);
    }
    else
    {
        printk("Deadline is not missed for %s\r\n", threads[id].t_name);
    }
    deadline_flags[id] = true;
    k_mutex_unlock(&deadline_mutex[id]);
    k_sem_give(&my_sem[id]);
}
void init_timers()
{
    for (int i = 0; i < NUM_THREADS; i++)
    {
        t_id[i] = i;
        k_timer_init(&my_timer[i], timer_handler, NULL);
        //k_timer_user_data_set(&my_timer[i], (void *)&t_id[i]);
        k_timer_user_data_set(&my_timer[i], (void *)i);
    }
}
int main()
{
    timing_init();

    init_mutexes();
    init_semaphores();
    init_timers();
    init_threads();
    /*  Start the timer after shell command   */
    printk("Timer has started\r\n");
    k_timer_start(&activate_thread_timer, K_MSEC(3000), K_NO_WAIT);
    return 0;
}
