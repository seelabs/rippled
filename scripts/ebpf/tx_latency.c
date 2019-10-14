#include <uapi/linux/ptrace.h>

BPF_HASH(start, u32);
BPF_HISTOGRAM(dist);
BPF_HISTOGRAM(tecs, int, 51);
BPF_HISTOGRAM(result, int, 51);
BPF_HISTOGRAM(negs, int, 400);

int trace_func_entry(struct pt_regs *ctx) {
  u64 pid_tgid = bpf_get_current_pid_tgid();
  u32 pid = pid_tgid;
  u32 tgid = pid_tgid >> 32;
  u64 ts = bpf_ktime_get_ns();

  FILTER
  start.update(&pid, &ts);

  return 0;
}

int trace_func_return(struct pt_regs *ctx) {
  u64 pid_tgid = bpf_get_current_pid_tgid();
  u32 pid = pid_tgid;
  u32 tgid = pid_tgid >> 32;

  // calculate delta time
  u64* tsp = start.lookup(&pid);
  if (tsp == 0) {
    return 0; // missed start
  }
  u64 delta = bpf_ktime_get_ns() - *tsp;
  start.delete(&pid);

  // store as histogram (convert from nsec to usec)
  dist.increment(bpf_log2l(delta/1000));

  int ret = PT_REGS_RC(ctx);
  if (ret>100 && ret<150)
  {
      tecs.increment(ret-100);
  }
  else if (ret<0)
      negs.increment(-ret);

  result.increment(!!ret);

  return 0;
}
