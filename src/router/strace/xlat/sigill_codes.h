/* Generated by ./xlat/gen.sh from ./xlat/sigill_codes.in; do not edit. */

#include "gcc_compat.h"
#include "static_assert.h"

#if defined(ILL_ILLOPC) || (defined(HAVE_DECL_ILL_ILLOPC) && HAVE_DECL_ILL_ILLOPC)
DIAG_PUSH_IGNORE_TAUTOLOGICAL_COMPARE
static_assert((ILL_ILLOPC) == (1), "ILL_ILLOPC != 1");
DIAG_POP_IGNORE_TAUTOLOGICAL_COMPARE
#else
# define ILL_ILLOPC 1
#endif
#if defined(ILL_ILLOPN) || (defined(HAVE_DECL_ILL_ILLOPN) && HAVE_DECL_ILL_ILLOPN)
DIAG_PUSH_IGNORE_TAUTOLOGICAL_COMPARE
static_assert((ILL_ILLOPN) == (2), "ILL_ILLOPN != 2");
DIAG_POP_IGNORE_TAUTOLOGICAL_COMPARE
#else
# define ILL_ILLOPN 2
#endif
#if defined(ILL_ILLADR) || (defined(HAVE_DECL_ILL_ILLADR) && HAVE_DECL_ILL_ILLADR)
DIAG_PUSH_IGNORE_TAUTOLOGICAL_COMPARE
static_assert((ILL_ILLADR) == (3), "ILL_ILLADR != 3");
DIAG_POP_IGNORE_TAUTOLOGICAL_COMPARE
#else
# define ILL_ILLADR 3
#endif
#if defined(ILL_ILLTRP) || (defined(HAVE_DECL_ILL_ILLTRP) && HAVE_DECL_ILL_ILLTRP)
DIAG_PUSH_IGNORE_TAUTOLOGICAL_COMPARE
static_assert((ILL_ILLTRP) == (4), "ILL_ILLTRP != 4");
DIAG_POP_IGNORE_TAUTOLOGICAL_COMPARE
#else
# define ILL_ILLTRP 4
#endif
#if defined(ILL_PRVOPC) || (defined(HAVE_DECL_ILL_PRVOPC) && HAVE_DECL_ILL_PRVOPC)
DIAG_PUSH_IGNORE_TAUTOLOGICAL_COMPARE
static_assert((ILL_PRVOPC) == (5), "ILL_PRVOPC != 5");
DIAG_POP_IGNORE_TAUTOLOGICAL_COMPARE
#else
# define ILL_PRVOPC 5
#endif
#if defined(ILL_PRVREG) || (defined(HAVE_DECL_ILL_PRVREG) && HAVE_DECL_ILL_PRVREG)
DIAG_PUSH_IGNORE_TAUTOLOGICAL_COMPARE
static_assert((ILL_PRVREG) == (6), "ILL_PRVREG != 6");
DIAG_POP_IGNORE_TAUTOLOGICAL_COMPARE
#else
# define ILL_PRVREG 6
#endif
#if defined(ILL_COPROC) || (defined(HAVE_DECL_ILL_COPROC) && HAVE_DECL_ILL_COPROC)
DIAG_PUSH_IGNORE_TAUTOLOGICAL_COMPARE
static_assert((ILL_COPROC) == (7), "ILL_COPROC != 7");
DIAG_POP_IGNORE_TAUTOLOGICAL_COMPARE
#else
# define ILL_COPROC 7
#endif
#if defined(ILL_BADSTK) || (defined(HAVE_DECL_ILL_BADSTK) && HAVE_DECL_ILL_BADSTK)
DIAG_PUSH_IGNORE_TAUTOLOGICAL_COMPARE
static_assert((ILL_BADSTK) == (8), "ILL_BADSTK != 8");
DIAG_POP_IGNORE_TAUTOLOGICAL_COMPARE
#else
# define ILL_BADSTK 8
#endif

#ifndef XLAT_MACROS_ONLY

# ifdef IN_MPERS

extern const struct xlat sigill_codes[];

# else

static const struct xlat_data sigill_codes_xdata[] = {
 XLAT(ILL_ILLOPC),
#if defined(ILL_ILLPARAOP) || (defined(HAVE_DECL_ILL_ILLPARAOP) && HAVE_DECL_ILL_ILLPARAOP)
  XLAT(ILL_ILLPARAOP),
#endif
 XLAT(ILL_ILLOPN),
 XLAT(ILL_ILLADR),
#if defined(ILL_ILLEXCPT) || (defined(HAVE_DECL_ILL_ILLEXCPT) && HAVE_DECL_ILL_ILLEXCPT)
  XLAT(ILL_ILLEXCPT),
#endif
 XLAT(ILL_ILLTRP),
 XLAT(ILL_PRVOPC),
 XLAT(ILL_PRVREG),
 XLAT(ILL_COPROC),
 XLAT(ILL_BADSTK),
#if defined(ILL_CPLB_VI) || (defined(HAVE_DECL_ILL_CPLB_VI) && HAVE_DECL_ILL_CPLB_VI)
  XLAT(ILL_CPLB_VI),
#endif
#if defined(ILL_CPLB_MISS) || (defined(HAVE_DECL_ILL_CPLB_MISS) && HAVE_DECL_ILL_CPLB_MISS)
  XLAT(ILL_CPLB_MISS),
#endif
#if defined(ILL_CPLB_MULHIT) || (defined(HAVE_DECL_ILL_CPLB_MULHIT) && HAVE_DECL_ILL_CPLB_MULHIT)
  XLAT(ILL_CPLB_MULHIT),
#endif
#if defined(ILL_DBLFLT) || (defined(HAVE_DECL_ILL_DBLFLT) && HAVE_DECL_ILL_DBLFLT)
  XLAT(ILL_DBLFLT),
#endif
#if defined(ILL_HARDWALL) || (defined(HAVE_DECL_ILL_HARDWALL) && HAVE_DECL_ILL_HARDWALL)
  XLAT(ILL_HARDWALL),
#endif
#if defined(ILL_BADIADDR) || (defined(HAVE_DECL_ILL_BADIADDR) && HAVE_DECL_ILL_BADIADDR)
  XLAT(ILL_BADIADDR),
#endif
#if defined(__ILL_BREAK) || (defined(HAVE_DECL___ILL_BREAK) && HAVE_DECL___ILL_BREAK)
  XLAT(__ILL_BREAK),
#endif
#if defined(__ILL_BNDMOD) || (defined(HAVE_DECL___ILL_BNDMOD) && HAVE_DECL___ILL_BNDMOD)
  XLAT(__ILL_BNDMOD),
#endif
};
#  if !(defined HAVE_M32_MPERS || defined HAVE_MX32_MPERS)
static
#  endif
const struct xlat sigill_codes[1] = { {
 .data = sigill_codes_xdata,
 .size = ARRAY_SIZE(sigill_codes_xdata),
 .type = XT_NORMAL,
} };

# endif /* !IN_MPERS */

#endif /* !XLAT_MACROS_ONLY */
