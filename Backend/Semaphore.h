#pragma once
#include <semaphore.h>
#include <stdexcept>
#include <string>

// =============================================================================
// Semaphore.h — POSIX Semaphore Wrapper
// OS Concept: Semaphores control access to a finite resource pool.
// Here we use it to limit the number of traders allowed in the "trading room"
// simultaneously, simulating server capacity constraints.
// =============================================================================

class Semaphore {
public:
    // initialValue = maximum concurrent traders allowed
    explicit Semaphore(int initialValue) {
        if (sem_init(&m_sem, 0, initialValue) != 0) {
            throw std::runtime_error("Failed to initialize semaphore");
        }
        m_maxValue = initialValue;
    }

    ~Semaphore() {
        sem_destroy(&m_sem);
    }

    // Called BEFORE entering the critical trading section.
    // Blocks if no capacity is available (count == 0).
    void wait() {
        sem_wait(&m_sem);   // P() / down() — decrements counter
    }

    // Called AFTER leaving the trading section.
    // Wakes up a blocked trader if any are waiting.
    void signal() {
        sem_post(&m_sem);   // V() / up() — increments counter
    }

    // Non-blocking try — returns false if no capacity
    bool tryWait() {
        return sem_trywait(&m_sem) == 0;
    }

    int getValue() {
        int val = 0;
        sem_getvalue(&m_sem, &val);
        return val;
    }

    int maxValue() const { return m_maxValue; }

    // RAII guard — automatically releases semaphore on scope exit
    class Guard {
    public:
        explicit Guard(Semaphore& s) : m_sem(s) { m_sem.wait(); }
        ~Guard() { m_sem.signal(); }
    private:
        Semaphore& m_sem;
    };

private:
    sem_t m_sem;
    int   m_maxValue;

    // Non-copyable
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;
};
