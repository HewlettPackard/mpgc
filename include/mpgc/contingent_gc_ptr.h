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

#ifndef CONTINGENT_GC_PTR_H
#define CONTINGENT_GC_PTR_H

#include <utility>
#include "mpgc/gc_fwd.h"
/*
 * Note: We need to explicitly include gc_fwd.h, because it declares
 * the default argument for the second template arg for
 * contingent_gc_ptr.
 */
#include "mpgc/weak_gc_ptr.h"


namespace mpgc {
  /*
   * As an initial dummy implementation, a contingent_gc_ptr will
   * include a weak_gc_ptr (which is, itself, a dummy), and a gc_ptr.
   */

  template <typename T, typename C>
  class contingent_gc_ptr {
    weak_gc_ptr<C> _control;
    mutable gc_ptr<T> _ptr;

    template <typename X, typename Y>
    using if_assignable = std::enable_if_t<std::is_assignable<T*&,X*>::value
                                           && std::is_assignable<C*&,Y*>::value>;
  public:
    using control_type = C;
    using element_type = T;

    constexpr contingent_gc_ptr() noexcept
      : _control{nullptr}, _ptr{nullptr}
    {}
    constexpr contingent_gc_ptr(std::nullptr_t) noexcept
      : contingent_gc_ptr{}
    {}

    contingent_gc_ptr(const contingent_gc_ptr &rhs) noexcept = default;
    contingent_gc_ptr(contingent_gc_ptr &&rhs) noexcept = default;
    contingent_gc_ptr &operator =(const contingent_gc_ptr &rhs) noexcept = default;
    contingent_gc_ptr &operator =(contingent_gc_ptr &&rhs) noexcept = default;

    template <typename X, typename Y, typename = if_assignable<X,Y> >
    contingent_gc_ptr(const contingent_gc_ptr<X,Y> &rhs) noexcept
      : _control{rhs._control}, _ptr{rhs._ptr}
    {}
    template <typename X, typename Y, typename = if_assignable<X,Y> >
    contingent_gc_ptr(contingent_gc_ptr<X,Y> &&rhs) noexcept
      : _control{std::move(rhs._control)}, _ptr{std::move(rhs._ptr)}
    {}
    template <typename X, typename Y, typename = if_assignable<X,Y> >
    contingent_gc_ptr(const gc_ptr<X> &p, const gc_ptr<Y> &c)
      : _control{c}, _ptr{c == nullptr ? nullptr : p}
    {}
    template <typename X, typename Y, typename = if_assignable<X,Y> >
    contingent_gc_ptr(const gc_ptr<X> &p, const weak_gc_ptr<Y> &c)
      : _control{c}, _ptr{c}
    {}

    template <typename X, typename Y, typename = if_assignable<X,Y> >
    explicit contingent_gc_ptr(std::pair<gc_ptr<X>, gc_ptr<Y>> &pair)
      : contingent_gc_ptr{pair.first, pair.second}
    {}
    template <typename X, typename Y, typename = if_assignable<X,Y> >
    explicit contingent_gc_ptr(std::pair<gc_ptr<X>, weak_gc_ptr<Y>> &pair)
      : contingent_gc_ptr{pair.first, pair.second}
    {}

    /*
     * And similarly for externals and rval refs, probably in all
     * combinations.
     */

    contingent_gc_ptr &operator =(nullptr_t) noexcept {
      /*
       * Or should this leave the control alone (like other single
       * pointer assignments) and only change the pointer?
       */
      _control = nullptr;
      _ptr = nullptr;
      return *this;
    }
    template <typename X, typename Y, typename = if_assignable<X,Y> >
    contingent_gc_ptr &operator =(const contingent_gc_ptr<X,Y> &rhs) noexcept {
      /*
       * We will need to be careful about the atomicity of this (and
       * the default).  It certainly has to work from a GC standpoint,
       * but we also have to be careful about what happens if somebody
       * calls get() while we're in the middle of an assignment.  For
       * now, it's just a naive implementation.
       */
      _control = rhs._control;
      _ptr = rhs._ptr;
    }
    template <typename X, typename Y, typename = if_assignable<X,Y> >
    contingent_gc_ptr &operator =(const std::pair<gc_ptr<X>, gc_ptr<Y>> &rhs) noexcept {
      return (*this)=(contingent_gc_ptr{rhs});
    }
    template <typename X, typename Y, typename = if_assignable<X,Y> >
    contingent_gc_ptr &operator =(const std::pair<gc_ptr<X>, weak_gc_ptr<Y>> &rhs) noexcept {
      return (*this)=(contingent_gc_ptr{rhs});
    }
    /*
     * And so on with externals.
     */

