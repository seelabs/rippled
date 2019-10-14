#if defined(__linux__) && __has_include(<sys/sdt.h>)
#include <sys/sdt.h>

#define RIPD_PROBE(provider, probe) DTRACE_PROBE(provider, probe)

#define RIPD_PROBE1(provider, probe, parm1)                                    \
  DTRACE_PROBE1(provider, probe, parm1)

#define RIPD_PROBE2(provider, probe, parm1, parm2)                             \
  DTRACE_PROBE2(provider, probe, parm1, parm2)
#define RIPD_PROBE3(provider, probe, parm1, parm2, parm3)                      \
  DTRACE_PROBE3(provider, probe, parm1, parm2, parm3)

#define RIPD_PROBE4(provider, probe, parm1, parm2, parm3, parm4)               \
  DTRACE_PROBE4(provider, probe, parm1, parm2, parm3, parm4)

#define RIPD_PROBE5(provider, probe, parm1, parm2, parm3, parm4, parm5)        \
  DTRACE_PROBE5(provider, probe, parm1, parm2, parm3, parm4, parm5)

#define RIPD_PROBE6(provider, probe, parm1, parm2, parm3, parm4, parm5, parm6) \
  DTRACE_PROBE6(provider, probe, parm1, parm2, parm3, parm4, parm5, parm6)

#define RIPD_PROBE7(provider, probe, parm1, parm2, parm3, parm4, parm5, parm6, \
                    parm7)                                                     \
  DTRACE_PROBE7(provider, probe, parm1, parm2, parm3, parm4, parm5, parm6,     \
                parm7)

#define RIPD_PROBE8(provider, probe, parm1, parm2, parm3, parm4, parm5, parm6, \
                    parm7, parm8)                                              \
  DTRACE_PROBE8(provider, probe, parm1, parm2, parm3, parm4, parm5, parm6,     \
                parm7, parm8)

#define RIPD_PROBE9(provider, probe, parm1, parm2, parm3, parm4, parm5, parm6, \
                    parm7, parm8, parm9)                                       \
  DTRACE_PROBE9(provider, probe, parm1, parm2, parm3, parm4, parm5, parm6,     \
                parm7, parm8, parm9)

#define RIPD_PROBE10(provider, probe, parm1, parm2, parm3, parm4, parm5,       \
                     parm6, parm7, parm8, parm9, parm10)                       \
  DTRACE_PROBE10(provider, probe, parm1, parm2, parm3, parm4, parm5, parm6,    \
                 parm7, parm8, parm9, parm10)

#define RIPD_PROBE11(provider, probe, parm1, parm2, parm3, parm4, parm5,       \
                     parm6, parm7, parm8, parm9, parm10, parm11)               \
  DTRACE_PROBE11(provider, probe, parm1, parm2, parm3, parm4, parm5, parm6,    \
                 parm7, parm8, parm9, parm10, parm11)

#define RIPD_PROBE12(provider, probe, parm1, parm2, parm3, parm4, parm5,       \
                     parm6, parm7, parm8, parm9, parm10, parm11, parm12)       \
  DTRACE_PROBE12(provider, probe, parm1, parm2, parm3, parm4, parm5, parm6,    \
                 parm7, parm8, parm9, parm10, parm11, parm12)

#else
#define RIPD_PROBE(provider,probe)
#define RIPD_PROBE1(provider,probe,parm1)
#define RIPD_PROBE2(provider,probe,parm1,parm2)
#define RIPD_PROBE3(provider,probe,parm1,parm2,parm3)
#define RIPD_PROBE4(provider,probe,parm1,parm2,parm3,parm4)
#define RIPD_PROBE5(provider,probe,parm1,parm2,parm3,parm4,parm5)
#define RIPD_PROBE6(provider,probe,parm1,parm2,parm3,parm4,parm5,parm6)
#define RIPD_PROBE7(provider,probe,parm1,parm2,parm3,parm4,parm5,parm6,parm7)
#define RIPD_PROBE8(provider,probe,parm1,parm2,parm3,parm4,parm5,parm6,parm7,parm8)
#define RIPD_PROBE9(provider,probe,parm1,parm2,parm3,parm4,parm5,parm6,parm7,parm8,parm9)
#define RIPD_PROBE10(provider,probe,parm1,parm2,parm3,parm4,parm5,parm6,parm7,parm8,parm9,parm10)
#define RIPD_PROBE11(provider,probe,parm1,parm2,parm3,parm4,parm5,parm6,parm7,parm8,parm9,parm10,parm11)
#define RIPD_PROBE12(provider,probe,parm1,parm2,parm3,parm4,parm5,parm6,parm7,parm8,parm9,parm10,parm11,parm12)
#endif
