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

#include "mpgc/gc_skiplist_allocator.h"
#include "mpgc/gc_handshake.h"
#include "mpgc/bump_allocation_slots.h"
#include "mpgc/gc.h"

namespace mpgc {
  extern void global_allocation_epilogue(gc_control_block&, gc_handshake::in_memory_thread_struct&);

  namespace gc_allocator {
    std::atomic<std::size_t> skip_node::n_skip_nodes{0};

      bool skiplist::_help_unfinished_bump_alloc(gc_control_block &cb,
                                                 bump_chunk &exp,
                                                 bump_chunk &exp1,
                                                 slot_number &curr_slot,
                                                 slot_number &curr_slot1,
                                                 bool set_slot_0) {
        //Assumption is that if there is a non-zero slot number in bump_ptr, then it
        //can't be nullptr.
        if (curr_slot.data != curr_slot1.data) {
          slot curr_slot_data = cb.bump_alloc_slots.get(curr_slot);
          std::atomic<std::size_t> *curr_id_ptr =
               reinterpret_cast<std::atomic<std::size_t>*>(base_offset_ptr::base() +
                                                          (curr_slot_data.ptr_offset << alignment_log));
          std::size_t expected_id = curr_id_ptr->load();
          std::atomic_signal_fence(std::memory_order_acquire);
          exp1 = tail.atomic_bump_ptr.load();
          curr_slot1 = slot_number(slot_fld.decode(exp1.begin), slot_fld.decode(exp1.end));
          if (curr_slot.data == curr_slot1.data &&
              bump_ptr_fld.decode(exp.begin) == bump_ptr_fld.decode(exp1.begin)) {
            curr_id_ptr->compare_exchange_strong(expected_id, curr_slot_data.id);
            if (set_slot_0) {
              bump_chunk des;
              des.begin = bump_ptr_fld.decode(exp1.begin);
              des.end = bump_ptr_fld.decode(exp1.end);
              tail.atomic_bump_ptr.compare_exchange_strong(exp1, des);
            }
          } else if (!set_slot_0) {
            curr_slot = curr_slot1;
            curr_slot1.data = 0;
          }
          return true;
        }
        return false;
      }

      offset_ptr<global_chunk> skiplist::bump_pointer_allocate(gc_control_block &cb,
                                                     slot_number &myslot,
                                                     const std::size_t size) {
        bump_chunk exp{bump_chunk::from_volatile, tail.bump_ptr}, exp1;
        slot_number curr_slot(slot_fld.decode(exp.begin), slot_fld.decode(exp.end)),
                                           curr_slot1(0, 0);
        slot &myslot_ref = cb.bump_alloc_slots.get_ref(myslot);
        myslot_ref.id = size;
        do {
          if (_help_unfinished_bump_alloc(cb, exp, exp1, curr_slot, curr_slot1, false)) {
            exp = exp1;
          } else if (exp.end == 0 && exp.begin != 0) {
            std::size_t exp_level_key = tail.level_orig_end;
            std::atomic_signal_fence(std::memory_order_acquire);
            exp1 = exp;
            exp = bump_chunk{bump_chunk::from_volatile, tail.bump_ptr};
            if (exp.begin == exp1.begin) {
              help_inserting_bump_chunk(exp_level_key, exp);
            } 
          } else if (bump_ptr_fld.decode(exp.end) - bump_ptr_fld.decode(exp.begin) >= size) {
            bump_chunk desired;
            myslot_ref.ptr_offset = bump_ptr_fld.decode(exp.begin);
            desired.begin = slot_fld.encode(myslot.sentinel_idx) | bump_ptr_fld.encode(myslot_ref.ptr_offset + size);
            do {
              desired.end = slot_fld.replace(exp.end, myslot.block_idx);
              if (tail.atomic_bump_ptr.compare_exchange_strong(exp, desired)) {
                global_chunk* ret = new (base_offset_ptr::base() + (myslot_ref.ptr_offset << alignment_log)) global_chunk(size);
                do {
                  exp.begin = slot_fld.replace(desired.begin, 0);
                  exp.end = slot_fld.replace(desired.end, 0);
                  if (tail.atomic_bump_ptr.compare_exchange_strong(desired, exp)) {
                    break;
                  }
                //Following while condition is false if some other thread has done our work. Or some skip node was allocated
                } while (slot_fld.decode(desired.begin) == myslot.sentinel_idx &&
                         slot_fld.decode(desired.end) == myslot.block_idx);
                return ret;
              }
              //Following while condition checks if anything except the 'end' ptr has changed or not.
            } while(slot_fld.decode(exp.begin) == curr_slot.sentinel_idx &&
                    slot_fld.decode(exp.end) == curr_slot.block_idx &&
                    bump_ptr_fld.decode(exp.begin) == myslot_ref.ptr_offset &&
                    bump_ptr_fld.decode(exp.end) - bump_ptr_fld.decode(exp.begin) >= size);

            if (bump_ptr_fld.decode(exp.end) - bump_ptr_fld.decode(exp.begin) < size) {
              return nullptr;
            }
            //Reset the arguments to the first if condition in the outermost loop.
            curr_slot.sentinel_idx = slot_fld.decode(exp.begin);
            curr_slot.block_idx = slot_fld.decode(exp.end);
            curr_slot1.data = 0;
          } else {
            return nullptr;
          }
        } while(true);
      }

      void skiplist::help_unfinished_bump_alloc() {
        bump_chunk exp{bump_chunk::from_volatile, tail.bump_ptr}, exp1;
        slot_number sn(slot_fld.decode(exp.begin),
                                              slot_fld.decode(exp.end)), sn1;
        _help_unfinished_bump_alloc(control_block(), exp, exp1, sn, sn1, true);
      }

