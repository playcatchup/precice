#!python

import json
import sys





#{
#  "traceEvents": [
#    {"name": "Asub", "cat": "PERF", "ph": "B", "pid": 22630, "tid": 22630, "ts": 829},
#    {"name": "Asub", "cat": "PERF", "ph": "E", "pid": 22630, "tid": 22630, "ts": 833}
#  ],
#  "displayTimeUnit": "ns",
#  "systemTraceEvents": "SystemTraceData",
#  "otherData": {
#    "version": "My Application v1.0"
#  },
#  "stackFrames": {...}
#  "samples": [...],
#}



def totrace(filename):
    with open(filename, 'r') as openfile:
        content = json.load(openfile)

    meta = content["meta"]

    phaseMap = {
        "b": "B",
        "p": "E",
        "r": "B",
        "e": "E"
    }

    pid = 0
    tid = 0
    cat = "preCICE"
    name, rank = meta["name"], meta["rank"]
    events = [
        {
            "name": e["en"],
            "cat": cat,
            "ph" : phaseMap[e["et"]],
            "pid" : pid,
            "tid" :tid,
            "ts" : e["ts"],
        }
        for e in content["events"]
        if e["et"] != "d"
    ]

    metaEvents = [
        {"name": "process_name", "ph": "M", "pid": pid, "tid": tid, "args": {"name" : name} },
        {"name": "thread_name", "ph": "M", "pid": pid, "tid": tid, "args": {"name" : f"Rank {tid}"} }
    ]

    return {
        "traceEvents": metaEvents + events,
        "displayTimeUnit": "ms"
    }


if __name__ == '__main__':
    trace = totrace("precice-Fluid-0-1.json")
    json_object = json.dumps(trace, indent=2)
    with open("trace.json", "w") as outfile:
        outfile.write(json_object)

