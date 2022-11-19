//
// Created by newbiediver on 22. 11. 10.
//

#include <chrono>
#include <future>
#include <condition_variable>
#include <gtest/gtest.h>
#include "rioring.h"
#include "rioring/udp_server.h"
#include "rioring/address_resolver.h"

using namespace std::chrono_literals;

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

#ifdef WIN32
TEST(BasicTest, InitWinsock) {
    ASSERT_TRUE( rioring::io_service::initialize_winsock() );
}
#endif

TEST(BasicTest, TcpTesting) {
    const char *testing_word = "Hello World";

    rioring::io_service server_io;
    ASSERT_TRUE( server_io.run( 2 ) );

    auto tcp_server = rioring::tcp_server::create( &server_io );
    tcp_server->set_accept_event( on_accept );
    ASSERT_NE( tcp_server, nullptr );
    ASSERT_TRUE( tcp_server->run( 32800 ) );

    rioring::io_service client_io;
    ASSERT_TRUE( client_io.run( 2 ) );

    std::this_thread::sleep_for( 500ms );

    rioring::tcp_socket_ptr tcp_socket = rioring::tcp_socket::create( &client_io );
    ASSERT_NE( tcp_socket, nullptr );

    tcp_socket->set_close_event( on_client_close );
    tcp_socket->set_receive_event( on_tcp_client_read );
    ASSERT_TRUE( tcp_socket->connect( "localhost", 32800 ) );

    auto prom_tcp = tcp_feedback.get_future();
    EXPECT_TRUE( tcp_socket->send( testing_word, strlen( testing_word ) ) );

    ASSERT_NE( prom_tcp.wait_for( 5s ), std::future_status::timeout );
    EXPECT_STREQ( prom_tcp.get().c_str(), testing_word );

    tcp_socket->close();
    std::unique_lock< std::mutex > ul{ lock };
    socket_closed.wait_for( ul, 2s );
    EXPECT_FALSE( tcp_socket->connected() );

    client_io.stop();
    EXPECT_EQ( client_io.running_count(), 0 );

    tcp_server->stop();
    server_io.stop();

    EXPECT_EQ( server_io.running_count(), 0 );
}

TEST(BasicTest, UDPTesting) {
    const char *testing_word = "Hello World";

    rioring::io_service server_io;
    ASSERT_TRUE( server_io.run(2 ) );

    auto udp_server = rioring::udp_server::create( &server_io );
    udp_server->set_receive_event( on_udp_server_read );
    ASSERT_NE( udp_server, nullptr );
    ASSERT_TRUE( udp_server->run( 38100 ) );

    rioring::io_service client_io;
    ASSERT_TRUE( client_io.run( 2 ) );

    std::this_thread::sleep_for( 500ms );

    rioring::udp_socket_ptr udp_socket = rioring::udp_socket::create( &client_io );
    ASSERT_NE( udp_socket, nullptr );

    udp_socket->set_close_event( on_client_close );
    udp_socket->set_receive_event( on_udp_client_read );
    ASSERT_TRUE( udp_socket->activate( rioring::udp_socket::family_type::ipv4 ) );

    sockaddr_in server_addr{};
    rioring::convert_socket_address( "localhost", 38100, &server_addr );

    auto prom_udp = udp_feedback.get_future();

    EXPECT_TRUE( udp_socket->send_to( testing_word, strlen( testing_word ), (sockaddr*)&server_addr ) );
    ASSERT_NE( prom_udp.wait_for( 5s ), std::future_status::timeout );
    EXPECT_STREQ( prom_udp.get().c_str(), testing_word );

    udp_socket->close();

    client_io.stop();
    EXPECT_EQ( client_io.running_count(), 0 );

    udp_server->stop();
    server_io.stop();

    EXPECT_EQ( server_io.running_count(), 0 );
}

#ifdef WIN32
TEST(BasicTest, DeinitWinsock) {
    rioring::io_service::deinitialize_winsock();
}
#endif