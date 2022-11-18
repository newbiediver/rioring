//
// Created by newbiediver on 22. 11. 5.
//

#include <utility>

#include "rioring/io_service.h"
#include "rioring/socket_object.h"

namespace rioring {

void socket_object::set_receive_event( socket_object::receive_event event ) {
    recv_event = std::move( event );
}

void socket_object::set_send_complete_event( socket_object::send_event event ) {
    send_complete_event = std::move( event );
}

void socket_object::set_close_event( socket_object::close_event event ) {
    socket_close_event = std::move( event );
}

socket_ptr socket_object::cast_socket_ptr() {
    return std::dynamic_pointer_cast< socket_object, object_base >( shared_from_this() );
}

sockaddr *socket_object::socket_address() const {
    return (sockaddr *)&addr_storage;
}

void socket_object::on_send_complete() {
    if ( shutdown_gracefully ) {
        submit_shutdown();
    }
}

void socket_object::close( socket_object::close_type type ) {
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

void socket_object::io_received( size_t bytes_transferred, sockaddr *addr ) {
    recv_buffer.push( std::data( recv_bind_buffer ), bytes_transferred );

    submit_receiving();
    if ( recv_event ) {
        auto ptr = cast_socket_ptr();
        recv_event( ptr, &recv_buffer, addr );
    }
}

void socket_object::io_sent( size_t bytes_transferred, sockaddr *addr ) {
    std::scoped_lock sc{ lock };
    send_buffer.pop( bytes_transferred );
    send_buffer.elevate();

    if ( !send_buffer.empty() ) {
        submit_sending( addr );
    } else {
        if ( send_complete_event ) {
            auto ptr = cast_socket_ptr();
            send_complete_event( ptr );
        }

        on_send_complete();
    }
}

void socket_object::io_shutdown() {
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
        auto ptr = cast_socket_ptr();
        socket_close_event( ptr );
    }
}

#ifdef WIN32
void socket_object::on_active() {
    // RQ 생성 및 버퍼들을 register 해줌
    // RQ creation and register buffers
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

void socket_object::submit_shutdown() {
    auto ctx = current_io->allocate_context( context_type::base );
    ctx->handler = shared_from_this();
    ctx->type = io_context::io_type::shutdown;

    current_io->submit( ctx );
}

}