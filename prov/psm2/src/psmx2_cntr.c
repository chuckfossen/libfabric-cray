/*
 * Copyright (c) 2013-2014 Intel Corporation. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "psmx2.h"

int psmx2_process_trigger(struct psmx2_fid_domain *domain,
			  struct psmx2_trigger *trigger)
{
	switch (trigger->op) {
	case PSMX2_TRIGGERED_SEND:
		psmx2_send_generic(trigger->send.ep,
				   trigger->send.buf,
				   trigger->send.len,
				   trigger->send.desc,
				   trigger->send.dest_addr,
				   trigger->send.context,
				   trigger->send.flags,
				   trigger->send.data);
		break;
	case PSMX2_TRIGGERED_SENDV:
		psmx2_sendv_generic(trigger->sendv.ep,
				    trigger->sendv.iov,
				    trigger->sendv.desc,
				    trigger->sendv.count,
				    trigger->sendv.dest_addr,
				    trigger->sendv.context,
				    trigger->sendv.flags,
				    trigger->sendv.data);
		break;
	case PSMX2_TRIGGERED_RECV:
		psmx2_recv_generic(trigger->recv.ep,
				   trigger->recv.buf,
				   trigger->recv.len,
				   trigger->recv.desc,
				   trigger->recv.src_addr,
				   trigger->recv.context,
				   trigger->recv.flags);
		break;
	case PSMX2_TRIGGERED_TSEND:
		psmx2_tagged_send_generic(trigger->tsend.ep,
					  trigger->tsend.buf,
					  trigger->tsend.len,
					  trigger->tsend.desc,
					  trigger->tsend.dest_addr,
					  trigger->tsend.tag,
					  trigger->tsend.context,
					  trigger->tsend.flags,
					  trigger->tsend.data);
		break;
	case PSMX2_TRIGGERED_TSENDV:
		psmx2_tagged_sendv_generic(trigger->tsendv.ep,
					   trigger->tsendv.iov,
					   trigger->tsendv.desc,
					   trigger->tsendv.count,
					   trigger->tsendv.dest_addr,
					   trigger->tsendv.tag,
					   trigger->tsendv.context,
					   trigger->tsendv.flags,
					   trigger->tsendv.data);
		break;
	case PSMX2_TRIGGERED_TRECV:
		psmx2_tagged_recv_generic(trigger->trecv.ep,
					  trigger->trecv.buf,
					  trigger->trecv.len,
					  trigger->trecv.desc,
					  trigger->trecv.src_addr,
					  trigger->trecv.tag,
					  trigger->trecv.ignore,
					  trigger->trecv.context,
					  trigger->trecv.flags);
		break;
	case PSMX2_TRIGGERED_WRITE:
		psmx2_write_generic(trigger->write.ep,
				    trigger->write.buf,
				    trigger->write.len,
				    trigger->write.desc,
				    trigger->write.dest_addr,
				    trigger->write.addr,
				    trigger->write.key,
				    trigger->write.context,
				    trigger->write.flags,
				    trigger->write.data);
		break;

	case PSMX2_TRIGGERED_WRITEV:
		psmx2_writev_generic(trigger->writev.ep,
				     trigger->writev.iov,
				     trigger->writev.desc,
				     trigger->writev.count,
				     trigger->writev.dest_addr,
				     trigger->writev.addr,
				     trigger->writev.key,
				     trigger->writev.context,
				     trigger->writev.flags,
				     trigger->writev.data);
		break;

	case PSMX2_TRIGGERED_READ:
		psmx2_read_generic(trigger->read.ep,
				   trigger->read.buf,
				   trigger->read.len,
				   trigger->read.desc,
				   trigger->read.src_addr,
				   trigger->read.addr,
				   trigger->read.key,
				   trigger->read.context,
				   trigger->read.flags);
		break;

	case PSMX2_TRIGGERED_ATOMIC_WRITE:
		psmx2_atomic_write_generic(
				trigger->atomic_write.ep,
				trigger->atomic_write.buf,
				trigger->atomic_write.count,
				trigger->atomic_write.desc,
				trigger->atomic_write.dest_addr,
				trigger->atomic_write.addr,
				trigger->atomic_write.key,
				trigger->atomic_write.datatype,
				trigger->atomic_write.atomic_op,
				trigger->atomic_write.context,
				trigger->atomic_write.flags);
		break;

	case PSMX2_TRIGGERED_ATOMIC_READWRITE:
		psmx2_atomic_readwrite_generic(
				trigger->atomic_readwrite.ep,
				trigger->atomic_readwrite.buf,
				trigger->atomic_readwrite.count,
				trigger->atomic_readwrite.desc,
				trigger->atomic_readwrite.result,
				trigger->atomic_readwrite.result_desc,
				trigger->atomic_readwrite.dest_addr,
				trigger->atomic_readwrite.addr,
				trigger->atomic_readwrite.key,
				trigger->atomic_readwrite.datatype,
				trigger->atomic_readwrite.atomic_op,
				trigger->atomic_readwrite.context,
				trigger->atomic_readwrite.flags);
		break;

	case PSMX2_TRIGGERED_ATOMIC_COMPWRITE:
		psmx2_atomic_compwrite_generic(
				trigger->atomic_compwrite.ep,
				trigger->atomic_compwrite.buf,
				trigger->atomic_compwrite.count,
				trigger->atomic_compwrite.desc,
				trigger->atomic_compwrite.compare,
				trigger->atomic_compwrite.compare_desc,
				trigger->atomic_compwrite.result,
				trigger->atomic_compwrite.result_desc,
				trigger->atomic_compwrite.dest_addr,
				trigger->atomic_compwrite.addr,
				trigger->atomic_compwrite.key,
				trigger->atomic_compwrite.datatype,
				trigger->atomic_compwrite.atomic_op,
				trigger->atomic_compwrite.context,
				trigger->atomic_compwrite.flags);
		break;
	default:
		FI_INFO(&psmx2_prov, FI_LOG_CQ,
			"%d unsupported op\n", trigger->op);
		break;
	}

	free(trigger);
	return 0;
}

void psmx2_cntr_check_trigger(struct psmx2_fid_cntr *cntr)
{
	struct psmx2_fid_domain *domain = cntr->domain;
	struct psmx2_trigger *trigger;

	if (!cntr->trigger)
		return;

	pthread_mutex_lock(&cntr->trigger_lock);

	trigger = cntr->trigger;
	while (trigger) {
		if (cntr->counter < trigger->threshold)
			break;

		cntr->trigger = trigger->next;

		if (domain->am_initialized) {
			fastlock_acquire(&domain->trigger_queue.lock);
			slist_insert_tail(&trigger->list_entry,
					  &domain->trigger_queue.list);
			fastlock_release(&domain->trigger_queue.lock);
		} else {
			psmx2_process_trigger(domain, trigger);
		}

		trigger = cntr->trigger;
	}

	pthread_mutex_unlock(&cntr->trigger_lock);
}

void psmx2_cntr_add_trigger(struct psmx2_fid_cntr *cntr,
			    struct psmx2_trigger *trigger)
{
	struct psmx2_trigger *p, *q;

	pthread_mutex_lock(&cntr->trigger_lock);

	q = NULL;
	p = cntr->trigger;
	while (p && p->threshold <= trigger->threshold) {
		q = p;
		p = p->next;
	}
	if (q)
		q->next = trigger;
	else
		cntr->trigger = trigger;
	trigger->next = p;

	pthread_mutex_unlock(&cntr->trigger_lock);

	psmx2_cntr_check_trigger(cntr);
}

#define PSMX2_CNTR_POLL_THRESHOLD 100
static uint64_t psmx2_cntr_read(struct fid_cntr *cntr)
{
	struct psmx2_fid_cntr *cntr_priv;
	static int poll_cnt = 0;

	cntr_priv = container_of(cntr, struct psmx2_fid_cntr, cntr);

	if (poll_cnt++ == PSMX2_CNTR_POLL_THRESHOLD) {
		psmx2_progress(cntr_priv->domain);
		poll_cnt = 0;
	}

	cntr_priv->counter_last_read = cntr_priv->counter;

	return cntr_priv->counter_last_read;
}

static uint64_t psmx2_cntr_readerr(struct fid_cntr *cntr)
{
	struct psmx2_fid_cntr *cntr_priv;

	cntr_priv = container_of(cntr, struct psmx2_fid_cntr, cntr);

	cntr_priv->error_counter_last_read = cntr_priv->error_counter;

	return cntr_priv->error_counter_last_read;
}

static int psmx2_cntr_add(struct fid_cntr *cntr, uint64_t value)
{
	struct psmx2_fid_cntr *cntr_priv;

	cntr_priv = container_of(cntr, struct psmx2_fid_cntr, cntr);
	cntr_priv->counter += value;

	psmx2_cntr_check_trigger(cntr_priv);

	if (cntr_priv->wait)
		psmx2_wait_signal((struct fid_wait *)cntr_priv->wait);

	return 0;
}

static int psmx2_cntr_set(struct fid_cntr *cntr, uint64_t value)
{
	struct psmx2_fid_cntr *cntr_priv;

	cntr_priv = container_of(cntr, struct psmx2_fid_cntr, cntr);
	cntr_priv->counter = value;

	psmx2_cntr_check_trigger(cntr_priv);

	if (cntr_priv->wait)
		psmx2_wait_signal((struct fid_wait *)cntr_priv->wait);

	return 0;
}

static int psmx2_cntr_wait(struct fid_cntr *cntr, uint64_t threshold, int timeout)
{
	struct psmx2_fid_cntr *cntr_priv;
	struct timespec ts0, ts;
	int msec_passed = 0;
	int ret = 0;

	cntr_priv = container_of(cntr, struct psmx2_fid_cntr, cntr);

	clock_gettime(CLOCK_REALTIME, &ts0);

	while (cntr_priv->counter < threshold) {
		if (cntr_priv->wait) {
			ret = psmx2_wait_wait((struct fid_wait *)cntr_priv->wait,
					      timeout - msec_passed);
			if (ret == -FI_ETIMEDOUT)
				break;
		} else {
			psmx2_progress(cntr_priv->domain);
		}

		if (cntr_priv->counter >= threshold)
			break;

		if (timeout < 0)
			continue;

		clock_gettime(CLOCK_REALTIME, &ts);
		msec_passed = (ts.tv_sec - ts0.tv_sec) * 1000 +
			      (ts.tv_nsec - ts0.tv_nsec) / 1000000;

		if (msec_passed >= timeout) {
			ret = -FI_ETIMEDOUT;
			break;
		}
	}

	return ret;
}

static int psmx2_cntr_close(fid_t fid)
{
	struct psmx2_fid_cntr *cntr;

	cntr = container_of(fid, struct psmx2_fid_cntr, cntr.fid);

	psmx2_domain_release(cntr->domain);

	if (cntr->wait && cntr->wait_is_local)
		fi_close((fid_t)cntr->wait);

	pthread_mutex_destroy(&cntr->trigger_lock);
	free(cntr);

	return 0;
}

static int psmx2_cntr_control(fid_t fid, int command, void *arg)
{
	struct psmx2_fid_cntr *cntr;
	int ret = 0;

	cntr = container_of(fid, struct psmx2_fid_cntr, cntr.fid);

	switch (command) {
	case FI_SETOPSFLAG:
		cntr->flags = *(uint64_t *)arg;
		break;

	case FI_GETOPSFLAG:
		if (!arg)
			return -FI_EINVAL;
		*(uint64_t *)arg = cntr->flags;
		break;

	case FI_GETWAIT:
		/*
		ret = psmx2_wait_get_obj(cntr->wait, arg);
		break;
		*/
	default:
		return -FI_ENOSYS;
	}

	return ret;
}

