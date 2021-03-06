/*
 * Copyright (c) 2013-2015 Intel Corporation, Inc.  All rights reserved.
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

#include "config.h"

#include "fi_verbs.h"


static ssize_t
fi_ibv_eq_readerr(struct fid_eq *eq, struct fi_eq_err_entry *entry,
		  uint64_t flags)
{
	struct fi_ibv_eq *_eq;

	_eq = container_of(eq, struct fi_ibv_eq, eq_fid.fid);
	if (!_eq->err.err)
		return 0;

	*entry = _eq->err;
	_eq->err.err = 0;
	_eq->err.prov_errno = 0;
	return sizeof(*entry);
}

/* TODO: This should copy the listening fi_info as the base */
static struct fi_info *
fi_ibv_eq_cm_getinfo(struct fi_ibv_fabric *fab, struct rdma_cm_event *event)
{
	struct fi_info *info, *fi;
	struct fi_ibv_connreq *connreq;

	fi = fi_ibv_get_verbs_info(ibv_get_device_name(event->id->verbs->device));
	if (!fi)
		return NULL;

	info = fi_dupinfo(fi);
	if (!info)
		return NULL;

	info->fabric_attr->fabric = &fab->fabric_fid;
	if (!(info->fabric_attr->prov_name = strdup(VERBS_PROV_NAME)))
		goto err;

	fi_ibv_update_info(NULL, info);

	info->src_addrlen = fi_ibv_sockaddr_len(rdma_get_local_addr(event->id));
	if (!(info->src_addr = malloc(info->src_addrlen)))
		goto err;
	memcpy(info->src_addr, rdma_get_local_addr(event->id), info->src_addrlen);

	info->dest_addrlen = fi_ibv_sockaddr_len(rdma_get_peer_addr(event->id));
	if (!(info->dest_addr = malloc(info->dest_addrlen)))
		goto err;
	memcpy(info->dest_addr, rdma_get_peer_addr(event->id), info->dest_addrlen);

	FI_INFO(&fi_ibv_prov, FI_LOG_CORE, "src_addr: %s:%d\n",
		inet_ntoa(((struct sockaddr_in *)info->src_addr)->sin_addr),
		ntohs(((struct sockaddr_in *)info->src_addr)->sin_port));

	FI_INFO(&fi_ibv_prov, FI_LOG_CORE, "dst_addr: %s:%d\n",
		inet_ntoa(((struct sockaddr_in *)info->dest_addr)->sin_addr),
		ntohs(((struct sockaddr_in *)info->dest_addr)->sin_port));

	connreq = calloc(1, sizeof *connreq);
	if (!connreq)
		goto err;

	connreq->handle.fclass = FI_CLASS_CONNREQ;
	connreq->id = event->id;
	info->handle = &connreq->handle;
	return info;
err:
	fi_freeinfo(info);
	return NULL;
}

static ssize_t
fi_ibv_eq_cm_process_event(struct fi_ibv_eq *eq, struct rdma_cm_event *cma_event,
	uint32_t *event, struct fi_eq_cm_entry *entry, size_t len)
{
	fid_t fid;
	size_t datalen;

	fid = cma_event->id->context;
	switch (cma_event->event) {
//	case RDMA_CM_EVENT_ADDR_RESOLVED:
//		return 0;
//	case RDMA_CM_EVENT_ROUTE_RESOLVED:
//		return 0;
	case RDMA_CM_EVENT_CONNECT_REQUEST:
		*event = FI_CONNREQ;
		entry->info = fi_ibv_eq_cm_getinfo(eq->fab, cma_event);
		if (!entry->info) {
			rdma_destroy_id(cma_event->id);
			return 0;
		}
		break;
	case RDMA_CM_EVENT_ESTABLISHED:
		*event = FI_CONNECTED;
		entry->info = NULL;
		break;
	case RDMA_CM_EVENT_DISCONNECTED:
		*event = FI_SHUTDOWN;
		entry->info = NULL;
		break;
	case RDMA_CM_EVENT_ADDR_ERROR:
	case RDMA_CM_EVENT_ROUTE_ERROR:
	case RDMA_CM_EVENT_CONNECT_ERROR:
	case RDMA_CM_EVENT_UNREACHABLE:
		eq->err.fid = fid;
		eq->err.err = cma_event->status;
		return -FI_EAVAIL;
	case RDMA_CM_EVENT_REJECTED:
		eq->err.fid = fid;
		eq->err.err = ECONNREFUSED;
		eq->err.prov_errno = cma_event->status;
		return -FI_EAVAIL;
	case RDMA_CM_EVENT_DEVICE_REMOVAL:
		eq->err.fid = fid;
		eq->err.err = ENODEV;
		return -FI_EAVAIL;
	case RDMA_CM_EVENT_ADDR_CHANGE:
		eq->err.fid = fid;
		eq->err.err = EADDRNOTAVAIL;
		return -FI_EAVAIL;
	default:
		return 0;
	}

	entry->fid = fid;
	datalen = MIN(len - sizeof(*entry), cma_event->param.conn.private_data_len);
	if (datalen)
		memcpy(entry->data, cma_event->param.conn.private_data, datalen);
	return sizeof(*entry) + datalen;
}

