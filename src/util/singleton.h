// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// Date: Mon Sep 09 14:49:21 CST 2024.

#ifndef VAR_UTIL_SINGLETON_H
#define VAR_UTIL_SINGLETON_H

#include <pthread.h>

namespace var {

template<typename T>
class GetSingleton {
public:
    static pthread_once_t g_create_singleton_once;
    static T* g_singleton;
    static void create_singleton();
};

template<typename T>
pthread_once_t GetSingleton<T>::g_create_singleton_once = PTHREAD_ONCE_INIT;

template<typename T>
T* GetSingleton<T>::g_singleton = nullptr;

template<typename T>
void GetSingleton<T>::create_singleton() {
    g_singleton = new T;
}

// To get a never-deleted singleton of a type T, just call get_singleton<T>().
// Most daemon threads or objects that need to be always-on can be created
// by this function.
// This function can be called safely before main() w/o initialization issues of
// global variables.
template<typename T>
inline T* get_singleton() {
    pthread_once(&GetSingleton<T>::g_create_singleton_once, GetSingleton<T>::create_singleton);
    return GetSingleton<T>::g_singleton;
}

} // end namespace var

#endif // VAR_UTIL_SINGLETON_H