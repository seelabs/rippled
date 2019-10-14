#/usr/bin/env python
#
# report_common Non-gui parts of a report. This will be used by different backends to
#               show the report

from functools import lru_cache
import numpy as np
import math
import pandas as pd
import sqlite3


class ReportData:
    def default_collection_id(file_name: str):
        '''
        Useful for caching Collection Data. If a client calls for CollectionData
        with a `None` collection_id the cache doesn't know if it's present unless
        it has the collection_id.
        '''
        conn = sqlite3.connect(file_name)
        c = conn.cursor()
        collections = pd.read_sql_query(
            'select * from collections order by start DESC limit(1);',
            conn,
            index_col='id')
        return collections.index[-1]

    def __init__(self, file_name: str = 'probes.db', collection_id=None):
        self.file_name = file_name
        self.conn = sqlite3.connect(self.file_name)
        c = self.conn.cursor()
        c.execute(
            "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='probes';"
        )
        r = c.fetchone()
        if r[0] == 0:
            raise ValueError("Invalid Collection Database.")

        self.probes = pd.read_sql_query(
            'select * from probes;', self.conn, index_col='id')

        self.collections = pd.read_sql_query(
            'select * from collections order by start;',
            self.conn,
            index_col='id')
        if collection_id is None:
            collection_id = self.collections.index[-1]
        start, end = self.collections.loc[collection_id, ['start', 'end']]

        where_clause = f'where timestamp >= {start} and timestamp <= {end}'
        self.timings = pd.read_sql_query(
            f'select * from timings {where_clause} order by log_bin;',
            self.conn)
        self.txns = pd.read_sql_query(
            f'select * from transactions {where_clause} order by timestamp;',
            self.conn)
        self.ters = pd.read_sql_query(f'select * from ters {where_clause};',
                                      self.conn)
        self.tags = pd.read_sql_query(
            f'select * from tags where collection_id=={collection_id};',
            self.conn)


class CollectionData:
    min_ter = -99
    max_ter = 150

    def __init__(self, rd: ReportData):
        self.rd = rd
        self._init_timing_dataframe()
        self._init_histograms()

    def _init_timing_dataframe(self):
        data = {
            'timestamp': [],
            'probe_id': [],
            'mean': [],
            'median': [],
            'min': [],
            'max': [],
            'count': []
        }
        for (ts, probe_id), g in self.rd.timings.groupby(
            ['timestamp', 'probe_id']):
            count = 0
            total = 0
            bin_right_bound = np.left_shift(1, g['log_bin'])
            for b, c in zip(bin_right_bound, g['counts']):
                count += c
                total += c * b
            if count == 0:
                continue
            mean = total / count
            total = 0
            culm_count = 0
            median_index = count / 2
            median = None
            min_val = None
            max_val = None
            for b, c in zip(bin_right_bound, g['counts']):
                culm_count += c
                if culm_count >= median_index and median is None:
                    median = b
                if min_val is None:
                    min_val = b
                max_val = b
            data['timestamp'].append(ts)
            data['probe_id'].append(probe_id)
            data['mean'].append(math.log2(mean))
            data['median'].append(math.log2(median))
            data['min'].append(math.log2(min_val))
            data['max'].append(math.log2(max_val))
            data['count'].append(count)

        self.data_frame = pd.DataFrame(data)

    def _init_histograms(self):
        num_probes = len(self.rd.probes)
        groups = self.rd.timings.groupby(['probe_id'])

        # row for every probe, col for the histogram
        global_histogram = np.zeros([num_probes, 64])
        # local histogram is a dictionary keyed on probe_id, the value has a
        # column for every timstamp and a row for every histogram bin
        # The row and column indexes are this way so it may be easily displayed as an image
        local_histograms = {}
        for n, g in groups:
            num_timestamps = len(g)
            local_histograms[n] = np.zeros([64, num_timestamps])

        for n, g in groups:
            probe_id = n
            sorted = g.sort_values(by='timestamp')
            lh = local_histograms[probe_id]
            for timestamp_index, (lb, c) in enumerate(
                    zip(sorted['log_bin'], sorted['counts'])):
                global_histogram[probe_id, lb] += c
                lh[lb, timestamp_index] += c

        groups = self.rd.ters.groupby(['probe_id'])

        # row for every probe, col for the histogram
        num_bins = self.max_ter - self.min_ter + 1
        global_ter_histogram = np.zeros([num_probes, num_bins])
        # local histogram is a dictionary keyed on probe_id, the value has a
        # column for every timstamp and a row for every histogram bin
        # The row and column indexes are this way so it may be easily displayed as an image
        local_ter_histograms = {}
        for n, g in groups:
            num_timestamps = len(g)
            local_ter_histograms[n] = np.zeros([num_bins, num_timestamps])

        for n, g in groups:
            probe_id = n
            sorted = g.sort_values(by='timestamp')
            lh = local_ter_histograms[probe_id]
            for timestamp_index, (tv, c) in enumerate(
                    zip(sorted['ter'], sorted['counts'])):
                global_ter_histogram[probe_id, tv - self.min_ter] += c
                lh[tv - self.min_ter, timestamp_index] += c

        self.global_histogram = global_histogram
        self.local_histograms = local_histograms
        self.global_ter_histogram = global_ter_histogram
        self.local_ter_histograms = local_ter_histograms
        # TBD: normalize the local histograms so timestamps with more stamples don't distort the plot


@lru_cache(maxsize=32)
def _memoized_get_collection_data(db_file_name: str, collection_id: int):
    rd = ReportData(db_file_name, collection_id)
    return CollectionData(rd)


def get_collection_data(db_file_name: str = 'probes.db', collection_id=None):
    if collection_id is None:
        collection_id = ReportData.default_collection_id(db_file_name)
    return _memoized_get_collection_data(db_file_name, collection_id)
