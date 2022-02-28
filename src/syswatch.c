/*
 * File      : syswatch.c
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-01-08     qiyongzhong       first version
 * 2020-05-08     qiyongzhong       optimized initialization operation 
 */
     
#include <rtthread.h>
#include <rtdevice.h>
#include <rthw.h>
#include <syswatch.h>

#ifdef SYSWATCH_USING

//#define SYSWATCH_DEBUG

#define DBG_TAG                         SYSWATCH_THREAD_NAME
#ifdef SYSWATCH_DEBUG
#define DBG_LVL                         DBG_LOG
#else
#define DBG_LVL                         DBG_INFO
#endif
#include <rtdbg.h>

typedef struct{
    rt_bool_t init_ok;
    rt_uint16_t sec_cnt;//time counter
    rt_uint16_t op_step;//opration step : 0--wait thread except, 1--check except thread
    syswatch_event_hook_t event_hook;
    rt_device_t wdt;//watch dog device
    rt_thread_t lowest_thread;//the lowest priority thread that the system can switch to,except for idle thread 
    rt_slist_t wait_resume_slist;//list of threads waiting for resume
}syswatch_data_t;

#if (SYSWATCH_EXCEPT_RESOLVE_MODE == 2)
typedef struct thread_msg{
    rt_slist_t slist;
    void * thread;
    void * entry;
    void * parameter;
    void * stack_addr;
    rt_uint32_t stack_size;
    rt_uint32_t init_tick;
    rt_uint8_t priority;
    char name[RT_NAME_MAX];
}thread_msg_t;
#endif

static syswatch_data_t sw_data = {RT_FALSE,0,0,RT_NULL,RT_NULL,RT_NULL};

static void syswatch_event_hook_call(syswatch_event_t eid, rt_thread_t except_thread)
{
    if (sw_data.event_hook)
    {
        (sw_data.event_hook)(eid, except_thread);
    }
}

static int syswatch_wdt_startup(void)
{
    rt_uint32_t wdt_tmo = SYSWATCH_WDT_TIMEOUT;
    rt_device_t wdt = rt_device_find(SYSWATCH_WDT_NAME);
    
    if (wdt == RT_NULL)
    {
        return -RT_ERROR;
    }

    sw_data.wdt = wdt;
    rt_device_init(wdt);
    rt_device_control(wdt, RT_DEVICE_CTRL_WDT_SET_TIMEOUT, &wdt_tmo);
    rt_device_control(wdt, RT_DEVICE_CTRL_WDT_START, RT_NULL);

    return RT_EOK;
}

static void syswatch_wdt_feed(void)
{
    rt_device_control(sw_data.wdt, RT_DEVICE_CTRL_WDT_KEEPALIVE, RT_NULL);
}

#if (SYSWATCH_EXCEPT_RESOLVE_MODE == 0)
/* thread except resolve mode 0--system reset */
static void syswatch_thread_except_resolve_reset(rt_thread_t thread)
{
    LOG_E("%.*s thread exception, priority = %d, execute system reset", RT_NAME_MAX, thread->name, thread->current_priority);
    syswatch_event_hook_call(SYSWATCH_EVENT_SYSTEM_RESET, thread);
    rt_thread_mdelay(100);
    rt_hw_cpu_reset();
}
#endif

#if ((SYSWATCH_EXCEPT_RESOLVE_MODE == 1)||(SYSWATCH_EXCEPT_RESOLVE_MODE == 2))
/* thread except resolve mode 1 or 2--kill exception thread  */
static void syswatch_thread_except_resolve_kill(rt_thread_t thread)
{
    syswatch_event_hook_call(SYSWATCH_EVENT_THREAD_KILL, thread);
    
    if (rt_object_is_systemobject((rt_object_t)thread))
    {
        rt_thread_detach(thread);
    }
    else
    {
        rt_thread_delete(thread);
    }
    
    LOG_E("%.*s thread exception, priority = %d, successfully killed", RT_NAME_MAX, thread->name, thread->current_priority);
}
#endif

