#include <fcntl.h>
#include <liburing.h>

#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <istream>

//@todo: надо нормально выходить закрывать предыдущие ресурсы
static constexpr size_t MAX_BUFFER_SIZE_READ = 46;
static constexpr size_t MAX_BUFFER_SIZE_WRITE = 5;
static constexpr uint8_t MAX_TASKS_IO = 5;
int main() {
    //@todo: по хорошему чтение надо тоже через uring сделать
    char buffer_read[MAX_BUFFER_SIZE_READ];
    int fd_file_read = open("test_ur.txt", O_RDONLY, 660);
    if (fd_file_read < 0) {
        perror(strerror(errno));
        return -1;
    }
    std::cout << "file read success" << std::endl;
    ssize_t size_file = lseek(fd_file_read, 0, SEEK_END);
    lseek(fd_file_read, 0, SEEK_SET);
    int fd_file_write = open("test_ur_w.txt", O_WRONLY | O_TRUNC | O_CREAT, 0660);
    if (fd_file_write < 0) {
        close(fd_file_read);
        perror(strerror(errno));
        return -1;
    }
    std::cout << "file write success" << std::endl;
    //@todo: может просчитать не все надо проверить (хотел 100 а отдаст 20)
    ssize_t bytes = read(fd_file_read, buffer_read, size_file);
    if (bytes < 0) {
        auto err = errno;
        close(fd_file_write);
        close(fd_file_read);
        perror(strerror(err));
        return -1;
    } else if (bytes == 0) {
        std::cout << "read " << bytes << std::endl;
        return 0;
    } else if (size_file != bytes) {
        std::cout << "bytes " << bytes << " size_file " << size_file << std::endl;
        return -1;
    }

    /* ---------============ uring ============--------- */

    struct io_uring uring_instance {};
    if (io_uring_queue_init(MAX_TASKS_IO, &uring_instance, 0) < 0) {
        auto err = errno;
        close(fd_file_write);
        close(fd_file_read);
        perror(strerror(err));
        return -1;
    }

    uint64_t curr_bytes_write = 0;
    uint64_t confirmed_write = 0;
    uint64_t tasks = 1;
    while (true) {
        if (tasks == 1 && curr_bytes_write != size_file) {
            while (true) {
                struct io_uring_sqe *sqe = io_uring_get_sqe(&uring_instance);
                if (!sqe) {
                    auto err = errno;
                    close(fd_file_write);
                    close(fd_file_read);
                    perror(strerror(err));
                    return -1;
                }
                assert(curr_bytes_write <= MAX_BUFFER_SIZE_READ);
                auto dif = size_file - curr_bytes_write;
                if (dif >= MAX_BUFFER_SIZE_WRITE) {
                    io_uring_prep_write(sqe, fd_file_write, buffer_read + curr_bytes_write, MAX_BUFFER_SIZE_WRITE, curr_bytes_write);
                    curr_bytes_write += MAX_BUFFER_SIZE_WRITE;
                } else {
                    std::cout << "find tail = " << dif << std::endl;
                    io_uring_prep_write(sqe, fd_file_write, buffer_read + curr_bytes_write, dif, curr_bytes_write);
                    curr_bytes_write += dif;
                }
                sqe->user_data = tasks;
                std::cout << "load: " << tasks << " tasks" << std::endl;
                if (tasks == MAX_TASKS_IO || curr_bytes_write == size_file) {
                    break;
                }
                tasks++;
            }
            if (auto res = io_uring_submit(&uring_instance); res < 0) {
                auto err = errno;
                close(fd_file_write);
                close(fd_file_read);
                perror(strerror(err));
                return -1;
            } else {
                std::cout << "submit: " << tasks << " tasks" << std::endl;
            }
        } else {
            while (tasks != 0) {
                struct io_uring_cqe *cqes = nullptr;
                if (uint64_t completed_task = io_uring_wait_cqe(&uring_instance, &cqes); completed_task == 0) {
                    std::cout << "=== found completed task" << std::endl;
                    if (cqes[0].res > 0) {
                        confirmed_write += cqes[0].res;
                        tasks--;
                        std::cout << "\ttask: " << cqes[0].user_data << " total need write bytes: " << size_file
                                  << " write bytes: " << confirmed_write << std::endl;
                        io_uring_cqe_seen(&uring_instance, cqes);
                    }
                }
            }
            tasks = 1;
        }
        if (confirmed_write == size_file) {
            break;
        }
    }
    io_uring_queue_exit(&uring_instance);
    close(fd_file_write);
    close(fd_file_read);
    return 0;
}
