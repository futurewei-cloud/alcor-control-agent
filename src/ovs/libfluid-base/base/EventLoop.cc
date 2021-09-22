#include "libfluid-base/base/EventLoop.hh"
#include <stdlib.h>
#include <event2/event.h>

namespace fluid_base {

// Define our own value, since the stdint.h define doesn't work in C++
#define OF_MAX_LEN 0xFFFF

// See FIXME in EventLoop::EventLoop
//extern "C" void event_base_add_virtual(struct event_base *);
//extern "C" void event_base_del_virtual(struct event_base *);

class EventLoop::LibEventEventLoop {
private:
    friend class EventLoop;
    struct event_base *base;
};

EventLoop::EventLoop(int id) {
    this->id = id;
    this->m_implementation = new EventLoop::LibEventEventLoop;

    this->m_implementation->base = event_base_new();

    this->stopped = false;
    if (!this->m_implementation->base) {
        fprintf(stderr, "Error creating EventLoop %d\n", id);
        exit(EXIT_FAILURE);
    }

    /* FIXME: dirty hack warning!
    We add a virtual event to prevent the loop from exiting when there are
    no events.

    This fix is needed because libevent 2.0 doesn't have the flag
    EVLOOP_NO_EXIT_ON_EMPTY. Version 2.1 fixes this, so this will have to
    be changed in the future (to make it prettier and to avoid breaking
    anything).

    See:
    http://stackoverflow.com/questions/7645217/user-triggered-event-in-libevent
    */
    //event_base_add_virtual(this->m_implementation->base);
}

EventLoop::~EventLoop() {
    event_base_free(this->m_implementation->base);
    delete this->m_implementation;
}

void EventLoop::run() {
    // Only run if EventLoop::stop hasn't been called first
    if (stopped) return;

    event_base_dispatch(this->m_implementation->base);
    // See note in EventLoop::EventLoop. Here we disable the virtual event
    // to guarantee that nothing blocks.
    //event_base_del_virtual(this->m_implementation->base);
    event_base_loop(this->m_implementation->base, EVLOOP_NO_EXIT_ON_EMPTY);
}

void EventLoop::stop() {
    // Prevent run from running if it's not started :)
    this->stopped = true;
    event_base_loopbreak(this->m_implementation->base);
}

void* EventLoop::thread_adapter(void* arg) {
    ((EventLoop*) arg)->run();
    return NULL;
}

void* EventLoop::get_base() {
    return this->m_implementation->base;
}

}
