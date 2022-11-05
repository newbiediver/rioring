//
// Created by newbiediver on 22. 11. 2.
//

#ifndef RIORING_EVENT_HANDLER_H
#define RIORING_EVENT_HANDLER_H

#include <array>
#include <netinet/in.h>
#include <system_error>
#include <memory>
#include <functional>
#include "thread_lock.h"
#include "io_context.h"
#include "io_buffer.h"

namespace rioring {

#define DATA_BUFFER_SIZE (1024*64)

class io_service;

typedef std::shared_ptr< socket_base > socket_ptr;

class socket_base : public std::enable_shared_from_this< socket_base > {
public:
    typedef void (*receive_event) (socket_ptr&, io_buffer*);
    typedef void (*send_event) (socket_ptr&);
    typedef void (*close_event) (socket_ptr&);
    typedef void (*error_callback) (socket_ptr&, const std::error_code &);

    socket_base() = delete;
    virtual ~socket_base() = default;

    explicit socket_base( io_service *io ) : current_io{ io } {}
    explicit socket_base( io_service *io, int sock ) : current_io{ io }, socket_handler{ sock } {}

    [[nodiscard]] const char* remote_ipv4() const   { return std::data( remote_v4_addr_string ); }
    [[nodiscard]] const char* remote_ipv6() const   { return std::data( remote_v6_addr_string ); }
    [[nodiscard]] unsigned short remote_port() const { return remote_port_number; }

    void set_receive_event( receive_event event );
    void set_send_complete_event( send_event event );
    void set_close_event( close_event event );
    void set_error_callback( error_callback callback );

protected:
    void io_received( size_t bytes_transferred );
    void io_sent( size_t bytes_transferred );
    void io_shutdown();
    void io_error( const std::error_code &ec );

protected:
    void submit_receiving();
    void submit_sending();
    void submit_shutdown();

protected:
    virtual void on_active() {}
    virtual void on_send_complete() {}

protected:
    friend class io_service;
    friend class tcp_server;

    io_service      *current_io;

    int             socket_handler{ 0 };
    unsigned short  remote_port_number{ 0 };

    std::array< char, 16 >  remote_v4_addr_string{ 0 };
    std::array< char, 40 >  remote_v6_addr_string{ 0 };

    critical_section        lock;
    io_buffer               recv_buffer;
    io_double_buffer        send_buffer;

    std::array< unsigned char, DATA_BUFFER_SIZE > recv_bind_buffer{ 0 };

    sockaddr_in6    addr6{};
    socklen_t       addr_len{ sizeof( sockaddr_in6 ) };

    std::function< void( socket_ptr&, io_buffer* ) >                recv_event;
    std::function< void( socket_ptr& ) >                            send_complete_event;
    std::function< void( socket_ptr& ) >                            socket_close_event;
    std::function< void( socket_ptr&, const std::error_code& ) >    error_event;
};

}

#endif //RIORING_EVENT_HANDLER_H
