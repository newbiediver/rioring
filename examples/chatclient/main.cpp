#include <iostream>
#include <rioring/io_service.h>
#include <rioring/tcp_socket.h>
#include <cstring>
#include "scheme.h"

using namespace rioring;
using namespace std::chrono_literals;

void on_receive( socket_ptr &socket, io_buffer *buffer ) {
    std::array< unsigned char, 1024 > buf{ 0 };
    auto sc = reinterpret_cast< scheme* >( &buf[0] );

    while ( true ) {
        if ( !buffer->read( &buf[0], sizeof( scheme ) ) ) break;
        if ( !buffer->read( &buf[0], sc->size ) ) break;
        buffer->pop( sc->size );

        if ( sc->msg != resp_chat ) break;

        auto resp = reinterpret_cast< response_chat* >( sc );
        auto chat_message = reinterpret_cast< char* >( resp + 1 );

        if ( resp->chat_size > 0 ) {
            std::cout << resp->name << ": " << chat_message << std::endl;
        }
    }
}

void on_close( socket_ptr &obj ) {
    std::cout << "Disconnected" << std::endl;
}

int main() {
    io_service io;
    if ( !io.run( 2 ) ) {
        std::cerr << "Error to run io service" << std::endl;
        return 1;
    }

    std::cout << "io running" << std::endl;

    auto socket = tcp_socket::create( &io );
    socket->set_receive_event( on_receive );
    socket->set_close_event( on_close );

    if ( !socket->connect( "localhost", 8000 ) ) {
        std::cerr << "Connection refused" << std::endl;
        return 1;
    }

    std::string input;
    while ( true ) {
        std::cin >> input;
        if ( input == "exit" ) {
            break;
        }

        std::array< unsigned char, 1024 > buf{ 0 };
        auto request = reinterpret_cast< request_chat* >( &buf[0] );
        auto message = reinterpret_cast< char* >( request + 1 );
        new (request) request_chat();

        auto chat_size = std::min< size_t >( input.size(), 1000 );
        std::memcpy( message, input.c_str(), chat_size );
        request->chat_size = static_cast< int >( chat_size );
        request->size += chat_size;

        socket->send( request, request->size );
    }

    socket->close();
    while ( socket->connected() ) {
        std::this_thread::sleep_for( 1ms );
    }

    io.stop();

    return 0;
}
