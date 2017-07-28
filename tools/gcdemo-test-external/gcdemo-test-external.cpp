#include "../gcdemo/gcdemo.h"
#include<deque>
#include<thread>
#include<chrono>


using namespace mpgc;
using namespace std;
using namespace chrono_literals;

using ptr_type = external_weak_gc_ptr<Post>;
thread_local RandomSeed random_seed;
static external_weak_gc_ptr<Comment> gwp;
//external_gc_ptr<Comment> gsp;
void foo() __attribute__ ((noinline));

void foo()
{
  gwp = make_gc<Comment>();
  cout <<"Allocated\n";
}

int main(int argc, char *argv[]) {
  foo();
  string prName = "com.hpe.gcdemo.users";
  UserGraphPtr userPtr = persistent_roots().lookup<WrappedUserGraph>(prName);
  deque<ptr_type> deque;
  UniformRNG rng(userPtr->size());
  for (size_t i = 0; i < 32; i++) {
    auto feed = (*userPtr)[rng.randElt()]->feed.load();
    deque.push_back(feed->back());
  }

  for (int i = 0; i < atoi(argv[1]); i++)
    this_thread::sleep_for(5s);

  size_t i = 1;
  for (ptr_type &p : deque) {
    gc_ptr<Post> sp = p.lock();
    if (sp) {
      cout << i << ": " << sp->postText << "\n";
      i++;
    }
  }

  gc_ptr<Comment> p = gwp.lock();
  if (p) {
    cout << p->commentText << "\n";
  }
  return 0;
}
