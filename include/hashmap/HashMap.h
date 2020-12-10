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

#ifndef HASH_MAP_H_
#define HASH_MAP_H_

#include <cstdint>
#include <iostream>
#include <functional>
#include <mutex>
#include "HashNode.h"

constexpr size_t HASH_SIZE_DEFAULT = 1031; // A prime number as hash size gives a better distribution of values in buckets
namespace CTSL //Concurrent Thread Safe Library
{
//The class represting the hash map.
//It is expected for user defined types, the hash function will be provided.
//By default, the std::hash function will be used
//If the hash size is not provided, then a defult size of 1031 will be used
//The hash table itself consists of an array of hash buckets.
//Each hash bucket is implemented as singly linked list with the head as a dummy node created
//during the creation of the bucket. All the hash buckets are created during the construction of the map.
//Locks are taken per bucket, hence multiple threads can write simultaneously in different buckets in the hash map
template <typename K, typename V, typename F = std::hash<K> > class HashMap {
  public:
  HashMap(size_t hashSize_ = HASH_SIZE_DEFAULT) : hashSize(hashSize_)
  {
    hashTable = new HashBucket<K, V>[hashSize]; //create the hash table as an array of hash buckets
  }

  ~HashMap()
  {
    delete[] hashTable;
  }
  //Copy and Move of the HashMap are not supported at this moment
  HashMap(const HashMap &) = delete;
  HashMap(HashMap &&) = delete;
  HashMap &operator=(const HashMap &) = delete;
  HashMap &operator=(HashMap &&) = delete;

  //Function to find an entry in the hash map matching the key.
  //If key is found, the corresponding value is copied into the parameter "value" and function returns true.
  //If key is not found, function returns false.
  bool find(const K &key, V &value) const
  {
    size_t hashValue = hashFn(key) % hashSize;
    return hashTable[hashValue].find(key, value);
  }

  //Function to insert into the hash map.
  //If key already exists, update the value, else insert a new node in the bucket with the <key, value> pair.
  void insert(const K &key, const V &value)
  {
    size_t hashValue = hashFn(key) % hashSize;
    hashTable[hashValue].insert(key, value);
  }

  //Function to remove an entry from the bucket, if found
  void erase(const K &key)
  {
    size_t hashValue = hashFn(key) % hashSize;
    hashTable[hashValue].erase(key);
  }

  //Function to clean up the hasp map, i.e., remove all entries from it
  void clear()
  {
    for (size_t i = 0; i < hashSize; i++) {
      (hashTable[i]).clear();
    }
  }

  HashBucket<K, V> *hashTable;
  const size_t hashSize;

  private:
  F hashFn;
};
} // namespace CTSL
#endif
