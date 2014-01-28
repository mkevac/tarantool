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
#include "fiber.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "say.h"
#include "stat.h"
#include "assoc.h"
#include "memory.h"

__thread struct cord *cord_self_ptr = NULL;
static uint32_t last_used_cid = 0;

static void
update_last_stack_frame(struct fiber *fiber)
{
#ifdef ENABLE_BACKTRACE
	fiber->last_stack_frame = __builtin_frame_address(0);
#else
	(void)fiber;
#endif /* ENABLE_BACKTRACE */
}

void
fiber_call(struct fiber *callee, ...)
{
	struct fiber *caller = fiber_self();

	assert(cord_self()->sp + 1 - cord_self()->call_stack < FIBER_CALL_STACK);
	assert(caller);

	cord_self()->fiber = callee;
	*cord_self()->sp++ = caller;

	update_last_stack_frame(caller);

	callee->csw++;

	fiber_self()->flags &= ~FIBER_READY;

	va_start(fiber_self()->f_data, callee);
	coro_transfer(&caller->coro.ctx, &callee->coro.ctx);
	va_end(fiber_self()->f_data);
}

void
fiber_checkstack()
{
	if (cord_self()->sp + 1 - cord_self()->call_stack >= FIBER_CALL_STACK)
		tnt_raise(ClientError, ER_FIBER_STACK);
}

/** Interrupt a synchronous wait of a fiber inside the event loop.
 * We do so by keeping an "async" event in every fiber, solely
 * for this purpose, and raising this event here.
 */

void
fiber_wakeup(struct fiber *f)
{
	if (f->flags & FIBER_READY)
		return;
	f->flags |= FIBER_READY;
	if (rlist_empty(&cord_self()->ready_fibers))
		ev_async_send(&cord_self()->ready_async);
	rlist_move_tail_entry(&cord_self()->ready_fibers, f, state);
}

/** Cancel the subject fiber.
 *
 * Note: this is not guaranteed to succeed, and requires a level
 * of cooperation on behalf of the fiber. A fiber may opt to set
 * FIBER_CANCELLABLE to false, and never test that it was
 * cancelled.  Such fiber can not ever be cancelled, and
 * for such fiber this call will lead to an infinite wait.
 * However, fiber_testcancel() is embedded to the rest of fiber_*
 * API (@sa fiber_yield()), which makes most of the fibers that opt in,
 * cancellable.
 *
 * Currently cancellation can only be synchronous: this call
 * returns only when the subject fiber has terminated.
 *
 * The fiber which is cancelled, has FiberCancelException raised
 * in it. For cancellation to work, this exception type should be
 * re-raised whenever (if) it is caught.
 */

void
fiber_cancel(struct fiber *f)
{
	assert(f->fid != 0);
	assert(!(f->flags & FIBER_CANCEL));

	f->flags |= FIBER_CANCEL;

	if (f == fiber_self()) {
		fiber_testcancel();
		return;
	}
	/*
	 * The subject fiber is passing through a wait
	 * point and can be kicked out of it right away.
	 */
	if (f->flags & FIBER_CANCELLABLE)
		fiber_call(f);

	if (f->fid) {
		/*
		 * The fiber is not dead. We have no other
		 * choice but wait for it to discover that
		 * it has been cancelled, and die.
		 */
		assert(f->waiter == NULL);
		f->waiter = fiber_self();
		fiber_yield();
	}
	/*
	 * Here we can't even check f->fid is 0 since
	 * f could have already been reused. Knowing
	 * at least that we can't get scheduled ourselves
	 * unless asynchronously woken up is somewhat a relief.
	 */
	fiber_testcancel(); /* Check if we're ourselves cancelled. */
}

bool
fiber_is_cancelled()
{
	return fiber_self()->flags & FIBER_CANCEL;
}

/** Test if this fiber is in a cancellable state and was indeed
 * cancelled, and raise an exception (FiberCancelException) if
 * that's the case.
 */
void
fiber_testcancel(void)
{
	if (fiber_is_cancelled())
		tnt_raise(FiberCancelException);
}



