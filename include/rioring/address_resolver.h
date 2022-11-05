//
// Created by newbiediver on 22. 11. 5.
//

#ifndef RIORING_ADDRESS_RESOLVER_H
#define RIORING_ADDRESS_RESOLVER_H

#include <netdb.h>
#include <string>

namespace rioring {

enum class resolve_type {
    tcp,
    udp,
};

inline struct ::addrinfo *resolve( resolve_type type, std::string_view address, unsigned short port ) {
    ::addrinfo *result, hint = {};
    auto port_string = std::to_string( port );

    hint.ai_family = AF_UNSPEC;
    if ( type == resolve_type::tcp ) {
        hint.ai_socktype = SOCK_STREAM;
        hint.ai_protocol = IPPROTO_TCP;
    } else {
        // udp
    }

    if ( getaddrinfo( address.data(), port_string.c_str(), &hint, &result ) != 0 ) {
        return nullptr;
    }

    return result;
}

}

#endif //RIORING_ADDRESS_RESOLVER_H
