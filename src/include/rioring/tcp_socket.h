//
// Created by newbiediver on 22. 11. 3.
//

#ifndef RIORING_TCP_SOCKET_H
#define RIORING_TCP_SOCKET_H

#include "socket_object.h"

namespace rioring {

class io_service;
class tcp_socket;

using tcp_socket_ptr = std::shared_ptr< tcp_socket >;

// Transform to downstream
inline tcp_socket_ptr to_tcp_socket_ptr( const object_ptr &s ) {
    return std::dynamic_pointer_cast< tcp_socket, object_base >( s );
}

// Transform to downstream
inline tcp_socket_ptr to_tcp_socket_ptr( const socket_ptr &s ) {
    return std::dynamic_pointer_cast< tcp_socket, socket_object >( s );
}

class tcp_socket : public socket_object {
public:
    enum class close_type {
        graceful,
        force
    };
    tcp_socket() = delete;
    ~tcp_socket() override = default;

    static tcp_socket_ptr create( io_service *io );

#ifdef WIN32
    static tcp_socket_ptr create( io_service *io, SOCKET sock );
#else
    static tcp_socket_ptr create( io_service *io, int sock );
#endif

    bool connect( std::string_view address, unsigned short port );
    void set_user_data( void *data )                { user_data = data; }

    // 사용자의 데이터를 socket과 바인딩 시킬 때 사용.
    // Used to bind the user's data to socket.
    [[nodiscard]] void* get_user_data() const       { return user_data; }
    [[nodiscard]] bool connected() const            { return connected_flag; }

    void close( close_type type = close_type::graceful );
    bool send( const void *bytes, size_t size );

protected:
    explicit tcp_socket( io_service *io );

#ifdef WIN32
    explicit tcp_socket( io_service *io, SOCKET sock );
#else
    explicit tcp_socket( io_service *io, int sock );
    void submit_shutdown() override;
#endif

    void submit_receiving() override;
    void submit_sending() override;

    void on_shutdown() override;
    void on_active() override;
    void on_send_complete() override;

private:
    void set_remote_info( struct ::sockaddr *addr );

private:
    friend class tcp_server;

    void*           user_data{ nullptr };
    bool            connected_flag{ false };
    bool            shutdown_gracefully{ false };
};

}

#endif //RIORING_TCP_SOCKET_H
