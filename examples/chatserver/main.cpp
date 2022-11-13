#include <iostream>
#include <rioring.h>
#include <condition_variable>
#include <csignal>
#include "receiver.h"
#include "pool.h"

using namespace rioring;

std::mutex lock;
std::condition_variable app_stop;

void signals( int ) {
    app_stop.notify_all();
}

#ifdef WIN32
BOOL WINAPI extra_signal_for_win32( unsigned long type ) {
    if ( type == CTRL_C_EVENT ) {
        app_stop.notify_all();
    }

    return TRUE;
}
#endif


int main() {
    io_service io;

    if ( !io.run( 4 ) ) {
        std::cerr << "Error to run io service" << std::endl;
        return 1;
    }

    std::cout << "io running" << std::endl;

    tcp_server_ptr server = tcp_server::create( &io );
    server->set_accept_event( []( auto &&new_socket ) {
        event_receiver::on_accept( std::forward< decltype( new_socket ) >( new_socket ) );
    } );
    server->set_error_callback( []( auto &&obj, auto &&ec ) {
        event_receiver::on_error( std::forward< decltype( obj ) >( obj ), std::forward< decltype( ec ) >( ec ) );
    } );

    if ( !server->run( 8000 ) ) {
        std::cerr << "Error to run tcp server" << std::endl;
        return 1;
    }

    std::cout << "server running" << std::endl;

    std::signal( SIGTERM, signals );
    std::signal( SIGINT, signals );
#ifdef WIN32
    SetConsoleCtrlHandler( extra_signal_for_win32, TRUE );
#endif

    std::unique_lock ul{ lock };
    app_stop.wait( ul );

    std::cout << "stopping server" << std::endl;

    auto &client_pool = get_pool();
    client_pool.leave_all();
    client_pool.wait_for_leave();

    server->stop();
    io.stop();

    return 0;
}
