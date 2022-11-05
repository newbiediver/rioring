//
// Created by newbiediver on 22. 11. 2.
//

#ifndef RIORING_CONTEXT_H
#define RIORING_CONTEXT_H

#include <memory>
#include <bits/types/struct_iovec.h>

namespace rioring {

class socket_base;

struct io_context {
    enum class io_type {
        accept,
        read,
        write,
        shutdown
    };

    io_type         type;
    iovec           iov{};
    std::shared_ptr< socket_base >     handler;
};

}



#endif //RIORING_CONTEXT_H
