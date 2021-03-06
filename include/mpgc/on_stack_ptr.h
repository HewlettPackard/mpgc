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

#ifndef ON_STACK_PTR_H
#define ON_STACK_PTR_H

#include "mpgc/gc_ptr.h"

namespace mpgc {
  template <typename T>
  class on_stack_ptr {
    T* _ptr;

    constexpr on_stack_ptr(const T* &p) noexcept : _ptr(p) {}
    constexpr on_stack_ptr(T* &&p) noexcept : _ptr(std::move(p)) {}
  public:
    using element_type = T;

    /**
     * Default construction creates a null pointer.
     */
    constexpr on_stack_ptr() noexcept : _ptr{nullptr} {};
    /**
     * Conversion from `nullptr` is allowed and `constexpr`.
     */
    constexpr on_stack_ptr(std::nullptr_t) noexcept : on_stack_ptr{} {}

    constexpr on_stack_ptr(const gc_ptr<T> &p) noexcept
      : _ptr(p.as_bare_pointer())
    {}
    constexpr on_stack_ptr(gc_ptr<T> &&p) noexcept
      : _ptr(std::move(p.as_bare_pointer())) 
    {}

    constexpr on_stack_ptr(const on_stack_ptr&) noexcept = default;
    constexpr on_stack_ptr(on_stack_ptr&&) noexcept = default;

    on_stack_ptr &operator =(const on_stack_ptr&) noexcept = default;
    on_stack_ptr &operator =(on_stack_ptr&&) noexcept = default;

    on_stack_ptr &operator =(const gc_ptr<T> &rhs) noexcept {
      _ptr = rhs.as_bare_pointer();
      return *this;
    }
    on_stack_ptr &operator =(gc_ptr<T> &&rhs) noexcept {
      _ptr = std::move(rhs.as_bare_pointer());
      return *this;
    }

    operator gc_ptr<T>() const {
      return gc_ptr_from_bare_ptr(_ptr);
    }
    
    /** 
     * A reference to the object pointed to.
     *
     * @note This is specialized to allow the specialization for
     * pointers to wrapped values.
     *
     * @pre this pointer is not null.
     */
    template <typename S=T, typename = std::enable_if_t<!is_gc_wrapped<S>::value> >
    T &operator *() const {
      return *_ptr;
    }

    /** 
     * A reference to the content of the wrapped object pointed to.
     *
     * @pre this pointer is not null.
     * @par
     * @pre `T` is `gc_wrapped<X>` for some `X`.
     */
    template <typename S=T, typename = std::enable_if_t<is_gc_wrapped<S>::value> >
    auto &operator *() const {
      return _ptr->content();
    }

    /** 
     * Dereferece through the pointer 
     *
     * @returns the underlying offset pointer, which is itself
     * dereferenced.
     *
     * @note This is specialized to allow the specialization for
     * pointers to wrapped values.
     *
     * @pre the pointer is not null.
     **/
    template <typename S=T, typename = std::enable_if_t<!is_gc_wrapped<S>::value> >
    T* operator ->() const noexcept {
      return _ptr;
    }

    /** 
     * Dereferece through the pointer to the content of a wrapped
     * object.
     *
     * @returns the underlying offset pointer, which is itself
     * dereferenced.
     *
     * @pre the pointer is not null.
     * @par
     * @pre `T` is `gc_wrapped<X>` for some `X`.
     **/
    template <typename S=T, typename = std::enable_if_t<is_gc_wrapped<S>::value> >
    auto *operator ->() const noexcept {
      return &_ptr->content();
    }

    void swap(on_stack_ptr &other) noexcept {
      std::swap(_ptr, other._ptr);
    }

    constexpr operator bool() const noexcept {
      return _ptr != nullptr;
    }

    /**
     * The size of a referenced gc_array or `0` if null.
     *
     * @pre `T` is a gc_array.
     *
     * Allows you to say
     * ~~~
     * gc_array_ptr<X> a;
     * ...
     * size_t n = a.size();
     * ~~~
     * without worrying about whether the pointer is null.
     */
    template <typename S=T, typename E=std::enable_if_t<is_gc_array<S>::value> >
    typename S::size_type size() const {
      return _ptr == nullptr ? 0 : _ptr->size();
    }

    /**
     * Whether a referenced gc_array is empty (size zero).
     *
     * @pre `T` is a gc_array.
     *
     * Actual gc_array objects are never empty, so this merely tests
     * the pointer.
     * Allows you to say
     * ~~~
     * gc_array_ptr<X> a;
     * ...
     * if (a.empty()) { ... }
     * ~~~
     */
    template <typename S=T, typename E=std::enable_if_t<is_gc_array<S>::value> >
    bool empty() const {
      return _ptr == nullptr;
    }

