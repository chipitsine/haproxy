/*
 * Minimal reproducer for a cppcheck false positive.
 *
 * cppcheck incorrectly reports a memory leak for the pattern below.
 * The pointer <local_ptr> is a local copy of <buf->ptr>. When realloc()
 * fails and returns NULL, <local_ptr> becomes NULL, but the original
 * allocation is still referenced by <buf->ptr>. The failure branch then
 * uses <buf->ptr> to properly free the original memory, so there is no
 * real leak.
 *
 * Reference: https://www.mail-archive.com/haproxy@formilux.org/msg46968.html
 * Origin:    src/quic_rx.c, function qc_try_store_new_token()
 */

#include <stdlib.h>
#include <string.h>

struct buf {
	char   *ptr;
	size_t  len;
};

static inline char *buf_ptr(struct buf b) { return b.ptr; }
static inline size_t buf_len(struct buf b) { return b.len; }
static inline void buf_free(struct buf *b) { free(b->ptr); b->ptr = NULL; b->len = 0; }

/* Update the raw-byte buffer <b> with <data> of length <len>, reallocating
 * when needed.  The buffer stores raw bytes; no null terminator is appended.
 *
 * False positive: cppcheck may report a memleak at the realloc() line because
 * it sees <local_ptr> (a local variable) overwritten with the NULL return value
 * of realloc() and concludes the original allocation is lost.  In reality the
 * original pointer is preserved in <b->ptr> and properly freed in the else
 * branch, so no leak can occur.
 */
static void update_buf(struct buf *b, const char *data, size_t len)
{
	char *local_ptr;

	local_ptr = buf_ptr(*b);
	if (len > buf_len(*b)) {
		local_ptr = realloc(local_ptr, len);
		if (local_ptr)
			b->ptr = local_ptr;
		else {
			memset(buf_ptr(*b), 0, buf_len(*b));
			buf_free(b);
		}
	}

	if (local_ptr) {
		memcpy(local_ptr, data, len);
		b->len = len;
	}
}

int main(void)
{
	struct buf b = { NULL, 0 };

	update_buf(&b, "hello", 5);
	update_buf(&b, "world!", 6);
	buf_free(&b);
	return 0;
}
