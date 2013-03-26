/*
 * Copyright (C) 2008-2013 Tobias Brunner
 * Copyright (C) 2005-2006 Martin Willi
 * Copyright (C) 2005 Jan Hutter
 * Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>

#include "chunk.h"
#include "debug.h"

/**
 * Empty chunk.
 */
chunk_t chunk_empty = { NULL, 0 };

/**
 * Described in header.
 */
chunk_t chunk_create_clone(u_char *ptr, chunk_t chunk)
{
	chunk_t clone = chunk_empty;

	if (chunk.ptr && chunk.len > 0)
	{
		clone.ptr = ptr;
		clone.len = chunk.len;
		memcpy(clone.ptr, chunk.ptr, chunk.len);
	}

	return clone;
}

/**
 * Described in header.
 */
size_t chunk_length(const char* mode, ...)
{
	va_list chunks;
	size_t length = 0;

	va_start(chunks, mode);
	while (TRUE)
	{
		switch (*mode++)
		{
			case 'm':
			case 'c':
			case 's':
			{
				chunk_t ch = va_arg(chunks, chunk_t);
				length += ch.len;
				continue;
			}
			default:
				break;
		}
		break;
	}
	va_end(chunks);
	return length;
}

/**
 * Described in header.
 */
chunk_t chunk_create_cat(u_char *ptr, const char* mode, ...)
{
	va_list chunks;
	chunk_t construct = chunk_create(ptr, 0);

	va_start(chunks, mode);
	while (TRUE)
	{
		bool free_chunk = FALSE, clear_chunk = FALSE;
		chunk_t ch;

		switch (*mode++)
		{
			case 's':
				clear_chunk = TRUE;
				/* FALL */
			case 'm':
				free_chunk = TRUE;
				/* FALL */
			case 'c':
				ch = va_arg(chunks, chunk_t);
				memcpy(ptr, ch.ptr, ch.len);
				ptr += ch.len;
				construct.len += ch.len;
				if (clear_chunk)
				{
					chunk_clear(&ch);
				}
				else if (free_chunk)
				{
					free(ch.ptr);
				}
				continue;
			default:
				break;
		}
		break;
	}
	va_end(chunks);

	return construct;
}

/**
 * Described in header.
 */
void chunk_split(chunk_t chunk, const char *mode, ...)
{
	va_list chunks;
	u_int len;
	chunk_t *ch;

	va_start(chunks, mode);
	while (TRUE)
	{
		if (*mode == '\0')
		{
			break;
		}
		len = va_arg(chunks, u_int);
		ch = va_arg(chunks, chunk_t*);
		/* a null chunk means skip len bytes */
		if (ch == NULL)
		{
			chunk = chunk_skip(chunk, len);
			continue;
		}
		switch (*mode++)
		{
			case 'm':
			{
				ch->len = min(chunk.len, len);
				if (ch->len)
				{
					ch->ptr = chunk.ptr;
				}
				else
				{
					ch->ptr = NULL;
				}
				chunk = chunk_skip(chunk, ch->len);
				continue;
			}
			case 'a':
			{
				ch->len = min(chunk.len, len);
				if (ch->len)
				{
					ch->ptr = malloc(ch->len);
					memcpy(ch->ptr, chunk.ptr, ch->len);
				}
				else
				{
					ch->ptr = NULL;
				}
				chunk = chunk_skip(chunk, ch->len);
				continue;
			}
			case 'c':
			{
				ch->len = min(ch->len, chunk.len);
				ch->len = min(ch->len, len);
				if (ch->len)
				{
					memcpy(ch->ptr, chunk.ptr, ch->len);
				}
				else
				{
					ch->ptr = NULL;
				}
				chunk = chunk_skip(chunk, ch->len);
				continue;
			}
			default:
				break;
		}
		break;
	}
	va_end(chunks);
}

/**
 * Described in header.
 */
