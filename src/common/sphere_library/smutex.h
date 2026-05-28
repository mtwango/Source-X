/**
* @file smutex.h
* @brief Sphere simple mutex implementation.
*/

#ifndef _INC_SMUTEX_H
#define _INC_SMUTEX_H

#ifndef _WIN32
#   include <pthread.h>
#else
#   include "../common.h"
#endif

#ifdef _BSD
#   define PTHREAD_MUTEX_RECURSIVE_NP PTHREAD_MUTEX_RECURSIVE
#endif


/**
* Simple mutex class calling OS APIs directly.
*/
class SimpleMutex
{
#ifdef _WIN32
    CRITICAL_SECTION m_criticalSection;   // Windows API specific mutex.
#else
    pthread_mutex_t m_criticalSection;   // Unix API specific mutex.
    pthread_mutexattr_t m_criticalSectionAttr;   // Unix API mutex attr.
#endif

public:
	/** @name Constructors, Destructor, Assign operator:
	*/
	///@{
	SimpleMutex() noexcept;
	~SimpleMutex() noexcept;

	SimpleMutex(const SimpleMutex& copy) = delete;
	SimpleMutex& operator=(const SimpleMutex& other) = delete;
	///@}

	/** @name Interaction:
	*/
	///@{
	/**
	* @brief Locks the mutex.
	*
	* Waits for ownership of the specified critical section object. The function
	* returns when the calling thread is granted ownership.
	*/
	void lock() noexcept
	{
#ifdef _WIN32
		EnterCriticalSection(&m_criticalSection);
#else
		pthread_mutex_lock(&m_criticalSection);
#endif
	}
	/**
	* @brief Tries to lock the mutex.
	*
	* Attempts to enter a critical section without blocking. If the call is
	* successful, the calling thread takes ownership of the critical section.
    * @return True If the critical section is successfully entered or the current
    * thread already owns the critical section, false otherwise.
	*/
    bool tryLock() noexcept
	{
#ifdef _WIN32
		return TryEnterCriticalSection(&m_criticalSection) == TRUE;
#else
		return pthread_mutex_trylock(&m_criticalSection) == 0;
#endif
	}
	/**
	* @brief Unlocks the mutex.
	*
	* Releases ownership of the specified critical section object.
	*/
    void unlock() noexcept
	{
#ifdef _WIN32
		LeaveCriticalSection(&m_criticalSection);
#else
		pthread_mutex_unlock(&m_criticalSection);
#endif
	}
	///@}
};

/**
* @brief A simple automatic thread lock.
*
* Locks on creation and unlocks on destruction.
*/
class SimpleThreadLock
{
public:
	/** @name Constructors, Destructor, Assign operator:
	*/
	///@{
    explicit SimpleThreadLock(SimpleMutex &mutex) noexcept
        : m_mutex(mutex), m_locked(true) {
		mutex.lock();
	}
    ~SimpleThreadLock() noexcept {
		m_mutex.unlock();
	}
    void unlock() noexcept {
        m_mutex.unlock();
    }

	SimpleThreadLock(const SimpleThreadLock& copy) = delete;
	SimpleThreadLock& operator=(const SimpleThreadLock& other) = delete;
	///@}

	/** @name Operators:
	*/
	///@{
	/**
	* @brief Is the thread locked?
	*
	* In practice, this is always true.
	*/
    operator bool() const {
		return m_locked;
	}
	///@}

private:
	SimpleMutex &m_mutex;   // Mutex to control the lock.
	bool m_locked;    // Always true.
};

/**
*
*/
class ManualThreadLock
{
public:
	/** @name Constructors, Destructor, Assign operator:
	*/
	///@{
    ManualThreadLock() noexcept
        : m_mutex(nullptr), m_locked(false) {
	}
    explicit ManualThreadLock(SimpleMutex * mutex) noexcept : m_locked(false) {
		setMutex(mutex);
	}
    ~ManualThreadLock() noexcept {
		if (m_mutex != nullptr)
			doUnlock();
	}

	ManualThreadLock(const ManualThreadLock& copy) = delete;
	ManualThreadLock& operator=(const ManualThreadLock& other) = delete;
	///@}

	/** @name Modifiers:
	*/
	///@{
	/**
	* @brief Sets the mutex to modify.
	* @param mutex
	*/
    void setMutex(SimpleMutex * mutex) noexcept {
		m_mutex = mutex;
	}
	///@}
	/** @name Operators:
	*/
	///@{
    operator bool() const {
		return m_locked;
	}
	///@}
	/** @name Interaction:
	*/
	///@{
	/**
	* @brief Locks the mutex.
	*
	* Waits for ownership of the specified critical section object. The function
	* returns when the calling thread is granted ownership.
	*/
	void doLock() noexcept;
	/**
	* @brief Tries to lock the mutex.
	*
	* Attempts to enter a critical section without blocking. If the call is
	* successful, the calling thread takes ownership of the critical section.
    * @return True If the critical section is successfully entered or the current
    * thread already owns the critical section, false otherwise.
	*/
	bool doTryLock() noexcept;
	/**
	* @brief Unlocks the mutex.
	*
	* Releases ownership of the specified critical section object.
	*/
	void doUnlock() noexcept;
	///@}

private:
	SimpleMutex * m_mutex;   // Mutex to control the lock.
	bool m_locked;    // Is the thread locked?
};


#endif // _INC_SMUTEX_H