static struct fi_ops psmx2_fi_ops = {
	.size = sizeof(struct fi_ops),
	.close = psmx2_cntr_close,
	.bind = fi_no_bind,
	.control = psmx2_cntr_control,
	.ops_open = fi_no_ops_open,
};

static struct fi_ops_cntr psmx2_cntr_ops = {
	.size = sizeof(struct fi_ops_cntr),
	.read = psmx2_cntr_read,
	.readerr = psmx2_cntr_readerr,
	.add = psmx2_cntr_add,
	.set = psmx2_cntr_set,
	.wait = psmx2_cntr_wait,
};

int psmx2_cntr_open(struct fid_domain *domain, struct fi_cntr_attr *attr,
			struct fid_cntr **cntr, void *context)
{
	struct psmx2_fid_domain *domain_priv;
	struct psmx2_fid_cntr *cntr_priv;
	struct psmx2_fid_wait *wait = NULL;
	struct fi_wait_attr wait_attr;
	int wait_is_local = 0;
	int events;
	uint64_t flags;
	int err;

	events = FI_CNTR_EVENTS_COMP;
	flags = 0;
	domain_priv = container_of(domain, struct psmx2_fid_domain, domain);

	switch (attr->events) {
	case FI_CNTR_EVENTS_COMP:
		events = attr->events;
		break;

	default:
		FI_INFO(&psmx2_prov, FI_LOG_CQ,
			"attr->events=%d, supported=%d\n",
			attr->events, FI_CNTR_EVENTS_COMP);
		return -FI_EINVAL;
	}

