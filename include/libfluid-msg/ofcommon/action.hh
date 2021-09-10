// Copyright (c) 2014 Open Networking Foundation
// Copyright 2021 The Alcor Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ACTION_H
#define ACTION_H

#include <list>
#include <set>
#include "../util/util.h"
#include "openflow-common.hh"

namespace fluid_msg {

class Action {
protected:
    uint16_t type_;
    uint16_t length_;
public:
    Action();
    Action(uint16_t type, uint16_t length);
    virtual ~Action() {
    }
    ;
    virtual size_t pack(uint8_t *buffer);
    virtual of_error unpack(uint8_t *buffer);
    virtual bool equals(const Action & other);
    virtual bool operator==(const Action &other) const;
    virtual bool operator!=(const Action &other) const;
    virtual Action* clone() {
        return new Action(*this);
    }
    virtual uint16_t set_order() const {
        return 0;
    }
    uint16_t type() {
        return this->type_;
    }
    uint16_t length() {
        return this->length_;
    }
    void type(uint16_t type) {
        this->type_ = type;
    }
    void length(uint16_t length) {
        this->length_ = length;
    }
    static Action * make_of10_action(uint16_t type);
    static Action * make_of13_action(uint16_t type);
    static bool delete_all(Action * action) {
        delete action;
        return true;
    }

};

class ActionList {
private:
    uint16_t length_;
    std::list<Action*> action_list_;
public:
    ActionList()
        : length_(0) {
    }
    ;
    ActionList(std::list<Action*> action_list);
    ActionList(const ActionList &other);
    bool operator==(const ActionList &other) const;
    bool operator!=(const ActionList &other) const;
    ActionList& operator=(ActionList other);
    ~ActionList();
    size_t pack(uint8_t *buffer);
    of_error unpack10(uint8_t *buffer);
    of_error unpack13(uint8_t *buffer);
    friend void swap(ActionList& first, ActionList& second);
    uint16_t length() {
        return this->length_;
    }
    std::list<Action*> action_list(){
        return this->action_list_;
    }
    void add_action(Action &action);
    void add_action(Action *act);
    void length(uint16_t length) {
        this->length_ = length;
    }
};

struct comp_action_set_order {
    bool operator()(Action * lhs, Action* rhs) const {
        return lhs->set_order() < rhs->set_order();
    }
};

class ActionSet {
private:
    uint16_t length_;
    std::set<Action*, comp_action_set_order> action_set_;
public:
    ActionSet()
        : length_(0) {
    }
    ;
    ActionSet(std::set<Action*> action_set);
    ActionSet(const ActionSet &other);
    bool operator==(const ActionSet &other) const;
    bool operator!=(const ActionSet &other) const;
    ActionSet& operator=(ActionSet other);
    ~ActionSet();
    size_t pack(uint8_t *buffer);
    of_error unpack(uint8_t *buffer);
    friend void swap(ActionSet& first, ActionSet& second);
    uint16_t length() {
        return this->length_;
    }
    std::set<Action*, comp_action_set_order> action_set(){
        return this->action_set_;
    }
    void add_action(Action &action);
    void add_action(Action *act);
    void length(uint16_t length) {
        this->length_ = length;
    }
};

} //End of namespace fluid_msg
#endif