/** Change the current cancellation state of a fiber. This is not
 * a cancellation point.
 */
bool
fiber_setcancellable(bool enable)
{
	bool prev = fiber_self()->flags & FIBER_CANCELLABLE;
	if (enable == true)
		fiber_self()->flags |= FIBER_CANCELLABLE;
	else
		fiber_self()->flags &= ~FIBER_CANCELLABLE;
	return prev;
}

/**
 * @note: this is not a cancellation point (@sa fiber_testcancel())
 * but it is considered good practice to call testcancel()
 * after each yield.
 */
void
fiber_yield(void)
{
	struct fiber *callee = *(--cord_self()->sp);
	struct fiber *caller = fiber_self();

	cord_self()->fiber = callee;
	update_last_stack_frame(caller);

	callee->csw++;
	coro_transfer(&caller->coro.ctx, &callee->coro.ctx);
}


struct fiber_watcher_data {
	struct fiber *f;
	bool timed_out;
};

static void
fiber_schedule_timeout(ev_timer *watcher, int revents)
{
	(void) revents;

	assert(fiber_self() == &cord_self()->sched);
	struct fiber_watcher_data *state =
			(struct fiber_watcher_data *) watcher->data;
	state->timed_out = true;
	fiber_call(state->f);
}

/**
 * @brief yield & check timeout
 * @return true if timeout exceeded
 */
bool
fiber_yield_timeout(ev_tstamp delay)
{
	struct ev_timer timer;
	ev_timer_init(&timer, fiber_schedule_timeout, delay, 0);
	struct fiber_watcher_data state = { fiber_self(), false };
	timer.data = &state;
	ev_timer_start(&timer);
	fiber_yield();
	ev_timer_stop(&timer);
	return state.timed_out;
}

void
fiber_yield_to(struct fiber *f)
{
	fiber_wakeup(f);
	fiber_yield();
	fiber_testcancel();
}

/**
 * @note: this is a cancellation point (@sa fiber_testcancel())
 */

void
fiber_sleep(ev_tstamp delay)
{
	fiber_yield_timeout(delay);
	fiber_testcancel();
}

/** Wait for a forked child to complete.
 * @note: this is a cancellation point (@sa fiber_testcancel()).
 * @return process return status
*/
void
fiber_schedule_child(ev_child *watcher, int event __attribute__((unused)))
{
	return fiber_schedule((ev_watcher *) watcher, event);
}

int
wait_for_child(pid_t pid)
{
	ev_child cw;
	ev_init(&cw, fiber_schedule_child);
	ev_child_set(&cw, pid, 0);
	cw.data = fiber_self();
	ev_child_start(&cw);
	fiber_yield();
	ev_child_stop(&cw);
	int status = cw.rstatus;
	fiber_testcancel();
	return status;
}

void
fiber_schedule(ev_watcher *watcher, int event __attribute__((unused)))
{
	assert(fiber_self() == &cord_self()->sched);
	fiber_call((struct fiber *) watcher->data);
}

static void
fiber_ready_async(ev_async *watcher, int revents)
{
	(void) watcher;
	(void) revents;

	while(!rlist_empty(&cord_self()->ready_fibers)) {
		struct fiber *f =
			rlist_first_entry(&cord_self()->ready_fibers, struct fiber, state);
		rlist_del_entry(f, state);
		fiber_call(f);
	}
}

struct fiber *
fiber_find(uint32_t fid)
{
	mh_int_t k = mh_i32ptr_find(cord_self()->fiber_registry, fid, NULL);

	if (k == mh_end(cord_self()->fiber_registry))
		return NULL;
	return (struct fiber *) mh_i32ptr_node(cord_self()->fiber_registry, k)->val;
}

static void
register_fid(struct fiber *fiber)
{
	struct mh_i32ptr_node_t node = { fiber->fid, fiber };
	mh_i32ptr_put(cord_self()->fiber_registry, &node, NULL, NULL);
}

static void
unregister_fid(struct fiber *fiber)
{
	struct mh_i32ptr_node_t node = { fiber->fid, NULL };
	mh_i32ptr_remove(cord_self()->fiber_registry, &node, NULL);
}

