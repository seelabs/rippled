#include <uapi/linux/ptrace.h>

struct tx_exit_data_t
{
    // weirdly, program verification is dependent on the order  and type of
    // these declearaions. Be careful moving the order around or changing data
    // type, the probram may not verify.
    u32 type;
    int ter;
    u64 duration;
    u8 id[32];
};

BPF_HASH(start, u32);
BPF_PERF_OUTPUT(exit_data);

int
trace_txn_entry(struct pt_regs* ctx)
{
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid;
    u32 tgid = pid_tgid >> 32;
    u64 ts = bpf_ktime_get_ns();

    FILTER

    start.update(&pid, &ts);

    return 0;
}

int
trace_txn_exit(struct pt_regs* ctx)
{
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid;
    u32 tgid = pid_tgid >> 32;
    u64 ts = bpf_ktime_get_ns();

    FILTER

    struct tx_exit_data_t data;

    // calculate delta time
    u64* tsp = start.lookup(&pid);
    if (tsp == 0)
    {
        return 0;  // missed start
    }
    data.duration = bpf_ktime_get_ns() - *tsp;
    start.delete(&pid);

    uint64_t addr;
    bpf_usdt_readarg(1, ctx, &addr);
    bpf_probe_read(data.id, 32 * sizeof(u8), (void*)addr);
    bpf_usdt_readarg(2, ctx, &addr);
    int typeAsInt;
    bpf_probe_read(&typeAsInt, sizeof(int), (void*)addr);
    data.type = typeAsInt;
    bpf_usdt_readarg(3, ctx, &addr);
    bpf_probe_read(&data.ter, sizeof(int), (void*)addr);
    exit_data.perf_submit(ctx, &data, sizeof(data));
    return 0;
}
