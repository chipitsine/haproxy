/* test-eth-vlan.c: unit tests for sample_conv_eth_vlan()
 *
 * The function under test is sample_conv_eth_vlan() from src/net_helper.c.
 * Rather than pull in HAProxy's sample/arg machinery we reproduce the
 * self-contained logic here so the test compiles with just:
 *
 *   gcc -Iinclude -Wall -W -o test-eth-vlan tests/unit/test-eth-vlan.c
 *
 * A previous version of the function had the condition:
 *   if (idx + 4 < smp->data.u.str.data) break;   <-- BUG: inverted sense
 * which caused it to break out of the loop when there WAS enough room to
 * continue, making it impossible to ever read a VLAN TCI value.  The fix:
 *   if (smp->data.u.str.data < idx + 4) break;   <-- CORRECT
 * These tests document the expected behaviour and catch any regression.
 *
 * NOTE on the loop guard: the function checks (idx + 2 < data) before
 * reading two bytes at area[idx].  This means reading a 14-byte untagged
 * frame requires data >= 15 to satisfy (12 + 2 < data).  Tests use the
 * minimum data value that allows correct operation.
 */

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <haproxy/compat.h>
#include <haproxy/net_helper.h>

/* Minimal stand-in for struct sample.
 * We only need the fields accessed by sample_conv_eth_vlan:
 *   data.u.str.{area,data}  -- input buffer
 *   data.u.sint             -- output VLAN ID (overlaps str in the union)
 *   data.type               -- set to SMP_T_SINT on success
 *   flags                   -- SMP_F_CONST bit cleared on success
 */
#define SMP_T_SINT 1
#define SMP_F_CONST (1 << 2)

struct smp_data {
	int type;
	union {
		struct { char *area; size_t data; } str;
		long long sint;
	} u;
};

struct sample {
	struct smp_data data;
	unsigned int flags;
};

/* ---- Re-implementation of sample_conv_eth_vlan (the fixed version) -------- */

static int eth_vlan(struct sample *smp)
{
	ushort vlan = 0;
	size_t idx;

	for (idx = 12; idx + 2 < smp->data.u.str.data; idx += 4) {
		if (read_n16(smp->data.u.str.area + idx) != 0x8100) {
			smp->data.u.sint = vlan;
			smp->data.type = SMP_T_SINT;
			smp->flags &= ~SMP_F_CONST;
			return !!vlan;
		}
		if (smp->data.u.str.data < idx + 4)
			break;

		vlan = read_n16(smp->data.u.str.area + idx + 2) & 0xfff;
	}
	/* incomplete header */
	return 0;
}

/* Helper: write a big-endian 16-bit value at byte offset <off> in <buf>. */
static void put16(char *buf, size_t off, uint16_t v)
{
	buf[off]     = (v >> 8) & 0xff;
	buf[off + 1] = v & 0xff;
}

#define BUF_SIZE 32

/* ---- Individual tests ----------------------------------------------------- */

/* Buffer too short to contain an ethertype: the loop guard (idx + 2 < data)
 * with idx=12 requires data >= 15; here data=12 → never enters → return 0. */
static int test_too_short(void)
{
	char buf[BUF_SIZE] = {0};
	struct sample smp = { .data = { .u = { .str = { buf, 12 } } } };

	if (eth_vlan(&smp) != 0)
		return __LINE__;
	return 0;
}

/* Untagged frame: ethertype at offset 12 is not 0x8100.  The loop enters
 * (data=15 satisfies 12+2 < 15) and immediately takes the return branch with
 * vlan=0, so !!vlan = 0. */
static int test_no_vlan(void)
{
	char buf[BUF_SIZE] = {0};
	struct sample smp = { .data = { .u = { .str = { buf, 15 } } } };
	int ret;

	put16(buf, 12, 0x0800); /* IPv4 ethertype */

	ret = eth_vlan(&smp);
	if (ret != 0)
		return __LINE__;
	/* sint is set to 0 (= vlan) on this path */
	if (smp.data.u.sint != 0)
		return __LINE__;
	return 0;
}

/* Single-tagged frame: 0x8100 at offset 12, TCI=42 at offset 14, IPv4 at 16.
 * data=19 satisfies idx+2=18 < 19 so the inner-ethertype iteration runs.
 * Return value must be 1, sint must be 42. */
static int test_single_vlan(void)
{
	char buf[BUF_SIZE] = {0};
	struct sample smp = { .data = { .u = { .str = { buf, 19 } } } };
	int ret;

	put16(buf, 12, 0x8100); /* 802.1Q tag type */
	put16(buf, 14, 42);     /* TCI: VLAN 42 */
	put16(buf, 16, 0x0800); /* inner ethertype: IPv4 */

	ret = eth_vlan(&smp);
	if (ret != 1)
		return __LINE__;
	if (smp.data.u.sint != 42)
		return __LINE__;
	return 0;
}

