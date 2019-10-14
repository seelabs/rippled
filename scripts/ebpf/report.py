#/usr/bin/env python
#
# report    Reporting script for the database collected by the tx_latency script
#           Use `bokey serve --show report.py` to show the gui in a web browser
# TBD: Option for combining multiple samples
#      Show global histogram on right side of plot (see gallery example)

import bokeh
from bokeh.io import curdoc, show
from bokeh.layouts import gridplot, layout, widgetbox, row, column
from bokeh.models import ColumnDataSource, Dropdown, TextInput, Spacer
from bokeh.plotting import figure

import argparse
import datetime
import numpy as np
import math
import pandas as pd
import sqlite3


class ReportData:
    def __init__(self, file_name='probes.db'):
        self.file_name=None
        curdoc().title = "Rippled eBPF Probes"
        if file_name:
            self._update_db_file(file_name)
        else:
            self.db_file_control = TextInput(value='', title='Db file:')
            self.db_file_control.on_change('value', lambda attr, old, new: self._update_db())
            curdoc().add_root(layout([self.db_file_control]))

    def _update_db_file(self, file_name='data.db'):
        if self.file_name:
            # TBD: Reload the database from the new file
            raise ValueError("Database Already Loaded")

        if not file_name:
            return
        self.file_name = file_name
        self.conn = sqlite3.connect(self.file_name)
        # create tables, if needed
        c = self.conn.cursor()
        c.execute(
            "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='probes';"
        )
        r = c.fetchone()
        if r[0] == 0:
            raise ValueError("Invalid Collection Database.")
        self.collections = pd.read_sql_query(
            'select * from collections order by start;',
            self.conn,
            index_col='id')
        self.probes = pd.read_sql_query(
            'select * from probes;', self.conn, index_col='id')
        self.timings = pd.read_sql_query(
            'select * from timings order by log_bin;', self.conn)
        self.ters = pd.read_sql_query('select * from ters;', self.conn)
        self.tags = pd.read_sql_query('select * from tags;', self.conn)
        self.cached_collection_data = {}
        self.grid_dims = (2,2)
        num_grid_cells = self.grid_dims[0]*self.grid_dims[1]
        self.sources = np.array(
            [ColumnDataSource(data=dict(x=[], y=[])) for i in range(num_grid_cells)]).reshape(*self.grid_dims)
        tools = []
        fig_dims = [(600, 475), (200, 475)] # first element is main plot, second is global histogram
        fig_labels = [('timestamp', 'stat'), ('count', 'bin')]
        self.figures = np.array([
            figure(
                plot_height=fig_dims[i%2][1], plot_width=fig_dims[i%2][0], #tools=tools, 
                x_axis_label=fig_labels[i%2][0], y_axis_label=fig_labels[i%2][1])
            for i in range(num_grid_cells)
        ]).reshape(*self.grid_dims)
        for row in range(self.figures.shape[0]):
            for col in range(self.figures.shape[1]//2):
                self.figures[row, 2*col].circle(
                    x='x', y='y', source=self.sources[row, 2*col])
                self.figures[row, 2*col+1].hbar(y='x', right='y', height=1, source=self.sources[row, 2*col+1])
                self.figures[row, 2*col+1].y_range = self.figures[row, 2*col].y_range
                self.figures[row, 2*col].background_fill_color = '#fafafa'
                self.figures[row, 2*col+1].background_fill_color = '#fafafa'

        self._init_controls()
        self.timings_plots()

    def _init_controls(self):
        if not self.file_name:
            return

        collection_menu = [
            self._collection_menu_item(row)
            for row in self.collections.iterrows()
        ]
        probe_menu = [
            self._probe_menu_item(row) for row in self.probes.iterrows()
        ]
        stats_menu = [('mean', 'mean'), ('median', 'median'), ('min', 'min'),
                      ('max', 'max'), ('count', 'count'), None, ('ter', 'ter')]
        num_grid_cells = self.grid_dims[0]*self.grid_dims[1]
        self.collection_controls = np.array([
            (Dropdown(
                label='Collection',
                button_type='warning',
                menu=collection_menu) if i%2==0 else None) for i in range(num_grid_cells)
        ]).reshape(*self.grid_dims)
        self.probe_controls = np.array([
            Dropdown(label='Probe', button_type='warning', menu=probe_menu)
            for i in range(num_grid_cells)
        ]).reshape(*self.grid_dims)
        self.stat_controls = np.array([
            Dropdown(label='Stat', button_type='warning', menu=stats_menu)
            for i in range(num_grid_cells)
        ]).reshape(*self.grid_dims)
        controls = [
            self.collection_controls, self.probe_controls, self.stat_controls
        ]
        all_controls = []
        for row in range(self.grid_dims[0]):
            for col in range(self.grid_dims[1]):
                for c in controls:
                    all_controls.append(c[row, col])
                    if c[row, col] is None:
                        continue
                    c[row, col].on_change(
                        'value', lambda attr, old, new, row=row, col=col: self._update(row, col))
        self.grid_controls=np.array(all_controls).reshape(*self.grid_dims, len(controls))

    def _collection_menu_item(self, collection_row):
        id = collection_row[0]
        v = collection_row[1]
        date = datetime.datetime.fromtimestamp(
            v.start).strftime('%Y-%m-%d %H:%M:%S')
        name = f'{id}: {date}'
        return (name, f'{id}')

    def _probe_menu_item(self, probe_row):
        id = probe_row[0]
        description = probe_row[1].description
        name = f'{id}: {description}'
        return (name, f'{id}')

    def _update(self, row, col):
        if self.collection_controls[row, col] is None: return
        collection_id = self.collection_controls[row, col].value
        probe_id = self.probe_controls[row, col].value
        stat = self.stat_controls[row, col].value
        if collection_id is None or probe_id is None or stat is None:
            self.sources[row, col].data = dict(x=[], y=[])
            self.sources[row, col+1].data = dict(x=[], y=[])
            return

        collection_id = int(collection_id)
        probe_id = int(probe_id)

        probe_name = self.probes.loc[probe_id, 'description']
        td = self.timings_data(collection_id)
        if stat == 'ter':
            df = td.ter_data_frame
        else:
            df = td.data_frame
        df = df[df['probe_id'] == probe_id]
        title=probe_name + ' ' + stat
        f = self.figures[row, col]
        f.xaxis.axis_label = 'timestamp'
        if stat == 'ter':
            f.yaxis.axis_label = probe_name + ' ' + stat
        else:
            f.yaxis.axis_label = probe_name + ' ' + stat + ' (log2 usec)'
        tags = self.tags[self.tags['collection_id'] == collection_id]
        git_hash = self.collections.loc[collection_id,'git_commit']
        f.title.text = title + ': ' + ','.join(tags['tag']) + f' ({git_hash})'
        if stat == 'ter':
            hist = np.trim_zeros(td.global_ter_histogram[probe_id], 'f')
            num_leading_zeros = len(td.global_ter_histogram[probe_id]) - len(hist)
        else:
            hist = np.trim_zeros(td.global_histogram[probe_id], 'f')
            num_leading_zeros = len(td.global_histogram[probe_id]) - len(hist)
        hist = np.trim_zeros(hist, 'b')

        self.sources[row, col].data = dict(x=df['timestamp'], y=df[stat])
        if stat == 'ter':
            self.sources[row, col+1].data = dict(y=hist, x=[i+num_leading_zeros+td.min_ter for i in range(len(hist))])
        else:
            self.sources[row, col+1].data = dict(y=hist, x=[i+num_leading_zeros for i in range(len(hist))])

    def _update_db(self):
        self._update_db_file(self.db_file_control.value)

    def timings_data(self, collection_id):
        if collection_id in self.cached_collection_data:
            return self.cached_collection_data[collection_id]

        start, end = self.collections.loc[collection_id, ['start', 'end']]
        t = self.timings.loc[(self.timings['timestamp'] >= start) & (self.timings['timestamp'] <= end), :]
        ter = self.ters.loc[(self.ters['timestamp'] >= start) & (self.ters['timestamp'] <= end), :]
        self.cached_collection_data[collection_id] = CollectionData(t, ter, len(self.probes))
        return self.cached_collection_data[collection_id]

    def timings_plots(self):
        rows = []
        for r in range(self.grid_dims[0]):
            for c in range(self.grid_dims[1]//2):
                rows.append(row(widgetbox(*self.grid_controls[r,2*c]), self.figures[r,2*c], self.figures[r,2*c+1]))
                rows.append(Spacer(height=10))
        l = column(*rows)
        curdoc().add_root(l)
        curdoc().title = "Rippled eBPF Probes"


class CollectionData:
    def __init__(self, timing_df, ter_df, num_probes):
        self.min_ter = -99
        self.max_ter = 150
        self.init_timing_dataframe(timing_df)
        self.init_histograms(timing_df, ter_df, num_probes)
        self.ter_data_frame = ter_df

    def init_timing_dataframe(self, in_df):
        data = {
            'timestamp': [],
            'probe_id': [],
            'mean': [],
            'median': [],
            'min': [],
            'max': [],
            'count': []
        }
        for g in in_df.groupby(['timestamp', 'probe_id']):
            count = 0
            total = 0
            bin_right_bound = np.left_shift(1, g[1]['log_bin'])
            for b, c in zip(bin_right_bound, g[1]['counts']):
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
            for b, c in zip(bin_right_bound, g[1]['counts']):
                culm_count += c
                if culm_count >= median_index and median is None:
                    median = b
                if min_val is None:
                    min_val = b
                max_val = b
            data['timestamp'].append(g[0][0])
            data['probe_id'].append(g[0][1])
            data['mean'].append(math.log2(mean))
            data['median'].append(math.log2(median))
            data['min'].append(math.log2(min_val))
            data['max'].append(math.log2(max_val))
            data['count'].append(count)

        self.data_frame = pd.DataFrame(data)

    def init_histograms(self, in_df, ter_df, num_probes):
        groups = in_df.groupby(['probe_id'])

        # row for every probe, col for the histogram
        global_histogram= np.zeros([num_probes, 64])
        # local histogram is a dictionary keyed on probe_id, the value has a
        # column for every timstamp and a row for every histogram bin
        # The row and column indexes are this way so it may be easily displayed as an image
        local_histograms = {}
        for g in groups:
            num_timestamps = len(g[1])
            local_histograms[g[0]] = np.zeros([64, num_timestamps])

        for g in groups:
            probe_id = g[0]
            sorted = g[1].sort_values(by='timestamp')
            lh = local_histograms[probe_id]
            for timestamp_index, (lb, c) in enumerate(zip(sorted['log_bin'], sorted['counts'])):
                global_histogram[probe_id, lb] += c
                lh[lb, timestamp_index] += c

        groups = ter_df.groupby(['probe_id'])

        # row for every probe, col for the histogram
        num_bins = self.max_ter - self.min_ter + 1
        global_ter_histogram= np.zeros([num_probes, num_bins])
        # local histogram is a dictionary keyed on probe_id, the value has a
        # column for every timstamp and a row for every histogram bin
        # The row and column indexes are this way so it may be easily displayed as an image
        local_ter_histograms = {}
        for g in groups:
            num_timestamps = len(g[1])
            local_ter_histograms[g[0]] = np.zeros([num_bins, num_timestamps])

        for g in groups:
            probe_id = g[0]
            sorted = g[1].sort_values(by='timestamp')
            lh = local_ter_histograms[probe_id]
            for timestamp_index, (tv, c) in enumerate(zip(sorted['ter'], sorted['counts'])):
                global_ter_histogram[probe_id, tv-self.min_ter] += c
                lh[tv-self.min_ter, timestamp_index] += c

        self.global_histogram = global_histogram
        self.local_histograms = local_histograms
        self.global_ter_histogram = global_ter_histogram
        self.local_ter_histograms = local_ter_histograms
        # TBD: normalize the local histograms so timestamps with more stamples don't distort the plot


def run(db_file='probes.db'):
    rd = ReportData(db_file)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Report trace probe info from a collection database")
    parser.add_argument(
        "--db", required=True, help="Database file to store the trace results")
    args = parser.parse_args()

    run(args.db)
else:
    run()
