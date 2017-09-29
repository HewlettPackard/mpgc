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

class with_cp : public gc_allocated {
  gc_ptr<User>                     _dummy;
  contingent_gc_ptr<Comment, User> _test;

 public:
  with_cp(gc_token &gc, gc_ptr<Comment> &c, on_stack_ptr<User> &u)
    : gc_allocated(gc), _dummy(u), _test(c, u) {}

  static const auto &descriptor() {
    static gc_descriptor d =
      GC_DESC(with_cp)
      .template WITH_FIELD(&with_cp::_dummy)
      .template WITH_FIELD(&with_cp::_test);
    return d;
  }

  void print() const {
    on_stack_ptr<User> u = _test.lock();
    if (u) {
      cout << u->name << "\n";
    }
  }
};

on_stack_ptr<with_cp> foo(on_stack_ptr<User> u) {
  gc_ptr<Comment> c = make_gc<Comment>();
  gwp = c;
  cout <<"Allocated\n";
  return make_gc<with_cp>(c, u);
}

void print_comment(on_stack_ptr<Comment> c) {
  if (c) {
   cout << c->commentText << "\n";
  }
}

int main(int argc, char *argv[]) {
  string prName = "com.hpe.gcdemo.users";
  UserGraphPtr userPtr = persistent_roots().lookup<WrappedUserGraph>(prName);
  UniformRNG rng(userPtr->size());
  on_stack_ptr<with_cp> cp = foo((*userPtr)[rng.randElt()]);
  deque<ptr_type> deque;

  for (size_t i = 0; i < 32; i++) {
    auto feed = (*userPtr)[rng.randElt()]->feed.load();
    if (feed) {
      deque.push_back(feed->back());
    }
  }

  for (int i = 0; i < atoi(argv[1]); i+=4)
    this_thread::sleep_for(5s);

  size_t i = 1;
  for (ptr_type &p : deque) {
    on_stack_ptr<Post> sp = p.lock();
    if (sp) {
      cout << i << ": " << sp->postText << "\n";
      i++;
    }
  }

  print_comment(gwp.lock());

  cout << "Printing user info now\n";
  for (int i = 0; i < atoi(argv[1]); i++)
    this_thread::sleep_for(5s);

  cp->print();
  return 0;
}
