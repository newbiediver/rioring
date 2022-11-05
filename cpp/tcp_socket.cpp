//
// Created by newbiediver on 22. 11. 3.
//

#include <netinet/tcp.h>
#include <csignal>
#include "rioring/tcp_socket.h"
#include "rioring/address_resolver.h"

namespace rioring {

tcp_socket::tcp_socket( rioring::io_service *io ) : socket_base{ io } {

}

tcp_socket::tcp_socket( rioring::io_service *io, int sock ) : socket_base{ io, sock } {

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
        if ( s < 0 ) return false;
        if ( ::connect( s, a->ai_addr, a->ai_addrlen ) != 0 ) {
            ::close( s );
            return false;
        }

        socket_handler = s;
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

}