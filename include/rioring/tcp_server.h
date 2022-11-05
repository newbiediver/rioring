//
// Created by newbiediver on 22. 11. 5.
//

#ifndef RIORING_TCP_SERVER_H
#define RIORING_TCP_SERVER_H

#include "tcp_socket.h"

namespace rioring {

class io_service;
class tcp_server;

typedef std::shared_ptr< tcp_server > tcp_server_ptr;

// Transform to downstream
inline tcp_server_ptr to_tcp_server_ptr( socket_ptr &s ) {
    return std::dynamic_pointer_cast< tcp_server, socket_base >( s );
}

class tcp_server : public socket_base {
public:
    typedef void (*accept_event) (tcp_socket_ptr&);

    tcp_server() = delete;
    ~tcp_server() override = default;

    static tcp_server_ptr create( io_service *io );
    void set_accept_event( accept_event event );

    bool run( unsigned short port );
    void stop();

protected:
    void io_accepting( int new_socket, struct ::sockaddr *addr );
    void on_accept( socket_ptr &new_socket );

private:
    explicit tcp_server( io_service *io );
    void submit_accept();

private:
    friend class io_service;
    std::function< void( tcp_socket_ptr& ) >    new_connect_event;
};

}

#endif //RIORING_TCP_SERVER_H
