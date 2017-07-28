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
 * gcdemo.cpp
 *
 *  Created on: May 31, 2016
 *      Author: uversky
 */

#include "gcdemo.h"
#include <climits>
#include <getopt.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

using namespace std;
using namespace mpgc;

unsigned const long _DEFAULT_NUM_USERS = 1e6,
                    _DEFAULT_NUM_ITERS = 10000;
unsigned const int  _DEFAULT_NUM_THREADS = 1,
                    _DEFAULT_IDEAL_WORK_RATE = 100;
const double        _DEFAULT_MEAN_POST_TAGS = 3.0,
                    _DEFAULT_MEAN_COMMENT_TAGS = 1.0,
                    _DEFAULT_RATIO = 0.6;
const string        _DEFAULT_NAME = "com.hpe.gcdemo.users";

static string prName = _DEFAULT_NAME;

constexpr unsigned long STEADY_STATE_ITERS = 50000;

bool g_isDebugMode = false;
bool g_isSteadyState = false;

once_flag g_onceFlags[4];

AtomicULPtr   g_iterCtr;
unsigned long g_iterMax;

AtomicUIPtr   g_threadCtr;
unsigned int  g_threadMax;

AtomicULPtr   g_usersNotified;
gc_array_ptr<atomic<bool>> g_feedsFull;
AtomicULPtr   g_feedsFullCtr;

size_t        g_idealWorkRate;
AtomicULPtr   g_misses;
AtomicULPtr   g_total_window;

thread_local chrono::steady_clock::time_point t1;
thread_local size_t current_rcu_count = 0;

constexpr size_t rcu_count_per_window = 10;
static chrono::microseconds maxTimeForIter;

thread_local RandomSeed random_seed;

void show_usage() {
   cerr << "usage: ./gcdemo [options]\n"
        << "\n"
        << "Runs a client that randomly picks a user from the graph to either post or comment.\n"
        << "  - Posts are tagged with a random selection of the user's friends and then pushed to\n"
        << "    the feeds of tagged users and their friends.\n"
        << "  - Comments are tied to a random post in a user's feed, add a random selection of\n"
        << "    the user's friends to tags, and then bump the post to all of the tagged users\n"
        << "    and their friends.\n"
        << "\n"
        << "Options:\n"
        << "-b, --txn-rate-" << underline("b") << "ench\n"
        << "  EXPERIMENTAL\n"
        << "  This flag currently sets a mode for measuring the transaction rate - i.e., the\n"
        << "  number of action iterations per second per thread.\n"
        << "-C, --" << underline("c") << "omment-tag-mean <C>\n"
        << "  Sets the mean number of tags added by each new comment to its parent post.\n"
        << "  Default: " << _DEFAULT_MEAN_COMMENT_TAGS << ".\n"
        << "-d, --" << underline("d") << "ebug-mode\n"
        << "  Outputs memory statistics occasionally during benchmarking mode.\n"
        << "  No effect unless -b is set.\n"
        << "-h, --" << underline("h") << "elp\n"
        << "  Display this message.\n"
        << "-i, --" << underline("i") << "ters <i>\n"
        << "  Each process will perform <i> action iterations, divided evenly among threads.\n"
        << "  An action involves picking a random user and then having that user either\n"
        << "    comment or post, based on the probability specified by -r.\n"
        << "  No effect if -b is set.\n"
        << "  Default: " << _DEFAULT_NUM_ITERS << ".\n"
        << "-n, --" << underline("n") << "ame <n>\n"
        << "  Specify the name (i.e. key) used to locate the graph in the persistent heap.\n"
        << "  Default: \'" << _DEFAULT_NAME << "\'.\n"
        << "-P, --" << underline("p") << "ost-tag-mean <P>\n"
        << "  Sets the mean number of users tagged when a new post is made.\n"
        << "  Default: " << _DEFAULT_MEAN_POST_TAGS << ".\n"
        << "-r, --" << underline("r") << "atio <r: [0-1]>\n"
        << "  Indicates the probability that an action taken by a client process will result\n"
        << "  in a post.  1 indicates that all clients will only post, 0 that all will comment.\n"
        << "  Default: " << _DEFAULT_RATIO << ".\n"
        << "-t, --num-" << underline("t") << "hreads <t>\n"
        << "  Specifies the number of worker threads to use per process.\n"
        << "  Default: " << _DEFAULT_NUM_THREADS << ".\n"
        << "-w, --ideal-" << underline("w") << "ork-rate <w>\n"
        << "  Specifies the ideal work rate (number of iterations per second) for the client.\n"
        << "  Default: " << _DEFAULT_IDEAL_WORK_RATE << ".\n";
}

