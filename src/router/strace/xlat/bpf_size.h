/* Generated by ./xlat/gen.sh from ./xlat/bpf_size.in; do not edit. */

#include "gcc_compat.h"
#include "static_assert.h"

#if defined(BPF_W) || (defined(HAVE_DECL_BPF_W) && HAVE_DECL_BPF_W)
DIAG_PUSH_IGNORE_TAUTOLOGICAL_COMPARE
static_assert((BPF_W) == (0x00), "BPF_W != 0x00");
DIAG_POP_IGNORE_TAUTOLOGICAL_COMPARE
#else
# define BPF_W 0x00
#endif
#if defined(BPF_H) || (defined(HAVE_DECL_BPF_H) && HAVE_DECL_BPF_H)
DIAG_PUSH_IGNORE_TAUTOLOGICAL_COMPARE
static_assert((BPF_H) == (0x08), "BPF_H != 0x08");
DIAG_POP_IGNORE_TAUTOLOGICAL_COMPARE
#else
# define BPF_H 0x08
#endif
#if defined(BPF_B) || (defined(HAVE_DECL_BPF_B) && HAVE_DECL_BPF_B)
DIAG_PUSH_IGNORE_TAUTOLOGICAL_COMPARE
static_assert((BPF_B) == (0x10), "BPF_B != 0x10");
DIAG_POP_IGNORE_TAUTOLOGICAL_COMPARE
#else
# define BPF_B 0x10
#endif
#if defined(BPF_DW) || (defined(HAVE_DECL_BPF_DW) && HAVE_DECL_BPF_DW)
DIAG_PUSH_IGNORE_TAUTOLOGICAL_COMPARE
static_assert((BPF_DW) == (0x18), "BPF_DW != 0x18");
DIAG_POP_IGNORE_TAUTOLOGICAL_COMPARE
#else
# define BPF_DW 0x18
#endif

#ifndef XLAT_MACROS_ONLY

# ifdef IN_MPERS

#  error static const struct xlat bpf_size in mpers mode

# else

static const struct xlat_data bpf_size_xdata[] = {
 XLAT(BPF_W),
 XLAT(BPF_H),
 XLAT(BPF_B),
 XLAT(BPF_DW),
};
static
const struct xlat bpf_size[1] = { {
 .data = bpf_size_xdata,
 .size = ARRAY_SIZE(bpf_size_xdata),
 .type = XT_NORMAL,
} };

# endif /* !IN_MPERS */

#endif /* !XLAT_MACROS_ONLY */
