# eBPF and Rippled

This directory contains a sample program to collect transaction information,
including summary information for running times and return codes for payment and
offer create transactions. This can be attached to an unmodified, running rippled server. 

In addition, with a slight modification to the rippled code to insert user static tracing,
detailed transaction information can be collection on a per transaction basis. This recored
transaction duration, transation type, return code, timestamp and id.

If the probes are not attached, the trace will be a no-op in the assembly code.

The purpose is to collect data from betas and detect performance regressions.
There are lots of ways to expand this (collecting stack traces at random, for
example).

# TBD

Note this project is a work in progress. The last change collects data on a per
transaction basis, but there are no reporting tools to display that data yet. There
are reporting tools to display the summery information collected on payment and offer
create transactions


## To run:
1) bcc must be installed (`sudo apt install bpfcc-tools python3-bfpcc linux-headers-$(uname -r)`)
2) The program must run as root
3) The bcc python module must be on the path
4) This will sample for 60 seconds:
```
sudo PYTHONPATH=/usr/lib/python3/dist-packages $(which python3) ./tx_latency.py -p $(pgrep rippled) -d 60 -s 10 -c $(git -C $(dirname $(readlink -f /proc/$(pgrep rippled)/exe)) rev-parse HEAD) -t 1.1.0-b3 --db probes.db
```

## View Report
To view a report, make sure bokeh is installed (I use anaconda python, which ships with bokeh).
Run:
```
bokeh serve report.py
 ```
Open a web browser to the URL from `bokeh serve`. On my system, this is `http://localhost:5006/report`

## About eBPF
eBPF is a linux tracing tool that can run a restricted C program _in the linux
kernel_ in response program events. The current sample uses events for entering
and exiting user functions, and has a static probe for transactor information.

eBFP has simple data types for arrays, hash tables, and histograms. It also has
some built in functions to help with performance profiling. For example:
`bpf_ktime_get_ns()` gets the current time, `bpf_get_current_pid_tgid()` gets
the current process and thread id, `bpf_log2l(u64)` computes the log2 value.
The registers are avialable through the passed in `ctx` parameter and may be used
to track function arguments and return values.

There is support for user defined static tracing. Data is passed to userspace
through `BPF_PERF_OUTPUT` buffers and is read through python callbacks.

## Files
There are two files used in the implementation:

1) `tx_latency.py` is a python script that replaces the placeholders in the C
program, compiles the C program, attaches the probes, and outputs the result. 
2) `tx_latency.c` is the eBPF C program (with placeholders) that the linux kernel
runs in response to events.

The program is very simple: on a function's entry it records the time, on
another function's exit record the time delta in a histogram. For example, to
time payments, start a timer in `preflight` and stop the timer when `doapply`
exits.


