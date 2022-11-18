//
// Created by newbiediver on 22. 11. 15.
//

#include <thread>
#include <chrono>
#include "rioring/udp_server.h"

using namespace std::chrono_literals;

namespace rioring {

udp_server::udp_server( io_service *io ) : udp_socket{ io } {

}

udp_server_ptr udp_server::create( io_service *io ) {
    auto socket = new udp_server{ io };
    return udp_server_ptr{ socket };
}

bool udp_server::run( unsigned short port ) {
    auto error_occur = [&] {
        io_error( std::make_error_code( std::errc( errno ) ) );
    };

    socket_handler = socket( AF_INET6, SOCK_DGRAM, IPPROTO_UDP );
    if ( socket_handler == -1 ) {
        error_occur();
        return false;
    }

    constexpr int reuse = RIORING_REUSE_ADDR, v6only = RIORING_ONLY_IPV6;
    setsockopt( socket_handler, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( int ) );
    setsockopt( socket_handler, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof( int ) );

    sockaddr_in6 in{};
    in.sin6_family = AF_INET6;
    in.sin6_port = htons( port );
    in.sin6_addr = in6addr_any;

    if ( bind( socket_handler, reinterpret_cast< ::sockaddr* >( &in ), sizeof( in ) ) < 0 ) {
        error_occur();
        return false;
    }

    on_active();
    submit_receiving();

    return true;
}

void udp_server::stop() {
    close();
    while ( socket_handler != 0 ) {
        std::this_thread::sleep_for( 1ms );
    }
}

}