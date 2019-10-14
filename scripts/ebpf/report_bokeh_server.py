#/usr/bin/env python
#
# report    Reporting script for the database collected by the tx_latency script
#           Use `bokey serve --show report_bokeh_server.py` to show the gui in a web browser
# TBD: Individual transaction summary stats

import bokeh
from bokeh.io import curdoc, show
from bokeh.layouts import gridplot, layout, widgetbox, row, column
from bokeh.models import ColumnDataSource, Dropdown, TextInput, Spacer
from bokeh.plotting import figure

from report_common import get_collection_data
import datetime
import numpy as np
import math
import pandas as pd


class BokehServerReport:
    def __init__(self, doc=None):
        if doc is None:
            doc = curdoc()
        self.doc = doc
        self.file_name = None
        self.doc.title = "Rippled eBPF Probes"
        self.db_file_control = TextInput(value='', title='Db file:')
        self.db_file_control.on_change(
            'value', lambda attr, old, new: self._update_db())
        self.doc.add_root(layout([self.db_file_control]))

    def _update_db_file(self, file_name: str = 'probes.db'):
        if self.file_name == file_name:
            return

        self.file_name = file_name
        self.collection_data = get_collection_data(file_name)
        self.grid_dims = (2, 2)
        num_grid_cells = self.grid_dims[0] * self.grid_dims[1]
        self.sources = np.array([
            ColumnDataSource(data=dict(x=[], y=[]))
            for i in range(num_grid_cells)
        ]).reshape(*self.grid_dims)
        tools = []
        fig_dims = [(600, 475), (200, 475)
                    ]  # first element is main plot, second is global histogram
        fig_labels = [('timestamp', 'stat'), ('count', 'bin')]
        self.figures = np.array([
            figure(
                plot_height=fig_dims[i % 2][1],
                plot_width=fig_dims[i % 2][0],  #tools=tools, 
                x_axis_label=fig_labels[i % 2][0],
                y_axis_label=fig_labels[i % 2][1])
            for i in range(num_grid_cells)
        ]).reshape(*self.grid_dims)
        for row in range(self.figures.shape[0]):
            for col in range(self.figures.shape[1] // 2):
                self.figures[row, 2 * col].circle(
                    x='x', y='y', source=self.sources[row, 2 * col])
                self.figures[row, 2 * col + 1].hbar(
                    y='x',
                    right='y',
                    height=1,
                    source=self.sources[row, 2 * col + 1])
                self.figures[row, 2 * col +
                             1].y_range = self.figures[row, 2 * col].y_range
                self.figures[row, 2 * col].background_fill_color = '#fafafa'
                self.figures[row, 2 * col +
                             1].background_fill_color = '#fafafa'

        self._init_controls()
        self.timings_plots()

    def _init_controls(self):
        collections = self.collection_data.rd.collections
        probes = self.collection_data.rd.probes

        collection_menu = [
            self._collection_menu_item(row) for row in collections.iterrows()
        ]
        probe_menu = [self._probe_menu_item(row) for row in probes.iterrows()]
        stats_menu = [('mean', 'mean'), ('median', 'median'), ('min', 'min'),
                      ('max', 'max'), ('count', 'count'), None, ('ter', 'ter')]
        num_grid_cells = self.grid_dims[0] * self.grid_dims[1]
        self.collection_controls = np.array(
            [(Dropdown(
                label='Collection',
                button_type='warning',
                menu=collection_menu) if i % 2 == 0 else None)
             for i in range(num_grid_cells)]).reshape(*self.grid_dims)
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
        self.grid_controls = np.array(all_controls).reshape(
            *self.grid_dims, len(controls))

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

    def _update(self, row: int, col: int):
        if self.collection_controls[row, col] is None: return
        collection_id = self.collection_controls[row, col].value
        probe_id = self.probe_controls[row, col].value
        stat = self.stat_controls[row, col].value
        if None in [collection_id, probe_id, stat]:
            self.sources[row, col].data = dict(x=[], y=[])
            self.sources[row, col + 1].data = dict(x=[], y=[])
            return

        collection_id = int(collection_id)
        probe_id = int(probe_id)
        probe_name = self.collection_data.rd.probes.loc[probe_id,
                                                        'description']
        self.collection_data = get_collection_data(self.file_name,
                                                   collection_id)
        if stat == 'ter':
            df = self.collection_data.rd.ters
        else:
            df = self.collection_data.data_frame
        df = df[df['probe_id'] == probe_id]
        title = probe_name + ' ' + stat
        f = self.figures[row, col]
        f.xaxis.axis_label = 'timestamp'
        if stat == 'ter':
            f.yaxis.axis_label = probe_name + ' ' + stat
        else:
            f.yaxis.axis_label = probe_name + ' ' + stat + ' (log2 usec)'
        tags = self.collection_data.rd.tags
        git_hash = self.collection_data.rd.collections.loc[collection_id, 'git_commit']
        f.title.text = title + ': ' + ','.join(tags['tag']) + f' ({git_hash})'
        if stat == 'ter':
            hist = np.trim_zeros(
                self.collection_data.global_ter_histogram[probe_id], 'f')
            num_leading_zeros = len(
                self.collection_data.global_ter_histogram[probe_id]) - len(hist)
        else:
            hist = np.trim_zeros(self.collection_data.global_histogram[probe_id],
                                 'f')
            num_leading_zeros = len(
                self.collection_data.global_histogram[probe_id]) - len(hist)
        hist = np.trim_zeros(hist, 'b')

        self.sources[row, col].data = dict(x=df['timestamp'], y=df[stat])
        if stat == 'ter':
            self.sources[row, col + 1].data = dict(
                y=hist,
                x=[
                    i + num_leading_zeros + self.collection_data.min_ter
                    for i in range(len(hist))
                ])
        else:
            self.sources[row, col + 1].data = dict(
                y=hist, x=[i + num_leading_zeros for i in range(len(hist))])

    def _update_db(self):
        self._update_db_file(self.db_file_control.value)

    def timings_plots(self):
        rows = []
        for r in range(self.grid_dims[0]):
            for c in range(self.grid_dims[1] // 2):
                rows.append(
                    row(
                        widgetbox(*self.grid_controls[r, 2 * c]),
                        self.figures[r, 2 * c], self.figures[r, 2 * c + 1]))
                rows.append(Spacer(height=10))
        l = column(*rows)
        self.doc.add_root(l)
        self.doc.title = "Rippled eBPF Probes"


def main():
    # for debugging
    from tornado.ioloop import IOLoop

    from bokeh.application.handlers import FunctionHandler
    from bokeh.application import Application
    from bokeh.server.server import Server

    io_loop = IOLoop.current()

    sr = None

    def modify_doc(doc):
        global sr
        sr = BokehServerReport(doc)

    bokeh_app = Application(FunctionHandler(modify_doc))

    server = Server({'/': bokeh_app}, io_loop=io_loop)
    server.start()

    print('Opening Bokeh application on http://localhost:5006/')
    io_loop.add_callback(server.show, "/")
    io_loop.start()


if __name__ == '__main__':
    main()
else:
    sr = BokehServerReport()
