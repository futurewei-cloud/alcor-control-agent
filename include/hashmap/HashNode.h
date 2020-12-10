// MIT License
//
// Copyright (c) 2018 Kaushik Basu
// Copyright 2019 The Alcor Authors - file modified.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef HASH_NODE_H_
#define HASH_NODE_H_

#include <shared_mutex>
namespace CTSL //Concurrent Thread Safe Library
{
// Class representing a templatized hash node
template <typename K, typename V> class HashNode {
  public:
  HashNode() : next(nullptr)
  {
  }
  HashNode(K key_, V value_) : next(nullptr), key(key_), value(value_)
  {
  }
  ~HashNode()
  {
    next = nullptr;
  }

  const K &getKey() const
  {
    return key;
  }
  void setValue(V value_)
  {
    value = value_;
  }
  const V &getValue() const
  {
    return value;
  }

  HashNode *next; //Pointer to the next node in the same bucket
  private:
  K key; //the hash key
  V value; //the value corresponding to the key
};

//Class representing a hash bucket. The bucket is implemented as a singly linked list.
//A bucket is always constructed with a dummy head node
template <typename K, typename V> class HashBucket {
  public:
  HashBucket() : head(nullptr)
  {
  }

  ~HashBucket() //delete the bucket
  {
    clear();
  }

  //Function to find an entry in the bucket matching the key
  //If key is found, the corresponding value is copied into the parameter "value" and function returns true.
  //If key is not found, function returns false
  bool find(const K &key, V &value) const
  {
    // A shared mutex is used to enable mutiple concurrent reads
    std::shared_lock<std::shared_timed_mutex> lock(mutex_);
    HashNode<K, V> *node = head;

    while (node != nullptr) {
      if (node->getKey() == key) {
        value = node->getValue();
        return true;
      }
      node = node->next;
    }
    return false;
  }

  //Function to insert into the bucket
  //If key already exists, update the value, else insert a new node in the bucket with the <key, value> pair
  void insert(const K &key, const V &value)
  {
    //Exclusive lock to enable single write in the bucket
    std::unique_lock<std::shared_timed_mutex> lock(mutex_);
    HashNode<K, V> *prev = nullptr;
    HashNode<K, V> *node = head;

    while (node != nullptr && node->getKey() != key) {
      prev = node;
      node = node->next;
    }

    if (nullptr == node) //New entry, create a node and add to bucket
    {
      if (nullptr == head) {
        head = new HashNode<K, V>(key, value);
      } else {
        prev->next = new HashNode<K, V>(key, value);
      }
    } else {
      node->setValue(value); //Key found in bucket, update the value
    }
  }

  //Function to remove an entry from the bucket, if found
  void erase(const K &key)
  {
    //Exclusive lock to enable single write in the bucket
    std::unique_lock<std::shared_timed_mutex> lock(mutex_);
    HashNode<K, V> *prev = nullptr;
    HashNode<K, V> *node = head;

    while (node != nullptr && node->getKey() != key) {
      prev = node;
      node = node->next;
    }

    if (nullptr == node) //Key not found, nothing to be done
    {
      return;
    } else //Remove the node from the bucket
    {
      if (head == node) {
        head = node->next;
      } else {
        prev->next = node->next;
      }

      //Free up the memory
      delete node->getValue();
      delete node;
    }
  }

  //Function to clear the bucket
  void clear()
  {
    //Exclusive lock to enable single write in the bucket
    std::unique_lock<std::shared_timed_mutex> lock(mutex_);
    HashNode<K, V> *prev = nullptr;
    HashNode<K, V> *node = head;
    while (node != nullptr) {
      prev = node;
      node = node->next;

      //Free up the memory
      delete prev->getValue();
      delete prev;
    }
    head = nullptr;
  }

  HashNode<K, V> *head; //The head node of the bucket
  mutable std::shared_timed_mutex mutex_; //The mutex for this bucket
};
} // namespace CTSL

#endif
