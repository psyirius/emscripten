/*
 * Copyright 2021 The Emscripten Authors.  All rights reserved.
 * Emscripten is available under two separate licenses, the MIT license and the
 * University of Illinois/NCSA Open Source License.  Both these licenses can be
 * found in the LICENSE file.
 */

#pragma once

#include <emscripten/emscripten.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to a set of thread-local work queues to which work can be
// asynchronously or synchronously proxied from other threads. When work is
// proxied to a queue on a particular thread, that thread is notified to start
// processing work from that queue if it is not already doing so.
typedef struct em_proxying_queue em_proxying_queue;

// Create and destroy proxying queues.
em_proxying_queue* em_proxying_queue_create();
void em_proxying_queue_destroy(em_proxying_queue* q);

// Get the queue used for proxying low-level runtime work. Work on this queue
// may be processed at any time inside system functions, so it must be
// nonblocking and safe to run at any time, similar to a native signal handler.
em_proxying_queue* emscripten_proxy_get_system_queue();

// Execute all the tasks enqueued for the current thread on the given queue. New
// tasks that are enqueued concurrently with this execution will be executed as
// well. This function returns once it observes an empty queue.
void emscripten_proxy_execute_queue(em_proxying_queue* q);

// Opaque handle to a currently-executing proxied task, used to signal the end
// of the task.
typedef struct em_proxying_ctx em_proxying_ctx;

// Signal the end of a proxied task.
void emscripten_proxy_finish(em_proxying_ctx* ctx);

// Enqueue `func` on the given queue and thread and return immediately. Returns
// 1 if the work was successfully enqueued and the target thread notified or 0
// otherwise.
int emscripten_proxy_async(em_proxying_queue* q,
                           pthread_t target_thread,
                           void (*func)(void*),
                           void* arg);

// Enqueue `func` on the given queue and thread and wait for it to finish
// executing before returning. Returns 1 if the task was successfully completed
// and 0 otherwise.
int emscripten_proxy_sync(em_proxying_queue* q,
                          pthread_t target_thread,
                          void (*func)(void*),
                          void* arg);

// Enqueue `func` on the given queue and thread and wait for it to be executed
// and for the task to be marked finished with `emscripten_proxying_finish`
// before returning. Returns 1 if the task was successfully completed and 0
// otherwise.
int emscripten_proxy_sync_with_ctx(em_proxying_queue* q,
                                   pthread_t target_thread,
                                   void (*func)(em_proxying_ctx*, void*),
                                   void* arg);

#ifdef __cplusplus
} // extern "C"

#if __cplusplus < 201103L
#warning "C++ ProxyingQueue support requires building with -std=c++11 or newer!"
#else

#include <functional>
#include <thread>
#include <utility>

namespace emscripten {

// A thin C++ wrapper around the underlying C API.
class ProxyingQueue {
  em_proxying_queue* queue = nullptr;

  static void runAndFree(void* arg) {
    auto f = (std::function<void()>*)arg;
    (*f)();
    delete f;
  }

  static void run(void* arg) {
    auto f = *(std::function<void()>*)arg;
    f();
  }

  static void runWithCtx(em_proxying_ctx* ctx, void* arg) {
    auto f = *(std::function<void(ProxyingCtx)>*)arg;
    f(ProxyingCtx{ctx});
  }

public:
  ProxyingQueue() { queue = em_proxying_queue_create(); }
  // ProxyingQueue can be moved but not copied. It is not valid to call any
  // methods on ProxyingQueues that have been moved out of.
  ProxyingQueue& operator=(const ProxyingQueue&) = delete;
  ProxyingQueue& operator=(ProxyingQueue&& other) {
    queue = other.queue;
    other.queue = nullptr;
    return *this;
  }
  ProxyingQueue(const ProxyingQueue&) = delete;
  ProxyingQueue(ProxyingQueue&& other) { *this = std::move(other); }
  ~ProxyingQueue() {
    if (queue) {
      em_proxying_queue_destroy(queue);
    }
  }

  class ProxyingCtx {
    em_proxying_ctx* ctx;

  public:
    ProxyingCtx(em_proxying_ctx* ctx) : ctx(ctx) {}
    void finish() { emscripten_proxy_finish(ctx); }
  };

  void execute() { emscripten_proxy_execute_queue(queue); }

  // Return true if the work was successfully enqueued, false otherwise.
  bool proxyAsync(pthread_t target, std::function<void()>&& func) {
    std::function<void()>* arg = new std::function<void()>(std::move(func));
    return emscripten_proxy_async(queue, target, runAndFree, (void*)arg);
  }
  bool proxySync(const pthread_t target, const std::function<void()>& func) {
    return emscripten_proxy_sync(queue, target, run, (void*)&func);
  }
  bool proxySyncWithCtx(const pthread_t target,
                        const std::function<void(ProxyingCtx)>& func) {
    return emscripten_proxy_sync_with_ctx(
      queue, target, runWithCtx, (void*)&func);
  }
};

} // namespace emscripten

#endif // __cplusplus < 201103L
#endif // __cplusplus