static ssize_t fi_ibv_eq_write_event(struct fi_ibv_eq *eq, uint32_t event,
		const void *buf, size_t len)
{
	struct fi_ibv_eq_entry *entry;

	entry = calloc(1, sizeof(struct fi_ibv_eq_entry) + len);
	if (!entry)
		return -FI_ENOMEM;

	entry->event = event;
	entry->len = len;
	memcpy(entry->eq_entry, buf, len);

	fastlock_acquire(&eq->lock);
	dlistfd_insert_tail(&entry->item, &eq->list_head);
	fastlock_release(&eq->lock);

	return len;
}

ssize_t fi_ibv_eq_write(struct fid_eq *eq_fid, uint32_t event,
		const void *buf, size_t len, uint64_t flags)
{
	struct fi_ibv_eq *eq;

	eq = container_of(eq_fid, struct fi_ibv_eq, eq_fid.fid);
	if (!(eq->flags & FI_WRITE))
		return -FI_EINVAL;

	return fi_ibv_eq_write_event(eq, event, buf, len);
}

static size_t fi_ibv_eq_read_event(struct fi_ibv_eq *eq, uint32_t *event,
		void *buf, size_t len, uint64_t flags)
{
	struct fi_ibv_eq_entry *entry;
	ssize_t ret = 0;

	fastlock_acquire(&eq->lock);

	if (dlistfd_empty(&eq->list_head))
		goto out;

	entry = container_of(eq->list_head.list.next, struct fi_ibv_eq_entry, item);
	if (entry->len > len) {
		ret = -FI_ETOOSMALL;
		goto out;
	}

	ret = entry->len;
	*event = entry->event;
	memcpy(buf, entry->eq_entry, entry->len);

	if (!(flags & FI_PEEK)) {
		dlistfd_remove(eq->list_head.list.next, &eq->list_head);
		free(entry);
	}

out:
	fastlock_release(&eq->lock);
	return ret;
}

static ssize_t
fi_ibv_eq_read(struct fid_eq *eq_fid, uint32_t *event,
	       void *buf, size_t len, uint64_t flags)
{
	struct fi_ibv_eq *eq;
	struct rdma_cm_event *cma_event;
	ssize_t ret = 0;

	eq = container_of(eq_fid, struct fi_ibv_eq, eq_fid.fid);

	if (eq->err.err)
		return -FI_EAVAIL;

	if ((ret = fi_ibv_eq_read_event(eq, event, buf, len, flags)))
		return ret;

	if (eq->channel) {
		ret = rdma_get_cm_event(eq->channel, &cma_event);
		if (ret)
			return -errno;

		if (len < sizeof(struct fi_eq_cm_entry))
			return -FI_ETOOSMALL;

		ret = fi_ibv_eq_cm_process_event(eq, cma_event, event,
				(struct fi_eq_cm_entry *)buf, len);

		if (flags & FI_PEEK)
			fi_ibv_eq_write_event(eq, *event, buf, len);

		rdma_ack_cm_event(cma_event);
		return ret;
	}

	return -FI_EAGAIN;
}

static ssize_t
fi_ibv_eq_sread(struct fid_eq *eq_fid, uint32_t *event,
		void *buf, size_t len, int timeout, uint64_t flags)
{
	struct fi_ibv_eq *eq;
	struct epoll_event events[2];
	ssize_t ret;

	eq = container_of(eq_fid, struct fi_ibv_eq, eq_fid.fid);

	while (1) {
		ret = fi_ibv_eq_read(eq_fid, event, buf, len, flags);
		if (ret && (ret != -FI_EAGAIN))
			return ret;

		ret = epoll_wait(eq->epfd, events, 2, timeout);
		if (ret == 0)
			return -FI_EAGAIN;
		else if (ret < 0)
			return ret;
	};
}

