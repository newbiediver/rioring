//
// Created by newbiediver on 22. 10. 31.
//

#ifndef RIORING_IO_SERVICE_H
#define RIORING_IO_SERVICE_H

#include <liburing.h>
#include "rioring/thread_generator.h"
#include "rioring/thread_object.h"
#include "rioring/thread_lock.h"
#include "rioring/simple_pool.h"

namespace rioring {

class io_context;

/*
 io_uring 은 기본적으로 async 하지만 각 io_uring 의 스레드 컨텍스트를 구성하여 효율을 높임.
 io_service 는 file descriptor 는 지원하지 않으며 현재는 오직 socket stream 만 지원함.
 io_uring is essentially async, but configures thread context for each io_uring to increase efficiency.
 The io_service does not support file descriptors and currently supports only socket streams.
*/
class io_service : private thread_object {
public:
    io_service();
    ~io_service() override = default;

    bool run( int concurrency );
    bool submit( io_context *ctx );
    void stop();

    io_context *allocate_context();

protected:
    void on_thread() override;

private:
    void io( io_uring *ring );
    void deallocate_context( io_context *ctx );

private:
    spin_lock                   lock;
    std::vector< io_uring* >    io_array;
    simple_pool< io_context >   context_pool;

    thread_generator            tg;
};

}

#endif //RIORING_IO_SERVICE_H