    /**
     * Index into a referenced gc_array.
     *
     * @param pos the index into the array.
     *
     * @returns a reference to the array element.  This is a const
     * reference if the pointer is to a const gc_array.
     *
     * @pre `T` is a gc_array.
     * @par
     * @pre the pointer is not null.
     * @par
     * @pre `pos` is in the array.
     *
     * @warning The reference returned is currently an unchecked bare
     * C++ reference.  If the pointer is null or if the index is out
     * of bounds, this will be referring to random memory on the heap.
     * Also, if the array gets collected, these will be dangling
     * references.
     * @warning Both of these issues are expected to be fixed soon,
     * which will result in an assertion failure for the first two
     * cases and the reference holding on to the array for the latter
     * case, but for now you have to be careful.
     * 
     */
    template <typename S=T, typename E=std::enable_if_t<is_gc_array<S>::value> >
    auto &operator[](typename S::size_type pos) const {
      // throw something if null
      return (*_ptr)[pos];
    }

    /**
     * A bare pointer pointing to the referenced object.
     *
     * @warning The pointer returned may become a dangling pointer if
     * the object referred to is garbage collected.  If it is later
     * reconstituted as a gc_ptr by calling gc_ptr_from_bare_ptr(), it
     * could break the MPGC system.  It is usually only safe to use
     * the pointer returned (e.g., as a parameter to a function that
     * requires a bare pointer) while holding on to the gc_ptr to the
     * object.
     */
    element_type *as_bare_pointer() const noexcept {
      return _ptr;
    }

