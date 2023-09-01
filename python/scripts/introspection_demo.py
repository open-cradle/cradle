"""
Introspection demo. Example usage:

Terminal A; C++ development branch, root directory:
    Start WebSocket server, e.g.
    $ ./build-debug/websocket_server
Terminal B; directory "python", in Python virtual environment:
    $ python3 scripts/introspection_demo.py [-a]
    and repeat to see what's happening
Terminal C; same setup as terminal B:
    Issue several requests simulating an internal calculation that will take ±30 seconds:
    $ pytest tests/manual/multiple_internal_calcs.py [-s]
    Alternatively, post a calculation to Thinknode:
    $ pytest tests/integration/test_post_calculation.py [-s]
"""
import argparse
import datetime
from typing import Any, List, Optional

import cradle


JsonData = Any


def create_datetime(json: JsonData) -> datetime.datetime:
    return datetime.datetime.fromtimestamp(json / 1000.0)


class TaskletEvent:
    name: str
    when: datetime.datetime
    details: str

    def __init__(self, json: JsonData):
        self.name = json['what']
        self.when = create_datetime(json['when'])
        self.details = json['details']


def format_abs_time(when: datetime.datetime) -> str:
    millis = when.microsecond // 1000
    return when.strftime('%H:%M:%S.') + '{0:03d}'.format(millis)


def format_rel_time(when: datetime.datetime, base: datetime.datetime) -> str:
    delta = max((when - base).total_seconds(), 0.0)
    hours = int(delta / 3600)
    minutes = int((delta % 3600) / 60)
    seconds = int(delta % 60)
    millis = int((delta % 1) * 1000)
    if hours > 0:
        text = '{0:d}:{1:02d}:{2:02d}'.format(hours, minutes, seconds)
    elif minutes > 0:
        text = '{0:d}:{1:02d}'.format(minutes, seconds)
    else:
        text = '{0:d}'.format(seconds)
    return text + '.{0:03d}'.format(millis)


class Tasklet:
    own_id: int
    client_id: Optional[int]
    pool_name: str
    description: str
    events: List[TaskletEvent]

    def __init__(self, json: JsonData):
        self.own_id = json['tasklet_id']
        self.client_id = json.get('client_id')
        self.pool_name = json['pool_name']
        self.description = json['description']
        self.events = [TaskletEvent(e) for e in json['events']]

    def dump(self, now: datetime.datetime, with_pool_name: bool, indent: str) -> None:
        if not self.events:
            print('No events?!')
            return
        finished = self.events[-1].name == 'finished'
        start_time = self.events[0].when
        cur_time = self.events[-1].when if finished else now
        text = f'{indent}Task {self.own_id}'
        if with_pool_name:
            text += f' on pool {self.pool_name}'
        text += f' [{self.description}'
        if self.client_id is not None:
            if self.client_id >= 0:
                text += f' on behalf of {self.client_id}'
            else:
                text += f' on behalf of LOCAL {-self.client_id}'
        text += ']'
        for event in self.events:
            if event.name == 'scheduled':
                when_text = format_abs_time(event.when)
                event_text = f' {when_text} {event.name}'
            else:
                when_text = format_rel_time(event.when, start_time)
                event_text = f'; +{when_text} {event.name}'
                if event.name in ('running', 'before co_await'):
                    marker = 'took' if finished else 'taking'
                    into_text = format_rel_time(cur_time, event.when)
                    event_text += f' ({marker} {into_text})'
            if event.details:
                event_text += f' {event.details}'
            text += event_text
        print(text)

    def get_event(self, event_name: str) -> Optional[TaskletEvent]:
        for event in self.events:
            if event.name == event_name:
                return event
        return None


# A machine is local (possibly RPC client) or an RPC server
class MachineSnapshot:
    machine_name: str
    tasklets: List[Tasklet]

    def __init__(self, machine_name: str):
        self.machine_name = machine_name
        self.tasklets = []


class Snapshot:
    machines: List[MachineSnapshot]

    def __init__(self, json: JsonData):
        self.json = json
        self.now = create_datetime(json['now'])
        self.create_machines()

    def create_machines(self) -> None:
        self.machines = []
        for machine_json in self.json['machines']:
            machine_name = machine_json['machine_name']
            machine = MachineSnapshot(machine_name)
            self.machines.append(machine)
            for tasklet_json in machine_json['tasklets']:
                tasklet = Tasklet(tasklet_json)
                machine.tasklets.append(tasklet)

    def dump(self, include_tasklets: bool = False) -> None:
        for machine in self.machines:
            print(f'== Snapshot for {machine.machine_name.upper()}')
            pool_names = {t.pool_name for t in machine.tasklets}
            for pool_name in sorted(pool_names):
                self.dump_pool_oneliner(machine, pool_name)
                if include_tasklets:
                    self.dump_tasklets_for_pool(machine, pool_name)

    def dump_pool_oneliner(self, machine: MachineSnapshot, pool_name: str) -> None:
        tasklet_ids = [t.own_id for t in machine.tasklets if t.pool_name == pool_name]
        text = f'Pool {pool_name} has {len(tasklet_ids)} task(s):'
        for tid in tasklet_ids:
            text += f' {tid}'
        print(text)

    def dump_tasklets_for_pool(self, machine: MachineSnapshot, pool_name: str) -> None:
        for tasklet in machine.tasklets:
            if tasklet.pool_name == pool_name:
                tasklet.dump(self.now, False, '    ')


class Demo:
    def __init__(self, include_finished: bool = False):
        self.session = cradle.Session()
        self.include_finished = include_finished

    def set_enabled(self, enabled: bool) -> None:
        self.session.introspection_set_enabled(enabled)

    def clear_admin(self) -> None:
        self.session.introspection_clear_admin()

    def filter(self, filtering: bool) -> None:
        self.include_finished = not filtering

    def snapshot(self) -> Snapshot:
        json = self.session.query_introspection_status(self.include_finished)
        return Snapshot(json)


if __name__ == '__main__':
    parser = argparse.ArgumentParser('Introspection demo')
    parser.add_argument('-a', '--include-finished',
                        action='store_true', dest='include_finished', default=False,
                        help='include finished tasklets')
    parser.add_argument('-c', '--clear',
                        action='store_true', dest='clear', default=False,
                        help='clear tasklets administration')
    parser.add_argument('--off',
                        action='store_true', dest='off', default=False,
                        help='turn introspection off')
    args = parser.parse_args()

    demo = Demo(args.include_finished)
    demo.set_enabled(not args.off)
    if args.clear:
        demo.clear_admin()
    else:
        snapshot = demo.snapshot()
        snapshot.dump(True)
