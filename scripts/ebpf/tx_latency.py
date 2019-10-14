#/usr/bin/env python
#
# txlatency   Time transactions and print latency as a histogram.
#             For Linux, uses BCC, eBPF.

# TODO:
# Support for user probes

from bcc import BPF, USDT
import argparse
from collections import defaultdict
from contextlib import contextmanager
import ctypes as ct
import numpy as np
import os
import signal
import sqlite3
import time

mangled_names = {}
mangled_preflight = {}
mangled_doapply = {}
# Transactor::operator()()
mangled_names['transactor'] = '_ZN6ripple10TransactorclEv'
mangled_preflight[
    'payment'] = '_ZN6ripple7Payment9preflightERKNS_16PreflightContextE'
mangled_doapply['payment'] = '_ZN6ripple7Payment7doApplyEv'
mangled_preflight[
    'createoffer'] = '_ZN6ripple11CreateOffer9preflightERKNS_16PreflightContextE'
mangled_doapply['createoffer'] = '_ZN6ripple11CreateOffer7doApplyEv'


class TXLatency:
    def __init__(self, trace_entry, trace_exit=None, pid=None, exe=None):
        if pid and not exe:
            # get the exe from the pid
            exe = f'/proc/{pid}/exe'

        if not trace_entry:
            raise ValueError("must specify entry to trace")
        if not trace_exit:
            trace_exit = trace_entry

        library = exe
        libpath = BPF.find_library(library) or BPF.find_exe(library)
        if not libpath:
            raise ValueError("can't resolve library %s" % library)
        library = libpath

        self.trace_entry = trace_entry
        self.trace_exit = trace_exit
        self.pid = pid
        self.library = library

        # load the program from the c file
        prog_file = os.path.dirname(
            os.path.realpath(__file__)) + '/tx_latency.c'
        with open(prog_file, 'r') as file:
            bpf_text = file.read()

        self.b = BPF(text=self.substitutions(bpf_text))

    def substitutions(self, program):
        filter = ''
        if self.pid:
            filter = 'if (tgid != %d) { return 0; }' % self.pid
        bpf_text = program.replace('FILTER', filter)
        return bpf_text

    def attach_probes(self):
        self.b.attach_uprobe(
            name=self.library,
            sym_re=self.trace_entry,
            fn_name="trace_func_entry",
            pid=self.pid or -1)
        self.b.attach_uretprobe(
            name=self.library,
            sym_re=self.trace_exit,
            fn_name="trace_func_return",
            pid=self.pid or -1)
        matched = self.b.num_open_uprobes()

        if matched == 0:
            raise ValueError(
                "0 functions matched by \"%s\". Exiting." % self.pattern)

    def _table_to_np(self, table):
        return np.array([t.value for t in table.itervalues()])
        pass

    def dist(self):
        return self._table_to_np(self.b.get_table("dist"))

    def result(self):
        return self._table_to_np(self.b.get_table("result"))

    def raw_result(self):
        return self.b.get_table("result")

    def tecs(self):
        return self._table_to_np(self.b.get_table("tecs"))

    def negs(self):
        return self._table_to_np(self.b.get_table("negs"))



class TXUSDTProbes:

    class TxExitData(ct.Structure):
        _fields_ = [("tx_type", ct.c_uint32),
                    ("ter", ct.c_int32),
                    ("duration", ct.c_uint64),
                    ("id", ct.c_uint8*32)]

    def __init__(self, db, pid=None, exe=None):

        if pid and not exe:
            # get the exe from the pid
            exe = f'/proc/{pid}/exe'

        self.db = db
        library = exe
        libpath = BPF.find_library(library) or BPF.find_exe(library)
        if not libpath:
            raise ValueError("can't resolve library %s" % library)
        library = libpath

        self.pid = pid
        self.library = library

        # load the program from the c file
        prog_file = os.path.dirname(
            os.path.realpath(__file__)) + '/tx_usdt_probes.c'
        with open(prog_file, 'r') as file:
            self.bpf_text = file.read()

        self.usdt_exit = USDT(pid=self.pid)

    hex_table = {0:'0',1:'1',2:'2',3:'3',4:'4',5:'5',6:'6',7:'7',8:'8',9:'9',
                 10:'A',11:'B',12:'C',13: 'D',14:'E',15:'F'}

    def to_hex(self, raw):
        ht = self.hex_table
        return ''.join([ht[i>>4] + ht[i&0xf] for i in raw])

    def substitutions(self, program):
        filter = ''
        if self.pid:
            filter = 'if (tgid != %d) { return 0; }' % self.pid
        bpf_text = program.replace('FILTER', filter)
        return bpf_text

    def tx_exit_callback(self, cpu, data, size):
        pd = ct.cast(data, ct.POINTER(self.TxExitData)).contents
        timestamp = int(time.time())
        self.db.add_tx(self.to_hex(pd.id), timestamp, pd.duration, pd.tx_type, pd.ter)

    def attach_probes(self):
        # probe must be enabled before the BPF program is compiled or it will never trigger
        # I don't know why
        self.usdt_exit.enable_probe(probe="transactor_exit", fn_name="trace_txn_exit")
        self.b = BPF(text=self.substitutions(self.bpf_text), usdt_contexts=[self.usdt_exit])
        self.b["exit_data"].open_perf_buffer(lambda cpu, data, size: self.tx_exit_callback(cpu, data, size))
        trace_entry=mangled_names['transactor']
        self.b.attach_uprobe(
            name=self.library,
            sym_re=trace_entry,
            fn_name="trace_txn_entry",
            pid=self.pid or -1)
        matched = self.b.num_open_uprobes()

        if matched == 0:
            raise ValueError(
                "0 functions matched by \"%s\". Exiting." % self.pattern)


