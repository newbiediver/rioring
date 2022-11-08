//
// Created by newbiediver on 22. 10. 31.
//

#include <chrono>

using namespace std::chrono_literals;

#ifdef WIN32

#include <cassert>
#include <array>
#include <chrono>
#include "rioring/io_service.h"
#include "rioring/io_context.h"
#include "rioring/socket_base.h"

namespace rioring {

constexpr DWORD MAX_RIO_RESULT = 256;
constexpr DWORD MAX_SEND_RQ_SIZE_PER_SOCKET = 32;
constexpr DWORD MAX_RECV_RQ_SIZE_PER_SOCKET = 32;
constexpr DWORD MAX_CLIENT_PER_RIO_THREAD = 2560;
constexpr DWORD MAX_CQ_SIZE_PER_RIO_THREAD = ( MAX_SEND_RQ_SIZE_PER_SOCKET + MAX_RECV_RQ_SIZE_PER_SOCKET ) * MAX_CLIENT_PER_RIO_THREAD;

static bool g_init_winsock = false;

static void start_winsock() {
    WSAData data{};
    int result = WSAStartup( MAKEWORD( 2, 2 ), &data );
    if ( result != 0 ) {
        int err = WSAGetLastError();
        int a = 0;
    }
    g_init_winsock = true;
}

static void cleanup_winsock() {
    WSACleanup();
    g_init_winsock = false;
}

io_service::io_service() : context_pool{ 1000 } {
    if ( !g_init_winsock ) {
        start_winsock();
    }
}

io_service::~io_service() noexcept {
    if ( g_init_winsock ) {
        cleanup_winsock();
    }
}

bool io_service::load_rio() {
    GUID id = WSAID_MULTIPLE_RIO;
    unsigned long size = 0;

    SOCKET tmp = WSASocketW( AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_REGISTERED_IO );
    if ( tmp == INVALID_SOCKET ) {
        auto er = WSAGetLastError();
        return false;
    }

    if ( WSAIoctl( tmp, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER, &id, sizeof( id ),
                   &rio, sizeof( rio ), &size,
                   nullptr, nullptr ) ) {
        closesocket( tmp );
        return false;
    }

    closesocket( tmp );
    return true;
}

bool io_service::run( int concurrency ) {
    if ( concurrency < 1 ) return false;
    if ( !load_rio() ) return false;

    io_array.reserve( concurrency );
    tg.run_object( this, concurrency );

    while( concurrency > running_io ) {
        std::this_thread::sleep_for( 1ms );
    }

    return true;
}

void io_service::stop() {
    stop_thread();
    io_array.clear();
    tg.wait_for_terminate();
}

io_context *io_service::allocate_context() {
    return context_pool.pop();
}

void io_service::deallocate_context( io_context *ctx ) {
    ctx->handler.reset();
    ctx->rq = nullptr;

    context_pool.push( ctx );
}

bool io_service::submit( io_context *ctx ) const {
    if ( ctx->type == io_context::io_type::read ) {
        rio.RIOReceive( ctx->rq, ctx, 1, 0, ctx );
    } else {
        rio.RIOSend( ctx->rq, ctx, 1, 0, ctx );
    }

    return true;
}

void io_service::on_thread() {
    RIO_CQ cq = rio.RIOCreateCompletionQueue( MAX_CQ_SIZE_PER_RIO_THREAD, nullptr );
    if ( cq == RIO_INVALID_CQ ) {
        assert( cq != RIO_INVALID_CQ && "Failed to create completion queue" );
        int *p = nullptr;
        *p = 0;
    }

    io_array.push_back( cq );
    running_io++;

    io( cq );
    rio.RIOCloseCompletionQueue( cq );

    running_io--;
}

void io_service::io( RIO_CQ cq ) {
    std::array< RIORESULT, MAX_RIO_RESULT > result{};

    while ( !thread_stopped() ) {
        memset( &result[0], 0, sizeof( RIORESULT ) );
        auto cnt = rio.RIODequeueCompletion( cq, &result[0], MAX_RIO_RESULT );
        if ( cnt == 0 ) {
            std::this_thread::sleep_for( 1ms );
            continue;
        } else if ( cnt == RIO_CORRUPT_CQ ) {
            assert( cnt != RIO_CORRUPT_CQ && "What?" );
            int *p = nullptr;
            *p = 0;
        }

        for ( decltype( cnt ) i = 0; i < cnt; ++i ) {
            auto context = reinterpret_cast< io_context* >( result[i].RequestContext );
            auto connector = context->handler;
            auto bytes_transferred = result[i].BytesTransferred;

            switch ( context->type ) {
            case io_context::io_type::read:
                if ( bytes_transferred == 0 ) {
                    connector->io_shutdown();
                } else {
                    connector->io_received( bytes_transferred );
                }
                break;
            case io_context::io_type::write:
                connector->io_sent( bytes_transferred );
                break;
            }

            deallocate_context( context );
        }
    }
}

RIO_BUFFERID io_service::register_buffer( void *buffer, size_t size ) const {
    return rio.RIORegisterBuffer( reinterpret_cast< char* >( buffer ), size );
}

void io_service::unregister_buffer( RIO_BUFFERID id ) const {
    rio.RIODeregisterBuffer( id );
}

RIO_RQ io_service::create_request_queue( SOCKET s ) {
    auto h1 = static_cast< size_t >( s );
    auto h2 = h1 + 1;

    RIO_CQ & recv = io_array[ h1 % io_array.size() ];
    RIO_CQ & send = io_array[ h2 % io_array.size() ];

    return rio.RIOCreateRequestQueue( s, MAX_RECV_RQ_SIZE_PER_SOCKET, 1,
                                      MAX_SEND_RQ_SIZE_PER_SOCKET, 1, recv, send, nullptr );
}

}

