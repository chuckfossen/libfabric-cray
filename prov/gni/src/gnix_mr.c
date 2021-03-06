/*
 * Copyright (c) 2015 Cray Inc. All rights reserved.
 * Copyright (c) 2015 Los Alamos National Security, LLC. All rights reserved.
 *
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

#include <stdlib.h>
#include <string.h>

#include "gnix.h"
#include "gnix_nic.h"
#include "gnix_util.h"
#include "gnix_mr.h"

/* forward declarations */

static int fi_gnix_mr_close(fid_t fid);

/* global declarations */
/* memory registration operations */
static struct fi_ops fi_gnix_mr_ops = {
	.size = sizeof(struct fi_ops),
	.close = fi_gnix_mr_close,
	.bind = fi_no_bind,
	.control = fi_no_control,
	.ops_open = fi_no_ops_open,
};

/**
 * Sign extends the value passed into up to length parameter
 *
 * @param[in]  val  value to be sign extended
 * @param[in]  len  length to sign extend the value
 * @return          sign extended value to length, len
 */
static inline int64_t __sign_extend(
		uint64_t val,
		int len)
{
	int64_t m = 1UL << (len - 1);
	int64_t r = (val ^ m) - m;

	return r;
}

/**
 * Converts a key to a gni memory handle without calculating crc
 *
 * @param key   gnix memory registration key
 * @param mhdl  gni memory handle
 */
void _gnix_convert_key_to_mhdl_no_crc(
		gnix_mr_key_t *key,
		gni_mem_handle_t *mhdl)
{
	uint64_t va = key->pfn;
	uint8_t flags = 0;

	va = (uint64_t) __sign_extend(va << GNIX_MR_PAGE_SHIFT,
				      GNIX_MR_VA_BITS);

	if (key->flags & GNIX_MR_FLAG_READONLY)
		flags |= GNI_MEMHNDL_ATTR_READONLY;

	GNI_MEMHNDL_INIT((*mhdl));
	GNI_MEMHNDL_SET_VA((*mhdl), va);
	GNI_MEMHNDL_SET_MDH((*mhdl), key->mdd);
	GNI_MEMHNDL_SET_NPAGES((*mhdl), GNI_MEMHNDL_NPGS_MASK);
	GNI_MEMHNDL_SET_FLAGS((*mhdl), flags);
	GNI_MEMHNDL_SET_PAGESIZE((*mhdl), GNIX_MR_PAGE_SHIFT);
}

/**
 * Converts a key to a gni memory handle
 *
 * @param key   gnix memory registration key
 * @param mhdl  gni memory handle
 */
void _gnix_convert_key_to_mhdl(
		gnix_mr_key_t *key,
		gni_mem_handle_t *mhdl)
{
	_gnix_convert_key_to_mhdl_no_crc(key, mhdl);
	compiler_barrier();
	GNI_MEMHNDL_SET_CRC((*mhdl));
}

/**
 * Converts a gni memory handle to gnix memory registration key
 *
 * @param mhdl  gni memory handle
 * @return uint64_t representation of a gnix memory registration key
 */
uint64_t _gnix_convert_mhdl_to_key(gni_mem_handle_t *mhdl)
{
	gnix_mr_key_t key = {{{{0}}}};
	key.pfn = GNI_MEMHNDL_GET_VA((*mhdl)) >> GNIX_MR_PAGE_SHIFT;
	key.mdd = GNI_MEMHNDL_GET_MDH((*mhdl));
	//key->format = GNI_MEMHNDL_NEW_FRMT((*mhdl));
	key.flags = 0;

	if (GNI_MEMHNDL_GET_FLAGS((*mhdl)) & GNI_MEMHNDL_FLAG_READONLY)
		key.flags |= GNIX_MR_FLAG_READONLY;

	return key.value;
}

/**
 * Helper function to calculate the length of a potential registration
 * based on some rules of the registration cache.
 *
 * Registrations should be page aligned and contain all of page(s)
 *
 * @param address   base address of the registration
 * @param length    length of the registration
 * @param pagesize  assumed page size of the registration
 * @return length for the new registration
 */
static inline uint64_t __calculate_length(
		uint64_t address,
		uint64_t length,
		uint64_t pagesize)
{
	uint64_t baseaddr = address & ~(pagesize - 1);
	uint64_t reg_len = (address + length) - baseaddr;
	uint64_t pages = reg_len / pagesize;

	if (reg_len % pagesize != 0)
		pages += 1;

	return pages * pagesize;
}

