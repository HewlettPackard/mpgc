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
 * bit_field.h
 *
 *  Created on: Aug 27, 2014
 *      Author: evank
 */

#ifndef BIT_FIELD_H_
#define BIT_FIELD_H_

#include <cstdint>
#include <climits>
#include <type_traits>

template <typename T = std::uint64_t>
constexpr T bit_mask(int n) {
  return (T {1} << n) - 1;
}

namespace bits {
  template <typename U = std::uint64_t>
  constexpr U mask(std::size_t n, std::size_t shift = 0) {
    return ((U{1} << n) - 1) << shift;
  }

  template <typename T, typename U=std::uint64_t>
  class field {
    const U _mask;
    const std::size_t _shift;
    const std::size_t _width;

    class reference {
      U &_loc;
      const U _mask;
      const std::size_t _shift;
      constexpr explicit reference(U &loc, U m, std::size_t s)
	: _loc(loc), _mask(m), _shift(s)
      {}
    public:
      constexpr operator T() const {
	return static_cast<T>((_loc & _mask) >> _shift);
      }
      reference &operator =(const T &val) {
	const U field_val = (static_cast<U>(val) << _shift) & _mask;
	_loc = (_loc & ~_mask) | field_val;
	return *this;
      }
      
    };

  public:

    constexpr field(std::size_t right, std::size_t width = 1)
      : _mask(bits::mask<U>(width, right)), _shift(right), _width(width)
    {}

    constexpr U mask() const {
      return _mask;
    }

    constexpr std::size_t shift() const {
      return _shift;
    }

    constexpr std::size_t width() const {
      return _width;
    }

    constexpr T max_val() const {
      return (T{1} << _width) - 1;
    }

    constexpr T decode(const U &from) const {
      return static_cast<T>((from & _mask) >> _shift);
    }

    constexpr U encode(const T &val) const {
      return (static_cast<U>(val) << _shift) & _mask;
    }

    constexpr U replace(const U &from, const T &val) const {
      return (from & ~_mask) | encode(val);
    }

    constexpr T operator[](const U &from) const {
      return decode(from);
    }

    reference operator[](U &from) const {
      return reference(from, _mask, _shift);
    }

  };
  
  template <typename U>
  class field<bool,U> {
    const U _mask;
    const std::size_t _shift;
    const std::size_t _width;

    class reference {
      U &_loc;
      const U _mask;
      constexpr explicit reference(U &loc, U m)
	: _loc(loc), _mask(m)
      {}
    public:
      constexpr operator bool() const {
	return (_loc & _mask);
      }
      reference &operator =(bool val) {
	if (val) {
	  _loc |= _mask;
	} else {
	  _loc &= ~_mask;
	}
	return *this;
      }
      
    };

  public:

    constexpr field(std::size_t right, std::size_t width = 1)
      : _mask(bits::mask<U>(width, right)), _shift(right), _width(width)
    {}

    constexpr U mask() const {
      return _mask;
    }

    constexpr std::size_t shift() const {
      return _shift;
    }

    constexpr std::size_t width() const {
      return _width;
    }

    constexpr bool max_val() const {
      return _width > 0;
    }

    constexpr bool decode(const U &from) const {
      return (from & _mask);
    }

    constexpr U encode(bool val) const {
      return val ? _mask : 0;
    }

    constexpr U replace(const U &from, bool val) const {
      return val ? (from | _mask) : (from & ~_mask);
    }

    constexpr bool operator[](const U &from) const {
      return decode(from);
    }

    reference operator[](U &from) const {
      return reference(from, _mask);
    }

  };
  
}


template <typename T>
constexpr T bit_field(T word, int width, int shift = 0) {
  return (word >> shift) & bit_mask<T>(width);
}

template <typename T>
constexpr T left_bit_field(T word, int width, int shift = 0) {
  return (word >> (CHAR_BIT*sizeof(T)-shift-width)) & bit_mask<T>(width);
}


template <typename T, typename U=std::uint64_t>
constexpr U to_field(T *val, int width, int shift = 0) {
  return (reinterpret_cast<U>(val) & bit_mask<U>(width)) << shift;
}

template <typename T, typename U=std::uint64_t, typename = std::enable_if_t<std::is_arithmetic<T>::value>>
constexpr U to_field(T val, int width, int shift = 0) {
  return (static_cast<U>(val) & bit_mask<U>(width)) << shift;
}

template <typename T, typename U, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
constexpr U replace_field(U from, T val, int width, int shift = 0) {
  return (from & ~(bit_mask<U>(width) << shift)) | to_field<U>(val, width, shift);
}

// Eclipse give spurious error without this.  (GCC compiles it just fine.)
template <typename U=std::uint64_t>
constexpr U to_field(unsigned val, int width, int shift = 0) {
  return (static_cast<U>(val) & bit_mask<U>(width)) << shift;
}

template <typename T, std::size_t NB, std::size_t Off, typename Rep = std::uint64_t,
    typename = std::enable_if_t<
    std::is_arithmetic<Rep>::value && std::is_scalar<T>::value
    >>
class bit_field_ref
{
public:
  using type = T;
  using field_of = Rep;
  static const std::size_t nBits = NB;
  static const std::size_t offset = Off;
  static_assert(nBits+offset <= 8*sizeof(field_of), "Not enough room for field");
private:
  constexpr static field_of mask = ((field_of(1) << nBits) - 1) << offset;
  field_of &_obj;
public:
  constexpr explicit bit_field_ref(field_of &obj) : _obj(obj) {}

  constexpr operator type() const {
    std::uintptr_t p = (_obj & mask) >> offset;
    return reinterpret_cast<type>(p);
  }

  bit_field_ref &operator =(const type &rhs) {
    _obj = (_obj & ~mask) | ((reinterpret_cast<field_of>(rhs) << offset) & mask);
    return *this;
  }
};

template <size_t NB, std::size_t Off, typename Rep>
class bit_field_ref<bool, NB, Off, Rep, 
		    std::enable_if_t<std::is_arithmetic<Rep>::value>>
{
public:
  using type = bool;
  using field_of = Rep;
  static const std::size_t nBits = NB;
  static const std::size_t offset = Off;
  static_assert(nBits+offset <= 8*sizeof(field_of), "Not enough room for field");
private:
  constexpr static field_of mask = ((field_of(1) << nBits) - 1) << offset;
  field_of &_obj;
public:
  constexpr explicit bit_field_ref(field_of &obj) : _obj(obj) {}

  constexpr operator bool() const {
    return _obj & mask;
  }

  bit_field_ref &operator =(bool rhs) {
    if (rhs) {
      _obj != mask;
    } else {
      _obj &= ~mask;
    }
    return *this;
  }
};



#endif /* BIT_FIELD_H_ */
