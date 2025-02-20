// SPDX-License-Identifier: GPL-2.0-or-later
/* ASN.1 Object identifier (OID) registry
 *
 * Copyright (C) 2012 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 */

#include <linux/module.h>
#include <linux/export.h>
#include <linux/oid_registry.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/bug.h>
#include "oid_registry_data.c"

MODULE_DESCRIPTION("OID Registry");
MODULE_AUTHOR("Red Hat, Inc.");
MODULE_LICENSE("GPL");

/**
 * look_up_OID - Find an OID registration for the specified data
 * @data: Binary representation of the OID
 * @datasize: Size of the binary representation
 */
enum OID look_up_OID(const void *data, size_t datasize)
{
	const unsigned char *octets = data;
	enum OID oid;
	unsigned char xhash;
	unsigned i, j, k, hash;
	size_t len;

	/* Hash the OID data */
	hash = datasize - 1;

	for (i = 0; i < datasize; i++)
		hash += octets[i] * 33;
	hash = (hash >> 24) ^ (hash >> 16) ^ (hash >> 8) ^ hash;
	hash &= 0xff;

	/* Binary search the OID registry.  OIDs are stored in ascending order
	 * of hash value then ascending order of size and then in ascending
	 * order of reverse value.
	 */
	i = 0;
	k = OID__NR;
	while (i < k) {
		j = (i + k) / 2;

		xhash = oid_search_table[j].hash;
		if (xhash > hash) {
			k = j;
			continue;
		}
		if (xhash < hash) {
			i = j + 1;
			continue;
		}

		oid = oid_search_table[j].oid;
		len = oid_index[oid + 1] - oid_index[oid];
		if (len > datasize) {
			k = j;
			continue;
		}
		if (len < datasize) {
			i = j + 1;
			continue;
		}

		/* Variation is most likely to be at the tail end of the
		 * OID, so do the comparison in reverse.
		 */
		while (len > 0) {
			unsigned char a = oid_data[oid_index[oid] + --len];
			unsigned char b = octets[len];
			if (a > b) {
				k = j;
				goto next;
			}
			if (a < b) {
				i = j + 1;
				goto next;
			}
		}
		return oid;
	next:
		;
	}

	return OID__NR;
}
EXPORT_SYMBOL_GPL(look_up_OID);

int lookup_oid_digest_info(enum OID oid,
		const char **digest_algo, u32 *digest_len, enum OID *digest_oid) {
	int ret = 0;
	switch (oid) {
	case OID_md4WithRSAEncryption:
		if (digest_algo) *digest_algo = "md4";
		if (digest_oid) *digest_oid = OID_md4;
		if (digest_len) *digest_len = 16;
		break;
	case OID_sha1WithRSAEncryption:
	case OID_id_ecdsa_with_sha1:
		if (digest_algo) *digest_algo = "sha1";
		if (digest_oid) *digest_oid = OID_sha1;
		if (digest_len) *digest_len = 20;
		break;
	case OID_sha224WithRSAEncryption:
		if (digest_algo) *digest_algo = "sha224";
		if (digest_oid) *digest_oid = OID_sha224;
		if (digest_len) *digest_len = 28;
		break;
	case OID_sha256WithRSAEncryption:
	case OID_id_ecdsa_with_sha256:
		if (digest_algo) *digest_algo = "sha256";
		if (digest_oid) *digest_oid = OID_sha256;
		if (digest_len) *digest_len = 32;
		break;
	case OID_sha384WithRSAEncryption:
	case OID_id_ecdsa_with_sha384:
		if (digest_algo) *digest_algo = "sha384";
		if (digest_oid) *digest_oid = OID_sha384;
		if (digest_len) *digest_len = 48;
		break;
	case OID_sha512WithRSAEncryption:
	case OID_id_ecdsa_with_sha512:
		if (digest_algo) *digest_algo = "sha512";
		if (digest_oid) *digest_oid = OID_sha512;
		if (digest_len) *digest_len = 64;
		break;
	default:
		ret = -1;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(lookup_oid_digest_info);

/*
 * sprint_OID - Print an Object Identifier into a buffer
 * @data: The encoded OID to print
 * @datasize: The size of the encoded OID
 * @buffer: The buffer to render into
 * @bufsize: The size of the buffer
 *
 * The OID is rendered into the buffer in "a.b.c.d" format and the number of
 * bytes is returned.  -EBADMSG is returned if the data could not be intepreted
 * and -ENOBUFS if the buffer was too small.
 */
int sprint_oid(const void *data, size_t datasize, char *buffer, size_t bufsize)
{
	const unsigned char *v = data, *end = v + datasize;
	unsigned long num;
	unsigned char n;
	size_t ret;
	int count;

	if (v >= end)
		goto bad;

	n = *v++;
	ret = count = snprintf(buffer, bufsize, "%u.%u", n / 40, n % 40);
	if (count >= bufsize)
		return -ENOBUFS;
	buffer += count;
	bufsize -= count;

	while (v < end) {
		num = 0;
		n = *v++;
		if (!(n & 0x80)) {
			num = n;
		} else {
			num = n & 0x7f;
			do {
				if (v >= end)
					goto bad;
				n = *v++;
				num <<= 7;
				num |= n & 0x7f;
			} while (n & 0x80);
		}
		ret += count = snprintf(buffer, bufsize, ".%lu", num);
		if (count >= bufsize)
			return -ENOBUFS;
		buffer += count;
		bufsize -= count;
	}

	return ret;

bad:
	snprintf(buffer, bufsize, "(bad)");
	return -EBADMSG;
}
EXPORT_SYMBOL_GPL(sprint_oid);

/**
 * sprint_OID - Print an Object Identifier into a buffer
 * @oid: The OID to print
 * @buffer: The buffer to render into
 * @bufsize: The size of the buffer
 *
 * The OID is rendered into the buffer in "a.b.c.d" format and the number of
 * bytes is returned.
 */
int sprint_OID(enum OID oid, char *buffer, size_t bufsize)
{
	int ret;

	BUG_ON(oid >= OID__NR);

	ret = sprint_oid(oid_data + oid_index[oid],
			 oid_index[oid + 1] - oid_index[oid],
			 buffer, bufsize);
	BUG_ON(ret == -EBADMSG);
	return ret;
}
EXPORT_SYMBOL_GPL(sprint_OID);