#else

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

    while( concurrency > running_io ) {
        std::this_thread::sleep_for( 1ms );
    }

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
    size_t index = ctx->handler->object_id() % io_array.size();
    auto ring = io_array[ index ];
    io_uring_sqe *sqe = io_uring_get_sqe( ring );

    if ( !sqe ) {
        ctx->handler->io_error( std::make_error_code( std::errc( EIO ) ) );
        return false;
    }

    switch ( ctx->type ) {
    case io_context::io_type::accept:
        if ( auto server = to_tcp_server_ptr( ctx->handler ); server != nullptr ) {
            io_uring_prep_accept( sqe,
                                  server->server_socket,
                                  reinterpret_cast< sockaddr * >( &server->addr6 ),
                                  &server->addr_len,
                                  0 );
        }
        break;
    case io_context::io_type::read:
        if ( auto socket = to_socket_ptr( ctx->handler ); socket != nullptr ) {
            io_uring_prep_readv( sqe, socket->socket_handler, &ctx->iov, 1, 0 );
        }
        break;
    case io_context::io_type::write:
        if ( auto socket = to_socket_ptr( ctx->handler ); socket != nullptr ) {
            io_uring_prep_writev( sqe, socket->socket_handler, &ctx->iov, 1, 0 );
        }
        break;
    case io_context::io_type::shutdown:
        if ( auto socket = to_socket_ptr( ctx->handler ); socket != nullptr ) {
            io_uring_prep_shutdown( sqe, socket->socket_handler, 0 );
        } else {
            if ( auto server = to_tcp_server_ptr( ctx->handler ); server != nullptr ) {
                io_uring_prep_shutdown( sqe, server->server_socket, 0 );
            }
        }
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

    running_io++;

    io( &ring );
    io_uring_queue_exit( &ring );

    running_io--;
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
        auto socket = to_socket_ptr( context->handler );

        switch ( context->type ) {
        case io_context::io_type::accept:
            if ( cqe->res > 0 ) {
                if ( auto server = to_tcp_server_ptr( context->handler ); server != nullptr ) {
                    server->io_accepting( cqe->res, reinterpret_cast< sockaddr * >( &server->addr6 ) );
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
            if ( socket ) {
                if ( socket->socket_handler ) {
                    int fd = socket->socket_handler;
                    socket->socket_handler = 0;

                    ::shutdown( fd, SHUT_RDWR );
                    ::close( fd );
                }
            } else {
                if ( auto server = to_tcp_server_ptr( context->handler ); server != nullptr ) {
                    int fd = server->server_socket;
                    server->server_socket = 0;

                    ::shutdown( fd, SHUT_RDWR );
                    ::close( fd );
                }
            }

            break;
        }

        deallocate_context( context );
        io_uring_cqe_seen( ring, cqe );
    }
}

}

#endif