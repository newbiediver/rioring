//
// Created by newbiediver on 22. 11. 3.
//

#ifdef WIN32

#include "rioring/io_service.h"
#include "rioring/tcp_socket.h"
#include "rioring/address_resolver.h"
#include <mstcpip.h>
#include <csignal>

namespace rioring {

tcp_socket::tcp_socket( rioring::io_service *io ) : socket_base{ io } {

}

tcp_socket::tcp_socket( rioring::io_service *io, SOCKET sock ) : socket_base{ io, sock } {

}

tcp_socket_ptr tcp_socket::create( io_service *io ) {
    auto ptr = new tcp_socket{ io };
    return tcp_socket_ptr{ ptr };
}

tcp_socket_ptr tcp_socket::create( io_service *io, SOCKET sock ) {
    auto ptr = new tcp_socket{ io, sock };
    return tcp_socket_ptr{ ptr };
}

bool tcp_socket::connect( std::string_view address, unsigned short port ) {
    auto addr_info = resolve( resolve_type::tcp, address, port );
    if ( !addr_info ) return false;

    for ( auto a = addr_info; a != nullptr; a = a->ai_next ) {
        SOCKET s = WSASocketW( a->ai_family, a->ai_socktype, a->ai_protocol, nullptr, 0, WSA_FLAG_REGISTERED_IO );
        if ( s == INVALID_SOCKET ) {
            continue;
        }

        if ( ::connect( s, a->ai_addr, static_cast< int >( a->ai_addrlen ) ) == SOCKET_ERROR ) {
            closesocket( s );
            continue;
        }

        socket_handler = s;
        break;
    }

    freeaddrinfo( addr_info );
    on_active();

    return true;
}

void tcp_socket::close( tcp_socket::close_type type ) {
    if ( type == close_type::graceful ) {
        std::scoped_lock sc{ lock };
        if ( send_buffer.empty() ) {
            closesocket( socket_handler );
            socket_handler = INVALID_SOCKET;
        } else {
            shutdown_gracefully = true;
        }
    } else {
        closesocket( socket_handler );
        socket_handler = INVALID_SOCKET;
    }
}

bool tcp_socket::send( const void *bytes, size_t size ) {
    if ( shutdown_gracefully ) return false;
    if ( !connected() ) return false;

    std::scoped_lock sc{ lock };
    if ( send_buffer.empty() ) {
        send_buffer.push( static_cast< const unsigned char* >( bytes ), size );
        submit_sending();
    } else {
        send_buffer.push( static_cast< const unsigned char* >( bytes ), size );
    }

    return true;
}

void tcp_socket::on_active() {
    connected_flag = true;

    constexpr linger lingerArg{ 1, 0 };
    constexpr int arg = 1;
    tcp_keepalive keepAlive{};
    DWORD byteReturn = 0;
    keepAlive.onoff = 1;
    keepAlive.keepalivetime = 2000;
    keepAlive.keepaliveinterval = 1000;

    recv_buffer.assign( DATA_BUFFER_SIZE );
    send_buffer.assign( DATA_BUFFER_SIZE );

    setsockopt( socket_handler, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast< const char* >( &arg ), sizeof( int ) );
    setsockopt( socket_handler, SOL_SOCKET, SO_LINGER, reinterpret_cast< const char* >( &lingerArg ), sizeof( lingerArg ) );
    setsockopt( socket_handler, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast< const char* >( &arg ), sizeof( int ) );
    setsockopt( socket_handler, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast< const char* >( &arg ), sizeof( int ) );
    WSAIoctl( socket_handler, SIO_KEEPALIVE_VALS, &keepAlive, sizeof( tcp_keepalive ), nullptr, 0, &byteReturn, nullptr, nullptr );

    socket_base::on_active();

    submit_receiving();
}

void tcp_socket::on_shutdown() {
    connected_flag = false;
}

void tcp_socket::on_send_complete() {
    if ( shutdown_gracefully ) {
        closesocket( socket_handler );
        socket_handler = INVALID_SOCKET;
    }
}

void tcp_socket::submit_receiving() {
    auto ctx = current_io->allocate_context();
    ctx->handler = shared_from_this();
    ctx->type = io_context::io_type::read;
    ctx->rq = request_queue;
    ctx->Offset = 0;
    ctx->BufferId = recv_buffer_id;
    ctx->Length = DATA_BUFFER_SIZE;

    current_io->submit( ctx );
}

void tcp_socket::submit_sending() {
    auto size = std::min< size_t >( send_buffer.size(), DATA_BUFFER_SIZE );
    std::memcpy( std::data( send_bind_buffer ), *send_buffer, size );

    auto ctx = current_io->allocate_context();
    ctx->handler = shared_from_this();
    ctx->type = io_context::io_type::write;
    ctx->rq = request_queue;
    ctx->Offset = 0;
    ctx->BufferId = send_buffer_id;
    ctx->Length = size;

    current_io->submit( ctx );
}

void tcp_socket::set_remote_info( struct ::sockaddr *addr ) {
    if ( addr->sa_family == AF_INET ) {
        auto in = reinterpret_cast< ::sockaddr_in* >( addr );
        remote_port_number = ntohs( in->sin_port );
        inet_ntop( AF_INET, &in->sin_addr, std::data( remote_v4_addr_string ), std::size( remote_v4_addr_string ) );
    } else {
        auto in = reinterpret_cast< ::sockaddr_in6* >( addr );
        remote_port_number = ntohs( in->sin6_port );
        inet_ntop( AF_INET6, &in->sin6_addr, std::data( remote_v6_addr_string ), std::size( remote_v6_addr_string ) );

        const auto bytes = in->sin6_addr.s6_addr;
        const auto v4 = reinterpret_cast< const ::in_addr* >( bytes + 12 );
        inet_ntop( AF_INET, v4, std::data( remote_v4_addr_string ), std::size( remote_v4_addr_string ) );
    }

}

}