DIRECT_FN int gnix_mr_reg(struct fid *fid, const void *buf, size_t len,
			  uint64_t access, uint64_t offset,
			  uint64_t requested_key, uint64_t flags,
			  struct fid_mr **mr_o, void *context)
{
	struct gnix_fid_mem_desc *mr = NULL;
	struct gnix_fid_domain *domain;
	struct gnix_nic *nic;
	int rc;
	uint64_t reg_addr, reg_len;
	struct _gnix_fi_reg_context fi_reg_context = {
			.access = access,
			.offset = offset,
			.requested_key = requested_key,
			.flags = flags,
			.context = context,
	};

	GNIX_TRACE(FI_LOG_MR, "\n");

	/* Flags are reserved for future use and must be 0. */
	if (unlikely(flags))
		return -FI_EBADFLAGS;

	/* The offset parameter is reserved for future use and must be 0.
	 *   Additionally, check for invalid pointers, bad access flags and the
	 *   correct fclass on associated fid
	 */
	if (offset || !buf || !mr_o || !access ||
			(access & ~(FI_READ | FI_WRITE | FI_RECV | FI_SEND |
						FI_REMOTE_READ |
						FI_REMOTE_WRITE)) ||
			(fid->fclass != FI_CLASS_DOMAIN))

		return -FI_EINVAL;

	domain = container_of(fid, struct gnix_fid_domain, domain_fid.fid);

	/* If the nic list is empty, create a nic */
	if (unlikely(dlist_empty(&domain->nic_list))) {
		rc = gnix_nic_alloc(domain, NULL, &nic);
		if (rc) {
			GNIX_WARN(FI_LOG_MR, "could not allocate nic to do mr_reg,"
					" ret=%i\n", rc);
			goto err;
		}
	}

	reg_addr = ((uint64_t) buf) & ~((1 << GNIX_MR_PAGE_SHIFT) - 1);
	reg_len = __calculate_length((uint64_t) buf, len,
			1 << GNIX_MR_PAGE_SHIFT);

	/* call cache register op to retrieve the right entry */
	fastlock_acquire(&domain->mr_cache_lock);
	if (unlikely(!domain->mr_cache)) {
		rc = _gnix_mr_cache_init(&domain->mr_cache,
				&domain->mr_cache_attr);
		if (rc != FI_SUCCESS) {
			fastlock_release(&domain->mr_cache_lock);
			goto err;
		}
	}

	rc = _gnix_mr_cache_register(domain->mr_cache,
			(uint64_t) reg_addr, reg_len, &fi_reg_context,
			(void **) &mr);
	fastlock_release(&domain->mr_cache_lock);

	/* check retcode */
	if (unlikely(rc != FI_SUCCESS))
		goto err;

	/* md.mr_fid */
	mr->mr_fid.mem_desc = mr;
	mr->mr_fid.fid.fclass = FI_CLASS_MR;
	mr->mr_fid.fid.context = context;
	mr->mr_fid.fid.ops = &fi_gnix_mr_ops;

	/* setup internal key structure */
	mr->mr_fid.key = _gnix_convert_mhdl_to_key(&mr->mem_hndl);

	_gnix_ref_get(mr->domain);
	_gnix_ref_get(mr->nic);

	/* set up mr_o out pointer */
	*mr_o = &mr->mr_fid;
	return FI_SUCCESS;

err:
	return rc;
}

/**
 * Closes and deallocates a libfabric memory registration
 *
 * @param[in]  fid  libfabric memory registration fid
 *
 * @return     FI_SUCCESS on success
 *             -FI_EINVAL on invalid fid
 *             -FI_NOENT when there isn't a matching registration for the
 *               provided fid
 *             Otherwise, GNI_RC_* ret codes converted to FI_* err codes
 */
static int fi_gnix_mr_close(fid_t fid)
{
	struct gnix_fid_mem_desc *mr;
	gni_return_t ret;
	struct gnix_fid_domain *domain;
	struct gnix_nic *nic;

	GNIX_TRACE(FI_LOG_MR, "\n");

	if (unlikely(fid->fclass != FI_CLASS_MR))
		return -FI_EINVAL;

	mr = container_of(fid, struct gnix_fid_mem_desc, mr_fid.fid);

	domain = mr->domain;
	nic = mr->nic;

	/* call cache deregister op */
	fastlock_acquire(&domain->mr_cache_lock);
	ret = _gnix_mr_cache_deregister(mr->domain->mr_cache, mr);
	fastlock_release(&domain->mr_cache_lock);

	/* check retcode */
	if (likely(ret == FI_SUCCESS)) {
		/* release references to the domain and nic */
		_gnix_ref_put(domain);
		_gnix_ref_put(nic);
	} else {
		GNIX_WARN(FI_LOG_MR, "failed to deregister memory, "
				"ret=%i\n", ret);
	}

	return ret;
}