template<typename Fn>
void notifyUsers(unordered_set<gc_ptr<User>>& users, gc_ptr<Post>& post, Fn&& func) {
  bool feedFullFlag;
  for (auto u : users) {
    //Do Work
    forward<Fn>(func)(u, post);

    feedFullFlag = false;
    if (g_isSteadyState && u->isFeedFull() && g_feedsFull[u->id].compare_exchange_strong(feedFullFlag, true)) {
      auto c = g_feedsFullCtr->fetch_add(1);
      if (c % 1000 == 0) {
        cout << "Found " << c << " users with full feeds" << endl;
      }
    }

    current_rcu_count++;
    if (current_rcu_count == rcu_count_per_window) {
      current_rcu_count = 0;
      chrono::steady_clock::time_point t2 = chrono::steady_clock::now();
      auto timeForIter = chrono::duration_cast<chrono::microseconds>(t2 - t1);
      t1 = t2;
      if (timeForIter > maxTimeForIter) {
        // We exceeded the maximum time allotment for one iteration.  Increment the miss counter.
        auto windows_missed = timeForIter / maxTimeForIter;
        g_misses->fetch_add(windows_missed);
        g_total_window->fetch_add(windows_missed);
      } else {
        g_total_window->fetch_add(1);
        // Sleep for the remaining chunk of time.
        // --------------------------------------------------
        // |  timeForIter |              diff               |
        // --------------------------------------------------
        chrono::microseconds diff = maxTimeForIter - timeForIter;
        while (diff.count() > 0) {
          this_thread::sleep_for(diff);
          t2 = chrono::steady_clock::now();
          diff -= chrono::duration_cast<chrono::microseconds>(t2 - t1);
          t1 = t2;
        }
      }
    }
  }
}

unsigned long post(gc_ptr<User> usr, double postTagMean, unsigned long numUsers)
{
  // Create a payload for a new post, then the new post
  string payload(UniformRNG(100, 1000).randElt(), 'p');

  unsigned int numFriends = usr->friends.size();
  gc_array_ptr<gc_ptr<User>> newTags;
  if (numFriends) {
    TagRNG rng(postTagMean, 0, numFriends);
    unsigned int numTags = rng.numPostTags();
    newTags = make_gc_array<gc_ptr<User>>(numTags);

    // Tag a number of friends as well
    for (unsigned int i = 0; i < numTags; i++) {
      newTags[i] = usr->friends[rng.randElt()];
    }
  } else {
    newTags = make_gc_array<gc_ptr<User>>(0);
  }
  gc_ptr<Post> newPost = make_gc<Post>(usr, payload, newTags);

  // Identify set of tagged users and their friends
  unordered_set<gc_ptr<User>> usersToNotify;
  //All tags must be in my friends.
  numFriends++;
  for (auto u : newTags) {
    numFriends += u->friends.size();
  }

  usersToNotify.reserve(numFriends);
  usersToNotify.insert(usr);
  usersToNotify.insert(usr->friends.begin(), usr->friends.end());
  for (auto u : newTags) {
    for (auto f : u->friends) {
      usersToNotify.insert(f);
    }
  }

  // Push the new post to all of the users in the notification set
  notifyUsers(usersToNotify, newPost, [](gc_ptr<User> &u, gc_ptr<Post> &p) { return u->pushToFeed(p); });
 
  // Indicate how many users were affected by post
  return usersToNotify.size();
}