#else

#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <csignal>
#include "rioring/io_service.h"
#include "rioring/tcp_socket.h"
#include "rioring/address_resolver.h"

namespace rioring {

tcp_socket::tcp_socket( rioring::io_service *io ) : socket_object{ io } {

}

tcp_socket::tcp_socket( rioring::io_service *io, int sock ) : socket_object{ io, sock } {

}

tcp_socket_ptr tcp_socket::create( io_service *io ) {
    auto ptr = new tcp_socket{ io };
    return tcp_socket_ptr{ ptr };
}

tcp_socket_ptr tcp_socket::create( io_service *io, int sock ) {
    auto ptr = new tcp_socket{ io, sock };
    return tcp_socket_ptr{ ptr };
}

bool tcp_socket::connect( std::string_view address, unsigned short port ) {
    auto addr = resolve( resolve_type::tcp, address, port );
    if ( !addr ) return false;


    for ( auto a = addr; a != nullptr; a = a->ai_next ) {
        int s = socket( a->ai_family, a->ai_socktype, a->ai_protocol );
        if ( s < 0 ) {
            continue;
        }

        if ( ::connect( s, a->ai_addr, a->ai_addrlen ) != 0 ) {
            ::close( s );
            continue;
        }

        socket_handler = s;
        break;
    }

    freeaddrinfo( addr );
    on_active();

    return true;
}

void tcp_socket::close( tcp_socket::close_type type ) {
    if ( type == close_type::graceful ) {
        std::scoped_lock sc{ lock };
        if ( send_buffer.empty() ) {
            submit_shutdown();
        } else {
            shutdown_gracefully = true;
        }
    } else {
        submit_shutdown();
    }
}

bool tcp_socket::send( const void *bytes, size_t size ) {
    if ( shutdown_gracefully ) return false;
    if ( !connected() ) return false;

    std::scoped_lock sc{ lock };
    if ( send_buffer.empty() ) {
        send_buffer.push( static_cast< const unsigned char* >( bytes ), size );
        submit_sending();
    } else {
        send_buffer.push( static_cast< const unsigned char* >( bytes ), size );
    }

    return true;
}

void tcp_socket::on_active() {
    connected_flag = true;

    constexpr int arg = 1;
    constexpr int idle = 2, cnt = 3, interval = 2;
    constexpr linger lg{ 1, 0 };

    recv_buffer.assign( DATA_BUFFER_SIZE );
    send_buffer.assign( DATA_BUFFER_SIZE );

    setsockopt( socket_handler, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast< const void* >( &arg ), sizeof( int ) );
    setsockopt( socket_handler, SOL_SOCKET, SO_LINGER, &lg, sizeof( lg ) );
    setsockopt( socket_handler, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast< const void* >( &arg ), sizeof( int ) );
    setsockopt( socket_handler, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast< const void* >( &arg ), sizeof( int ) );
    setsockopt( socket_handler, SOL_TCP, TCP_KEEPIDLE, reinterpret_cast< const void* >( &idle ), sizeof( int ) );
    setsockopt( socket_handler, SOL_TCP, TCP_KEEPCNT, reinterpret_cast< const void* >( &cnt ), sizeof( int ) );
    setsockopt( socket_handler, SOL_TCP, TCP_KEEPINTVL, reinterpret_cast< const void* >( &interval ), sizeof( int ) );

    submit_receiving();
}

void tcp_socket::on_send_complete() {
    if ( shutdown_gracefully ) {
        submit_shutdown();
    }
}

void tcp_socket::on_shutdown() {
    connected_flag = false;
}

void tcp_socket::submit_receiving() {
    auto ctx = current_io->allocate_context();
    ctx->handler = shared_from_this();
    ctx->type = io_context::io_type::read;
    ctx->iov.iov_base = std::data( recv_bind_buffer );
    ctx->iov.iov_len = DATA_BUFFER_SIZE;

    current_io->submit( ctx );
}

void tcp_socket::submit_sending() {
    auto ctx = current_io->allocate_context();
    ctx->handler = shared_from_this();
    ctx->type = io_context::io_type::write;
    ctx->iov.iov_base = const_cast< unsigned char* >( *send_buffer );
    ctx->iov.iov_len = send_buffer.size();

    current_io->submit( ctx );
}

void tcp_socket::submit_shutdown() {
    auto ctx = current_io->allocate_context();
    ctx->handler = shared_from_this();
    ctx->type = io_context::io_type::shutdown;

    current_io->submit( ctx );
}

void tcp_socket::set_remote_info( struct ::sockaddr *addr ) {
    if ( addr->sa_family == AF_INET ) {
        auto in = reinterpret_cast< ::sockaddr_in* >( addr );
        remote_port_number = ntohs( in->sin_port );
        inet_ntop( AF_INET, &in->sin_addr, std::data( remote_v4_addr_string ), std::size( remote_v4_addr_string ) );
    } else {
        auto in = reinterpret_cast< ::sockaddr_in6* >( addr );
        remote_port_number = ntohs( in->sin6_port );
        inet_ntop( AF_INET6, &in->sin6_addr, std::data( remote_v6_addr_string ), std::size( remote_v6_addr_string ) );

        const auto bytes = in->sin6_addr.s6_addr;
        const auto v4 = reinterpret_cast< const ::in_addr* >( bytes + 12 );
        inet_ntop( AF_INET, v4, std::data( remote_v4_addr_string ), std::size( remote_v4_addr_string ) );
    }
}

}

#endif