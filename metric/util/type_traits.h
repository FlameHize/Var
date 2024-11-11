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

// Date: 2024.09.06 Author: zgx

#ifndef VAR_UTIL_TYPE_TRAITS_H
#define VAR_UTIL_TYPE_TRAITS_H

#include <type_traits>

namespace var {

// Select type by C.
template<bool C, typename TrueType, typename FalseType> struct conditional;
template<typename TrueType, typename FalseType> 
struct conditional<true, TrueType, FalseType> {
    typedef TrueType type;
};
template<typename TrueType, typename FalseType> 
struct conditional<false, TrueType, FalseType> {
    typedef FalseType type;
};

template<typename T> struct add_const { typedef T const type; };

template<typename T> struct add_reference { typedef T type; };
template<typename T> struct add_reference<T&> { typedef T& type; };

// template<> struct add_reference<void> { typedef void type; };
// template<> struct add_reference<void const> { typedef void const type; };
// template<> struct add_reference<void volatile> { typedef void volatile type; };
// template<> struct add_reference<void const volatile> { typedef void const volatile type; };

// Add const& for non-integral types.
// add_cr_non_integral<int>::type      -> int
// add_cr_non_integral<FooClass>::type -> const FooClass&
template<typename T> struct add_cr_non_integral {
    typedef typename conditional<std::is_integral<T>::value, T,
            typename add_reference<typename add_const<T>::type>::type>::type type;
};

} // end namespace var

#endif // VAR_UTIL_TYPE_TRAITS_H