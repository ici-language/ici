#ifndef ICI_ARRAY_H
#define ICI_ARRAY_H

/*
 * array.h - ICI array objects.
 */

#ifndef ICI_OBJECT_H
#include "object.h"
#endif

/*
 * The following portion of this file exports to ici.h. --ici.h-start--
 */
/*
 * The ICI array object.  From an ici program's point of view, an array is an
 * ordered sequence of objects, indexed by consecutive integers starting at 0.
 * The function nels() returns the number of entries currently in the
 * sequence.  Assigning to an index beyond the current end of the sequence
 * will grow the array to hold that index, and NULL-fill the gap.  The
 * functions push() and pop() provide efficient insertion and deletion at the
 * end of the sequence, allowing an array to be used as a stack.  The
 * functions rpush() and rpop() provide efficient insertion and deletion at
 * the beginning of the sequence, causing all existing elements to shift by
 * one place; this allows an array to be used as a double ended queue
 * ("deque").
 *
 * Array objects are implemented as growable circular buffers.  A single
 * contiguous allocation of object pointers is associated with the array.  A
 * pair of pointers, a_base and a_limit, point to the start of the allocation,
 * and one past its end, respectively.  The size of the allocation is thus
 * (a_limit - a_base).  A second pair of pointers, a_bot and a_top, point to
 * the start of the sequence, and one past its end, respectively.  An array is
 * empty when a_top == a_bot.
 *
 * When dealing with array objects in C, we recognise two kinds of array: a
 * "stack" and a "queue".  (The difference is only visible from C, not from
 * ici.)  A "stack" is defined as any array in which a_bot == a_base:
 *       ooooooooooooooooooooo.....................
 *       ^a_base              ^a_top               ^a_limit
 *       ^a_bot
 * The size (nels) of a stack is (a_top - a_bot).  The stack is at capacity
 * when a_top == a_limit.  Pushing and popping elements (up to capacity) is a
 * simple matter of moving a_top, so is efficient.  Any array which has never
 * been rpop()ed or rpush()ed is guaranteed to be a stack.
 *
 * A "queue" is defined as any array in which a_bot != a_base (ie, any array
 * that is not a stack).  This happens the first time rpop() or rpush() are
 * used on a stack, as these functions move a_bot.  If we rpop() a stack,
 * a_bot advances, and we get a queue of this form:
 *       ..............ooooooooooooooo.............
 *       ^a_base       ^a_bot         ^a_top       ^a_limit
 * If we rpush() a stack, a_bot wraps to a_limit, then moves backwards, and we
 * get a queue of this form, where a_top < a_bot, and the sequence has split
 * into two disjoint parts:
 *       oooooooooooooo.................ooooooooooo
 *       ^a_base       ^a_top           ^a_bot     ^a_limit
 * If we repeatedly push() a queue of the first form, so a_top reaches
 * a_limit, a_top wraps to a_base, forming a queue of the second form.  Note
 * that in a queue, unlike a stack, a_top must be strictly less than a_limit.
 *
 * As we push() or rpush() more objects onto a wrapped queue, a_top and a_bot
 * get closer to each other.  However, we cannot fill the last entry in the
 * allocation, because then a_top == a_bot, which signals an empty array.  So,
 * a queue (unlike a stack) must always have one empty entry, and is at
 * capacity when a_top == (a_bot - 1):
 *       oooooooooooooooooooooooooooooo.ooooooooooo
 *       ^a_base                        ^a_bot     ^a_limit
 *                                     ^a_top
 * The size (nels) of a queue is (a_top - a_bot) unless a_top < a_bot; in
 * that case the size of the two disjoint parts are calculated and added.
 *
 * For both queues and stacks, assigning to an array index that exceeds the
 * current capacity, or push()ing/rpush()ing an array that is at capacity (as
 * defined above), will cause the array to grow before the operation can
 * happen.  The allocation is increased by 50%, with a minimum allocation of 8
 * elements.  The existing array sequence is placed at the beginning of the
 * new allocation; so the resulting array is always a stack (a_bot == a_base)
 * even if it was a queue before.
 *
 * The ici execution engine itself makes extensive use of stacks, but never
 * turns them into queues.  For this reason, there are some macros and
 * functions in the ici C API which can only be used on array that is known to
 * be a stack, and which are more efficient than the functions that deal with
 * the general case.  These are:
 *
 *     ici_stk_push_chk(a, n) - check there is room to push at least n elements
 *     ici_stk_probe(a, i) - ensure the stack has i as a valid index
 *
 * But, if you can not guarantee (by context) that the array is a stack, you
 * can only push, pop, rpush or rpop single object pointers at a time.
 * Basically, the end you are dealing with may be near the wrap point of the
 * circular buffer.
 *
 * A data structure such as this should really use an integer count to
 * indicate how many used elements there are.  By using pure pointers we have
 * to keep one empty slot so we don't find ourselves unable to distinguish
 * full from empty (a_top == a_bot).  But by using simple pointers, only a_top
 * needs to change as we push and pop elements.  If a_top == a_bot, the array
 * is empty.
 *
 * Note that one must never take the atomic form of a stack, and assume the
 * result is still a stack.
 */
