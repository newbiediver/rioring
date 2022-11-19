#include <iostream>
#include <rioring.h>
#include <rioring/udp_server.h>
#include <rioring/address_resolver.h>

void on_recv_from( rioring::socket_ptr &socket, rioring::io_buffer *buffer, sockaddr *addr ) {
    rioring::udp_server_ptr server = rioring::to_udp_server_ptr( socket );

    std::array< unsigned char, 512 > buf{ 0 };
    if ( buffer->read( &buf[0], buffer->size() ) ) {
        server->send_to( std::data( buf ), buffer->size(), addr );
        buffer->pop( buffer->size() );
    }

}

void on_error( rioring::object_ptr &obj, const std::error_code &ec ) {
    std::cout << ec << std::endl;
}

int main() {
    unsigned short port = 9999;
#ifdef WIN32
    rioring::io_service::initialize_winsock();
#endif

    rioring::io_service io;
    io.run( 2 );

    rioring::udp_server_ptr server = rioring::udp_server::create( &io );
    server->set_error_callback( on_error );
    server->set_receive_event( on_recv_from );
    if ( server->run( port ) ) {

    }

    std::string in;
    while ( true ) {
        std::cin >> in;
        if ( in == "exit" ) {
            break;
        }
    }

    server->stop();
    io.stop();

#ifdef WIN32
    rioring::io_service::deinitialize_winsock();
#endif

    return 0;
}