#if (SYSWATCH_EXCEPT_RESOLVE_MODE == 2)
/* thread except resolve mode 2--save exception thread message */
static void syswatch_thread_except_resolve_resume_save_msg(rt_thread_t thread)
{
    thread_msg_t *thread_msg;

    thread_msg = rt_malloc(sizeof(thread_msg_t));
    if (thread_msg == RT_NULL)
    {
        LOG_E("save exception thread message fail, no memory");
        return;
    }
    
    thread_msg->thread = thread;
    thread_msg->entry = thread->entry;
    thread_msg->parameter = thread->parameter;
    thread_msg->stack_addr = thread->stack_addr;
    thread_msg->stack_size = thread->stack_size;
    thread_msg->init_tick = thread->init_tick;
    #if (RTTHREAD_VERSION < 40100)
    thread_msg->priority= thread->init_priority;
    #else
    thread_msg->priority= thread->current_priority;
    #endif
    rt_strncpy(thread_msg->name, thread->name, RT_NAME_MAX);

    rt_slist_append(&(sw_data.wait_resume_slist), (rt_slist_t *)thread_msg);

    LOG_I("save exception thread message successfully");
}

/* thread except resolve mode 2--resume exception thread from thread message */
static void syswatch_thread_except_resolve_resume_from_msg(thread_msg_t * thread_msg)
{
    if (rt_object_is_systemobject((rt_object_t)(thread_msg->thread)))
    {
        rt_thread_init( thread_msg->thread, 
                        thread_msg->name, 
                        (void (*)(void*))(thread_msg->entry), 
                        thread_msg->parameter, 
                        thread_msg->stack_addr, 
                        thread_msg->stack_size, 
                        thread_msg->priority,
                        thread_msg->init_tick
                        );
        rt_thread_startup(thread_msg->thread);
        syswatch_event_hook_call(SYSWATCH_EVENT_THREAD_RESUMED, thread_msg->thread);
        LOG_I("%.*s exception thread, priority = %d, successfully resumed", RT_NAME_MAX, thread_msg->name, thread_msg->priority);
    }
    else
    {
        rt_thread_t thread = rt_thread_create(  thread_msg->name, 
                                                (void (*)(void*))(thread_msg->entry),
                                                thread_msg->parameter, 
                                                thread_msg->stack_size, 
                                                thread_msg->priority,
                                                thread_msg->init_tick
                                                );
        if (thread)
        {
            rt_thread_startup(thread);
            syswatch_event_hook_call(SYSWATCH_EVENT_THREAD_RESUMED, thread);
            LOG_I("%.*s exception thread, priority = %d, successfully resumed", RT_NAME_MAX, thread_msg->name, thread_msg->priority);
        }
        else
        {
            LOG_E("%.*s exception thread, priority = %d, resume fail", RT_NAME_MAX, thread_msg->name, thread_msg->priority);
        }
    }
}

/* thread except resolve mode 2--resume all exception thread  */
static void syswatch_thread_except_resolve_resume_all(void)
{
    thread_msg_t *thread_msg;

    while(1)
    {
        thread_msg = (thread_msg_t *)rt_slist_first(&(sw_data.wait_resume_slist));
        if (thread_msg == RT_NULL)
        {
            break;
        }
        syswatch_thread_except_resolve_resume_from_msg(thread_msg);
        rt_slist_remove(&(sw_data.wait_resume_slist), (rt_slist_t *)thread_msg);
        rt_free(thread_msg);
    }
}
#endif

static void syswatch_thread_except_resolve(rt_thread_t thread)
{
    if ((thread == RT_NULL) || thread == rt_thread_self())
    {
        LOG_D("exception thread is null or self");
        return;
    }
    
#if (SYSWATCH_EXCEPT_RESOLVE_MODE == 0)//mode 0--system reset
    syswatch_thread_except_resolve_reset(thread);
#elif (SYSWATCH_EXCEPT_RESOLVE_MODE == 1)//mode 1--kill exception thread 
    syswatch_thread_except_resolve_kill(thread);
#elif (SYSWATCH_EXCEPT_RESOLVE_MODE == 2)//mode 2--resume exception thread 
    syswatch_thread_except_resolve_resume_save_msg(thread);
    syswatch_thread_except_resolve_kill(thread);
#else
#error "SYSWATCH_EXCEPT_DEAL_MODE define error,it must be 0/1/2"
#endif
}

