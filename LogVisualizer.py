import matplotlib.pyplot as plt
import numpy as np
import glob
from enum import Enum
import re
import json
from collections import defaultdict
from dataclasses import asdict
import argparse
import random

class SchedulingEventType(Enum):
    JOB_BEGUN = 1
    JOB_FINISHED = 2
    JOB_WAITING = 3
    JOB_RESUMED = 4
    JOB_REMOVED_FROM_WAITLIST = 5
    OTHER = 5

class RawLine:
    worker: int
    timestamp: int
    message: str
    def __init__(self, line: str):
        self.worker = int(line.split(" ")[0][1:-1])
        self.timestamp = int(line.split(" ")[1][1:-1])
        start = line.split(" ")[0] + " " + line.split(" ")[1]
        self.message = line.replace(start, "").strip()
    
class SchedulingEvent:
    type: SchedulingEventType
    fibre: int
    rawLine: RawLine
    name: str # only set if self.type == JOB_BEGUN
    def to_serializable(self) -> dict:
        return {
            "fibre" : self.fibre, 
            "type" : self.type.name, 
            "timestamp" : self.rawLine.timestamp, 
            "worker" : self.rawLine.worker
        } if self.type != SchedulingEventType.JOB_BEGUN else {
            "fibre" : self.fibre, 
            "type" : self.type.name, 
            "timestamp" : self.rawLine.timestamp, 
            "worker" : self.rawLine.worker,
            "name" : self.name
        }
        
    def __repr__(self):
        return f"(Event) worker: {self.rawLine.worker} time: {self.rawLine.timestamp} type: {self.type} fibre: {self.fibre}"
    def __init__(self, rawLine: RawLine):
        self.name = ""
        self.rawLine = rawLine
        if "new job dequeued" in rawLine.message:
            self.type = SchedulingEventType.JOB_BEGUN
            match = re.search(r"\bdequeued\s+([_a-zA-Z][_a-zA-Z0-9]*)\b", rawLine.message)
            self.name = str(match.group(1))
        elif "deallocating" in rawLine.message:
            self.type = SchedulingEventType.JOB_FINISHED
        elif ("from local queue" in rawLine.message) or ("stealing" in rawLine.message):
            self.type = SchedulingEventType.JOB_RESUMED
        elif ("from waitlist" in rawLine.message):
            self.type = SchedulingEventType.JOB_REMOVED_FROM_WAITLIST
        elif "waiting" in rawLine.message:
            self.type = SchedulingEventType.JOB_WAITING
        else:
            print(f"UNKNOWN MESSAGE: '{rawLine.message}'")
            self.type = SchedulingEventType.OTHER
            assert False
        match = re.search(r"fiber\s+(\d+(?:\.\d+)?)", rawLine.message)
        self.fibre = int(match.group(1))

def lines_to_scheduling_events(lines: list[RawLine]) -> list[SchedulingEvent]:
    r: list[SchedulingEvent] = []
    for l in lines:
        if "fiber" in l.message:
            r.append(SchedulingEvent(l))
    return r

def do_args():
    parser = argparse.ArgumentParser(
                    prog='LogVisualizer.py',
                    description='visualize log files for the fiber scheduler',
                    epilog='Jim Marshall - 2026')
    parser.add_argument("--txt_glob", default="../worker_thread_*.txt")
    parser.add_argument("--dump_json", action="store_true")
    args = parser.parse_args()
    return args

fnName = ""

names: dict[str, tuple] = dict()

def random_colour() -> tuple:
    return (random.random(), random.random(), random.random())

def get_job_colour(name: str) -> tuple:
    """
        returns an (r, g, b) tuple for each function name
    """
    global names
    if name in names:
        return names[name]
    names[name] = random_colour()
    return names[name]

def find_run(events: list[SchedulingEvent]) -> tuple:
    """
        returns this tuple: (thread_id, start, duration, label, color)
    """
    global fnName
    e = events.pop(0)
    while e.type != SchedulingEventType.JOB_BEGUN and e.type != SchedulingEventType.JOB_RESUMED:
        e = events.pop(0)
    if e.type == SchedulingEventType.JOB_BEGUN:
        fnName = e.name
        pass
    e2 = events.pop(0)
    while e2.type != SchedulingEventType.JOB_FINISHED and e2.type != SchedulingEventType.JOB_WAITING:
        e2 = events.pop(0)
    return (
        e.rawLine.worker,
        e.rawLine.timestamp / 1e6,
        (e2.rawLine.timestamp - e.rawLine.timestamp) / 1e6,
        f"{fnName} - {e.fibre}",
        get_job_colour(fnName)

    )

def scheduling_events_to_segments(events: list[SchedulingEvent]) -> list[tuple]:
    r = []
    while len(events) > 0:
        r.append(find_run(events))
    return r

def normalize_by_fiber_timestamps(byFiber: dict[int, list[SchedulingEvent]]) -> None:
    def find_lowest_timestamp(byFiber: dict[int, list[SchedulingEvent]]):
        ts = 0xFFFFFFFF
        for k in byFiber.keys():
            if byFiber[k][0].rawLine.timestamp < ts:
                ts = byFiber[k][0].rawLine.timestamp
        return ts
    lowest = find_lowest_timestamp(byFiber)
    for k in byFiber.keys():
        v = byFiber[k]
        for e in v:
            e.rawLine.timestamp = e.rawLine.timestamp - lowest
    pass    

def main():
    args = do_args()
    files = glob.glob(args.txt_glob)
    lines = []
    for fp in files:
        with open(fp, "r") as f:
            lines += f.readlines()

    lines = [x.strip() for x in lines]
    lines = [RawLine(x) for x in lines if x != ""]
    events: list[SchedulingEvent] = lines_to_scheduling_events(lines)

    byFiber: dict[int, list[SchedulingEvent]] = defaultdict(list)
    for e in events:
        byFiber[e.fibre].append(e)
    
    for k in byFiber.keys():
        byFiber[k].sort(key=lambda obj: obj.rawLine.timestamp)

    normalize_by_fiber_timestamps(byFiber)
    if args.dump_json:
        serializable = {
            str(k): [event.to_serializable() for event in v]
            for k, v in byFiber.items()
        }
        print(json.dumps(serializable, indent=4))
        return
    
    segments = []
    for k in byFiber.keys():
        v = byFiber[k]
        segments += scheduling_events_to_segments(v)

    fig, ax = plt.subplots()

    row_height = 8
    gap = 4

    for thread_id, start, duration, label, color in segments:
        y_base = thread_id * (row_height + gap)

        # draw bar
        ax.broken_barh(
            [(start, duration)],
            (y_base, row_height),
            facecolors=color,
            edgecolors="black"
        )

        # label inside each bar
        ax.text(
            start,
            y_base + row_height / 2,
            label,
            ha="left",
            va="center",
            fontsize=9,
            color="white"
        )

    # y-axis labeling (threads)
    ax.set_yticks([
        # row_height / 2,
        # (row_height + gap) + row_height / 2
    ])
    # ax.set_yticklabels(["Thread 0", "Thread 1"])



    plt.show()
    pass

main()