unsigned long comment(gc_ptr<User> usr, double commentTagMean, double postTagMean, unsigned long numUsers)
{
  auto localFeed = usr->feed.load();
  unsigned int feedSize   = localFeed.size();

  assert(feedSize > 0); // Shouldn't be commenting if we have nothing to comment on
  UniformRNG postrng(feedSize);
  gc_ptr<Post> p = nullptr;

  // Pick post at random from user's feed
#ifdef TEST_WEAK_PTRS
  for (size_t i = 0; i < feedSize; i++) {
    p = localFeed[postrng.randElt()].lock();
    if (p) {
      break;
    }
  }

  if (!p) {
    return post(usr, postTagMean, numUsers);
  }
#else
  p = localFeed[postrng.randElt()];
#endif
  // Create a comment and attach it to the post
  string payload(UniformRNG(100, 1000).randElt(), 'c');
  // Tag some friends
  gc_array_ptr<gc_ptr<User>> newTags;
  unsigned int numFriends = usr->friends.size();
  if (numFriends) {
    TagRNG rng(0, commentTagMean, numFriends);
    unsigned int numTags = rng.numCommentTags();
    newTags = make_gc_array<gc_ptr<User>>(numTags);
    for (unsigned int i = 0; i < numTags; i++) {
      newTags[i] = usr->friends[rng.randElt()];
    }
  } else {
    newTags = make_gc_array<gc_ptr<User>>(0);
  }

  auto comment = make_gc<Comment>(usr, p, payload, newTags);
  p->addComment(comment);

  // Bump post to top of feeds for each tagged user and their friends
  auto localSubs = (p->subscribers).load();
  unordered_set<gc_ptr<User>> usersToNotify;
  //Subscribers list contain this user.
  numFriends += localSubs.size();
  for (auto u : newTags) {
    numFriends += u->friends.size();
  }
  usersToNotify.reserve(numFriends);
  usersToNotify.insert(usr->friends.begin(), usr->friends.end());
  usersToNotify.insert(localSubs.begin(), localSubs.end());
  // Add the friends of all the tagged users to the notification set
  for (auto u : newTags)
    for (auto f : u->friends)
      usersToNotify.insert(f);

  if (usersToNotify.size() > 100000) {
    cout << "Notified: " << usersToNotify.size() << " Subscribers: " << localSubs.size() << "\n";
  }
  // Bump the post for all the users in the notification set
  notifyUsers(usersToNotify, p, [](gc_ptr<User>& u, gc_ptr<Post>& p) { return u->bumpInFeed(p); });

  return usersToNotify.size();
}

void clientThread(UserGraphPtr users, unsigned long iters, double postTagMean,
                  double commentTagMean, double ratio, unsigned int myTID)
{
  initialize_thread();  // required for gc-awareness
  pid_t myPID = getpid();

  auto numUsers = users->size();
  TagRNG tagrng(postTagMean, commentTagMean, numUsers);
  ActionRNG actionrng(ratio);

  // Open file for logging
  string logName = "outlog/" + to_string(myPID) + "-" + to_string(myTID) + ".log";
  ofstream log;
  log.open(logName);

  // Start all threads simultaneously
  unsigned long ctr = g_threadCtr->fetch_add(1);
  if (ctr == 0) {
    // Ensure that only one thread displays the progress bar header
    cout << "Beginning output logging." << endl << flush;
    displayProgressBarHeader();
  }
  while (g_threadCtr->load() != g_threadMax)
    ;

  // Run until global iteration counter maxes out
  unsigned long i = g_iterCtr->fetch_add(1);
  while ( i < iters ) {
    advanceProgressBar(i, iters, g_isDebugMode);

    // Pick random user
    auto u = (*users)[tagrng.randElt()];

    log << "(" << myPID << ":" << myTID << ") "
           << "Iteration <" << i << ">: "
           << "User " + u->name + " [id: " << u->id << "] wrote a new ";

    unsigned int feedSize = (u->feed).load().size();
    if (actionrng.isPost() || feedSize == 0) {
      post(u, postTagMean, numUsers);
      log << "post.";
    }
    else {
      comment(u, commentTagMean, postTagMean, numUsers);
      log << "comment.";
    }

    log << endl;
    i = g_iterCtr->fetch_add(1);
  }

  log.close();

  // Have the last thread wrap it up.
  if (i == iters) {
    advanceProgressBar(i, iters, g_isDebugMode);
    cout << "Client threads have completed." << endl;
    cout << "Check the outlog/ folder for logs." << endl << flush;
  }
}

