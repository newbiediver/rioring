//
// Created by newbiediver on 22. 11. 19.
//

#include <cstring>
#include "rioring/io_service.h"
#include "rioring/udp_socket.h"

namespace rioring {

udp_socket::udp_socket( io_service *io ) : socket_object{ io, socket_object::type::udp } {

}

udp_socket_ptr udp_socket::create( io_service *io ) {
    auto socket = new udp_socket{ io };
    return udp_socket_ptr{ socket };
}

void udp_socket::on_active() {
    recv_buffer.assign( RIORING_DATA_BUFFER_SIZE );
    send_buffer.assign( RIORING_DATA_BUFFER_SIZE );
}

void udp_socket::on_shutdown() {
    recv_buffer.destroy();
    send_buffer.destroy();
}

void udp_socket::submit_receiving() {
    auto ctx = current_io->allocate_context( context_type::extra );
    auto extra = ctx->to_extra();

    extra->handler = shared_from_this();
    extra->type = io_context::io_type::read;
    extra->iov.iov_base = std::data( recv_bind_buffer );
    extra->iov.iov_len = RIORING_DATA_BUFFER_SIZE;

    extra->msg.msg_iov = &extra->iov;
    extra->msg.msg_iovlen = 1;

    current_io->submit( extra );
}

void udp_socket::submit_sending( sockaddr *addr ) {
    auto ctx = current_io->allocate_context( context_type::extra );
    auto extra = ctx->to_extra();

    extra->handler = shared_from_this();
    extra->type = io_context::io_type::write;
    extra->iov.iov_base = const_cast< unsigned char* >( *send_buffer );
    extra->iov.iov_len = send_buffer.size();
    extra->msg.msg_iov = &extra->iov;
    extra->msg.msg_iovlen = 1;

    size_t addr_len = 0;
    switch ( addr->sa_family ) {
    case AF_INET:
        addr_len = sizeof( sockaddr_in );
        break;
    case AF_INET6:
        addr_len = sizeof( sockaddr_in6 );
        break;
    default:
        break;
    }

    if ( addr_len ) {
        std::memcpy( &extra->addr, addr, addr_len );
        current_io->submit( extra );
    }
}

bool udp_socket::send_to( const void *bytes, size_t size, sockaddr *addr ) {
    std::scoped_lock sc{ lock };
    if ( send_buffer.empty() ) {
        send_buffer.push( static_cast< const unsigned char* >( bytes ), size );
        submit_sending( addr );
    } else {
        send_buffer.push( static_cast< const unsigned char* >( bytes ), size );
    }
    return true;
}

}