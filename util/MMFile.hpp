//
// Created by tate on 5/8/26.
//

#pragma once

#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <sys/mman.h>

/**
 * Memory-mapped file
 */
class MMFile {
protected:
    /// File size in bytes
    size_t m_size{0};

    /// mmap'd buffer
    char* m_data{nullptr};

    /// File descriptor
    int m_fd{-1};

public:
    /// Thrown on error
    class Error : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    // Move-only
    MMFile() = delete;
    MMFile(const MMFile&) = delete;
    MMFile& operator=(const MMFile&) = delete;
    MMFile(MMFile&&) = default;

    explicit MMFile(const char* path, const long offset = 0) {
        m_fd = open(path, O_RDONLY);
        if (m_fd < 0) [[unlikely]] {
#ifdef FIY_DEBUG
            perror("open()");
#endif
            throw Error("Unable to open file " + std::string(path));
        }

        // lseek returns the offset from the beginning of the file
        m_size = lseek(m_fd, offset, SEEK_END);
        if (m_size <= 0) [[unlikely]] {
#ifdef FIY_DEBUG
            perror("lseek()");
#endif
            close(m_fd);
            throw Error("Unable to seek file " + std::string(path));
        }

        // we don't need to lseek again as mmap ignores the offset
        m_data = (char *)mmap(nullptr, m_size, PROT_READ, MAP_SHARED, m_fd, offset);
        if (m_data == MAP_FAILED) [[unlikely]] {
#ifdef FIY_DEBUG
            perror("mmap()");
#endif
            close(m_fd);
            throw Error("Failed to mmap file " + std::string(path));
        }
    }

    explicit MMFile(const std::string& path, const long offset = 0):
        MMFile(path.c_str(), offset) {}

    ~MMFile() {
        munmap(m_data, m_size);
        close(m_fd);
    }

    /**
     * Immutable access to the file contents
     * @return buffer containing file contents
     */
    [[nodiscard]] const char* data() const {
        return m_data;
    }

    /**
     * Get the size of the file/buffer
     * @return size of the file in bytes
     */
    [[nodiscard]] size_t size() const {
        return m_size;
    }

    [[nodiscard]] int native_handle() const {
        return m_fd;
    }

    operator std::string_view() const {
        return {m_data, m_size};
    }

    operator const char*() const {
        return m_data;
    }
};