    /**
     * Convert a pointer to a const gc_array to a gc_array::const_iterator.
     *
     * As with bare pointers, a gc_ptr to an array can be treated as
     * an iterator pointing to the first element of the array.  If the
     * pointer is a null pointer, this will be a null iterator.
     * gc_array iterators hold a GC reference to the array as a whole,
     * so it is safe to store them.
     * 
     * @pre `T` is a const gc_array.
     */
    template <typename S=T, typename E=std::enable_if_t<is_gc_array<S>::value &&
                                                        std::is_const<S>::value >>
    operator typename S::const_iterator() const {
      return _ptr == nullptr ? typename S::const_iterator{} : _ptr->cbegin();
    }
    /**
     * Convert a pointer to a non-const gc_array to a gc_array::iterator.
     *
     * As with bare pointers, a gc_ptr to an array can be treated as
     * an iterator pointing to the first element of the array.  If the
     * pointer is a null pointer, this will be a null iterator.
     * gc_array iterators hold a GC reference to the array as a whole,
     * so it is safe to store them.
     * 
     * @pre `T` is a non-const gc_array.
     */
    template <typename S=T, typename E=std::enable_if_t<is_gc_array<S>::value &&
                                                        !std::is_const<S>::value >>
    operator typename S::iterator() const {
      return _ptr == nullptr ? typename S::iterator{} : _ptr->begin();
    }
    /**
     * Obtain an iterator indexing into a referenced gc_array.
     *
     * @param delta an index into the array.  May be negative.  
     *
     * @returns an iterator or const_iterator into the array
     * (depending on whether `T` is a const gc_array or a non-const
     * gc_array).
     * 
     * If the pointer is a null pointer, this will be a null iterator,
     * otherwise it will be an iterator to the appropriate element.
     * It's okay if this is out of bounds (or even negative) as long
     * as you don't try to dereference it without further adding
     * values to get it in bounds.  gc_array iterators hold a GC
     * reference to the array as a whole, so it is safe to store them.
     * 
     * @pre `T` is a gc_array.
     */
    template <typename S=T, typename E=std::enable_if_t<is_gc_array<S>::value>>
    auto operator +(typename S::const_iterator::difference_type delta) const {
      // If _ptr is null, delta had better be zero.  To be safe, we'll just return null
      return _ptr == nullptr ? decltype(_ptr->begin()+delta){} : _ptr->begin()+delta;
    }
    /**
     * Obtain an iterator to the beginning of a referenced gc_array.
     *
     * @returns an iterator or const_iterator into the array
     * (depending on whether `T` is a const gc_array or a non-const
     * gc_array).  
     * 
     * If the pointer is a null pointer, this will be a null iterator,
     * otherwise it will be an iterator to the first element.
     * 
     * @pre `T` is a gc_array.
     */
    template <typename S=T, typename E=std::enable_if_t<is_gc_array<S>::value>>
    auto begin() const {
      return _ptr == nullptr ? decltype(_ptr->begin()){} : _ptr->begin();
    }
    /**
     * Obtain an iterator to the end of a referenced gc_array.
     *
     * @returns an iterator or const_iterator into the array
     * (depending on whether `T` is a const gc_array or a non-const
     * gc_array).  
     * 
     * If the pointer is a null pointer, this will be a null iterator,
     * otherwise it will be an iterator past the last element..
     * 
     * @pre `T` is a gc_array.
     */
    template <typename S=T, typename E=std::enable_if_t<is_gc_array<S>::value>>
    auto end() const {
      return _ptr == nullptr ? decltype(_ptr->end()){} : _ptr->end();
    }
    /**
     * Obtain a const iterator to the beginning of a referenced gc_array.
     *
     * @returns a const_iterator into the array.
     * 
     * If the pointer is a null pointer, this will be a null iterator,
     * otherwise it will be an iterator to the first element.
     * 
     * @pre `T` is a gc_array.
     */
    template <typename S=T, typename E=std::enable_if_t<is_gc_array<S>::value>>
    auto cbegin() const {
      return _ptr == nullptr ? decltype(_ptr->cbegin()){} : _ptr->cbegin();
    }
    /**
     * Obtain a const iterator to the end of a referenced gc_array.
     *
     * @returns a const_iterator into the array.
     * 
     * If the pointer is a null pointer, this will be a null iterator,
     * otherwise it will be an iterator past the last element..
     * 
     * @pre `T` is a gc_array.
     */
    template <typename S=T, typename E=std::enable_if_t<is_gc_array<S>::value>>
    auto cend() const {
      return _ptr == nullptr ? decltype(_ptr->cend()){} : _ptr->cend();
    }
    /**
     * Obtain a reverse iterator to the end of a referenced gc_array.
     * Incrementing this will enumerate the elements in reverse order.
     *
     * @returns an iterator or const_iterator into the array
     * (depending on whether `T` is a const gc_array or a non-const
     * gc_array).  
     * 
     * If the pointer is a null pointer, this will be a null iterator,
     * otherwise it will be a reverse iterator to the last element.
     * 
     * @pre `T` is a gc_array.
     */
    template <typename S=T, typename E=std::enable_if_t<is_gc_array<S>::value>>
    auto rbegin() const {
      return _ptr == nullptr ? decltype(_ptr->rbegin()){} : _ptr->rbegin();
    }
    /**
     * Obtain a reverse iterator to the beginning of a referenced gc_array.
     * This signals the end of a reverse iteration.
     *
     * @returns an iterator or const_iterator into the array
     * (depending on whether `T` is a const gc_array or a non-const
     * gc_array).  
     * 
     * If the pointer is a null pointer, this will be a null iterator,
     * otherwise it will be a reverse iterator before the first element.
     * 
     * @pre `T` is a gc_array.
     */
    template <typename S=T, typename E=std::enable_if_t<is_gc_array<S>::value>>
    auto rend() const {
      return _ptr == nullptr ? decltype(_ptr->rend()){} : _ptr->rend();
    }
    /**
     * Obtain a const reverse iterator to the end of a referenced
     * gc_array.  Incrementing this will enumerate the elements in
     * reverse order.
     *
     * @returns a const_iterator into the array.  
     * 
     * If the pointer is a null pointer, this will be a null iterator,
     * otherwise it will be a reverse iterator to the last element.
     * 
     * @pre `T` is a gc_array.
     */
    template <typename S=T, typename E=std::enable_if_t<is_gc_array<S>::value>>
    auto crbegin() const {
      return _ptr == nullptr ? decltype(_ptr->crbegin()){} : _ptr->crbegin();
    }
    /**
     * Obtain a const reverse iterator to the beginning of a
     * referenced gc_array.  This signals the end of a reverse
     * iteration.
     *
     * @returns a const_iterator into the array.  
     * 
     * If the pointer is a null pointer, this will be a null iterator,
     * otherwise it will be a reverse iterator before the first element.
     * 
     * @pre `T` is a gc_array.
     */
    template <typename S=T, typename E=std::enable_if_t<is_gc_array<S>::value>>
    auto crend() const {
      return _ptr == nullptr ? decltype(_ptr->crend()){} : _ptr->crend();
    }

    /**
     * Compute the first hash by delegating to the underlying bare pointer.
     *
     * hash1() and hash2() allow on_stack_ptr to be used as a key in
     * gc_cuckoo_map, as long as there is a specialization for
     * hash1<T*>() and hash2<T*>().
     */
    constexpr std::uint64_t hash1() const {
      return ruts::hash1<T*>()(_ptr);
    }
    /**
     * Compute the second hash by delegating to the underlying bare pointer.
     *
     * hash1() and hash2() allow on_stack_ptr to be used as a key in
     * gc_cuckoo_map, as long as there is a specialization for
     * hash1<T*>() and hash2<T*>().
     */
    constexpr std::uint64_t hash2() const {
      return ruts::hash2<T*>()(_ptr);
    }
    
    template <typename X, typename Y>
    friend on_stack_ptr<X> std::static_pointer_cast(const on_stack_ptr<Y>&);
    template <typename X, typename Y>
    friend on_stack_ptr<X> std::dynamic_pointer_cast(const on_stack_ptr<Y>&);
    template <typename X, typename Y>
    friend on_stack_ptr<X> std::const_pointer_cast(const on_stack_ptr<Y>&);