      offset_ptr<global_chunk> skiplist::get_from_global(gc_handshake::in_memory_thread_struct &tstruct,
                                                         const std::size_t req_size) {
      gc_control_block &cb = control_block();
      const std::size_t max_size = req_size > slab_size ? req_size : slab_size;
      do {
        /* This while loop will ensure that we don't end-up in a situation where some other
         * thread holds the entire memory chunk for its own allocation purpose, and hence
         * this thread fails.
         * However, once we have a working GC, the thread doesn't have to fail, it can go
         * and start helping the GC until it can reclaim the required free space.
         */
        offset_ptr<global_chunk> c =
             cb.global_free_lists[tstruct.status_idx.load().index()].allocate(cb, tstruct.persist_data->slot,
                                                                              req_size, max_size, tstruct.rand);
        if (c) {
          return c;
        }
        global_allocation_epilogue(cb, tstruct);
      } while (true);
      return nullptr;
    }

    inline
    std::size_t required_padding(local_chunk *c, std::size_t algn) {
      if (algn == 1) {
        return 0;
      }
      std::size_t n = reinterpret_cast<std::size_t>(c);
      std::size_t padded = align_size_up(n, algn << alignment_log);
      std::size_t padding = (padded-n) >> alignment_log;
      return padding;
    }

    inline
    std::tuple<local_chunk*, std::size_t, std::size_t>
    get_from_local(std::size_t size, std::size_t algn,
                   localPoolType &local_chunks)
    {
      localPoolType::iterator end = local_chunks.end();
      for (localPoolType::iterator it = local_chunks.lower_bound(size);
           it != end;
           it++)
        {
          std::size_t chunk_size = it->first;
          bool first_time = true;
          for (local_chunk **ppChunk = &(it->second);
               *ppChunk != nullptr;
               ppChunk = &(*ppChunk)->next())
            {
              local_chunk *chunk = (*ppChunk);
              std::size_t padding = required_padding(chunk, algn);
              if (size+padding <= chunk_size) {
                local_chunk *next = chunk->next();
              
                (*ppChunk) = next;
                if (first_time && next == nullptr) {
                  local_chunks.erase(it);
                }
                first_time = false;
                std::size_t leftover_size = chunk_size - size - padding;
                return std::make_tuple(chunk, leftover_size, padding);
              }
            }
        }
      return std::make_tuple(nullptr, 0, 0);
    }

    inline
    std::tuple<local_chunk*, std::size_t, std::size_t>
    get_from_global(std::size_t size, std::size_t algn,
                    gc_handshake::in_memory_thread_struct &tstruct)
    {
      // For now, if we get this far, we'll just allocate something
      // that's guaranteed to be big enough, even if what we get
      // would've been correctly aligned.
      std::size_t max_padding = algn-1;
      offset_ptr<global_chunk> c = skiplist::get_from_global(tstruct, size+max_padding);
      assert(c->size() >= size);
      local_chunk *chunk = reinterpret_cast<local_chunk*>(c.as_bare_pointer());
      std::size_t padding = required_padding(chunk, algn);
      std::size_t leftover_size = c->size() - size - padding;
      return std::make_tuple(chunk, leftover_size, padding);
    }

    inline
    void put_to_local(std::size_t *p, std::size_t size,
                      localPoolType &local_chunks)
    {
      if (size >= (sizeof(local_chunk) >> alignment_log)) {
        local_chunk*& temp = local_chunks[size];
        temp = new (p) local_chunk(size, temp);
      } else if (size > 0){
        *p = size;
      }
    }

    void* alloc (gc_handshake::in_memory_thread_struct &tstruct,
                 std::size_t size,
                 std::size_t req_alignment)
    {
      local_chunk *chunk;
      std::size_t leftover_size;
      std::size_t pad_size;

      size = align_size_up(size, alignment) >> alignment_log;
      req_alignment = align_size_up(req_alignment, alignment) >> alignment_log;
      localPoolType &local_chunks = tstruct.local_free_list;
      std::tie(chunk, leftover_size, pad_size)
        = get_from_local(size, req_alignment, local_chunks);
      if (chunk == nullptr) {
        //We don't have a big enough chunk
        std::tie(chunk, leftover_size, pad_size)
          = get_from_global(size, req_alignment, tstruct);
      }
      std::size_t *whole_chunk = reinterpret_cast<std::size_t*>(chunk);
      std::size_t *return_addr = whole_chunk+pad_size;
      std::size_t *extra = return_addr+size;

      // std::cout << "Resuest = " << size << " @ " << req_alignment << std::endl;
      // std::cout << "  Chunk = " << chunk << " [" << chunk->size() << "]" <<std::endl;
      // std::cout << "Padding = " << pad_size << std::endl;
      // std::cout << "   Left = " << leftover_size << std::endl;
      // std::cout << " Return = " << return_addr << std::endl;
      // std::cout << "  Extra = " << extra << std::endl;

      put_to_local(extra, leftover_size, local_chunks);
      /*
       * It is essential to keep the size of object in the first word until it
       * gets initialized with a gc_descriptor in the allocation_epilogue function
       * for fault-tolerance. If we clean it in the following memset, and the process
       * crashes after that but before gc_descriptor construction, then the garbage
       * gc_descriptor cleanup during sweep can get into a infinite-loop assuming the
       * size to be 0.
       */
      *return_addr = size;
      if (pad_size > 0) {
        // std::cout << "  adding pad chunk " << std::endl;
        put_to_local(whole_chunk, pad_size, local_chunks);
      }
      //Zero-out the memory
      std::memset(return_addr + 1, 0x0, (size - 1) << alignment_log);
      return reinterpret_cast<local_chunk*>(return_addr);
    }

  }
}
