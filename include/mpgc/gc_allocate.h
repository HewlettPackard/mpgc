/*
 *
 *  Multi Process Garbage Collector
 *  Copyright © 2016 Hewlett Packard Enterprise Development Company LP.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  As an exception, the copyright holders of this Library grant you permission
 *  to (i) compile an Application with the Library, and (ii) distribute the 
 *  Application containing code generated by the Library and added to the 
 *  Application during this compilation process under terms of your choice, 
 *  provided you also meet the terms and conditions of the Application license.
 *
 */

#ifndef GC_ALLOCATE_H
#define GC_ALLOCATE_H

#include <utility>
#include <iterator>

#include "mpgc/gc_skiplist_allocator.h"
#include "mpgc/gc_ptr.h"

namespace mpgc {
  namespace gc_handshake {
    struct in_memory_thread_struct;
  }

  extern gc_handshake::in_memory_thread_struct& allocation_prologue();
  extern void allocation_epilogue(gc_handshake::in_memory_thread_struct&, void*, gc_token&, std::size_t);

  class gc_managed_placement_t {};
  static const gc_managed_placement_t in_gc_managed_space;
  
  template <typename T>
  class gc_allocator__ {
    template <typename X, typename ...Args>
    friend gc_ptr<X> make_gc(Args&&...args);
    template <typename X> friend class gc_allocator__;

    static void check_descriptor() {
      // Check to make sure it's declared
      desc_for<T>();
    }

    template <typename ...Args>
    gc_ptr<T> allocate(Args&&...args) {
      //      check_descriptor();
      gc_descriptor valdesc = desc_for<T>();
      std::size_t vdsize = valdesc.object_size();
      if (vdsize * 8 != sizeof(T)) {
        std::cout << "Obj desc says "
                  << vdsize << " words.  Obj is "
                  << sizeof(T) << " bytes" << std::endl;
        valdesc.trace(typeid(T).name());
      }
      assert(valdesc.object_size()*8 == sizeof(T));
      gc_token tok(valdesc);
      static_assert(is_collectible<T>::value,
                    "Has non-trivial destructor and no is_collectible<T> specialization");

      gc_handshake::in_memory_thread_struct& ts = allocation_prologue();
      void *ptr = gc_allocator::alloc(ts, sizeof(T), alignof(T));
      allocation_epilogue(ts, ptr, tok, 0);

      auto res = new (ptr) T(tok, std::forward<Args>(args)...);
      assert(&(res->get_gc_descriptor()) == ptr);
      return gc_ptr_from_bare_ptr(res);
    }
  };
  template <typename T>
  class gc_allocator__<gc_array<T>> {
    using array_type = gc_array<T>;
    using value_type = typename array_type::value_type;
    using size_type = typename array_type::size_type;
    template <typename X, typename ...Args>
    friend gc_ptr<X> make_gc(Args&&...args);

    static void check_descriptor() {
      gc_allocator__<T>::check_descriptor();
    }
    
    static void *allocate_space_for(gc_handshake::in_memory_thread_struct &ts, size_type n) {
      std::size_t sz = sizeof(gc_array_base)+n*sizeof(value_type);
      return gc_allocator::alloc(ts, sz, alignof(T));
    }

    std::pair<void *, gc_token> allocate_(std::size_t n) 
    {
      check_descriptor();
      assert(n > 0);
      gc_descriptor valdesc = desc_for<value_type>();
      gc_token tok(valdesc.in_array<value_type>(n));

      gc_handshake::in_memory_thread_struct& ts = allocation_prologue();
      void *ptr = allocate_space_for(ts, n);
      allocation_epilogue(ts, ptr, tok, n);
      return std::make_pair(ptr, tok);
    }

    gc_ptr<array_type> allocate(size_type n) {
      if (n == 0) {
        return nullptr;
      }
      auto pair = allocate_(n);
      array_type *bp = new (pair.first) array_type(pair.second, n);
      gc_ptr<array_type> p = gc_ptr_from_bare_ptr(bp);
      //assert(p->get_gc_descriptor().object_size()*8
      //	     == (sizeof(gc_array_base)
      //		 + gc_allocator::align_size_up(n*sizeof(value_type), 8)));
      //      assert(&(p->get_gc_descriptor()) == ptr);
      return p;
    }

    /*
     * I would've thought we should use "Iter&&" here, but GCC 4.9.2
     * won't compile if "from" is an lval and "to" is an rval (e.g.,
     * "base" and "base+size").
     */
    template <typename Iter>
      gc_ptr<array_type> allocate(Iter from, Iter to)
    {
      if (from == to) {
        return nullptr;
      }
      size_type n = std::distance(from, to);
      auto pair = allocate_(n);
      array_type *bp = new (pair.first) array_type(pair.second, n, from, n);
      gc_ptr<array_type> p = gc_ptr_from_bare_ptr(bp);
      return p;
    }

    template <typename Iter>
      gc_ptr<array_type> allocate(size_t n, Iter from, Iter to)
    {
      if (n == 0) {
        return nullptr;
      }
      size_type k = std::distance(from, to);
      if (k == 0) {
        return allocate(n);
      } else if (k > n) {
        k = n;
      }
      auto pair = allocate_(n);
      array_type *bp = new (pair.first) array_type(pair.second, n, from, k);
      gc_ptr<array_type> p = gc_ptr_from_bare_ptr(bp);
      return p;
    }

  };
  /*
   * Allocate a T, perfect forwarding args as ctor parameters, along with an allocator reference.
   * T::descriptor() will be called to get the descriptor.
   * (For arrays, T::value_type::element_type::descriptor().)
   */
  template <typename T, typename ...Args>
  inline
  gc_ptr<T> make_gc(Args&&...args) {
    gc_allocator__<T> a;
    return a.allocate(std::forward<Args>(args)...);
  }
  template <typename T>
  inline
  gc_ptr<gc_array<T>> make_gc_array(std::size_t n) {
    if (n == 0) { return nullptr; }
    return make_gc<gc_array<T>>(n);
  }

  template <typename T, typename Iter>
  inline
  gc_ptr<gc_array<T>> make_gc_array(Iter &&from, Iter&&to) {
    return make_gc<gc_array<T>>(std::forward<Iter>(from), std::forward<Iter>(to));
  }

  template <typename T, typename Iter>
  inline
  gc_ptr<gc_array<T>> make_gc_array(std::size_t n, Iter &&from, Iter&&to) {
    return make_gc<gc_array<T>>(n, std::forward<Iter>(from), std::forward<Iter>(to));
  }

  template <typename T>
  inline
  auto as_bare_pointer(const gc_ptr<T> &p) {
    return p.as_bare_pointer();
  }
}

inline
void *operator new(std::size_t count, mpgc::gc_managed_placement_t) throw() {
  assert(false);
  return nullptr;
}

inline
void *operator new[](std::size_t count, mpgc::gc_managed_placement_t) throw() {
  assert(false);
  return nullptr;
}


#endif
