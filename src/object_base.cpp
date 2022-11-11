//
// Created by newbiediver on 22. 11. 8.
//

#include <atomic>
#include "rioring/object_base.h"

namespace rioring {

static std::atomic_size_t obj_id_container{ 1 };

object_base::object_base() {
    obj_id = obj_id_container++;
}

void object_base::set_error_callback( object_base::error_callback callback ) {
    error_event = callback;
}

void object_base::io_error( const std::error_code &ec) {
    if ( error_event ) {
        auto ptr = shared_from_this();
        error_event( ptr, ec );
    }
}

size_t object_base::object_id() const {
    return obj_id;
}

}