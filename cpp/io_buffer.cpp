//
// Created by newbiediver on 22. 11. 4.
//

#include <cstring>
#include <cstdlib>
#include <algorithm>
#include "rioring/io_buffer.h"

namespace rioring {

io_buffer::io_buffer( size_t size ) : buffer_size( size ), block_size( size ) {
    raw_buffer = static_cast< unsigned char* >( std::malloc( size ) );
}

io_buffer::~io_buffer() noexcept {
    destroy();
}

io_buffer::io_buffer( rioring::io_buffer &&other ) noexcept {
    raw_buffer = other.raw_buffer;
    buffer_size = other.buffer_size;
    buffer_offset = other.buffer_offset;
    block_size = other.block_size;

    other.raw_buffer = nullptr;
    other.buffer_offset = other.buffer_size = other.block_size = 0;
}

io_buffer &io_buffer::operator=( rioring::io_buffer &&other ) noexcept {
    raw_buffer = other.raw_buffer;
    buffer_size = other.buffer_size;
    buffer_offset = other.buffer_offset;
    block_size = other.block_size;

    other.raw_buffer = nullptr;
    other.buffer_offset = other.buffer_size = other.block_size = 0;

    return *this;
}

void io_buffer::assign( size_t size ) {
    if ( raw_buffer ) {
        std::free(raw_buffer );
    }

    raw_buffer = static_cast< unsigned char* >( std::malloc( size ) );
    buffer_size = block_size = size;
    buffer_offset = 0;
}

void io_buffer::destroy() {
    if ( raw_buffer ) {
        std::free( raw_buffer );
        raw_buffer = nullptr;

        buffer_offset = buffer_size = block_size = 0;
    }
}

bool io_buffer::read( unsigned char *buf, size_t size ) {
    if ( size > buffer_offset ) {
        return false;
    }

    std::memcpy( buf, raw_buffer, size );

    return true;
}

void io_buffer::pop( size_t size ) {
    if ( buffer_offset >= size ) {
        std::memmove( raw_buffer, raw_buffer + size, buffer_offset - size );
        buffer_offset -= size;
    }
}

void io_buffer::push( const unsigned char *buf, size_t size ) {
    while ( buffer_offset + size > buffer_size ) {
        auto new_size = buffer_size + block_size;
        while ( buffer_offset + size >= new_size ) {
            new_size += block_size;
        }

        on_overflow( new_size );
    }

    std::memcpy( raw_buffer + buffer_offset, buf, size );
    buffer_offset += size;
}

/*
 io_buffer 는 크기 이상의 push가 발생되면 버퍼를 realloc 하게 되므로
 크기 유지를 해야한다면 반드시 사전에 체크해서 push 여부를 가려야 함
 The io_buffer reallocates the buffer when push larger data than the size occurs.
 If you need to maintain the size, you must check in advance to determine whether to push or not.
*/
void io_buffer::on_overflow( size_t new_size ) {
    raw_buffer = static_cast< unsigned char* >( std::realloc( raw_buffer, new_size ) );
    buffer_size = new_size;
}


/*
 io_double_buffer: 두개의 io_buffer 를 운용하는 버퍼
 io_double_buffer: Has two io_buffer to operate.
*/
io_double_buffer::io_double_buffer( size_t size ) : main_buffer( size ) {

}

void io_double_buffer::assign( size_t size ) {
    main_buffer.assign( size );
}

void io_double_buffer::destroy() {
    main_buffer.destroy();
    sub_buffer.destroy();
}

bool io_double_buffer::read( unsigned char *buf, size_t size ) {
    return main_buffer.read( buf, size );
}

void io_double_buffer::pop( size_t size ) {
    return main_buffer.pop( size );
}

//
void io_double_buffer::push( const unsigned char *buf, size_t size ) {
    if ( !sub_buffer.empty() ) {
        sub_buffer.push( buf, size );
    } else if ( main_buffer.empty() ) {
        main_buffer.push( buf, size );
    } else if ( main_buffer.offset() + size > main_buffer.capacity() ) {
        if ( sub_buffer.capacity() == 0 ) {
            sub_buffer.assign( std::max( size * 2, main_buffer.capacity() ) );
        }
        sub_buffer.push( buf, size );
    } else {
        main_buffer.push( buf, size );
    }
}

// main_buffer 가 비워지면 sub_buffer 를 main 으로 끌어올림
// Raise sub_buffer to main when the main_buffer is empty
void io_double_buffer::elevate() {
    if ( main_buffer.empty() && !sub_buffer.empty() ) {
        main_buffer.destroy();
        main_buffer = std::move( sub_buffer );
    }
}

}