bool chunk_write(chunk_t chunk, char *path, char *label, mode_t mask, bool force)
{
	mode_t oldmask;
	FILE *fd;
	bool good = FALSE;

	if (!force && access(path, F_OK) == 0)
	{
		DBG1(DBG_LIB, "  %s file '%s' already exists", label, path);
		return FALSE;
	}
	oldmask = umask(mask);
	fd = fopen(path, "w");
	if (fd)
	{
		if (fwrite(chunk.ptr, sizeof(u_char), chunk.len, fd) == chunk.len)
		{
			DBG1(DBG_LIB, "  written %s file '%s' (%d bytes)",
				 label, path, chunk.len);
			good = TRUE;
		}
		else
		{
			DBG1(DBG_LIB, "  writing %s file '%s' failed: %s",
				 label, path, strerror(errno));
		}
		fclose(fd);
	}
	else
	{
		DBG1(DBG_LIB, "  could not open %s file '%s': %s", label, path,
			 strerror(errno));
	}
	umask(oldmask);
	return good;
}


/** hex conversion digits */
static char hexdig_upper[] = "0123456789ABCDEF";
static char hexdig_lower[] = "0123456789abcdef";

/**
 * Described in header.
 */
chunk_t chunk_to_hex(chunk_t chunk, char *buf, bool uppercase)
{
	int i, len;
	char *hexdig = hexdig_lower;

	if (uppercase)
	{
		hexdig = hexdig_upper;
	}

	len = chunk.len * 2;
	if (!buf)
	{
		buf = malloc(len + 1);
	}
	buf[len] = '\0';

	for (i = 0; i < chunk.len; i++)
	{
		buf[i*2]   = hexdig[(chunk.ptr[i] >> 4) & 0xF];
		buf[i*2+1] = hexdig[(chunk.ptr[i]     ) & 0xF];
	}
	return chunk_create(buf, len);
}

/**
 * convert a signle hex character to its binary value
 */
static char hex2bin(char hex)
{
	switch (hex)
	{
		case '0' ... '9':
			return hex - '0';
		case 'A' ... 'F':
			return hex - 'A' + 10;
		case 'a' ... 'f':
			return hex - 'a' + 10;
		default:
			return 0;
	}
}

/**
 * Described in header.
 */
chunk_t chunk_from_hex(chunk_t hex, char *buf)
{
	int i, len;
	u_char *ptr;
	bool odd = FALSE;

   /* subtract the number of optional ':' separation characters */
	len = hex.len;
	ptr = hex.ptr;
	for (i = 0; i < hex.len; i++)
	{
		if (*ptr++ == ':')
		{
			len--;
		}
	}

	/* compute the number of binary bytes */
	if (len % 2)
	{
		odd = TRUE;
		len++;
	}
	len /= 2;

	/* allocate buffer memory unless provided by caller */
	if (!buf)
	{
		buf = malloc(len);
	}

	/* buffer is filled from the right */
	memset(buf, 0, len);
	hex.ptr += hex.len;

	for (i = len - 1; i >= 0; i--)
	{
		/* skip separation characters */
		if (*(--hex.ptr) == ':')
		{
			--hex.ptr;
		}
		buf[i] = hex2bin(*hex.ptr);
		if (i > 0 || !odd)
		{
			buf[i] |= hex2bin(*(--hex.ptr)) << 4;
		}
	}
	return chunk_create(buf, len);
}

