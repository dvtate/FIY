#pragma once

#include <shared_mutex>

/**
 * Mutex that supports multiple simultaneous reads but only a single write
 * @deprecated use std::shared_mutex directly instead
 */
struct RWMutex {
    std::shared_mutex m_mtx;

    void read_lock() {
        m_mtx.lock_shared();
    }
    void read_unlock() {
        m_mtx.unlock_shared();
    }
    void write_lock() {
        m_mtx.lock();
    }
    void write_unlock() {
        m_mtx.unlock();
    }

    /// Scoped write_lock + write_unlock
    struct LockForWrite {
        explicit LockForWrite(RWMutex& mtx): m_mtx(mtx) {
            mtx.write_lock();
        }
        LockForWrite(const LockForWrite&) = delete;
        ~LockForWrite() {
            m_mtx.write_unlock();
        }
    private:
        RWMutex& m_mtx;
    };

    /// Scoped read_lock + read_unlock
    struct LockForRead {
        explicit LockForRead(RWMutex& mtx): m_mtx(mtx) {
            mtx.read_lock();
        }
        LockForRead(const LockForRead&) = delete;
        ~LockForRead() {
            m_mtx.read_unlock();
        }
    private:
        RWMutex& m_mtx;
    };
};