class DB:
    def __init__(self, file_name='data.db'):
        self.file_name = file_name
        self.conn = sqlite3.connect(self.file_name)
        # create tables, if needed
        c = self.conn.cursor()
        c.execute(
            "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='probes';"
        )
        r = c.fetchone()
        if r[0] == 0:
            print('Creating db tables')
            self.create_tables()

    def create_tables(self):
        c = self.conn.cursor()
        c.execute('''
        CREATE TABLE collections (id INTEGER PRIMARY KEY ASC,
                                  start INTEGER, end INTEGER, git_commit TEXT);
        ''')
        c.execute('''
        CREATE TABLE probes (id INTEGER PRIMARY KEY ASC, description TEXT);
        ''')
        # N.B. sqlite was not comipled with foreign key support on my dev machine.
        # the following will not work
        # CREATE TABLE timings (FOREIGN KEY (probe_id) REFERENCES probes(id)...
        c.execute('''
        CREATE TABLE timings (probe_id INTEGER, timestamp INTEGER,
                              log_bin INTEGER, counts INTEGER);
        ''')
        c.execute('''
        CREATE TABLE ters (probe_id INTEGER, timestamp INTEGER,
                           ter INTEGER, counts INTEGER);
        ''')

        # store the version and other info as tags
        c.execute('''
        CREATE TABLE tags (collection_id INTEGER, tag TEXT);
        ''')
        c.execute('''
        CREATE TABLE transactions (id CHARACTER(64),
              type INTEGER, timestamp INTEGER, duration INTEGER, ter INTEGER);
        ''')
        c.execute('''
        CREATE INDEX IdIndex ON transactions (id);
        ''')

        probes = [(0, 'transactor'), (1, 'payment'), (2, 'offer_create')]
        c.executemany('''INSERT INTO probes VALUES (?,?);''', probes)
        self.conn.commit()

    def add_timing(self, probe_id, timestamp, histogram):
        c = self.conn.cursor()
        values = []
        for i, v in enumerate(histogram):
            if v:
                values.append((probe_id, timestamp, i, int(v)))
        if values:
            c.executemany('INSERT INTO timings VALUES (?, ?, ?, ?);', values)
        self.conn.commit()

    def add_ters(self, probe_id, timestamp, result, tecs, negs):
        c = self.conn.cursor()
        values = []
        if result[0]:
            values.append((probe_id, timestamp, 0, int(result[0])))  # success
        for i, v in enumerate(tecs):
            if v:
                values.append((probe_id, timestamp, i + 100, int(v)))
        for i, v in enumerate(negs):
            if v:
                values.append((probe_id, timestamp, -i, int(v)))
        if values:
            c.executemany('INSERT INTO ters VALUES (?, ?, ?, ?);', values)
        self.conn.commit()

    def add_collection(self, start, end, commit, tags):
        c = self.conn.cursor()
        values = (start, end, commit)
        c.execute(
            'INSERT INTO collections (start, end, git_commit) VALUES (?, ?, ?);',
            values)
        values = []
        collection_id = c.lastrowid
        for t in tags:
            values.append((collection_id, t))
        c.executemany('INSERT INTO tags VALUES (?,?);', values)
        self.conn.commit()

    def add_tx(self, txid_hex, timestamp, duration, tx_type, ter):
        c = self.conn.cursor()
        values = (txid_hex, timestamp, duration, tx_type, ter)
        c.execute(
            'INSERT INTO transactions (id, timestamp, duration, type, ter) VALUES (?, ?, ?, ?, ?);',
            values)
        self.conn.commit()