	switch (attr->wait_obj) {
	case FI_WAIT_NONE:
	case FI_WAIT_UNSPEC:
		break;

	case FI_WAIT_SET:
		if (!attr->wait_set) {
			FI_INFO(&psmx2_prov, FI_LOG_CQ,
				"FI_WAIT_SET is specified but attr->wait_set is NULL\n");
			return -FI_EINVAL;
		}
		wait = (struct psmx2_fid_wait *)attr->wait_set;
		break;

	case FI_WAIT_FD:
	case FI_WAIT_MUTEX_COND:
		wait_attr.wait_obj = attr->wait_obj;
		wait_attr.flags = 0;
		err = psmx2_wait_open(&domain_priv->fabric->fabric,
				      &wait_attr, (struct fid_wait **)&wait);
		if (err)
			return err;
		wait_is_local = 1;
		break;

	default:
		FI_INFO(&psmx2_prov, FI_LOG_CQ,
			"attr->wait_obj=%d, supported=%d...%d\n",
			attr->wait_obj, FI_WAIT_NONE, FI_WAIT_MUTEX_COND);
		return -FI_EINVAL;
	}

	cntr_priv = (struct psmx2_fid_cntr *) calloc(1, sizeof *cntr_priv);
	if (!cntr_priv) {
		err = -FI_ENOMEM;
		goto fail;
	}

	psmx2_domain_acquire(domain_priv);

	cntr_priv->domain = domain_priv;
	cntr_priv->events = events;
	cntr_priv->wait = wait;
	cntr_priv->wait_is_local = wait_is_local;
	cntr_priv->flags = flags;
	cntr_priv->cntr.fid.fclass = FI_CLASS_CNTR;
	cntr_priv->cntr.fid.context = context;
	cntr_priv->cntr.fid.ops = &psmx2_fi_ops;
	cntr_priv->cntr.ops = &psmx2_cntr_ops;

	pthread_mutex_init(&cntr_priv->trigger_lock, NULL);

	*cntr = &cntr_priv->cntr;
	return 0;
fail:
	if (wait && wait_is_local)
		fi_close(&wait->wait.fid);
	return err;
}

