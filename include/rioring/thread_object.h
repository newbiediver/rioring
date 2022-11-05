//
// Created by newbiediver on 22. 10. 31.
//

#ifndef RIORING_THREAD_OBJECT_H
#define RIORING_THREAD_OBJECT_H

#include <thread>

namespace rioring {

class thread_object {
public:
    thread_object() = default;
    virtual ~thread_object() = default;

    void terminate()                        { is_terminated = true; }
    [[nodiscard]] bool terminated() const   { return is_terminated; }

protected:
    virtual void on_thread() = 0;

private:
    friend class thread_generator;
    
    bool            is_terminated{ false };
};

}



#endif //RIORING_THREAD_OBJECT_H