struct ici_array
{
    ici_obj_t   o_head;
    ici_obj_t   **a_top;    /* The next free slot. */
    ici_obj_t   **a_bot;    /* The first used slot. */
    ici_obj_t   **a_base;   /* The base of allocation. */
    ici_obj_t   **a_limit;  /* Allocation limit, first one you can't use. */
};
#define arrayof(o)  ((ici_array_t *)(o))
#define isarray(o)  ((o)->o_tcode == TC_ARRAY)

/*
 * Check that there is room for 'n' new elements on the end of 'a'.  May
 * reallocate array memory to get more room. Return non-zero on failure,
 * usual conventions.
 *
 * This macro can only be used where the array has never had elements
 * rpush()ed or rpop()ed. See the discussion on
 * 'Accessing ICI array object from C' before using.
 *
 * This --macro-- forms part of the --ici-ap--.
 */
#define ici_stk_push_chk(a, n) \
                ((a)->a_limit - (a)->a_top < (n) ? ici_grow_stack((a), (n)) : 0)

/*
 * Ensure that the stack a has i as a valid index.  Will grow and NULL fill
 * as necessary. Return non-zero on failure, usual conventions.
 */
#define ici_stk_probe(a, i) ((a)->a_top - (a)->a_bot <= (i) \
                ? ici_fault_stack((a), (i)) \
                : 0)

/*
 * A macro to assist in doing for loops over the elements of an array.
 * Use as:
 *
 *  ici_array_t  *a;
 *  ici_obj_t    **e;
 *  for (e = ici_astart(a); e != ici_alimit(a); e = ici_anext(a, e))
 *      ...
 *
 * This --macro-- forms part of the --ici-api--.
 */
#define ici_astart(a)   ((a)->a_bot == (a)->a_limit && (a)->a_bot != (a)->a_top \
                            ? (a)->a_base : (a)->a_bot)

/*
 * A macro to assist in doing for loops over the elements of an array.
 * Use as:
 *
 *  ici_array_t  *a;
 *  ici_obj_t    **e;
 *  for (e = ici_astart(a); e != ici_alimit(a); e = ici_anext(a, e))
 *      ...
 *
 * This --macro-- forms part of the --ici-api--.
 */
#define ici_alimit(a)   ((a)->a_top)

/*
 * A macro to assist in doing for loops over the elements of an array.
 * Use as:
 *
 *  ici_array_t  *a;
 *  ici_obj_t    **e;
 *  for (e = ici_astart(a); e != ici_alimit(a); e = ici_anext(a, e))
 *      ...
 *
 * This --macro-- forms part of the --ici-api--.
 */
#define ici_anext(a, e) ((e) + 1 == (a)->a_limit && (a)->a_limit != (a)->a_top \
                            ? (a)->a_base : (e) + 1)

 /*
 * End of ici.h export. --ici.h-end--
 */
#endif
