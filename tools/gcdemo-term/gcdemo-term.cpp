#include "mpgc/gc.h"

using namespace mpgc;
using namespace std;

int main(int argc, char ** argv) {
  string phIterCtrName = "com.hpe.gcdemo.users.iterCtr";
  gc_ptr<gc_wrapped<atomic<unsigned long>>> g_iterCtr =
             persistent_roots().lookup<gc_wrapped<atomic<unsigned long>>>(phIterCtrName);
  assert(g_iterCtr != nullptr);
  g_iterCtr->store(atol(argv[1]));
  return 0;
}