static void syswatch_thread_switch_hook(struct rt_thread * from, struct rt_thread * to)
{
    if (from->current_priority > to->current_priority)//low->high or same, no care
    {
        return;
    }
    if (to->current_priority == RT_THREAD_PRIORITY_MAX-1)//switch to idle thread
    {
#if (SYSWATCH_EXCEPT_RESOLVE_MODE == 2)
        switch(sw_data.op_step)
        {
        case 0://waiting thread exception
            sw_data.sec_cnt = 0;
            break;
        case 1://confirming exception thread 
            sw_data.sec_cnt = 0;
            if (! rt_slist_isempty(&(sw_data.wait_resume_slist)))//mode 2 : all exception threads have been killed
            {
                sw_data.op_step = 2;//to resume
            }
            else
            {
                sw_data.op_step = 0;//to waiting
            }
            break;
        case 2://resuming exception thread 
            break;
        default:
            break;
        }
#else
        sw_data.sec_cnt = 0;
        sw_data.op_step = 0;
#endif
        return;
    }
    if (sw_data.op_step != 1)//no confirming exception thread
    {
        return;
    }
    if (sw_data.lowest_thread == RT_NULL)//begin confirming exception thread 
    {
        sw_data.lowest_thread = to;
        return;
    }
    if (to->current_priority > sw_data.lowest_thread->current_priority)//to lower
    {
        sw_data.lowest_thread = to;
        return;
    }
    if (to->current_priority < sw_data.lowest_thread->current_priority)//to higher
    {
        return;
    }
    if (to->current_priority == from->current_priority)//same priority task switching 
    {
        if ((from->stat & RT_THREAD_STAT_MASK) == RT_THREAD_RUNNING)//is yield
        {
            sw_data.lowest_thread = from;
        }
    }
}

static rt_bool_t syswatch_check_timeout(rt_uint16_t tmo_s)
{
    sw_data.sec_cnt++;
    if (sw_data.sec_cnt < tmo_s)
    {
        return(RT_FALSE);
    }
    sw_data.sec_cnt = 0;
    return(RT_TRUE);
}

static void syswatch_fsm(void)
{       
    switch(sw_data.op_step)
    {
    case 0://waiting thread except
        if (syswatch_check_timeout(SYSWATCH_EXCEPT_TIMEOUT))
        {
            sw_data.op_step = 1;
            sw_data.lowest_thread = RT_NULL;
        }
        break;
    case 1://confirming except thread 
        if (syswatch_check_timeout(SYSWATCH_EXCEPT_CONFIRM_TMO))
        {
            syswatch_thread_except_resolve(sw_data.lowest_thread);
            sw_data.lowest_thread = RT_NULL; 
        }
        break;    
#if (SYSWATCH_EXCEPT_RESOLVE_MODE == 2)
    case 2://resuming except thread
        if (syswatch_check_timeout(SYSWATCH_EXCEPT_RESUME_DLY))
        {
            syswatch_thread_except_resolve_resume_all();
            sw_data.op_step = 0;
        }
        break;
#endif
    default:
        sw_data.op_step = 0;
        sw_data.sec_cnt = 0;
        break;
    }
}

static void syswatch_thread_entry(void *parameter)
{
    if (syswatch_wdt_startup() != RT_EOK)
    {
        LOG_E("watch dog startup fail");
        return;
    }
    
    LOG_I("system watch startup successfully");
    
    rt_slist_init(&(sw_data.wait_resume_slist));
    rt_scheduler_sethook(syswatch_thread_switch_hook);
    
    while(1)
    {
        rt_thread_delay(RT_TICK_PER_SECOND);
        syswatch_wdt_feed();
        syswatch_fsm();
    }
}

void syswatch_set_event_hook(syswatch_event_hook_t hook)
{
    sw_data.event_hook = hook;
}

int syswatch_init(void)
{
    rt_thread_t tid;

    if(sw_data.init_ok)
    {
        return RT_EOK;
    }

    tid = rt_thread_create(SYSWATCH_THREAD_NAME, syswatch_thread_entry, RT_NULL,
                           SYSWATCH_THREAD_STK_SIZE, SYSWATCH_THREAD_PRIO, 20);
    if (tid == RT_NULL)
    {
        LOG_E("create syswatch thread failed.");
        return -RT_ERROR;
    }
    
    LOG_I("create syswatch thread success.");
    rt_thread_startup(tid);
    sw_data.init_ok = RT_TRUE;
    
    return RT_EOK;
}
INIT_COMPONENT_EXPORT(syswatch_init);

#endif  //SYSWATCH_USING
