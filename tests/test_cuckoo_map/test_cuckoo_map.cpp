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

#include "mpgc/gc_interned_string.h"
#include "mpgc/gc_cuckoo_map.h"

using namespace mpgc;
using namespace std;
using namespace ruts;

int main() {
  using string_table_type = gc_interned_string_table<small_gc_cm_traits>;
  using string_type = typename string_table_type::value_type;
  using map_type = small_gc_cuckoo_map<string_type, size_t>;

  gc_ptr<string_table_type> table = make_gc<string_table_type>(100);
  gc_ptr<map_type> map = make_gc<map_type>();

  string_type s1 = table->intern("First");
  cout << "map[" << s1 << "] = " << map->at(s1) << endl;
  map->at(s1) = 5;
  cout << "map[" << s1 << "] = " << map->at(s1) << endl;
  map->at(s1) += 1;
  cout << "map[" << s1 << "] = " << map->at(s1) << endl;
  ++map->at(s1);
  cout << "map[" << s1 << "] = " << map->at(s1) << endl;
  auto was = map->at(s1)++;
  cout << "map[" << s1 << "] = " << map->at(s1) << endl;
  cout << "map[" << s1 << "] was " << was << endl;
  map->at(s1) *= 2;
  cout << "map[" << s1 << "] = " << map->at(s1) << endl;
  

}
