//
// Created by newbiediver on 22. 11. 2.
//

#ifndef RIORING_CONTEXT_H
#define RIORING_CONTEXT_H

#include <memory>

#ifdef WIN32

#include <mswsockdef.h>

namespace rioring {

class object_base;

struct io_context : public RIO_BUF {
    enum class io_type {
        read,
        write,
    };

    io_type         type;
    RIO_RQ          rq;
    std::shared_ptr< object_base >     handler;
};

}

#else

#include <bits/types/struct_iovec.h>
#include <netinet/in.h>

namespace rioring {

class object_base;

enum class context_type {
    base,
    extra
};

struct io_context {
    enum class io_type {
        unknown,
        accept,
        read,
        write,
        shutdown,
    };

    io_context() = default;
    virtual ~io_context() = default;
    virtual struct io_extra_context *to_extra() { return nullptr; }

    io_type         type{ io_type::unknown };
    iovec           iov{};
    std::shared_ptr< object_base >     handler;
};

struct io_extra_context : public io_context {
    io_extra_context() {
        msg.msg_name = &addr;
        msg.msg_namelen = sizeof( sockaddr_storage );
    }

    ~io_extra_context() override = default;
    io_extra_context *to_extra() override   { return this; }

    msghdr              msg{};
    sockaddr_storage    addr{};
};

}

#endif

#endif //RIORING_CONTEXT_H
