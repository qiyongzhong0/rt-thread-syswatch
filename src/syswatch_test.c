/*
 * File      : syswatch_test.c
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-01-10     qiyongzhong       first version
 */
     
#include <stdlib.h>
#include <stdio.h>
#include <rtthread.h>
#include <syswatch_config.h>

#ifdef SYSWATCH_USING_TEST

#define DBG_TAG                     "syswatch_test"
#define DBG_LVL                     DBG_LOG
#include <rtdbg.h>

static rt_uint32_t thread_cnt = 0;
static rt_uint32_t thread_sign = 0;

static void syswatch_test_thread_entry(void *parameter)
{
    rt_uint32_t delay_s = (rt_uint32_t)parameter;
    rt_thread_t thread = rt_thread_self();

#if (SYSWATCH_EXCEPT_RESOLVE_MODE == 2)

    rt_uint32_t idx;

    idx = atoi(thread->name+6);
    idx %= 32;
    
    if ((thread_sign & (1<<idx)) == 0)//first entry
    {
        thread_sign |= (1<<idx);

        LOG_I("system watch test thread startup, name = %.*s, priority = %d", RT_NAME_MAX, thread->name, thread->current_priority);
        LOG_I("system watch thread exception timeout = %d s, delay = %d s", SYSWATCH_EXCEPT_TIMEOUT, delay_s);
        LOG_I("system watch exception resolve mode = %d", SYSWATCH_EXCEPT_RESOLVE_MODE);

        rt_thread_mdelay(delay_s * 1000);
        LOG_I("%.*s thread exception begin. priority = %d", RT_NAME_MAX, thread->name ,thread->current_priority);
        while(1);
    }
    else//second entry
    {
        thread_sign &= ~(1<<idx);
        
        LOG_I("%.*s thread test completion, it will be closed after 10s .", RT_NAME_MAX, thread->name);
        rt_thread_mdelay(10 * 1000);
        LOG_I("%.*s thread closed.", RT_NAME_MAX, thread->name);
    }
#else
    LOG_I("system watch test thread startup, name = %.*s, priority = %d", RT_NAME_MAX, thread->name, thread->current_priority);
    LOG_I("system watch thread exception timeout = %d s, delay = %d s", SYSWATCH_EXCEPT_TIMEOUT, delay_s);
    LOG_I("system watch exception resolve mode = %d", SYSWATCH_EXCEPT_RESOLVE_MODE);

    rt_thread_mdelay(delay_s * 1000);
    LOG_I("%.*s thread exception begin. priority = %d", RT_NAME_MAX, thread->name ,thread->current_priority);
    while(1);
#endif
}

static void syswatch_test_startup(rt_uint8_t prio, rt_uint32_t except_delay_s)
{
    rt_thread_t tid;
    char name[RT_NAME_MAX+1];

    thread_cnt++;
    rt_sprintf(name, "swtst_%02d", thread_cnt);

    tid = rt_thread_create(name, syswatch_test_thread_entry, (void *)(except_delay_s),
                        512, prio, 20);
    if ( ! tid)
    {
         LOG_E("create syswatch test thread failed.");
         return;
    }

    rt_thread_startup(tid);
}

static void syswatch_test(int argc, char **argv)
{
    rt_uint8_t prio = 0;
    rt_uint32_t delay_s = 0;

    if(argc < 2)
    {
        rt_kprintf("Usage: syswatch_test <priority 0-%d> [delay_s]\n", RT_THREAD_PRIORITY_MAX-2);
        return ;
    }
    prio = atoi(argv[1]);
    if (prio > RT_THREAD_PRIORITY_MAX-2)
    {
        prio = RT_THREAD_PRIORITY_MAX-2;
    }
    if (argc == 3)
    {
        delay_s = atoi(argv[2]);
    }
    syswatch_test_startup(prio, delay_s);
}
MSH_CMD_EXPORT(syswatch_test, system watch test);
 
#endif //SYSWATCH_TEST_USING

