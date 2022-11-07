//
// Created by newbiediver on 22. 11. 5.
//

#include "rioring/io_service.h"
#include "rioring/tcp_server.h"

#ifdef WIN32

namespace rioring {

tcp_server::tcp_server( io_service *io ) : current_io{ io } {

}

tcp_server_ptr tcp_server::create( io_service *io ) {
    auto ptr = new tcp_server{ io };
    return tcp_server_ptr{ ptr };
}

void tcp_server::set_error_callback( error_callback ec ) {
    error = ec;
}

void tcp_server::set_accept_event( accept_event event ) {
    accept_event_callback = event;
}

void tcp_server::error_occur( std::errc err ) {
    if ( error ) {
        auto ptr = shared_from_this();
        error( ptr, std::make_error_code( err ) );
    }
}

bool tcp_server::run( unsigned short port ) {
    server_socket = WSASocketW( AF_INET6, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_REGISTERED_IO );
    if ( server_socket == INVALID_SOCKET ) {
        error_occur( std::errc( errno ) );
        return false;
    }

    int on = 1, off = 0;
    setsockopt( server_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast< const char* >( &on ), sizeof( int ) );
    setsockopt( server_socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast< const char* >( &off ), sizeof( int ) );

    struct ::sockaddr_in6 dualStack{};
    memset( &dualStack, 0, sizeof( dualStack ) );
    dualStack.sin6_family = AF_INET6;
    dualStack.sin6_port = htons( port );
    dualStack.sin6_addr = in6addr_any;

    if ( bind( server_socket, reinterpret_cast< const struct sockaddr * >( &dualStack ), sizeof( dualStack ) ) == SOCKET_ERROR ) {
        error_occur( std::errc( errno ) );
        return false;
    }

    if ( listen( server_socket, SOMAXCONN ) == SOCKET_ERROR ) {
        error_occur( std::errc( errno ) );
        return false;
    }

    tg.run_object( this, 1 );
    return true;
}

void tcp_server::stop() {
    closesocket( server_socket );
    server_socket = INVALID_SOCKET;

    tg.wait_for_terminate();
}

void tcp_server::on_thread() {
    sockaddr_in6 in{};
    int size = sizeof( in );

    while ( true ) {
        SOCKET new_socket = WSAAccept( server_socket, reinterpret_cast< ::sockaddr* >( &in ), &size, nullptr, 0 );
        if ( new_socket == INVALID_SOCKET ) {
            break;
        }

        auto new_connector = tcp_socket::create( current_io, new_socket );
        new_connector->set_remote_info( reinterpret_cast< ::sockaddr* >( &in ) );

        if ( accept_event_callback ) {
            accept_event_callback( new_connector );
        }

        new_connector->on_active();
    }
}

}

#else

#include <cstring>
#include <arpa/inet.h>

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

#endif