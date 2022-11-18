//
// Created by newbiediver on 22. 11. 19.
//

#ifndef RIORING_UDP_SOCKET_H
#define RIORING_UDP_SOCKET_H

#include "socket_object.h"

namespace rioring {

class io_service;
class udp_socket;

using udp_socket_ptr = std::shared_ptr< udp_socket >;

// Transform to downstream
inline udp_socket_ptr to_udp_socket_ptr( const object_ptr &s ) {
    return std::dynamic_pointer_cast< udp_socket, object_base >( s );
}

// Transform to downstream
inline udp_socket_ptr to_udp_socket_ptr( const socket_ptr &s ) {
    return std::dynamic_pointer_cast< udp_socket, socket_object >( s );
}

class udp_socket : public socket_object {
public:
    udp_socket() = delete;
    ~udp_socket() override = default;
    explicit udp_socket( io_service *io );

    static udp_socket_ptr create( io_service *io );

    bool send_to( const void *bytes, size_t size, sockaddr *addr );

protected:
    void submit_receiving() override;
    void submit_sending( sockaddr *addr ) override;

    void on_active() override;
    void on_shutdown() override;
};

}

#endif //RIORING_UDP_SOCKET_H
