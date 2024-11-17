#include <fcntl.h>
#include <liburing.h>

#include <cstring>
#include <iostream>
#include <istream>

//@todo: надо нормально выходить закрывать предыдущие ресурсы
static constexpr size_t MAX_BUFFER_SIZE = 1024u * 1024u * 2014u * 1024u;

int main() {
    //@todo: хак для простоты, по хорошему бы маленький буферок с страницу памяти
    char *buffer = reinterpret_cast<char *>(malloc(MAX_BUFFER_SIZE));
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
    ssize_t bytes = read(fd_file_read, buffer, size_file);
    if (bytes < 0) {
        auto err = errno;
        close(fd_file_write);
        close(fd_file_read);
        free(buffer);
        perror(strerror(err));
        return -1;
    } else if (bytes == 0) {
        std::cout << "read " << bytes << std::endl;
        return 0;
    } else if (size_file != bytes) {
        std::cout << "bytes " << bytes << " size_file " << size_file << std::endl;
        return -1;
    }

    struct io_uring uring_instance {};
    if (io_uring_queue_init(1, &uring_instance, 0) < 0) {
        auto err = errno;
        close(fd_file_write);
        close(fd_file_read);
        free(buffer);
        perror(strerror(err));
        return -1;
    }

    struct io_uring_sqe *sqe = io_uring_get_sqe(&uring_instance);
    io_uring_prep_write(sqe, fd_file_write, buffer, size_file, 0);

    if (auto res = io_uring_submit(&uring_instance); res < 0) {
        auto err = errno;
        close(fd_file_write);
        close(fd_file_read);
        free(buffer);
        perror(strerror(err));
        return -1;
    }

    // todo: надо цикл потому что в этот момент может ничего не быть (может быть задержка)
    struct io_uring_cqe *cqes = nullptr;
    if (auto res = io_uring_wait_cqe(&uring_instance, &cqes); res < 0) {
        auto err = errno;
        close(fd_file_write);
        close(fd_file_read);
        free(buffer);
        perror(strerror(err));
        return -1;
    }

    if (cqes[0].res > 0) {
        std::cout << "write " << bytes << std::endl;
    } else {
        std::cout << "error res" << bytes << std::endl;
    }
    io_uring_cqe_seen(&uring_instance, &cqes[0]);

    io_uring_queue_exit(&uring_instance);
    close(fd_file_write);
    close(fd_file_read);
    free(buffer);
    return 0;
}
