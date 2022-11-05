//
// Created by newbiediver on 22. 11. 5.
//

#include <cstring>
#include <arpa/inet.h>
#include "rioring/tcp_server.h"
#include "rioring/tcp_socket.h"
#include "rioring/io_service.h"

namespace rioring {

tcp_server::tcp_server( io_service *io ) : socket_base{ io } {

}

// 실제 tcp_server 를 생성하는 api
 tcp_server_ptr tcp_server::create( io_service *io ) {
    auto pointer = new tcp_server{ io };
    return tcp_server_ptr{ pointer };
}

void tcp_server::set_accept_event( tcp_server::accept_event event ) {
    new_connect_event = event;
}

bool tcp_server::run( unsigned short port ) {
    auto error_occur = [&] {
        io_error( std::make_error_code( std::errc( errno ) ) );
    };

    socket_handler = socket( AF_INET6, SOCK_STREAM, 0 );
    if ( socket_handler == -1 ) {
        error_occur();
        return false;
    }

    int on = 1, off = 0;
    setsockopt( socket_handler, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( int ) );
    setsockopt( socket_handler, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof( int ) );

    ::sockaddr_in6 in{};
    memset( &in, 0, sizeof( in ) );

    in.sin6_family = AF_INET6;
    in.sin6_port = htons( port );
    in.sin6_addr = in6addr_any;

    if ( bind( socket_handler, reinterpret_cast< ::sockaddr* >( &in ), sizeof( in ) ) < 0 ) {
        error_occur();
        return false;
    }

    if ( listen( socket_handler, 5 ) < 0 ) {
        error_occur();
        return false;
    }

    submit_accept();

    return true;
}

void tcp_server::stop() {
    auto ctx = current_io->allocate_context();
    ctx->handler = shared_from_this();
    ctx->type = io_context::io_type::shutdown;

    current_io->submit( ctx );
}

void tcp_server::submit_accept() {
    auto ctx = current_io->allocate_context();
    ctx->handler = shared_from_this();
    ctx->type = io_context::io_type::accept;

    current_io->submit( ctx );
}

void tcp_server::io_accepting( int new_socket, struct ::sockaddr *addr ) {
    if ( addr->sa_family == AF_INET ) {
        auto in = reinterpret_cast< ::sockaddr_in* >( addr );
        remote_port_number = ntohs( in->sin_port );
        inet_ntop( AF_INET, &in->sin_addr, std::data( remote_v4_addr_string ), std::size( remote_v4_addr_string ) );
    } else {
        auto in = reinterpret_cast< ::sockaddr_in6* >( addr );
        remote_port_number = ntohs( in->sin6_port );
        inet_ntop( AF_INET6, &in->sin6_addr, std::data( remote_v6_addr_string ), std::size( remote_v6_addr_string ) );

        const auto bytes = in->sin6_addr.s6_addr;
        const auto v4 = reinterpret_cast< const ::in_addr_t* >( bytes + 12 );
        inet_ntop( AF_INET, v4, std::data( remote_v4_addr_string ), std::size( remote_v4_addr_string ) );
    }

    socket_ptr socket = tcp_socket::create( current_io, new_socket );
    on_accept( socket );

    socket->on_active();
}

void tcp_server::on_accept( socket_ptr &new_socket ) {
    if ( new_connect_event ) {
        auto new_tcp_socket = to_tcp_socket_ptr( new_socket );
        new_connect_event( new_tcp_socket );
    }

    submit_accept();
}

}