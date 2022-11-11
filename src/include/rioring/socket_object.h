//
// Created by newbiediver on 22. 11. 2.
//

#ifndef RIORING_EVENT_HANDLER_H
#define RIORING_EVENT_HANDLER_H

#ifdef WIN32
#include <WS2tcpip.h>
#else
#include <netinet/in.h>
#endif
#include <array>
#include "object_base.h"
#include "thread_lock.h"
#include "io_context.h"
#include "io_buffer.h"
#include "io_double_buffer.h"

namespace rioring {

#define DATA_BUFFER_SIZE (1024*64)

class io_service;
class socket_object;

using socket_ptr = std::shared_ptr< socket_object >;

inline socket_ptr to_socket_ptr( object_ptr &s ) {
    return std::dynamic_pointer_cast< socket_object, object_base >( s );
}

class socket_object : public object_base {
public:
    using receive_event = void (*)(socket_ptr&, io_buffer*);
    using send_event = void (*)(socket_ptr&);
    using close_event = void (*)(socket_ptr&);

    socket_object() = delete;
    ~socket_object() override = default;

    explicit socket_object( io_service *io ) : current_io{ io } {}

#ifdef WIN32
    explicit socket_object( io_service *io, SOCKET sock ) : current_io{ io }, socket_handler{ sock } {}
#else
    explicit socket_object( io_service *io, int sock ) : current_io{ io }, socket_handler{ sock } {}
#endif

    [[nodiscard]] const char* remote_ipv4() const   { return std::data( remote_v4_addr_string ); }
    [[nodiscard]] const char* remote_ipv6() const   { return std::data( remote_v6_addr_string ); }
    [[nodiscard]] unsigned short remote_port() const { return remote_port_number; }

    void set_receive_event( receive_event event );
    void set_send_complete_event( send_event event );
    void set_close_event( close_event event );

protected:
    void io_received( size_t bytes_transferred );
    void io_sent( size_t bytes_transferred );
    void io_shutdown();

protected:
    virtual void submit_receiving() {}
    virtual void submit_sending() {}

#ifdef WIN32
    virtual void on_active();
#else
    virtual void submit_shutdown() {}
    virtual void on_active() {}
#endif

protected:
    virtual void on_shutdown() {}
    virtual void on_send_complete() {}

private:
    socket_ptr cast_socket_ptr();

protected:
    friend class io_service;
    friend class tcp_server;

    io_service      *current_io;

#ifdef WIN32
    SOCKET          socket_handler{ INVALID_SOCKET };
    RIO_RQ          request_queue{ nullptr };
    RIO_BUFFERID    recv_buffer_id{ nullptr }, send_buffer_id{ nullptr };
    std::array< unsigned char, DATA_BUFFER_SIZE > recv_bind_buffer{ 0 }, send_bind_buffer{ 0 };
#else
    int             socket_handler{ 0 };
    std::array< unsigned char, DATA_BUFFER_SIZE > recv_bind_buffer{ 0 };
#endif

    unsigned short  remote_port_number{ 0 };

    std::array< char, 16 >  remote_v4_addr_string{ 0 };
    std::array< char, 40 >  remote_v6_addr_string{ 0 };

    critical_section        lock;
    io_buffer               recv_buffer;
    io_double_buffer        send_buffer;

    sockaddr_in6    addr6{};
    socklen_t       addr_len{ sizeof( sockaddr_in6 ) };

    std::function< void( socket_ptr&, io_buffer* ) >                recv_event;
    std::function< void( socket_ptr& ) >                            send_complete_event;
    std::function< void( socket_ptr& ) >                            socket_close_event;
};

}

//#endif

#endif //RIORING_EVENT_HANDLER_H
