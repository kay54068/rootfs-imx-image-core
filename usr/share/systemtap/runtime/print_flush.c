/* -*- linux-c -*- 
 * Print Flush Function
 * Copyright (C) 2007-2008 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

/** Send the print buffer to the transport now.
 * Output accumulates in the print buffer until it
 * is filled, or this is called. This MUST be called before returning
 * from a probe or accumulated output in the print buffer will be lost.
 *
 * @note Preemption must be disabled to use this.
 */

static STP_DEFINE_SPINLOCK(_stp_print_lock);

void stp_print_flush(_stp_pbuf *pb)
{
	size_t len = pb->len;
	void *entry = NULL;

	/* check to see if there is anything in the buffer */
	if (likely(len == 0))
		return;

	pb->len = 0;

	if (unlikely(_stp_transport_get_state() != STP_TRANSPORT_RUNNING))
		return;

	dbug_trans(1, "len = %zu\n", len);

#ifdef STP_BULKMODE
#ifdef NO_PERCPU_HEADERS
	{
		struct context* __restrict__ c = NULL;
		char *bufp = pb->buf;
		int inode_locked;

		c = _stp_runtime_entryfn_get_context();

		if (!(inode_locked = _stp_transport_trylock_relay_inode())) {
			atomic_inc (&_stp_transport_failures);
#ifndef STP_TRANSPORT_RISKY
			_stp_runtime_entryfn_put_context(c);
			return;
#endif
		}

		while (len > 0) {
			size_t bytes_reserved;

			bytes_reserved = _stp_data_write_reserve(len, &entry);
			if (likely(entry && bytes_reserved > 0)) {
				memcpy(_stp_data_entry_data(entry), bufp,
				       bytes_reserved);
				_stp_data_write_commit(entry);
				bufp += bytes_reserved;
				len -= bytes_reserved;
			}
			else {
				atomic_inc(&_stp_transport_failures);
				break;
			}
		}

		if (inode_locked)
			_stp_transport_unlock_relay_inode();

		_stp_runtime_entryfn_put_context(c);
	}

#else  /* !NO_PERCPU_HEADERS */

	{
		struct context* __restrict__ c = NULL;
		char *bufp = pb->buf;
		struct _stp_trace t = {	.sequence = _stp_seq_inc(),
					.pdu_len = len};
		size_t bytes_reserved;
		int inode_locked;

		c = _stp_runtime_entryfn_get_context();

		if (!(inode_locked = _stp_transport_trylock_relay_inode())) {
			atomic_inc (&_stp_transport_failures);
#ifndef STP_TRANSPORT_RISKY
			_stp_runtime_entryfn_put_context(c);
			return;
#endif
		}

		bytes_reserved = _stp_data_write_reserve(sizeof(struct _stp_trace), &entry);
		if (likely(entry && bytes_reserved > 0)) {
			/* prevent unaligned access by using memcpy() */
			memcpy(_stp_data_entry_data(entry), &t, sizeof(t));
			_stp_data_write_commit(entry);
		}
		else {
			atomic_inc(&_stp_transport_failures);
			goto done;
		}

		while (len > 0) {
			bytes_reserved = _stp_data_write_reserve(len, &entry);
			if (likely(entry && bytes_reserved > 0)) {
				memcpy(_stp_data_entry_data(entry), bufp,
				       bytes_reserved);
				_stp_data_write_commit(entry);
				bufp += bytes_reserved;
				len -= bytes_reserved;
			}
			else {
				atomic_inc(&_stp_transport_failures);
				break;
			}
		}

	done:

		if (inode_locked)
			_stp_transport_unlock_relay_inode();

		_stp_runtime_entryfn_put_context(c);
	}
#endif /* !NO_PERCPU_HEADERS */

#else  /* !STP_BULKMODE */

	{
		unsigned long flags;
		struct context* __restrict__ c = NULL;
		char *bufp = pb->buf;
		int inode_locked;

		/* Prevent probe reentrancy on _stp_print_lock.
		 *
		 * Since stp_print_flush may be called from probe context, we
		 * have to make sure that its lock, _stp_print_lock, can't
		 * possibly be held outside probe context too.  We ensure this
		 * by grabbing the context here, so any probe triggered by this
		 * region will appear reentrant and be skipped rather than
		 * deadlock.  Failure to get_context just means we're already
		 * in a probe, which is fine.
		 *
		 * (see also _stp_ctl_send for a similar situation)
                 *
                 * A better solution would be to replace this
                 * concurrency-control-laden effort with a lockless
                 * algorithm.
		 */
		c = _stp_runtime_entryfn_get_context();

		if (!(inode_locked = _stp_transport_trylock_relay_inode())) {
			atomic_inc (&_stp_transport_failures);
#ifndef STP_TRANSPORT_RISKY
			dbug_trans(0, "discarding %zu bytes of data\n", len);
			_stp_runtime_entryfn_put_context(c);
			return;
#endif
		}

		dbug_trans(1, "calling _stp_data_write...\n");
		stp_spin_lock_irqsave(&_stp_print_lock, flags);
		while (len > 0) {
			size_t bytes_reserved;

			bytes_reserved = _stp_data_write_reserve(len, &entry);
			if (likely(entry && bytes_reserved > 0)) {
				memcpy(_stp_data_entry_data(entry), bufp,
				       bytes_reserved);
				_stp_data_write_commit(entry);
				bufp += bytes_reserved;
				len -= bytes_reserved;
			}
			else {
			    atomic_inc(&_stp_transport_failures);
			    break;
			}
		}
		stp_spin_unlock_irqrestore(&_stp_print_lock, flags);

		if (inode_locked)
			_stp_transport_unlock_relay_inode();

		_stp_runtime_entryfn_put_context(c);
	}
#endif /* !STP_BULKMODE */
}