void
fiber_gc(void)
{
	if (region_used(&fiber_self()->gc) < 128 * 1024) {
		region_reset(&fiber_self()->gc);
		return;
	}

	region_free(&fiber_self()->gc);
}


/** Destroy the currently active fiber and prepare it for reuse.
 */

static void
fiber_zombificate()
{
	if (fiber_self()->waiter)
		fiber_wakeup(fiber_self()->waiter);
	rlist_del(&fiber_self()->state);
	fiber_self()->waiter = NULL;
	fiber_self()->session = NULL;
	fiber_set_name(fiber_self(), "zombie");
	fiber_self()->f = NULL;
	unregister_fid(fiber_self());
	fiber_self()->fid = 0;
	fiber_self()->flags = 0;
	region_free(&fiber_self()->gc);
	rlist_move_entry(&cord_self()->zombie_fibers, fiber_self(), link);
}

static void
fiber_loop(void *data __attribute__((unused)))
{
	for (;;) {
		assert(fiber_self() != NULL && fiber_self()->f != NULL &&
		       fiber_self()->fid != 0);
		try {
			fiber_self()->f(fiber_self()->f_data);
		} catch (const FiberCancelException& e) {
			say_info("fiber `%s' has been cancelled",
				 fiber_name(fiber_self()));
			say_info("fiber `%s': exiting", fiber_name(fiber_self()));
		} catch (const Exception& e) {
			e.log();
		} catch (...) {
			/*
			 * This can only happen in case of a bug
			 * server bug.
			 */
			say_error("fiber `%s': unknown exception",
				fiber_name(fiber_self()));
			panic("fiber `%s': exiting", fiber_name(fiber_self()));
		}
		fiber_zombificate();
		fiber_yield();	/* give control back to scheduler */
	}
}

/** Set fiber name.
 *
 * @param[in] name the new name of the fiber. Truncated to
 * FIBER_NAME_MAXLEN.
*/

void
fiber_set_name(struct fiber *fiber, const char *name)
{
	assert(name != NULL);
	region_set_name(&fiber->gc, name);
}

/**
 * Create a new fiber.
 *
 * Takes a fiber from fiber cache, if it's not empty.
 * Can fail only if there is not enough memory for
 * the fiber structure or fiber stack.
 *
 * The created fiber automatically returns itself
 * to the fiber cache when its "main" function
 * completes.
 */
struct fiber *
fiber_new(const char *name, void (*f) (va_list))
{
	struct fiber *fiber = NULL;

	if (!rlist_empty(&cord_self()->zombie_fibers)) {
		fiber = rlist_first_entry(&cord_self()->zombie_fibers, struct fiber, link);
		rlist_move_entry(&cord_self()->fibers, fiber, link);
	} else {
		fiber = (struct fiber *) mempool_alloc(&cord_self()->fiber_pool);
		memset(fiber, 0, sizeof(*fiber));

		tarantool_coro_create(&fiber->coro, fiber_loop, NULL);

		region_create(&fiber->gc, &cord_self()->slabc);

		rlist_add_entry(&cord_self()->fibers, fiber, link);
		rlist_create(&fiber->state);
	}


	fiber->f = f;

	fiber->fid = cord_self()->last_used_fid;
	fiber->session = NULL;
	fiber->flags = 0;
	fiber->waiter = NULL;
	fiber_set_name(fiber, name);
	register_fid(fiber);

	return fiber;
}

/**
 * Free as much memory as possible taken by the fiber.
 *
 * @note we don't release memory allocated for
 * struct fiber and some of its members.
 */
void
fiber_destroy(struct fiber *f)
{
	if (f == fiber_self()) /* do not destroy running fiber */
		return;
	if (strcmp(fiber_name(f), "sched") == 0)
		return;

	rlist_del(&f->state);
	region_destroy(&f->gc);
	tarantool_coro_destroy(&f->coro);
}

