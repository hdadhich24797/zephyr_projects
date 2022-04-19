/*
 * CSE522 Real-time Embedded Systems
 * Spring 2022 Assignment 1CSE522 Real-time Embedded Systems
 * Spring 2022 Assignment 1
 *
 * Done by
 * Name: Hrishikesh Sunil Dadhich
 * ASU ID: 1222306930
 */

#include <device.h>
#include <drivers/gpio.h>
#include <shell/shell.h>
#include <string.h>
#include <sys/__assert.h>
#include <sys/printk.h>
#include <timing/timing.h>
#include <zephyr.h>

#include "task_model.h"

/* size of stack area used by each thread */
#define STACKSIZE 1024

bool g_activate_flag    = false;
bool g_done_flag        = false;
bool g_compute_flags[NUM_THREADS];

K_CONDVAR_DEFINE(g_cond_var);
K_MUTEX_DEFINE(g_cond_mutex);
K_THREAD_STACK_ARRAY_DEFINE(tstacks, NUM_THREADS, STACKSIZE);

k_tid_t         t_id_array[NUM_THREADS];

struct k_thread g_thread_array[NUM_THREADS];
struct k_mutex  g_mutex_array[NUM_MUTEXES];
struct k_sem    g_sem_array[NUM_THREADS];
struct k_timer  g_deadline_timer_array[NUM_THREADS];

void execution_timer_handler(struct k_timer *dummy);
void activate_threads(const struct shell *shell, size_t argc, char **argv);


K_TIMER_DEFINE(execution_timer, execution_timer_handler, NULL);
SHELL_CMD_REGISTER(activate, NULL, "Activate all threads", activate_threads);

/*
 *   Description:
 *   Funtion callback gets called after receiving the "activate" command from the user.
 *   The function will start the execution of all the threads.
 */
void activate_threads(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    /*  Start the execution timer of TOTAL_TIME */
    k_timer_start(&execution_timer, K_MSEC(TOTAL_TIME), K_NO_WAIT);

    /*  Broadcast to all threads to start the execution */
    k_mutex_lock(&g_cond_mutex, K_FOREVER);
    g_activate_flag = true;
    if (NUM_THREADS > 1)
        k_condvar_broadcast(&g_cond_var);
    else
        k_condvar_signal(&g_cond_var);
    k_mutex_unlock(&g_cond_mutex);
}

/*
 *   Description:
 *   Funtion callback gets called after receiving TOTAL_TIME has elapsed
 */
void execution_timer_handler(struct k_timer *dummy)
{
    ARG_UNUSED(dummy);
    printk("End of execution after %dms time\r\n", TOTAL_TIME);
    g_done_flag = true;
}

/*
 *   Description:
 *   Funtion to print user define task_s structure
 *
 *   parameters:
 *   @t = structure of a task_s
 */
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

/*
 *   Description:
 *   Funtion to print all user defined task_s structures
 */
void print_task_h(void)
{
    printk("*********************\r\n");
    printk("Num of mutexes are %d\r\n", NUM_MUTEXES);
    printk("Num of threads are %d\r\n", NUM_THREADS);
    printk("Total time is %d\r\n", TOTAL_TIME);
    printk("*********************\r\n");

    for (int i = 0; i < NUM_THREADS; i++)
    {
        printk("*********************\r\n");
        print_task_s(threads[i]);
    }
}

/*
 *   Description:
 *   Thread compute function invoked by each thread
 *   Each thread performs <compute_1> <mutex_lock> <compute_2> <mutex_unlock> <compute_3> <wait>
 *
 *   parameters:
 *   @p1 = pointer to a task_s struct object
 *   @p2 = thread id
 */
void compute(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p3);
    struct task_s t = *(struct task_s *)p1;
    int i = (int)p2;

    /*  period of a task    */
    int period = t.period;
    /*  Mutex id of a task  */
    int mutex_id = t.mutex_m;
    volatile uint64_t count = 0;

    /*  Conditional variable
     *  Wait till "activate" command from the user   */
    k_mutex_lock(&g_cond_mutex, K_FOREVER);
    while (g_activate_flag == false)
        k_condvar_wait(&g_cond_var, &g_cond_mutex, K_FOREVER);
    k_mutex_unlock(&g_cond_mutex);
    while (g_done_flag == false)
    {

        /*  Start the deadline timer before the start of execution  */
        k_timer_start(&g_deadline_timer_array[i], K_MSEC(period), K_NO_WAIT);
        /*  Computer 1  */
        count = (uint64_t)t.loop_iter[0];
        while(count > 0)
        {
            count--;
        }
        /*  Mutex lock  */
        k_mutex_lock(&g_mutex_array[mutex_id], K_FOREVER);
        /*  Computer 2  */
        count = (uint64_t)t.loop_iter[1];
        while(count > 0)
        {
            count--;
        }
        /*  Mutex unlock    */
        k_mutex_unlock(&g_mutex_array[mutex_id]);
        /*  Computer 3  */
        count = (uint64_t)t.loop_iter[2];
        while(count > 0)
        {
            count--;
        }
        /*  Set compute flag to true    */
        g_compute_flags[i] = true;
        /*  Wait for period to complet  */
        k_sem_take(&g_sem_array[i], K_FOREVER);
        /*  Set compute flag to default value   */
        g_compute_flags[i] = false;
    }
}

/*
 *   Description:
 *   Funtion to init all threads
 */
void init_threads(void)
{
    for (int i = 0; i < NUM_THREADS; i++)
    {
        printk("Thread %d is created\r\n", i+1);
        t_id_array[i] = k_thread_create(&g_thread_array[i], tstacks[i], STACKSIZE,
                compute, (void *)&threads[i], (void *)i,
                NULL, threads[i].priority, 0, K_NO_WAIT);
        /*  set thread name to view it on System View   */
        (void)k_thread_name_set(t_id_array[i], threads[i].t_name);
    }
}

/*
 *   Description:
 *   Funtion to init all semaphores
 */
void init_semaphores(void)
{
    for (int i = 0; i < NUM_THREADS; i++)
    {
        k_sem_init(&g_sem_array[i], 0, 1);
    }
}

/*
 *   Description:
 *   Funtion to init all mutexes
 */
void init_mutexes(void)
{
    for (int i = 0; i < NUM_MUTEXES; i++)
    {
        k_mutex_init(&g_mutex_array[i]);
    }
}

/*
 *   Description:
 *   Funtion to detect if deadline is missed or not
 *   The function checks the g_compute_flag and print error message
 *   The function gives the semaphore after period of a thread elapses
 *
 *   Parameters:
 *   @timer: k_timer object
 */
void timer_handler(struct k_timer *timer)
{
    /*  each timer is associated with thread i.e. timer[0] -> thread[0] */
    int id = (int)timer->user_data;

    if (g_compute_flags[id] == false)
    {
        printk("Deadline is missed for %s\r\n", threads[id].t_name);
    }

    k_sem_give(&g_sem_array[id]);
}

/*
 *   Description:
 *   Funtion to init all timers
 */
void init_timers(void)
{
    for (int i = 0; i < NUM_THREADS; i++)
    {
        k_timer_init(&g_deadline_timer_array[i], timer_handler, NULL);
        k_timer_user_data_set(&g_deadline_timer_array[i], (void *)i);
    }
}

int main()
{
    print_task_h();
    init_timers();
    init_mutexes();
    init_semaphores();
    init_threads();
    return 0;
}
