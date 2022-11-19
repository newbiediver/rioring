//
// Created by newbiediver on 22. 11. 10.
//

#include <chrono>
#include <future>
#include <condition_variable>
#include <gtest/gtest.h>
#ifdef WIN32
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include "rioring.h"
#include "rioring/udp_server.h"

using namespace std::chrono_literals;

rioring::io_service io;
rioring::tcp_server_ptr tcp_server;
rioring::udp_server_ptr udp_server;

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

void on_udp_server_read( rioring::socket_ptr &socket, rioring::io_buffer *buffer, sockaddr *remote_addr ) {
    std::array< unsigned char, 20 > buf{ 0 };
    auto udp = rioring::to_udp_socket_ptr( socket );
    if ( buffer->read( &buf[0], buffer->size() ) ) {
        udp->send_to( &buf[0], buffer->size(), remote_addr );
        buffer->pop( buffer->size() );
    }
}

TEST(BasicTest, ServerStarting) {
    ASSERT_TRUE( io.run(2) );

    tcp_server = rioring::tcp_server::create( &io );
    tcp_server->set_accept_event( on_accept );
    ASSERT_NE( tcp_server, nullptr );
    ASSERT_TRUE( tcp_server->run( 8000 ) );

    udp_server = rioring::udp_server::create( &io );
    udp_server->set_receive_event( on_udp_server_read );
    ASSERT_NE( udp_server, nullptr );
    ASSERT_TRUE( udp_server->run( 8100 ) );
}

std::promise< std::string > tcp_feedback, udp_feedback;
std::mutex lock;
std::condition_variable socket_closed;

void on_tcp_client_read( rioring::socket_ptr &socket, rioring::io_buffer *buffer, sockaddr *addr [[maybe_unused]] ) {
    std::array< unsigned char, 20 > buf{ 0 };
    if ( buffer->read( &buf[0], buffer->size() ) ) {
        buffer->pop( buffer->size() );
        tcp_feedback.set_value( std::string{ reinterpret_cast< const char* >( &buf[0] ) } );
    }
}

void on_udp_client_read( rioring::socket_ptr &socket, rioring::io_buffer *buffer, sockaddr *addr [[maybe_unused]] ) {
    std::array< unsigned char, 20 > buf{ 0 };
    if ( buffer->read( &buf[0], buffer->size() ) ) {
        buffer->pop( buffer->size() );
        udp_feedback.set_value( std::string{ reinterpret_cast< const char* >( &buf[0] ) } );
    }
}

void on_client_close( rioring::socket_ptr &socket ) {
    socket_closed.notify_all();
}

TEST(BasicTest, TCPEchoTesting) {
    std::unique_lock< std::mutex > ul{ lock };
    rioring::io_service client_io;
    const char *testing_word = "Hello World";

    ASSERT_TRUE( client_io.run( 2 ) );

    rioring::tcp_socket_ptr tcp_socket = rioring::tcp_socket::create( &client_io );
    ASSERT_NE( tcp_socket, nullptr );

    tcp_socket->set_close_event( on_client_close );
    tcp_socket->set_receive_event( on_tcp_client_read );
    ASSERT_TRUE( tcp_socket->connect( "localhost", 8000 ) );

    auto prom_tcp = tcp_feedback.get_future();
    EXPECT_TRUE( tcp_socket->send( testing_word, strlen( testing_word ) ) );

    ASSERT_NE( prom_tcp.wait_for( 5s ), std::future_status::timeout );
    EXPECT_STREQ( prom_tcp.get().c_str(), testing_word );

    tcp_socket->close();
    socket_closed.wait_for( ul, 2s );
    EXPECT_FALSE( tcp_socket->connected() );

    client_io.stop();
    EXPECT_EQ( client_io.running_count(), 0 );
}

TEST(BasicTest, UDPEchoTesting) {
    rioring::io_service client_io;
    const char *testing_word = "Hello World";

    ASSERT_TRUE( client_io.run( 2 ) );

    rioring::udp_socket_ptr udp_socket = rioring::udp_socket::create( &client_io );
    ASSERT_NE( udp_socket, nullptr );

    udp_socket->set_close_event( on_client_close );
    udp_socket->set_receive_event( on_udp_client_read );
    ASSERT_TRUE( udp_socket->activate( rioring::udp_socket::family_type::ipv4 ) );

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons( 8100 );
#ifdef WIN32
    InetPton( AF_INET, "127.0.0.1", &server_addr.sin_addr.s_addr );
#else
    server_addr.sin_addr.s_addr = inet_addr( "127.0.0.1" );
#endif

    auto prom_udp = udp_feedback.get_future();

    EXPECT_TRUE( udp_socket->send_to( testing_word, strlen( testing_word ), (sockaddr*)&server_addr ) );
    ASSERT_NE( prom_udp.wait_for( 5s ), std::future_status::timeout );
    EXPECT_STREQ( prom_udp.get().c_str(), testing_word );

    udp_socket->close();

    client_io.stop();
    EXPECT_EQ( client_io.running_count(), 0 );
}

TEST(BasicTest, ServerStopping) {
    tcp_server->stop();
    udp_server->stop();
    io.stop();

    EXPECT_EQ( io.running_count(), 0 );
}