    template <typename X, typename Y>
    friend bool operator ==(const on_stack_ptr<X>&, const on_stack_ptr<Y>&);
    template <typename X, typename Y>
    friend bool operator ==(const X*, const on_stack_ptr<Y>&);
    template <typename X, typename Y>
    friend bool operator ==(const on_stack_ptr<X>&, const Y*);

    template <typename X, typename Y>
    friend std::basic_ostream<X,Y> &operator <<(std::basic_ostream<X,Y>&, const on_stack_ptr&);
  };

  /**
   * @related on_stack_ptr.
   */
  template <typename X, typename Y>
  inline
  bool operator ==(const on_stack_ptr<X> &lhs, const on_stack_ptr<Y> &rhs) {
    return lhs._ptr == rhs._ptr;
  }
  /**
   * @related on_stack_ptr.
   */
  template <typename X, typename Y>
  inline
  bool operator ==(const X *lhs, const on_stack_ptr<Y> &rhs) {
    return lhs == rhs._ptr;
  }
  /**
   * @related on_stack_ptr.
   */
  template <typename X, typename Y>
  inline
  bool operator ==(const on_stack_ptr<X> &lhs, const Y *rhs) {
    return lhs._ptr == rhs;
  }
  /**
   * @related on_stack_ptr.
   */
  template <typename X, typename Y>
  inline
  bool operator !=(const on_stack_ptr<X> &lhs, const on_stack_ptr<Y> &rhs) {
    return !(lhs == rhs);
  }
  /**
   * @related on_stack_ptr.
   */
  template <typename X, typename Y>
  inline
  bool operator !=(const X *lhs, const on_stack_ptr<Y> &rhs) {
    return !(lhs == rhs);
  }
  /**
   * @related on_stack_ptr.
   */
  template <typename X, typename Y>
  inline
  bool operator !=(const on_stack_ptr<X> &lhs, const Y *rhs) {
    return !(lhs == rhs);
  }
  /**
   * @related on_stack_ptr.
   */
  template <typename T>
  inline
  bool operator ==(const on_stack_ptr<T> &lhs, std::nullptr_t) {
    return !lhs;
  }
  /**
   * @related on_stack_ptr.
   */
  template <typename T>
  inline
  bool operator ==(std::nullptr_t, const on_stack_ptr<T> &rhs) {
    return !rhs;
  }
  /**
   * @related on_stack_ptr.
   */
  template <typename T>
  inline
  bool operator !=(const on_stack_ptr<T> &lhs, std::nullptr_t) {
    return lhs;
  }
  /**
   * @related on_stack_ptr.
   */
  template <typename T>
  inline
  bool operator !=(std::nullptr_t, const on_stack_ptr<T> &rhs) {
    return rhs;
  }
}

namespace ruts {
  template <typename T>
  struct hash1<mpgc::on_stack_ptr<T>> {
    auto operator()(const mpgc::on_stack_ptr<T> &ptr) const {
      return ptr.hash1();
    }
  };
  template <typename T>
  struct hash2<mpgc::on_stack_ptr<T>> {
    auto operator()(const mpgc::on_stack_ptr<T> &ptr) const {
      return ptr.hash2();
    }
  };

}

namespace std {
  template <typename C, typename T, typename X>
  basic_ostream<C,T> &
  operator <<(basic_ostream<C,T> &os, const mpgc::on_stack_ptr<X> &ptr) {
    return os << mpgc::offset_ptr<X>(ptr._ptr);
  }
  
  template <typename T>
  inline
  void swap(mpgc::on_stack_ptr<T> &lhs, mpgc::on_stack_ptr<T> &rhs) {
    lhs.swap(rhs);
  }

  template <typename T, typename U>
  mpgc::on_stack_ptr<T>
  static_pointer_cast(const mpgc::on_stack_ptr<U> &r) {
    return static_cast<T*>(r._ptr);
  }

  template <typename T, typename U>
  inline
  mpgc::on_stack_ptr<T>
  dynamic_pointer_cast(const mpgc::on_stack_ptr<U> &r) {
    return dynamic_cast<T*>(r._ptr);
  }

  template <typename T, typename U>
  inline
  mpgc::on_stack_ptr<T>
  const_pointer_cast(const mpgc::on_stack_ptr<U> &r) {
    return const_cast<T*>(r._ptr);
  }

  template <typename T>
  struct hash<mpgc::on_stack_ptr<T>> {
    std::size_t operator()(const mpgc::on_stack_ptr<T> &val) const noexcept {
      return std::hash<T*>()(val.as_bare_pointer());
    }
  };
}
#endif //ON_STACK_PTR_H