void
fiber_destroy_all()
{
	struct fiber *f;
	rlist_foreach_entry(f, &cord_self()->fibers, link)
		fiber_destroy(f);
	rlist_foreach_entry(f, &cord_self()->zombie_fibers, link)
		fiber_destroy(f);
}

static void *
cord_thread(void *arg)
{
	struct cord *cord = (struct cord *) arg;

	/* set thread-local variable */
	cord_self_ptr = cord;

	(void) tt_pthread_mutex_lock(&cord->mutex);

	ev_async_start(&cord->ready_async);

	(void) tt_pthread_cond_signal(&cord->cond);
	(void) tt_pthread_mutex_unlock(&cord->mutex);

	void *retval = cord->start_routine(cord->arg);
	return retval;
}

int
cord_create(struct cord *cord, const char *name,
	    void *(*start_routine) (void *), void *arg)
{
	memset(cord, 0, sizeof(cord));

	slab_cache_create(&cord->slabc, &arena_runtime, 0);
	mempool_create(&cord->fiber_pool, &cord->slabc, sizeof(struct fiber));
	rlist_create(&cord->fibers);
	rlist_create(&cord->ready_fibers);
	rlist_create(&cord->zombie_fibers);
	cord->fiber_registry = mh_i32ptr_new();

	memset(&cord->sched, 0, sizeof(cord->sched));
	cord->sched.fid = 1;
	region_create(&cord->sched.gc, &cord->slabc);
	fiber_set_name(&cord->sched, "sched");

	cord->sp = cord->call_stack;
	cord->fiber = &cord->sched;
	/* fids from 0 to 100 are reserved */
	cord->last_used_fid = 100;
	ev_async_init(&cord->ready_async, fiber_ready_async);

	snprintf(cord->name, sizeof(cord->name), "%s", name);
	cord->cid = ++last_used_cid;
	if (start_routine == NULL) {
		assert(cord_self_ptr == NULL);
		cord->thread = pthread_self();
		cord_self_ptr = cord;
		ev_async_start(&cord->ready_async);
		return 0;
	}

	cord->start_routine = start_routine;
	cord->arg = arg;

	tt_pthread_mutex_init(&cord->mutex, 0);
	(void) tt_pthread_mutex_lock(&cord->mutex);
	if (tt_pthread_create(&cord->thread, NULL, cord_thread, cord) != 0)
		goto error;

	/* Synchronously wait until thread starts */
	tt_pthread_cond_init(&cord->cond, 0);
	while (!ev_is_active(&cord->ready_async)) {
		(void) tt_pthread_cond_wait(&cord->cond, &cord->mutex);
	}
	(void) tt_pthread_mutex_unlock(&cord->mutex);
	(void) tt_pthread_cond_destroy(&cord->cond);
	(void) tt_pthread_mutex_destroy(&cord->mutex);

	return 0;
error:
	(void) tt_pthread_mutex_unlock(&cord->mutex);
	(void) tt_pthread_mutex_destroy(&cord->mutex);
	cord_destroy(cord);
	return -1;
}

void
cord_destroy(struct cord *cord)
{
	if (ev_is_active(&cord->ready_async))
		ev_async_stop(&cord->ready_async);

	/* Only clean up if initialized. */
	if (cord->fiber_registry) {
		fiber_destroy_all();
		mh_i32ptr_delete(cord->fiber_registry);
	}

	mempool_destroy(&cord->fiber_pool);
	slab_cache_destroy(&cord->slabc);
}

int
cord_join(struct cord *cord, void **retval)
{
	if (tt_pthread_join(cord->thread, retval) != 0) {
		return -1;
	}

	cord_destroy(cord);
	return 0;
}

int fiber_stat(fiber_stat_cb cb, void *cb_ctx)
{
	struct fiber *fiber;
	int res;
	rlist_foreach_entry(fiber, &cord_self()->fibers, link) {
		res = cb(fiber, cb_ctx);
		if (res != 0)
			return res;
	}
	rlist_foreach_entry(fiber, &cord_self()->zombie_fibers, link) {
		res = cb(fiber, cb_ctx);
		if (res != 0)
			return res;
	}
	return 0;
}