# this class is meant to be used with a context manager so the end timestamp is correctly written
class TraceRippled:
    def __init__(self, pid, exe, commit, tags, db_file):
        self.db = DB(db_file)

        # transactor_trace = TXLatency(
        #     trace_entry=mangled_names['transactor'], pid=pid, exe=exe)
        pay_trace = TXLatency(
            trace_entry=mangled_preflight['payment'],
            trace_exit=mangled_doapply['payment'],
            pid=pid,
            exe=exe)
        offer_trace = TXLatency(
            trace_entry=mangled_preflight['createoffer'],
            trace_exit=mangled_doapply['createoffer'],
            pid=pid,
            exe=exe)
        self.usdt_probes = TXUSDTProbes(db=self.db, pid=pid, exe=exe)
        # tuble of probe_id (defined in the db class), if ters should be sampled, and trace
        self.traces = [
            # disable transactor trace as the USDT trace also traces the entry and we can't have two entry traces
            # (0, False, transactor_trace),
            (1, True, pay_trace),
            (2, True, offer_trace)]
        # eBPF is computing cumulative results. Save these results so contribution from this timeslice can be computed
        self.last_culm_timing = [None, None, None]
        self.last_culm_ters = [None, None, None]
        for t in self.traces:
            t[2].attach_probes()
        self.usdt_probes.attach_probes()
        self.start_timestamp = int(time.time())
        self.commit = commit
        self.tags = tags

    def shutdown(self):
        self.db.add_collection(self.start_timestamp, int(time.time()),
                               self.commit, self.tags)

    def sample_probes(self):
        for i, t in enumerate(self.traces):
            d = t[2].dist()
            timestamp = int(time.time())
            if self.last_culm_timing[i] is not None:
                # compute diff
                diff = d - self.last_culm_timing[i]
            else:
                diff = d
            self.last_culm_timing[i] = d
            self.db.add_timing(t[0], timestamp, diff)
            if t[1]:
                results = t[2].result()
                tecs = t[2].tecs()
                negs = t[2].negs()
                timestamp = int(time.time())
                if self.last_culm_ters[i]:
                    results_diff = results - self.last_culm_ters[i][0]
                    tecs_diff = tecs - self.last_culm_ters[i][1]
                    negs_diff = negs - self.last_culm_ters[i][2]
                else:
                    results_diff = results
                    tecs_diff = tecs
                    negs_diff = negs
                self.last_culm_ters[i] = (results, tecs, negs)
                self.db.add_ters(t[0], timestamp, results_diff, tecs_diff,
                                 negs_diff)

        self.usdt_probes.b.kprobe_poll(10)


@contextmanager
def trace_rippled(pid, exe, commit, tags, db_file):
    """Start a trace and return a trace client"""
    try:
        client = None
        client = TraceRippled(pid, exe, commit, tags, db_file)
        yield client
    finally:
        if client:
            client.shutdown()


def signal_ignore(signal, frame):
    pass


def run(pid, exe, commit, tags, db_file, timeslice, duration):
    with trace_rippled(pid, exe, commit, tags, db_file) as t:
        exiting = False
        seconds = 0
        while not exiting:
            try:
                time.sleep(timeslice)
                t.sample_probes()
                seconds += timeslice
                if duration > 0 and seconds >= duration:
                    exiting = True
            except KeyboardInterrupt:
                # trap Ctrl-C:
                signal.signal(signal.SIGINT, signal_ignore)
                exiting = True


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Save trace probe info to a database")
    parser.add_argument("-p", "--pid", type=int, help="trace this PID only")
    parser.add_argument("-e", "--exe", help="executable to trace")
    parser.add_argument(
        "-s",
        "--timeslice",
        type=int,
        help="Timeslice length, in seconds",
        default=600)
    parser.add_argument(
        "-d",
        "--duration",
        type=int,
        default=-1,
        help="total duration of trace, in seconds")
    parser.add_argument(
        "-c",
        "--commit",
        required=True,
        help="Git commit hash of the program being probed")
    parser.add_argument(
        "--db", required=True, help="Database file to store the trace results")
    parser.add_argument(
        "-t",
        "--tags",
        help=
        "Comma separated list of tags. Useful to store version number and other meta data"
    )
    args = parser.parse_args()

    tags = []
    if args.tags:
        # convert the comma separated text into a python list
        tags = [i.strip() for i in args.tags.split(',')]
    run(args.pid, args.exe, args.commit, tags, args.db, args.timeslice,
        args.duration)
