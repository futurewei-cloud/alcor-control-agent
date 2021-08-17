#include "libfluid-base/OFServerSettings.hh"

namespace fluid_base {

OFServerSettings::OFServerSettings() {
	this->_supported_versions = 0;
    this->add_version(1);
    this->version_set_by_hand = false;
    this->echo_interval(15);
    this->liveness_check(true);
    this->handshake(true);
    this->dispatch_all_messages(false);
    this->use_hello_elements(false);
    this->keep_data_ownership(true);
}

OFServerSettings& OFServerSettings::supported_version(const uint8_t version) {
    // If the user sets the version by hand, then all supported versions must
    // be explicitly declared.
    if (not this->version_set_by_hand) {
        this->version_set_by_hand = true;
        this->_supported_versions = 0;
    }
    this->add_version(version);
    return *this;
}

void OFServerSettings::add_version(const uint8_t version) {
    this->_supported_versions |= (1 << version);

    unsigned int x = 0;
    this->_max_supported_version = 0;
    for (x = (unsigned int) this->_supported_versions;
         x > 0;
         x = x >> 1, this->_max_supported_version++);
    this->_max_supported_version--;
}

uint32_t* OFServerSettings::supported_versions() {
    // We return a pointer because an OFServerSettings object is supposed to
    // be exclusively user by an OFServer instance which has a copy of it.
    
    // TODO: since this->_supported_versions is just an uint32_t, we can only
    // support OpenFlow versions lower than 31. It might be a problem some day,
    // so it would be nice to change the implementation to a proper uint32_t
    // array.
    return &this->_supported_versions;
}

uint8_t OFServerSettings::max_supported_version() {
    return this->_max_supported_version;
}

OFServerSettings& OFServerSettings::echo_interval(const int ei) {
    this->_echo_interval = ei;
    return *this;
}

int OFServerSettings::echo_interval() {
    return this->_echo_interval;
}

OFServerSettings& OFServerSettings::liveness_check(const bool liveness_check) {
    this->_liveness_check = liveness_check;
    return *this;
}

bool OFServerSettings::liveness_check() {
    return this->_liveness_check;
}

OFServerSettings& OFServerSettings::handshake(const bool handshake) {
    this->_handshake = handshake;
    return *this;
}

bool OFServerSettings::handshake() {
    return this->_handshake;
}

OFServerSettings& OFServerSettings::dispatch_all_messages(const bool dispatch_all_messages) {
    this->_dispatch_all_messages = dispatch_all_messages;
    return *this;
}

bool OFServerSettings::dispatch_all_messages() {
    return this->_dispatch_all_messages;
}

bool OFServerSettings::use_hello_elements() {
    return this->_use_hello_elements;
}

OFServerSettings& OFServerSettings::use_hello_elements(const bool use_hello_elements) {
    this->_use_hello_elements = use_hello_elements;
    return *this;
}

bool OFServerSettings::keep_data_ownership() {
    return this->_keep_data_ownership;
}

OFServerSettings& OFServerSettings::keep_data_ownership(const bool keep_data_ownership) {
    this->_keep_data_ownership = keep_data_ownership;
    return *this;
}

}
