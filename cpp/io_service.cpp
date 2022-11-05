//
// Created by newbiediver on 22. 10. 31.
//

#include <cassert>
#include "rioring/io_service.h"
#include "rioring/io_context.h"
#include "rioring/tcp_server.h"

namespace rioring {

io_service::io_service() : context_pool{ 100 } {}

bool io_service::run( int concurrency ) {
    if ( concurrency < 1 ) return false;

    io_array.reserve( concurrency );
    tg.run_object( this, concurrency );

    return true;
}

void io_service::stop() {
    for ( auto ring : io_array ) {
        io_uring_sqe *sqe = io_uring_get_sqe( ring );
        sqe->user_data = 0;

        io_uring_prep_cancel( sqe, nullptr, 0 );
        io_uring_submit( ring );
    }

    io_array.clear();
    tg.wait_for_terminate();
}

io_context *io_service::allocate_context() {
    return context_pool.pop();
}

void io_service::deallocate_context( io_context *ctx ) {
    ctx->handler.reset();
    ctx->iov.iov_base = nullptr;
    ctx->iov.iov_len = 0;

    context_pool.push( ctx );
}

bool io_service::submit( io_context *ctx ) {
    size_t index = ctx->handler->socket_handler % io_array.size();
    auto ring = io_array[ index ];
    io_uring_sqe *sqe = io_uring_get_sqe( ring );

    if ( !sqe ) {
        ctx->handler->io_error( std::make_error_code( std::errc( EIO ) ) );
        return false;
    }

    switch ( ctx->type ) {
    case io_context::io_type::accept:
        io_uring_prep_accept( sqe,
                              ctx->handler->socket_handler,
                              reinterpret_cast< sockaddr * >( &ctx->handler->addr6 ),
                              &ctx->handler->addr_len,
                              0 );
        break;
    case io_context::io_type::read:
        io_uring_prep_readv( sqe, ctx->handler->socket_handler, &ctx->iov, 1, 0 );
        break;
    case io_context::io_type::write:
        io_uring_prep_writev( sqe, ctx->handler->socket_handler, &ctx->iov, 1, 0 );
        break;
    case io_context::io_type::shutdown:
        io_uring_prep_shutdown( sqe, ctx->handler->socket_handler, 0 );
        break;
    }

    io_uring_sqe_set_data( sqe, ctx );
    io_uring_submit( ring );

    return true;
}

void io_service::on_thread() {
    io_uring ring{};
    io_uring_params params{};

    if ( auto r = io_uring_queue_init_params( 1024, &ring, &params );
            r != 0 || !( params.features & IORING_FEAT_FAST_POLL ) ) {
        assert( ( params.features & IORING_FEAT_FAST_POLL ) && "Kernel does not support fast poll!" );
        int *p = nullptr;
        *p = 0;
    } else {
        std::scoped_lock sc{ lock };
        io_array.push_back( &ring );
    }

    io( &ring );
    io_uring_queue_exit( &ring );
}

// io_uring 은 socket 의 close를 direct 로 처리하면 안됨. (submit 필요)
// io_uring should not directly treat the socket's close.
void io_service::io( io_uring *ring ) {
    io_uring_cqe *cqe;

    while ( true ) {
        if ( io_uring_wait_cqe( ring, &cqe ) < 0 ) {
            continue;
        }

        if ( !cqe->user_data ) {
            io_uring_cqe_seen( ring, cqe );
            break;
        }

        auto context = reinterpret_cast< io_context * >( cqe->user_data );
        auto socket = context->handler;

        switch ( context->type ) {
        case io_context::io_type::accept:
            if ( cqe->res > 0 ) {
                if ( auto server = to_tcp_server_ptr( socket ); server != nullptr ) {
                    server->io_accepting( cqe->res, reinterpret_cast< sockaddr * >( &socket->addr6 ) );
                }
            }
            break;
        case io_context::io_type::read:
            if ( cqe->res > 0 ) {
                socket->io_received( cqe->res );
            } else {
                socket->io_shutdown();
            }
            break;
        case io_context::io_type::write:
            if ( cqe->res > 0 ) {
                socket->io_sent( cqe->res );
            } else {
                socket->io_error( std::make_error_code( std::errc( -errno ) ) );
            }
            break;
        case io_context::io_type::shutdown:
            if ( socket->socket_handler ) {
                int fd = socket->socket_handler;
                socket->socket_handler = 0;

                ::shutdown( fd, SHUT_RDWR );
                ::close( fd );
            }
            break;
        }

        deallocate_context( context );
        io_uring_cqe_seen( ring, cqe );
    }
}

}