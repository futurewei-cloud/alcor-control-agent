/** @file */
#ifndef __OFSERVERSETTINGS_HH__
#define __OFSERVERSETTINGS_HH__

#include <stdint.h>

namespace fluid_base {

#define ECHO_XID 0x0F
#define HELLO_XID 0x0F

class OFServer;

/**
Configuration parameters for an OFServer. These parameters specify the
OpenFlow behavior of a class that deals with OFConnection objects.
*/
class OFServerSettings {
public:
    /**
    Create an OFServerSettings with default configuration values.

    Settings will have the following values by default:
    - Only OpenFlow 1.0 is supported (a sane value for the most compatibility)
    - `echo_interval`: `15`
    - `liveness_check`: `true`
    - `handshake`: `true`
    - `dispatch_all_messages`: `false`
    - `use_hello_elems`: `false` (to avoid compatibility issues with
       existing software and hardware)
    - `keep_data_ownership`: `true` (to simplify things)       
    */
    OFServerSettings();

    /**
    Add a supported version to the set of supported versions.

    Using this method will override the default version (1, OpenFlow 1.0). If
    you call this method with version 4, only version 4 will be supported. If
    you want to add support for version 1, you will need to do so explicitly,
    so that you can choose only the versions you want, while still having a
    nice default.

    @param version OpenFlow protocol version number (e.g.: 4 for OpenFlow 1.3)
    */
    OFServerSettings& supported_version(const uint8_t version);

    /**
    Return an array of OpenFlow versions bitmaps with the supported versions.
    */
    uint32_t* supported_versions();

    /**
    Return the largest version number supported.
    */
    uint8_t max_supported_version();

    /**
    Set the OpenFlow echo interval (in seconds). A connection will be closed if
    no echo replies arrive in this interval, and echo requests will be
    periodically sent using the same interval.

    @param echo_interval the echo interval (in seconds)
    */
    OFServerSettings& echo_interval(const int echo_interval);

    /**
    Return the echo interval.
    */
    int echo_interval();

    /**
    Set whether the OFServer instance should perform liveness checks (timed
    echo requests and replies).

    @param liveness_check true for liveness checking
    */
    OFServerSettings& liveness_check(const bool liveness_check);

    /**
    Return whether liveness check should be performed.
    */
    bool liveness_check();

    /**
    Set whether the OFServer instance should perform OpenFlow handshakes (hello
    messages, version negotiation and features request).

    @param handshake true for automatic OpenFlow handshakes
    */
    OFServerSettings& handshake(const bool handshake);

    /**
    Return whether handshake should be performed.
    */
    bool handshake();

    /**
    Set whether the OFServer instance should forward all OpenFlow messages to
    the user callback (OFHandler::message_callback), including those treated
    for handshake and liveness check.

    @param dispatch_all_messages true to enable forwarding for all messages
    */
    OFServerSettings& dispatch_all_messages(const bool dispatch_all_messages);

    /**
    Return whether all messages should be dispatched.
    */
    bool dispatch_all_messages();

    /**
    Set whether the OFServer instance should send and treat OpenFlow 1.3.1
    hello elements.

    See OFServerSettings::OFServerSettings for more details.

    @param use_hello_elements true to enable hello elems
    */
    OFServerSettings& use_hello_elements(const bool use_hello_elements);

    /**
    Return whether hello elements should be used.
    */
    bool use_hello_elements();

    /**
    Set whether the OFServer instance should own and manage the message data
    passed to its message callback (true) or if your application should be
    responsible for it (false).
    
    See OFServerSettings::OFServerSettings for more details.

    @param keep_data_ownership true if OFServer is responsible for managing
                               message data, false if your application is.
                               
    */
    OFServerSettings& keep_data_ownership(const bool keep_data_ownership);

    /**
    Return whether message data pointer ownership belongs to OFServer (true) or
    your application (false).
    */
    bool keep_data_ownership();
    
    private:
        friend class OFServer;

        uint32_t _supported_versions;
        uint8_t _max_supported_version;

        bool version_set_by_hand;
        void add_version(const uint8_t version);

        int _echo_interval;
        bool _liveness_check;
        bool _handshake;
        bool _dispatch_all_messages;
        bool _use_hello_elements;
        bool _keep_data_ownership;
};

}

#endif
