/**
 * @file index_sequence.hpp
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief C++17 index_sequence implementation (no STL needed)
 * @version 0.1
 * @date 2025-11-02
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef BE7CCE5A_221C_4B30_946B_296B3429EFB8
#define BE7CCE5A_221C_4B30_946B_296B3429EFB8


#include <stddef.h>

template< size_t... Is >
struct index_sequence
{
};

template< size_t N, size_t... Is >
struct make_index_sequence_helper : make_index_sequence_helper< N - 1, N - 1, Is... >
{
};

template< size_t... Is >
struct make_index_sequence_helper< 0, Is... >
{
  using type = index_sequence< Is... >;
};

template< size_t N >
using make_index_sequence = typename make_index_sequence_helper< N >::type;


#endif /* BE7CCE5A_221C_4B30_946B_296B3429EFB8 */
