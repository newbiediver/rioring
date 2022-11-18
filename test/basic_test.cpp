//
// Created by newbiediver on 22. 11. 10.
//

#include <chrono>
#include <future>
#include <condition_variable>
#include <gtest/gtest.h>
#include "rioring.h"

using namespace std::chrono_literals;

rioring::io_service io;
rioring::tcp_server_ptr server;

void on_read( rioring::socket_ptr &socket, rioring::io_buffer *buffer, sockaddr *addr [[maybe_unused]] ) {
    std::array< unsigned char, 20 > buf{ 0 };
    auto tcp = rioring::to_tcp_socket_ptr( socket );
    if ( buffer->read( &buf[0], buffer->size() ) ) {
        tcp->send( &buf[0], buffer->size() );
        buffer->pop( buffer->size() );
    }
}

void on_accept( rioring::tcp_socket_ptr &new_socket ) {
    new_socket->set_receive_event( on_read );
}

TEST(BasicTest, ServerStarting) {
    ASSERT_TRUE( io.run(2) );

    server = rioring::tcp_server::create( &io );
    server->set_accept_event( on_accept );
    ASSERT_NE( server, nullptr );
    ASSERT_TRUE( server->run( 8000 ) );
}

std::promise< std::string > feedback;
std::mutex lock;
std::condition_variable socket_closed;

void on_client_read( rioring::socket_ptr &socket, rioring::io_buffer *buffer, sockaddr *addr [[maybe_unused]] ) {
    std::array< unsigned char, 20 > buf{ 0 };
    if ( buffer->read( &buf[0], buffer->size() ) ) {
        buffer->pop( buffer->size() );
        feedback.set_value( std::string{ reinterpret_cast< const char* >( &buf[0] ) } );
    }
}

void on_client_close( rioring::socket_ptr &socket ) {
    socket_closed.notify_all();
}

TEST(BasicTest, EchoTesting) {
    std::unique_lock< std::mutex > ul{ lock };
    rioring::io_service client_io;
    const char *testing_word = "Hello World";

    ASSERT_TRUE( client_io.run( 2 ) );

    rioring::tcp_socket_ptr socket = rioring::tcp_socket::create( &client_io );
    ASSERT_NE( socket, nullptr );

    socket->set_close_event( on_client_close );
    socket->set_receive_event( on_client_read );
    ASSERT_TRUE( socket->connect( "localhost", 8000 ) );

    auto prom = feedback.get_future();
    EXPECT_TRUE( socket->send( testing_word, strlen( testing_word ) ) );

    ASSERT_NE( prom.wait_for( 5s ), std::future_status::timeout );
    EXPECT_STREQ( prom.get().c_str(), testing_word );

    socket->close();
    socket_closed.wait_for( ul, 2s );
    EXPECT_FALSE( socket->connected() );

    client_io.stop();
    EXPECT_EQ( client_io.running_count(), 0 );
}

TEST(BasicTest, ServerStopping) {
    server->stop();
    io.stop();

    EXPECT_EQ( io.running_count(), 0 );
}