void benchThread(UserGraphPtr users, unsigned long iters, double postTagMean, 
                 double commentTagMean, double ratio, unsigned int myTID,
                 unsigned int numThreads, bool ssFlag = false)
{
  initialize_thread();  // required for gc-awareness

  auto numUsers = users->size();
  TagRNG tagrng(postTagMean, commentTagMean, numUsers);
  ActionRNG actionrng(ratio);

  // Start all threads simultaneously
  unsigned int ctr = g_threadCtr->fetch_add(1);
  if (ctr == 0) {
    // Ensure that only one thread displays the progress bar header
    cout << "Beginning benchmark for " << iters << " iterations." << endl << flush;
    displayProgressBarHeader();
  }

  while (g_threadCtr->load() != g_threadMax)
    ;

  chrono::steady_clock::time_point startBench, endBench;
  startBench = chrono::steady_clock::now();
  t1 = startBench;
  // Run until global iteration counter maxes out
  unsigned long i = g_iterCtr->fetch_add(1);
  while ( i < iters || ssFlag) {
    advanceProgressBar(i, iters, g_isDebugMode);

    // Take an action
    auto index = tagrng.randElt();
    auto u = (*users)[index];
    unsigned int feedSize = (u->feed).load().size();

    unsigned long ret = (actionrng.isPost() || feedSize == 0)
      ? post(u, postTagMean, numUsers)
      : comment(u, commentTagMean, postTagMean, numUsers);

    g_usersNotified->fetch_add(ret);

    if (ssFlag && (double)g_feedsFullCtr->load() / numUsers >= 0.25) {
      break;
    }
    i = g_iterCtr->fetch_add(1);
  }
  ctr = g_threadCtr->fetch_sub(1);
  // Have the last thread to finish indicate that the benchmark is over.
  if (ctr == 1) {
    advanceProgressBar(i, iters, g_isDebugMode);
    endBench = chrono::steady_clock::now();

    if (ssFlag) {
      persistent_roots().remove(prName + ".ssFeeds-Full");
      persistent_roots().remove(prName + ".ssFeeds-Full-Counter");
      cout << "Total iterations done: " << g_iterCtr->load() << endl;
    }

    auto d = chrono::duration_cast<chrono::milliseconds>(endBench - startBench);
    cout << "Finished in " << d.count() << " ms." << endl;

    cout  << "Total RCU Windows: " << g_total_window->load()
          << " Missed: " << g_misses->load() << endl;

    unsigned long  userTotal = g_usersNotified->load();
    cout << "Users notified:"
         << " AVG:   " << userTotal / iters << " per iteration"
         << " Total: " << userTotal << endl;

    cout << "Miss rate displayed below. " << endl;
    cout << ((double)g_misses->load() / g_total_window->load()) << endl;
  }
}