/** base 64 conversion digits */
static char b64digits[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * Described in header.
 */
chunk_t chunk_to_base64(chunk_t chunk, char *buf)
{
	int i, len;
	char *pos;

	len = chunk.len + ((3 - chunk.len % 3) % 3);
	if (!buf)
	{
		buf = malloc(len * 4 / 3 + 1);
	}
	pos = buf;
	for (i = 0; i < len; i+=3)
	{
		*pos++ = b64digits[chunk.ptr[i] >> 2];
		if (i+1 >= chunk.len)
		{
			*pos++ = b64digits[(chunk.ptr[i] & 0x03) << 4];
			*pos++ = '=';
			*pos++ = '=';
			break;
		}
		*pos++ = b64digits[((chunk.ptr[i] & 0x03) << 4) | (chunk.ptr[i+1] >> 4)];
		if (i+2 >= chunk.len)
		{
			*pos++ = b64digits[(chunk.ptr[i+1] & 0x0F) << 2];
			*pos++ = '=';
			break;
		}
		*pos++ = b64digits[((chunk.ptr[i+1] & 0x0F) << 2) | (chunk.ptr[i+2] >> 6)];
		*pos++ = b64digits[chunk.ptr[i+2] & 0x3F];
	}
	*pos = '\0';
	return chunk_create(buf, len * 4 / 3);
}

/**
 * convert a base 64 digit to its binary form (inversion of b64digits array)
 */
static int b642bin(char b64)
{
	switch (b64)
	{
		case 'A' ... 'Z':
			return b64 - 'A';
		case 'a' ... 'z':
			return ('Z' - 'A' + 1) + b64 - 'a';
		case '0' ... '9':
			return ('Z' - 'A' + 1) + ('z' - 'a' + 1) + b64 - '0';
		case '+':
		case '-':
			return 62;
		case '/':
		case '_':
			return 63;
		case '=':
			return 0;
		default:
			return -1;
	}
}

/**
 * Described in header.
 */
chunk_t chunk_from_base64(chunk_t base64, char *buf)
{
	u_char *pos, byte[4];
	int i, j, len, outlen;

	len = base64.len / 4 * 3;
	if (!buf)
	{
		buf = malloc(len);
	}
	pos = base64.ptr;
	outlen = 0;
	for (i = 0; i < len; i+=3)
	{
		outlen += 3;
		for (j = 0; j < 4; j++)
		{
			if (*pos == '=')
			{
				outlen--;
			}
			byte[j] = b642bin(*pos++);
		}
		buf[i] = (byte[0] << 2) | (byte[1] >> 4);
		buf[i+1] = (byte[1] << 4) | (byte[2] >> 2);
		buf[i+2] = (byte[2] << 6) | (byte[3]);
	}
	return chunk_create(buf, outlen);
}

/** base 32 conversion digits */
static char b32digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

/**
 * Described in header.
 */
chunk_t chunk_to_base32(chunk_t chunk, char *buf)
{
	int i, len;
	char *pos;

	len = chunk.len + ((5 - chunk.len % 5) % 5);
	if (!buf)
	{
		buf = malloc(len * 8 / 5 + 1);
	}
	pos = buf;
	for (i = 0; i < len; i+=5)
	{
		*pos++ = b32digits[chunk.ptr[i] >> 3];
		if (i+1 >= chunk.len)
		{
			*pos++ = b32digits[(chunk.ptr[i] & 0x07) << 2];
			memset(pos, '=', 6);
			pos += 6;
			break;
		}
		*pos++ = b32digits[((chunk.ptr[i] & 0x07) << 2) |
						   (chunk.ptr[i+1] >> 6)];
		*pos++ = b32digits[(chunk.ptr[i+1] & 0x3E) >> 1];
		if (i+2 >= chunk.len)
		{
			*pos++ = b32digits[(chunk.ptr[i+1] & 0x01) << 4];
			memset(pos, '=', 4);
			pos += 4;
			break;
		}
		*pos++ = b32digits[((chunk.ptr[i+1] & 0x01) << 4) |
						   (chunk.ptr[i+2] >> 4)];
		if (i+3 >= chunk.len)
		{
			*pos++ = b32digits[(chunk.ptr[i+2] & 0x0F) << 1];
			memset(pos, '=', 3);
			pos += 3;
			break;
		}
		*pos++ = b32digits[((chunk.ptr[i+2] & 0x0F) << 1) |
						   (chunk.ptr[i+3] >> 7)];
		*pos++ = b32digits[(chunk.ptr[i+3] & 0x7F) >> 2];
		if (i+4 >= chunk.len)
		{
			*pos++ = b32digits[(chunk.ptr[i+3] & 0x03) << 3];
			*pos++ = '=';
			break;
		}
		*pos++ = b32digits[((chunk.ptr[i+3] & 0x03) << 3) |
						   (chunk.ptr[i+4] >> 5)];
		*pos++ = b32digits[chunk.ptr[i+4] & 0x1F];
	}
	*pos = '\0';
	return chunk_create(buf, len * 8 / 5);
}

/**
 * Described in header.
 */
int chunk_compare(chunk_t a, chunk_t b)
{
	int compare_len = a.len - b.len;
	int len = (compare_len < 0)? a.len : b.len;

	if (compare_len != 0 || len == 0)
	{
		return compare_len;
	}
	return memcmp(a.ptr, b.ptr, len);
};


/**
 * Described in header.
 */
bool chunk_increment(chunk_t chunk)
{
	int i;

	for (i = chunk.len - 1; i >= 0; i--)
	{
		if (++chunk.ptr[i] != 0)
		{
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Remove non-printable characters from a chunk.
 */
bool chunk_printable(chunk_t chunk, chunk_t *sane, char replace)
{
	bool printable = TRUE;
	int i;

	if (sane)
	{
		*sane = chunk_clone(chunk);
	}
	for (i = 0; i < chunk.len; i++)
	{
		if (!isprint(chunk.ptr[i]))
		{
			if (sane)
			{
				sane->ptr[i] = replace;
			}
			printable = FALSE;
		}
	}
	return printable;
}

/**
 * Helper functions for chunk_mac()
 */
static inline u_int64_t sipget(u_char *in)
{
	u_int64_t v = 0;
	int i;

	for (i = 0; i < 64; i += 8, ++in)
	{
		v |= ((u_int64_t)*in) << i;
	}
	return v;
}

static inline u_int64_t siprotate(u_int64_t v, int shift)
{
        return (v << shift) | (v >> (64 - shift));
}

static inline void sipround(u_int64_t *v0, u_int64_t *v1, u_int64_t *v2,
							u_int64_t *v3)
{
	*v0 += *v1;
	*v1 = siprotate(*v1, 13);
	*v1 ^= *v0;
	*v0 = siprotate(*v0, 32);

	*v2 += *v3;
	*v3 = siprotate(*v3, 16);
	*v3 ^= *v2;

	*v2 += *v1;
	*v1 = siprotate(*v1, 17);
	*v1 ^= *v2;
	*v2 = siprotate(*v2, 32);

	*v0 += *v3;
	*v3 = siprotate(*v3, 21);
	*v3 ^= *v0;
}

static inline void sipcompress(u_int64_t *v0, u_int64_t *v1, u_int64_t *v2,
							   u_int64_t *v3, u_int64_t m)
{
	*v3 ^= m;
	sipround(v0, v1, v2, v3);
	sipround(v0, v1, v2, v3);
	*v0 ^= m;
}

static inline u_int64_t siplast(size_t len, u_char *pos)
{
	u_int64_t b;
	int rem = len & 7;

	b = ((u_int64_t)len) << 56;
	switch (rem)
	{
		case 7:
			b |= ((u_int64_t)pos[6]) << 48;
		case 6:
			b |= ((u_int64_t)pos[5]) << 40;
		case 5:
			b |= ((u_int64_t)pos[4]) << 32;
		case 4:
			b |= ((u_int64_t)pos[3]) << 24;
		case 3:
			b |= ((u_int64_t)pos[2]) << 16;
		case 2:
			b |= ((u_int64_t)pos[1]) <<  8;
		case 1:
			b |= ((u_int64_t)pos[0]);
			break;
		case 0:
			break;
	}
	return b;
}

/**
 * Caculate SipHash-2-4 with an optional first block given as argument.
 */
static u_int64_t chunk_mac_inc(chunk_t chunk, u_char *key, u_int64_t m)
{
	u_int64_t v0, v1, v2, v3, k0, k1;
	size_t len = chunk.len;
	u_char *pos = chunk.ptr, *end;

	end = chunk.ptr + len - (len % 8);

	k0 = sipget(key);
	k1 = sipget(key + 8);

	v0 = k0 ^ 0x736f6d6570736575ULL;
	v1 = k1 ^ 0x646f72616e646f6dULL;
	v2 = k0 ^ 0x6c7967656e657261ULL;
	v3 = k1 ^ 0x7465646279746573ULL;

	if (m)
	{
		sipcompress(&v0, &v1, &v2, &v3, m);
	}

	/* compression with c = 2 */
	for (; pos != end; pos += 8)
	{
		m = sipget(pos);
		sipcompress(&v0, &v1, &v2, &v3, m);
	}
	sipcompress(&v0, &v1, &v2, &v3, siplast(len, pos));

	/* finalization with d = 4 */
	v2 ^= 0xff;
	sipround(&v0, &v1, &v2, &v3);
	sipround(&v0, &v1, &v2, &v3);
	sipround(&v0, &v1, &v2, &v3);
	sipround(&v0, &v1, &v2, &v3);
	return v0 ^ v1 ^ v2  ^ v3;
}

/**
 * Described in header.
 */
u_int64_t chunk_mac(chunk_t chunk, u_char *key)
{
	return chunk_mac_inc(chunk, key, 0);
}

/**
 * Secret key allocated randomly during first use.
 */
static u_char key[16];

/**
 * Only allocate the key once
 */
static pthread_once_t key_allocated = PTHREAD_ONCE_INIT;

/**
 * Allocate a key on first use, we do this manually to avoid dependencies on
 * plugins.
 */
static void allocate_key()
{
	ssize_t len;
	size_t done = 0;
	int fd;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd >= 0)
	{
		while (done < sizeof(key))
		{
			len = read(fd, key + done, sizeof(key) - done);
			if (len < 0)
			{
				break;
			}
			done += len;
		}
		close(fd);
	}
	/* on error we use random() to generate the key (better than nothing) */
	if (done < sizeof(key))
	{
		srandom(time(NULL) + getpid());
		for (; done < sizeof(key); done++)
		{
			key[done] = (u_char)random();
		}
	}
}

/**
 * Described in header.
 */
u_int32_t chunk_hash_inc(chunk_t chunk, u_int32_t hash)
{
	pthread_once(&key_allocated, allocate_key);
	/* we could use a mac of the previous hash, but this is faster */
	return chunk_mac_inc(chunk, key, ((u_int64_t)hash) << 32 | hash);
}

/**
 * Described in header.
 */
u_int32_t chunk_hash(chunk_t chunk)
{
	pthread_once(&key_allocated, allocate_key);
	return chunk_mac(chunk, key);
}

/**
 * Described in header.
 */
int chunk_printf_hook(printf_hook_data_t *data, printf_hook_spec_t *spec,
					  const void *const *args)
{
	chunk_t *chunk = *((chunk_t**)(args[0]));
	bool first = TRUE;
	chunk_t copy = *chunk;
	int written = 0;

	if (!spec->hash)
	{
		u_int chunk_len = chunk->len;
		const void *new_args[] = {&chunk->ptr, &chunk_len};
		return mem_printf_hook(data, spec, new_args);
	}

	while (copy.len > 0)
	{
		if (first)
		{
			first = FALSE;
		}
		else
		{
			written += print_in_hook(data, ":");
		}
		written += print_in_hook(data, "%02x", *copy.ptr++);
		copy.len--;
	}
	return written;
}
