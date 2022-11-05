//
// Created by newbiediver on 22. 11. 5.
//

#include "rioring/io_service.h"
#include "rioring/socket_base.h"

namespace rioring {

void socket_base::set_receive_event( socket_base::receive_event event ) {
    recv_event = event;
}

void socket_base::set_send_complete_event( socket_base::send_event event ) {
    send_complete_event = event;
}

void socket_base::set_close_event( socket_base::close_event event ) {
    socket_close_event = event;
}

void socket_base::set_error_callback( socket_base::error_callback callback ) {
    error_event = callback;
}

void socket_base::io_received( size_t bytes_transferred ) {
    recv_buffer.push( std::data( recv_bind_buffer ), bytes_transferred );

    submit_receiving();
    if ( recv_event ) {
        auto ptr = shared_from_this();
        recv_event( ptr, &recv_buffer );
    }
}

void socket_base::io_sent( size_t bytes_transferred ) {
    std::scoped_lock sc{ lock };
    send_buffer.pop( bytes_transferred );
    send_buffer.elevate();

    if ( !send_buffer.empty() ) {
        submit_sending();
    } else {
        if ( send_complete_event ) {
            auto ptr = shared_from_this();
            send_complete_event( ptr );
        }

        on_send_complete();
    }
}

void socket_base::io_shutdown() {
    if ( socket_close_event ) {
        auto ptr = shared_from_this();
        socket_close_event( ptr );
    }
}

void socket_base::io_error( const std::error_code &ec ) {
    if ( error_event ) {
        auto ptr = shared_from_this();
        error_event( ptr, ec );
    }
}

void socket_base::submit_receiving() {
    auto ctx = current_io->allocate_context();
    ctx->handler = shared_from_this();
    ctx->type = io_context::io_type::read;
    ctx->iov.iov_base = std::data( recv_bind_buffer );
    ctx->iov.iov_len = DATA_BUFFER_SIZE;

    current_io->submit( ctx );
}

void socket_base::submit_sending() {
    auto ctx = current_io->allocate_context();
    ctx->handler = shared_from_this();
    ctx->type = io_context::io_type::write;
    ctx->iov.iov_base = const_cast< unsigned char* >( *send_buffer );
    ctx->iov.iov_len = send_buffer.size();

    current_io->submit( ctx );
}

void socket_base::submit_shutdown() {
    auto ctx = current_io->allocate_context();
    ctx->handler = shared_from_this();
    ctx->type = io_context::io_type::shutdown;

    current_io->submit( ctx );
}

}