int main(int argc, char ** argv)
{
  // For legibility's sake, long numbers should print with comma separators.
  // This is accomplished by setting the locale for our output streams - here
  // we set to "", or the local machine's current language settings.
  cout.imbue(locale(""));
  cerr.imbue(locale(""));

  // Boilerplate for option processing
  struct option long_options[] = {
    {"txn-rate-bench",   no_argument,       0, 'b'},
    {"comment-tag-mean", required_argument, 0, 'C'},
    {"help",             no_argument,       0, 'h'},
    {"iters",            required_argument, 0, 'i'},
    {"name",             required_argument, 0, 'n'},
    {"post-tag-mean",    required_argument, 0, 'P'},
    {"ratio",            required_argument, 0, 'r'},
    {"num-threads",      required_argument, 0, 't'},
    {"ideal-work-rate",  required_argument, 0, 'w'},
    {0,                  0,                 0,  0 }
  };

  unsigned long iters          = _DEFAULT_NUM_ITERS;
  unsigned int  numThreads     = _DEFAULT_NUM_THREADS;
  double        postTagMean    = _DEFAULT_MEAN_POST_TAGS,
                commentTagMean = _DEFAULT_MEAN_COMMENT_TAGS,
                ratio          = _DEFAULT_RATIO;
  bool          isBenchmark    = false;

  int opt;
  g_idealWorkRate = _DEFAULT_IDEAL_WORK_RATE;

  while((opt = getopt_long(argc, argv, "bC:dhi:n:P:r:t:w:", long_options, nullptr)) != -1) {
    switch(opt) {
      case 'b': isBenchmark = true;
                break;
      case 'C': commentTagMean = strtod(optarg, nullptr);
                break;
      case 'd': g_isDebugMode = true;
                break;
      case 'h': show_usage();
                return 0;
      case 'i': iters = atol(optarg);
                break;
      case 'n': prName = optarg;
                break;
      case 'P': postTagMean = strtod(optarg, nullptr);
                break;
      case 'r': ratio = strtod(optarg, nullptr);
                break;
      case 't': numThreads = atoi(optarg);
                break;
      case 'w': g_idealWorkRate = atoi(optarg);
                break;
      case '?': show_usage();
                return -1;
    }
  }

  // More argument checking boilerplate...
  bool checkFailed = false;
  if ((checkFailed = (numThreads == 0)))
    cerr << "Number of threads must be a positive integer.\n";
  else if ((checkFailed = (iters == 0)))
    cerr << "Number of iterations must be a positive integer.\n";
  else if ((checkFailed = (postTagMean <= 0)))
    cerr << "Post tag mean must be positive.\n";
  else if ((checkFailed = (commentTagMean <= 0)))
    cerr << "Comment tag mean must be positive.\n";
  else if ((checkFailed = (ratio < 0 || ratio > 1)))
    cerr << "Ratio must be in [0-1] inclusive.\n";
  else if ((checkFailed = (g_idealWorkRate <= 0)))
    cerr << "Ideal work rate must be a positive integer.\n";
  if (checkFailed) {
    show_usage();
    return -1;
  }

  UserGraphPtr userPtr = persistent_roots().lookup<WrappedUserGraph>(prName);
  if (userPtr == nullptr) {
    cerr << "Persistent heap key lookup failed:" << endl
         << "   \'" << prName << "\' not found." << endl;
    return -1;
  }
  
  // Retrieve counters from persistent heap
  string phIterCtrName        = prName + ".iterCtr";
  string phRunningThreadsName = prName + ".runningThreads";
  string phMaxThreadsName     = prName + ".maxThreads";
  string phUsersNotifiedName  = prName + ".usersNotified";
  string phMissesName         = prName + ".misses";
  string phTotalWindowsName   = prName + ".totalWindows";
  string ssFeedsFullName      = prName + ".ssFeeds-Full";
  string ssFeedsFullCtrName   = prName + ".ssFeeds-Full-Counter";
  
  g_iterMax = iters;
  g_iterCtr = persistent_roots().lookup<gc_wrapped<atomic<unsigned long>>>(phIterCtrName);
  g_threadCtr = persistent_roots().lookup<gc_wrapped<atomic<unsigned int>>>(phRunningThreadsName);
  g_usersNotified = persistent_roots().lookup<gc_wrapped<atomic<unsigned long>>>(phUsersNotifiedName);
  g_misses = persistent_roots().lookup<gc_wrapped<atomic<unsigned long>>>(phMissesName);
  g_total_window = persistent_roots().lookup<gc_wrapped<atomic<unsigned long>>>(phTotalWindowsName);
  g_feedsFull = persistent_roots().lookup<gc_array<atomic<bool>>>(ssFeedsFullName);
  g_feedsFullCtr = persistent_roots().lookup<gc_wrapped<atomic<unsigned long>>>(ssFeedsFullCtrName);
  auto threadMaxPtr = persistent_roots().lookup<gc_wrapped<atomic<unsigned int>>>(phMaxThreadsName);

  assert(g_iterCtr != nullptr && g_threadCtr != nullptr && g_usersNotified != nullptr
         && threadMaxPtr != nullptr && g_misses != nullptr);

  g_threadMax = threadMaxPtr->load();
  g_isSteadyState = g_feedsFull && g_feedsFullCtr;
  maxTimeForIter = chrono::microseconds((1000 * 1000 * g_threadMax * rcu_count_per_window) / g_idealWorkRate);
  cout << "maxTimeForIter: " << maxTimeForIter.count() << endl;

  // Spawn worker threads
  thread t[numThreads];
  for (unsigned int i = 0; i < numThreads; i++)
    t[i] = (isBenchmark
            ? thread(benchThread, userPtr, iters, postTagMean, commentTagMean,
                     ratio, i+1, numThreads, g_feedsFull && g_feedsFullCtr)
            : thread(clientThread, userPtr, iters, postTagMean, commentTagMean,
                     ratio, i+1));
  for (unsigned int i = 0; i < numThreads; i++)
    t[i].join();
  return 0;
}
