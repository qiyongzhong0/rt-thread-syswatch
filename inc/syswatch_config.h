/*
 * File      : syswatch.h
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-01-10     qiyongzhong       first version
 */
     
#ifndef __SYSWATCH_CONFIG_H__
#define __SYSWATCH_CONFIG_H__

#include <rtconfig.h>

//#define PKG_USING_SYSWATCH
//#define SYSWATCH_USING_TEST

#ifdef PKG_USING_SYSWATCH

#define SYSWATCH_USING

/* thread exception resolve mode:
 * 0--system reset
 * 1--kill exception thread 
 * 2--resume exception thread
 */
#ifndef SYSWATCH_EXCEPT_RESOLVE_MODE
#define SYSWATCH_EXCEPT_RESOLVE_MODE    2
#endif

#ifndef SYSWATCH_EXCEPT_TIMEOUT
#define SYSWATCH_EXCEPT_TIMEOUT         60//thread exception blocking timeout, unit : s
#endif

#ifndef SYSWATCH_EXCEPT_CONFIRM_TMO
#define SYSWATCH_EXCEPT_CONFIRM_TMO     15//exception thread confirm timeout, unit : s
#endif

#ifndef SYSWATCH_EXCEPT_RESUME_DLY
#define SYSWATCH_EXCEPT_RESUME_DLY      15//exception thread resume delay, unit : s
#endif

#ifndef SYSWATCH_THREAD_PRIO
#define SYSWATCH_THREAD_PRIO            0//the highest priority allowed in your system
#endif

#ifndef SYSWATCH_THREAD_STK_SIZE
#define SYSWATCH_THREAD_STK_SIZE        512//stack size of system watcher
#endif

#ifndef SYSWATCH_THREAD_NAME
#define SYSWATCH_THREAD_NAME            "syswatch"//thread name of system watcher 
#endif

#ifndef SYSWATCH_WDT_NAME
#define SYSWATCH_WDT_NAME               "wdt"//name of watchdog device used
#endif

#ifndef SYSWATCH_WDT_TIMEOUT
#define SYSWATCH_WDT_TIMEOUT            5//timeout of watchdog device used
#endif

#endif  //PKG_USING_SYSWATCH

#endif  //__SYSWATCH_CONFIG_H__