static void *__gnix_register_region(
		void *handle,
		void *address,
		size_t length,
		struct _gnix_fi_reg_context *fi_reg_context,
		void *context)
{
	struct gnix_fid_mem_desc *md = (struct gnix_fid_mem_desc *) handle;
	struct gnix_fid_domain *domain = context;
	struct gnix_nic *nic;
	gni_cq_handle_t dst_cq_hndl = NULL;
	int flags = 0;
	int vmdh_index = -1;
	gni_return_t grc = GNI_RC_SUCCESS;

	/* If network would be able to write to this buffer, use read-write */
	if (fi_reg_context->access & (FI_RECV | FI_READ | FI_REMOTE_WRITE))
		flags |= GNI_MEM_READWRITE;
	else
		flags |= GNI_MEM_READ_ONLY;

	dlist_for_each(&domain->nic_list, nic, dom_nic_list)
	{
		fastlock_acquire(&nic->lock);
		grc = GNI_MemRegister(nic->gni_nic_hndl, (uint64_t) address,
				      length,	dst_cq_hndl, flags,
				      vmdh_index, &md->mem_hndl);
		fastlock_release(&nic->lock);
		if (grc == GNI_RC_SUCCESS)
			break;
	}

	if (unlikely(grc != GNI_RC_SUCCESS)) {
		GNIX_WARN(FI_LOG_MR, "failed to register memory with uGNI, "
			  "ret=%s\n", gni_err_str[grc]);
		return NULL;
	}

	/* set up the mem desc */
	md->nic = nic;
	md->domain = domain;

	/* take references on domain and nic */
	_gnix_ref_get(md->domain);
	_gnix_ref_get(md->nic);

	return md;
}

static int __gnix_deregister_region(
		void *handle,
		void *context)
{
	struct gnix_fid_mem_desc *mr = (struct gnix_fid_mem_desc *) handle;
	gni_return_t ret;
	struct gnix_fid_domain *domain;
	struct gnix_nic *nic;

	domain = mr->domain;
	nic = mr->nic;

	fastlock_acquire(&nic->lock);
	ret = GNI_MemDeregister(nic->gni_nic_hndl, &mr->mem_hndl);
	fastlock_release(&nic->lock);
	if (ret == GNI_RC_SUCCESS) {
		/* release reference to domain */
		_gnix_ref_put(domain);

		/* release reference to nic */
		_gnix_ref_put(nic);
	} else {
		GNIX_WARN(FI_LOG_MR, "failed to deregister memory"
				" region, entry=%p ret=%i\n", handle, ret);
	}

	return ret;
}

/**
 * Associates a registered memory region with a completion counter.
 *
 * @param[in] fid		the memory region
 * @param[in] bfid		the fabric identifier for the memory region
 * @param[in] flags		flags to apply to the registration
 *
 * @return FI_SUCCESS		Upon successfully registering the memory region
 * @return -FI_ENOSYS		If binding of the memory region is not supported
 * @return -FI_EBADFLAGS	If the flags are not supported
 * @return -FI_EKEYREJECTED	If the key is not available
 * @return -FI_ENOKEY		If the key is already in use
 */
DIRECT_FN int gnix_mr_bind(fid_t fid, struct fid *bfid, uint64_t flags)
{
	return -FI_ENOSYS;
}

static int __gnix_destruct_registration(void *context)
{
	return GNI_RC_SUCCESS;
}

gnix_mr_cache_attr_t _gnix_default_mr_cache_attr = {
		.soft_reg_limit      = 4096,
		.hard_reg_limit      = -1,
		.hard_stale_limit    = 128,
		.lazy_deregistration = 1,
		.reg_callback = __gnix_register_region,
		.dereg_callback = __gnix_deregister_region,
		.destruct_callback = __gnix_destruct_registration,
		.elem_size = sizeof(struct gnix_fid_mem_desc),
};