static const char *
fi_ibv_eq_strerror(struct fid_eq *eq, int prov_errno, const void *err_data,
		   char *buf, size_t len)
{
	if (buf && len)
		strncpy(buf, strerror(prov_errno), len);
	return strerror(prov_errno);
}

static struct fi_ops_eq fi_ibv_eq_ops = {
	.size = sizeof(struct fi_ops_eq),
	.read = fi_ibv_eq_read,
	.readerr = fi_ibv_eq_readerr,
	.write = fi_ibv_eq_write,
	.sread = fi_ibv_eq_sread,
	.strerror = fi_ibv_eq_strerror
};

static int fi_ibv_eq_control(fid_t fid, int command, void *arg)
{
	/*
	struct fi_ibv_eq *eq;
	int ret = 0;

	eq = container_of(fid, struct fi_ibv_eq, eq_fid.fid);
	switch (command) {
	case FI_GETWAIT:
		if (!eq->epfd) {
			ret = -FI_ENODATA;
			break;
		}
		*(int *) arg = eq->epfd;
		break;
	default:
		ret = -FI_ENOSYS;
		break;
	}

	return ret;
	*/
	return -FI_ENOSYS;
}

static int fi_ibv_eq_close(fid_t fid)
{
	struct fi_ibv_eq *eq;
	struct fi_ibv_eq_entry *entry;

	eq = container_of(fid, struct fi_ibv_eq, eq_fid.fid);

	if (eq->channel)
		rdma_destroy_event_channel(eq->channel);

	close(eq->epfd);

	fastlock_acquire(&eq->lock);
	while(!dlistfd_empty(&eq->list_head)) {
		entry = container_of(eq->list_head.list.next, struct fi_ibv_eq_entry, item);
		dlistfd_remove(eq->list_head.list.next, &eq->list_head);
		free(entry);
	}

	dlistfd_head_free(&eq->list_head);
	fastlock_destroy(&eq->lock);
	free(eq);

	return 0;
}

static struct fi_ops fi_ibv_eq_fi_ops = {
	.size = sizeof(struct fi_ops),
	.close = fi_ibv_eq_close,
	.bind = fi_no_bind,
	.control = fi_ibv_eq_control,
	.ops_open = fi_no_ops_open,
};

int fi_ibv_eq_open(struct fid_fabric *fabric, struct fi_eq_attr *attr,
		   struct fid_eq **eq, void *context)
{
	struct fi_ibv_eq *_eq;
	struct epoll_event event;
	int ret;

	_eq = calloc(1, sizeof *_eq);
	if (!_eq)
		return -ENOMEM;

	_eq->fab = container_of(fabric, struct fi_ibv_fabric, fabric_fid);

	fastlock_init(&_eq->lock);
	ret = dlistfd_head_init(&_eq->list_head);
	if (ret) {
		FI_INFO(&fi_ibv_prov, FI_LOG_EQ, "Unable to initialize dlistfd\n");
		goto err1;
	}

	_eq->epfd = epoll_create1(0);
	if (_eq->epfd < 0) {
		ret = -errno;
		goto err2;
	}

	memset(&event, 0, sizeof(event));
	event.events = EPOLLIN;

	if (epoll_ctl(_eq->epfd, EPOLL_CTL_ADD,
		      _eq->list_head.signal.fd[FI_READ_FD], &event)) {
		ret = -errno;
		goto err3;
	}

	switch (attr->wait_obj) {
	case FI_WAIT_NONE:
	case FI_WAIT_UNSPEC:
	case FI_WAIT_FD:
		_eq->channel = rdma_create_event_channel();
		if (!_eq->channel) {
			ret = -errno;
			goto err3;
		}

		ret = fi_fd_nonblock(_eq->channel->fd);
		if (ret)
			goto err4;

		if (epoll_ctl(_eq->epfd, EPOLL_CTL_ADD, _eq->channel->fd, &event)) {
			ret = -errno;
			goto err4;
		}

		break;
	default:
		ret = -FI_ENOSYS;
		goto err1;
	}

	_eq->flags = attr->flags;
	_eq->eq_fid.fid.fclass = FI_CLASS_EQ;
	_eq->eq_fid.fid.context = context;
	_eq->eq_fid.fid.ops = &fi_ibv_eq_fi_ops;
	_eq->eq_fid.ops = &fi_ibv_eq_ops;

	*eq = &_eq->eq_fid;
	return 0;
err4:
	if (_eq->channel)
		rdma_destroy_event_channel(_eq->channel);
err3:
	close(_eq->epfd);
err2:
	dlistfd_head_free(&_eq->list_head);
err1:
	fastlock_destroy(&_eq->lock);
	free(_eq);
	return ret;
}

