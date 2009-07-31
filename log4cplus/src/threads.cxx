// Module:  Log4CPLUS
// File:    threads.cxx
// Created: 6/2001
// Author:  Tad E. Smith
//
//
// Copyright (C) Tad E. Smith  All rights reserved.
//
// This software is published under the terms of the Apache Software
// License version 1.1, a copy of which has been included with this
// distribution in the LICENSE.APL file.
//

#ifndef LOG4CPLUS_SINGLE_THREADED

#include <cassert>
#include <exception>
#include <stdexcept>
#include <errno.h>

#include <log4cplus/config.hxx>

#if defined(LOG4CPLUS_USE_PTHREADS)
#  include <sched.h>
#  include <signal.h>
#elif defined (LOG4CPLUS_USE_WIN32_THREADS)
#  include <process.h> 
#endif

#include <log4cplus/helpers/threads.h>
#include <log4cplus/streams.h>
#include <log4cplus/ndc.h>
#include <log4cplus/helpers/loglog.h>
#include <log4cplus/helpers/stringhelper.h>
#include <log4cplus/helpers/timehelper.h>

#include <log4cplus/helpers/syncprims.h>


namespace log4cplus { namespace thread {


///////////////////////////////////////////////////////////////////////////////
// public methods
///////////////////////////////////////////////////////////////////////////////

LOG4CPLUS_MUTEX_PTR_DECLARE 
createNewMutex()
{
#if defined(LOG4CPLUS_USE_PTHREADS)
    ::pthread_mutex_t* m = new ::pthread_mutex_t;
    ::pthread_mutex_init(m, NULL);
#elif defined(LOG4CPLUS_USE_WIN32_THREADS)
    ::CRITICAL_SECTION* m = new ::CRITICAL_SECTION;
    ::InitializeCriticalSection(m);
#endif
    return m;
}


void 
deleteMutex(LOG4CPLUS_MUTEX_PTR_DECLARE m)
{
#if defined(LOG4CPLUS_USE_PTHREADS)
    ::pthread_mutex_destroy(m);
#elif defined(LOG4CPLUS_USE_WIN32_THREADS)
    ::DeleteCriticalSection(m);
#endif
    delete m;
}



#if defined(LOG4CPLUS_USE_PTHREADS)
pthread_key_t*
createPthreadKey(void (*cleanupfunc)(void *))
{
    ::pthread_key_t* key = new ::pthread_key_t;
    ::pthread_key_create(key, cleanupfunc);
    return key;
}
#endif


#ifndef LOG4CPLUS_SINGLE_THREADED
void
blockAllSignals()
{
#if defined (LOG4CPLUS_USE_PTHREADS)
    // Block all signals.
    ::sigset_t signal_set;
    ::sigfillset (&signal_set);
    ::pthread_sigmask (SIG_BLOCK, &signal_set, 0);
#endif    
}
#endif // LOG4CPLUS_SINGLE_THREADED


void
yield()
{
#if defined(LOG4CPLUS_USE_PTHREADS)
    ::sched_yield();
#elif defined(LOG4CPLUS_USE_WIN32_THREADS)
    ::Sleep(0);
#endif
}


tstring
getCurrentThreadName()
{
    tostringstream tmp;
    tmp << LOG4CPLUS_GET_CURRENT_THREAD;
    return tmp.str ();
}


#if defined(LOG4CPLUS_USE_PTHREADS)
    void* 
    threadStartFunc(void* arg)
#elif defined(LOG4CPLUS_USE_WIN32_THREADS)
    unsigned WINAPI
    threadStartFunc(void * arg)
#endif
{
    blockAllSignals ();
    helpers::SharedObjectPtr<helpers::LogLog> loglog
        = helpers::LogLog::getLogLog();
    if (! arg)
        loglog->error(LOG4CPLUS_TEXT("threadStartFunc()- arg is NULL"));
    else
    {
        AbstractThread * ptr = static_cast<AbstractThread*>(arg);
        AbstractThreadPtr thread(ptr);

        // Decrease reference count increased by AbstractThread::start().
        ptr->removeReference ();

        try
        {
            thread->run();
        }
        catch(std::exception& e)
        {
            tstring err = LOG4CPLUS_TEXT("threadStartFunc()- run() terminated with an exception: ");
            err += LOG4CPLUS_C_STR_TO_TSTRING(e.what());
            loglog->warn(err);
        }
        catch(...)
        {
            loglog->warn(LOG4CPLUS_TEXT("threadStartFunc()- run() terminated with an exception."));
        }
        thread->running = false;
        getNDC().remove();
    }

    return 0;
}



///////////////////////////////////////////////////////////////////////////////
// AbstractThread ctor and dtor
///////////////////////////////////////////////////////////////////////////////

AbstractThread::AbstractThread()
    : running(false)
#if defined(LOG4CPLUS_USE_WIN32_THREADS)
    , handle (INVALID_HANDLE_VALUE)
#endif
{
}



AbstractThread::~AbstractThread()
{
#if defined(LOG4CPLUS_USE_WIN32_THREADS)
    if (handle != INVALID_HANDLE_VALUE)
        ::CloseHandle (handle);
#endif
}



///////////////////////////////////////////////////////////////////////////////
// AbstractThread public methods
///////////////////////////////////////////////////////////////////////////////

void
AbstractThread::start()
{
    running = true;

    // Increase reference count here. It will be lowered by the running
    // thread itself.
    addReference ();

#if defined(LOG4CPLUS_USE_PTHREADS)
    if (::pthread_create(&handle, NULL, threadStartFunc, this) )
    {
        removeReference ();
        throw std::runtime_error("Thread creation was not successful");
    }
#elif defined(LOG4CPLUS_USE_WIN32_THREADS)
    HANDLE h = InterlockedExchangePointer (&handle, INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE)
        ::CloseHandle (h);

    h = reinterpret_cast<HANDLE>(
        ::_beginthreadex (0, 0, threadStartFunc, this, 0, &thread_id));
    if (! h)
    {
        removeReference ();
        throw std::runtime_error("Thread creation was not successful");
    }
    h = InterlockedExchangePointer (&handle, h);
    assert (h == INVALID_HANDLE_VALUE);
#endif
}


LOG4CPLUS_THREAD_KEY_TYPE
AbstractThread::getThreadId () const
{
#if defined(LOG4CPLUS_USE_PTHREADS)
    return handle;
#elif defined(LOG4CPLUS_USE_WIN32_THREADS)
    return thread_id;
#endif
}


LOG4CPLUS_THREAD_HANDLE_TYPE
AbstractThread::getThreadHandle () const
{
    return handle;
}


void
AbstractThread::join () const
{
#if defined(LOG4CPLUS_USE_PTHREADS)
    ::pthread_join (handle, 0);
#elif defined(LOG4CPLUS_USE_WIN32_THREADS)
    ::WaitForSingleObject (handle, INFINITE);
#endif
}


} } // namespace log4cplus { namespace thread {

#endif // LOG4CPLUS_SINGLE_THREADED