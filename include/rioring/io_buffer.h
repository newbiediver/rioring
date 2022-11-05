//
// Created by newbiediver on 22. 11. 3.
//

#ifndef RIORING_IO_BUFFER_H
#define RIORING_IO_BUFFER_H

#include "buffer_base.h"

namespace rioring {

class io_buffer : public buffer_base {
public:
    io_buffer() = default;
    ~io_buffer() noexcept;

    explicit io_buffer( size_t size );

    io_buffer( const io_buffer& ) = delete;
    io_buffer( io_buffer &&other ) noexcept;

    io_buffer& operator=( const io_buffer& ) = delete;
    io_buffer& operator=( io_buffer && other ) noexcept;

    void assign( size_t size );
    void destroy();

    bool read( unsigned char *buf, size_t size ) override;
    void pop( size_t size ) override;
    void push( const unsigned char *buf, size_t size ) override;

    void offset( size_t o )                 { buffer_offset = o; }
    [[nodiscard]] size_t offset() const     { return buffer_offset; }
    [[nodiscard]] size_t size() const       { return buffer_offset; }
    [[nodiscard]] bool empty() const        { return buffer_offset == 0; }
    [[nodiscard]] size_t capacity() const   { return buffer_size; }

    const unsigned char* operator*() const  { return raw_buffer; }

private:
    void on_overflow( size_t new_size );

private:
    unsigned char   *raw_buffer{ nullptr };
    size_t          buffer_size{ 0 }, block_size{ 0 };
    size_t          buffer_offset{ 0 };
};

class io_double_buffer : public buffer_base {
public:
    io_double_buffer() = default;
    explicit io_double_buffer( size_t size );

    void assign( size_t size );
    void destroy();

    bool read( unsigned char *buf, size_t size ) override;
    void pop( size_t size ) override;
    void push( const unsigned char *buf, size_t size ) override;
    void elevate();

    [[nodiscard]] bool empty() const    { return main_buffer.empty() && sub_buffer.empty(); }
    [[nodiscard]] size_t size() const   { return main_buffer.size(); }

    const unsigned char *operator*() const { return *main_buffer; }

private:
    io_buffer main_buffer, sub_buffer;
};

}

#endif //RIORING_IO_BUFFER_H
