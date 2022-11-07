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
    on_shutdown();

#ifdef WIN32
    if ( socket_handler != INVALID_SOCKET ) {
        closesocket( socket_handler );
        socket_handler = INVALID_SOCKET;
    }

    current_io->unregister_buffer( recv_buffer_id );
    current_io->unregister_buffer( send_buffer_id );
#endif

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

#ifdef WIN32
void socket_base::on_active() {
    request_queue = current_io->create_request_queue( socket_handler );
    if ( request_queue == RIO_INVALID_RQ ) {
        io_error( std::make_error_code( std::errc( errno ) ) );
        return;
    }

    recv_buffer_id = current_io->register_buffer( std::data( recv_bind_buffer ), std::size( recv_bind_buffer ) );
    if ( recv_buffer_id == RIO_INVALID_BUFFERID ) {
        io_error( std::make_error_code( std::errc( errno ) ) );
        return;
    }

    send_buffer_id = current_io->register_buffer( std::data( send_bind_buffer ), std::size( send_bind_buffer ) );
    if ( send_buffer_id == RIO_INVALID_BUFFERID ) {
        io_error( std::make_error_code( std::errc( errno ) ) );
    }
}
#endif

}