    /*
     * The non-contingent assignments just change the pointer, leaving
     * the control alone.  It's probably not worth checking the
     * control.
     */
    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    contingent_gc_ptr &operator =(const gc_ptr<Y> &rhs) noexcept  {
      _ptr = rhs;
      return *this;
    }

    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    contingent_gc_ptr &operator =(gc_ptr<Y> &&rhs) noexcept {
      _ptr = std::move(_ptr);
      return *this;
    }

    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    contingent_gc_ptr &operator =(const external_gc_ptr<Y> &rhs) noexcept {
      _ptr = rhs.value();
      return *this;
    }

    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    contingent_gc_ptr &operator =(external_gc_ptr<Y> &&rhs) noexcept {
      _ptr = std::move(rhs.value());
      return *this;
    }

    void reset_control(nullptr_t) noexcept {
      _control = nullptr;
      _ptr = nullptr;
    }
    template <typename Y, typename = std::enable_if_t<std::is_assignable<C*&,Y*>::value> >
    void reset_control(const gc_ptr<Y> &rhs) noexcept {
      _control = rhs;
    }
    template <typename Y, typename = std::enable_if_t<std::is_assignable<C*&,Y*>::value> >
    void reset_control(const weak_gc_ptr<Y> &rhs) noexcept {
      _control = rhs;
    }
    template <typename Y, typename = std::enable_if_t<std::is_assignable<C*&,Y*>::value> >
    void reset_control(weak_gc_ptr<Y> &&rhs) noexcept {
      _control = std::move(rhs);
    }
    /*
     * etc.
     */



    std::pair<gc_ptr<T>, gc_ptr<C> > lock_pair() const noexcept {
      /*
       * We will need to think about the atomicity of this.
       */
      gc_ptr<C> locked_control = _control.lock();
      if (locked_control == nullptr && _ptr != nullptr) {
        _ptr = nullptr;
      }
      return std::make_pair(locked_control, _ptr);
    }

    gc_ptr<T> lock() const noexcept {
      gc_ptr<T> p = _ptr;
      return p == nullptr ? nullptr : lock_pair().first;
    }

    gc_ptr<T> operator->() const noexcept {
      return lock();
    }

    weak_gc_ptr<C> control() const noexcept {
      return _control;
    }

    gc_ptr<C> lock_control() const noexcept {
      return _control.lock();
    }

    bool control_expired() const noexcept {
      return _control.expired();
    }

    void reset() noexcept {
      _control.reset();
      _ptr = nullptr;
    }

    void swap(contingent_gc_ptr &other) {
      /* discuss atomicity */
      _ptr.swap(other._ptr);
      _control.swap(other._control);
    }

    template <typename Ch, typename Tr>
    std::basic_ostream<Ch,Tr> &print_on(std::basic_ostream<Ch,Tr> &os) const {
      return os << _ptr << "{" << _control << "}";
    }
  };
}

namespace std {
  template <typename T, typename C>
  inline
  void swap(mpgc::contingent_gc_ptr<T,C> &lhs, mpgc::contingent_gc_ptr<T,C> &rhs) {
    lhs.swap(rhs);
  }

  template <typename C, typename T, typename X, typename Y>
  basic_ostream<C,T> &
  operator <<(basic_ostream<C,T> &os, const mpgc::contingent_gc_ptr<X,Y> &ptr) {
    return ptr.print_on(os);
  }

}


#endif
