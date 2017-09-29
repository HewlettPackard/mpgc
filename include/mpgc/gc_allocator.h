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

#ifndef GC_ALLOCATOR_H
#define GC_ALLOCATOR_H

#include <utility>
#include <map>
#include <atomic>
#include <array>
#include "mpgc/gc_fwd.h"
#include "mpgc/offset_ptr.h"

#include "ruts/mash.h"
#include "ruts/runtime_array.h"
#include "ruts/atomic16B.h"
#include "ruts/managed.h"

namespace mpgc {
  /* The main allocator class. */
  class gc_allocator {
  private:
   constexpr static uint8_t global_list_max_size = 48;
   constexpr static std::size_t slab_size = 4096;

  public:
    /* TODO: In future we should make chunks inherit from gc_allocated, once we have support
     * for free blobs in gc_descriptors. This way, during sweep, fetching object_size() would
     * work for any object/blob on the heap.
     */
    class local_chunk {
      std::size_t _size;
      local_chunk* _next;

     public:
      local_chunk() = delete;
      local_chunk(const local_chunk&) = default;
      local_chunk(std::size_t size, local_chunk* other) : _size(size), _next(other) {}

      local_chunk* next() { return _next;}
      void set_next(local_chunk* n) { _next = n; }
    };
    
    using pairType = std::pair<std::size_t, local_chunk*>;

    class global_chunk {
      /* _next, which is a offset_ptr, should *NOT* be the first word of global
       * chunk as that may be confused with an external gc_descriptor by async
       * signal handler.
       */
      std::size_t _size;
      offset_ptr<global_chunk> _next;

     public:
      global_chunk() = delete;
      global_chunk(const global_chunk&) = default;
      explicit global_chunk(std::size_t s) : _size(s), _next(nullptr) {}

      std::size_t size() const              {return _size;}
      offset_ptr<global_chunk>& next()      { return _next; }
      offset_ptr<global_chunk> next() const { return _next; }

      void set_size(std::size_t s)              {_size = s;}
      void set_next(offset_ptr<global_chunk> c) {_next = c;}
    };

    struct list_head {
      offset_ptr<global_chunk> _ptr;
      std::size_t _version;
      list_head(offset_ptr<global_chunk> p = nullptr, std::size_t v = 0) noexcept : _ptr(p), _version(v) {}
      bool empty() {
        return _ptr == nullptr && _version == 0;
      }
    };

    using localPoolType = std::map<std::size_t, local_chunk*>;

    using atomicSizedChunkType = ruts::atomic16B<list_head>;
    using globalListType = atomicSizedChunkType[global_list_max_size];

   private:
    constexpr static std::size_t bits_in_word() {
      return sizeof(void*) * 8;
    }
    
    constexpr static std::size_t min_global_chunk_size() {
      return sizeof(global_chunk);
    }

    constexpr static std::size_t global_chunk_log_bits() {
      return __builtin_ctzl(sizeof(global_chunk)) + 1;
    }

    static void _put_to_global(globalListType&, const std::size_t, offset_ptr<global_chunk>);
    static offset_ptr<global_chunk> _get_from_global(globalListType&, const std::size_t, const std::size_t);
    static offset_ptr<global_chunk> get_from_global(const std::size_t);
    static bool keep_iterating(std::size_t);

   public:
    constexpr static inline std::size_t global_list_index_for(std::size_t size) {
      return bits_in_word() - __builtin_clzl(size) - global_chunk_log_bits();
    }

    constexpr static inline std::size_t index_to_size(std::size_t index) {
      return 1 << (index + global_chunk_log_bits() - 1);
    }

    constexpr static inline std::size_t align_size_up(std::size_t size, std::size_t alignment) {
      return (size + (alignment - 1)) & ~(alignment - 1);
    }
    static void initialize(std::size_t s, globalListType *lists) {
      _global_list_size = global_list_index_for(s) + 1;
      assert(_global_list_size <= global_list_max_size);

      global_free_lists = lists;
    }
    static void put_to_global(globalListType& list, const std::size_t size, offset_ptr<global_chunk> c) {
      assert(size >= sizeof(global_chunk));
      _put_to_global(list, global_list_index_for(size), c);
    }

    static offset_ptr<global_chunk> get_chunk_from_global(globalListType&, const std::size_t);

    static const uint8_t global_list_size() {
      return _global_list_size;
    }

    static void* alloc (std::size_t);
   private:

    static globalListType *global_free_lists;
    static uint8_t _global_list_size;
  };

  
}

namespace ruts {
  template<> struct hash1<mpgc::gc_allocator::global_chunk*> {
    std::uint64_t operator()(mpgc::gc_allocator::global_chunk* p) const {
      static masher m1(5);
      return m1(reinterpret_cast<uint64_t>(p));
    }
  };

  template<> struct hash2<mpgc::gc_allocator::global_chunk*> {
    std::uint64_t operator()(mpgc::gc_allocator::global_chunk* p) const {
      static masher m1(17);
      return m1(reinterpret_cast<uint64_t>(p));
    }
  };
}

#endif
