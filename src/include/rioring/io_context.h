//
// Created by newbiediver on 22. 11. 2.
//

#ifndef RIORING_CONTEXT_H
#define RIORING_CONTEXT_H

#include <memory>
#include <mswsockdef.h>

#ifdef WIN32

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

namespace rioring {

class object_base;

struct io_context {
    enum class io_type {
        accept,
        read,
        write,
        shutdown,
    };

    io_type         type;
    iovec           iov{};
    std::shared_ptr< object_base >     handler;
};

}

#endif

#endif //RIORING_CONTEXT_H