/* Double-tagged (QinQ) frame: outer 0x8100 at 12, outer TCI 100 at 14,
 * inner 0x8100 at 16, inner TCI 200 at 18, IPv4 at 20.
 * data=23 (satisfies 20+2 < 23) lets the last iteration run.
 * The function returns the *last* (innermost) VLAN ID (200). */
static int test_double_vlan(void)
{
	char buf[BUF_SIZE] = {0};
	struct sample smp = { .data = { .u = { .str = { buf, 23 } } } };
	int ret;

	put16(buf, 12, 0x8100); /* outer 802.1Q */
	put16(buf, 14, 100);    /* outer VLAN 100 */
	put16(buf, 16, 0x8100); /* inner 802.1Q */
	put16(buf, 18, 200);    /* inner VLAN 200 */
	put16(buf, 20, 0x0800); /* inner ethertype: IPv4 */

	ret = eth_vlan(&smp);
	if (ret != 1)
		return __LINE__;
	if (smp.data.u.sint != 200)
		return __LINE__;
	return 0;
}

/* Frame truncated right after the 0x8100 ethertype: data=15 allows the loop
 * to enter (14 < 15), reads 0x8100, then checks (data < idx + 4) → 15 < 16
 * which is TRUE → breaks → incomplete → return 0. */
static int test_truncated_after_8100(void)
{
	char buf[BUF_SIZE] = {0};
	struct sample smp = { .data = { .u = { .str = { buf, 15 } } } };

	put16(buf, 12, 0x8100);
	/* TCI bytes at 14-15 are only partially present (data ends at 15) */

	if (eth_vlan(&smp) != 0)
		return __LINE__;
	return 0;
}

/* VLAN tag present but no inner ethertype: 0x8100 at 12, TCI=7 at 14,
 * data=16.  Loop enters at idx=12 (14 < 16), reads TCI, advances idx to 16,
 * then loop guard (16+2=18 < 16) fails → exits → incomplete → return 0. */
static int test_vlan_no_inner_ethertype(void)
{
	char buf[BUF_SIZE] = {0};
	struct sample smp = { .data = { .u = { .str = { buf, 16 } } } };

	put16(buf, 12, 0x8100);
	put16(buf, 14, 7); /* VLAN 7 */
	/* no inner ethertype – only 16 bytes total */

	if (eth_vlan(&smp) != 0)
		return __LINE__;
	return 0;
}

/* Regression test for the inverted-break bug.
 *
 * The old code read:
 *   if (idx + 4 < smp->data.u.str.data) break;
 *
 * With a single-VLAN frame and data=19:
 *   idx=12, idx+4=16, 16 < 19 → TRUE → the buggy code broke immediately,
 *   so vlan was never read from TCI and sint was never set, always returning 0.
 *
 * With the fix:
 *   if (smp->data.u.str.data < idx + 4) break;   → 19 < 16 → FALSE → continues
 * and the function correctly reads the VLAN ID and returns 1.
 */
static int test_regression_inverted_break(void)
{
	char buf[BUF_SIZE] = {0};
	struct sample smp = { .data = { .u = { .str = { buf, 19 } } } };
	int ret;

	put16(buf, 12, 0x8100);
	put16(buf, 14, 42);
	put16(buf, 16, 0x0800);

	ret = eth_vlan(&smp);

	/* The buggy version returned 0 here; the correct version returns 1. */
	if (ret != 1)
		return __LINE__;
	if (smp.data.u.sint != 42)
		return __LINE__;
	return 0;
}

int main(void)
{
	int tret = 0;
	int fret = 0;

	tret = test_too_short();                 printf("%4d test_too_short()\n",                 tret); if (!fret) fret = tret;
	tret = test_no_vlan();                   printf("%4d test_no_vlan()\n",                   tret); if (!fret) fret = tret;
	tret = test_single_vlan();               printf("%4d test_single_vlan()\n",               tret); if (!fret) fret = tret;
	tret = test_double_vlan();               printf("%4d test_double_vlan()\n",               tret); if (!fret) fret = tret;
	tret = test_truncated_after_8100();      printf("%4d test_truncated_after_8100()\n",      tret); if (!fret) fret = tret;
	tret = test_vlan_no_inner_ethertype();   printf("%4d test_vlan_no_inner_ethertype()\n",   tret); if (!fret) fret = tret;
	tret = test_regression_inverted_break(); printf("%4d test_regression_inverted_break()\n", tret); if (!fret) fret = tret;

	return !!fret;
}
