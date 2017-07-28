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

/*
 * null_terminated.h
 *
 *  Created on: Jun 28, 2015
 *      Author: Evan
 */

#ifndef NULL_TERMINATED_H_
#define NULL_TERMINATED_H_

#include <iterator>

namespace ruts {
  template <typename Iter>
  class null_terminated {
    using itraits = std::iterator_traits<Iter>;
  public:
    using difference_type = typename itraits::difference_type;
    using value_type = typename itraits::value_type;
    using pointer = typename itraits::pointer;
    using reference = typename itraits::reference;
    using iterator_category = std::forward_iterator_tag;
  private:
    bool _is_end;
    Iter _iter;
    constexpr null_terminated(bool ie, const Iter &i)
    : _is_end(ie), _iter(i)
    {}
  public:
    static const null_terminated end() {
      return null_terminated{true, Iter{}};
    }
    null_terminated(const Iter &i)
    : _is_end{false}, _iter{i}
    {}
    null_terminated(Iter &&i)
    : _is_end{false}, _iter{std::move(i)}
    {}
    null_terminated()
    : _is_end{false}, _iter{}
    {}
    reference operator *() const {
      return *_iter;
    }
    null_terminated &operator ++() {
      _iter++;
      return *this;
    }
    null_terminated operator ++(int) {
      null_terminated old = *this;
      operator ++();
      return old;
    }
    bool operator ==(const null_terminated &rhs) const {
      if (rhs._is_end) {
        return _is_end || *(*this) == value_type{};
      } else if (_is_end) {
        return *rhs == value_type{};
      } else {
        return _iter == rhs._iter;
      }
    }
    bool operator !=(const null_terminated &rhs) const {
      if (rhs._is_end) {
        return !_is_end && *(*this) != value_type{};
      } else if (_is_end) {
        return *rhs != value_type{};
      } else {
        return _iter != rhs._iter;
      }
    }
    auto operator->() const {
      return _iter.operator->();
    }

  };

}



#endif /* NULL_TERMINATED_H_ */
