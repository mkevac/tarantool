#ifndef TARANTOOL_FIBER_H_INCLUDED
#define TARANTOOL_FIBER_H_INCLUDED
/*
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "trivia/config.h"

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include "third_party/tarantool_ev.h"
#include "coro.h"
#include "trivia/util.h"
#include "third_party/queue.h"
#include "small/mempool.h"
#include "small/region.h"

#if defined(__cplusplus)
#include "exception.h"
#endif /* defined(__cplusplus) */
#include "salad/rlist.h"

#define FIBER_NAME_MAX REGION_NAME_MAX
#define FIBER_READING_INBOX (1 << 0)
/** This fiber can be cancelled synchronously. */
#define FIBER_CANCELLABLE   (1 << 1)
/** Indicates that a fiber has been cancelled. */
#define FIBER_CANCEL        (1 << 2)
/** This fiber was created via stored procedures API. */
#define FIBER_USER_MODE     (1 << 3)
/** This fiber was marked as ready for wake up */
#define FIBER_READY	    (1 << 4)

/** This is thrown by fiber_* API calls when the fiber is
 * cancelled.
 */

#if defined(__cplusplus)
class FiberCancelException: public Exception {
public:
	FiberCancelException(const char *file, unsigned line)
		: Exception(file, line) {
		/* Nothing */
	}

	virtual void log() const {
		say_debug("FiberCancelException");
	}
};
#endif /* defined(__cplusplus) */

struct fiber {
#ifdef ENABLE_BACKTRACE
	void *last_stack_frame;
#endif
	int csw;
	struct tarantool_coro coro;
	/* A garbage-collected memory pool. */
	struct region gc;
	/** Fiber id. */
	uint32_t fid;
	/**
	 * The logical user session the fiber is running
	 * on behalf of. The concept of an associated session
	 * is similar to the concept of controlling tty
	 * in a UNIX process. When a fiber is created,
	 * it has no session. If it's running a request on behalf
	 * of a user connection, it's session is changed
	 * to represent this connection.
	 */
	struct session *session;

	struct rlist link;
	struct rlist state;

	/* This struct is considered as non-POD when compiling by g++.
	 * You can safetly ignore all offset_of-related warnings.
	 * See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=31488
	 */
	void (*f) (va_list);
	va_list f_data;
	uint32_t flags;
	struct fiber *waiter;
	uint64_t cookie;
};

typedef void(*fiber_func)(va_list);
struct fiber *fiber_new(const char *name, fiber_func f);
void fiber_set_name(struct fiber *fiber, const char *name);
int wait_for_child(pid_t pid);

static inline const char *
fiber_name(struct fiber *f)
{
	return region_name(&f->gc);
}

void
fiber_checkstack();

void fiber_yield(void);
void fiber_yield_to(struct fiber *f);

/**
 * @brief yield & check for timeout
 * @return true if timeout exceeded
 */
bool fiber_yield_timeout(ev_tstamp delay);


void fiber_destroy_all();

void fiber_gc(void);
void fiber_call(struct fiber *callee, ...);
void fiber_wakeup(struct fiber *f);
struct fiber *fiber_find(uint32_t fid);
/** Cancel a fiber. A cancelled fiber will have
 * tnt_FiberCancelException raised in it.
 *
 * A fiber can be cancelled only if it is
 * FIBER_CANCELLABLE flag is set.
 */
void fiber_cancel(struct fiber *f);
/** Check if the current fiber has been cancelled.  Raises
 * tnt_FiberCancelException
 */
void fiber_testcancel(void);
/** Make it possible or not possible to cancel the current
 * fiber.
 *
 * return previous state.
 */
bool fiber_setcancellable(bool enable);
void fiber_sleep(ev_tstamp s);
struct tbuf;
void fiber_schedule(ev_watcher *watcher, int event __attribute__((unused)));

/** Set or clear this fiber's session. */
static inline void
fiber_set_session(struct fiber *f, struct session *session)
{
	f->session = session;
}

typedef int (*fiber_stat_cb)(struct fiber *f, void *ctx);

int fiber_stat(fiber_stat_cb cb, void *cb_ctx);

enum { FIBER_CALL_STACK = 16 };

/**
 * @brief An independent execution unit that can be managed by a separate OS
 * thread. Each cord consists of fibers to implement cooperative multitasking
 * model.
 */
struct cord {
	struct fiber *fiber;
	struct ev_loop *loop;
	pthread_t thread;
	uint32_t cid;
	uint32_t last_used_fid;
	struct fiber *call_stack[FIBER_CALL_STACK];
	struct fiber **sp;
	struct mh_i32ptr_t *fiber_registry;
	struct rlist fibers;
	struct rlist zombie_fibers;
	struct rlist ready_fibers;
	ev_async ready_async;
	struct mempool fiber_pool;
	struct fiber sched;
	struct slab_cache slabc;
	char name[REGION_NAME_MAX];

	/* used by cord_create */
	void *(*start_routine) (void *);
	void *arg;
};

extern __thread struct cord *cord_self_ptr;

static inline struct cord *
cord_self(void)
{
	return cord_self_ptr;
}

static inline struct fiber *
fiber_self(void)
{
	return cord_self()->fiber;
}

int
cord_create(struct cord *cord, const char *name,
	    void *(*start_routine) (void *), void *arg);

void
cord_destroy(void);

#endif /* TARANTOOL_FIBER_H_INCLUDED */
