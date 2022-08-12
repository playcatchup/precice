#!python

import json
import sys

def tracesFor(pid, tid, events):
    # We currently interpret pause and result as stop and start
    phaseMap = {
        "b": "B",
        "p": "E",
        "r": "B",
        "e": "E"
    }

    return [
        {
            "name": e["en"],
            "cat": "Solver" if e["en"].startswith("solver") else "preCICE",
            "ph" : phaseMap[e["et"]],
            "pid" : pid,
            "tid" :tid,
            "ts" : e["ts"],
        }
        for e in events
        if e["et"] != "d" # we currently ignore the data events
    ]

def eventsToTraces(filenames):
    pids = {}

    allmeta = []
    alltraces = []

    for filename in filenames:
        print(f"Processing {filename}")
        with open(filename, 'r') as openfile:
            content = json.load(openfile)

        meta = content["meta"]
        name, rank, size = meta["name"], int(meta["rank"]), int(meta["size"])

        # Give new participants a unique id
        if name not in pids:
            pids[name] = len(pids)
        pid = pids[name]

        print(f"  participant {name} ({pid}) rank {rank}")

        rankName = "Primary" if rank == 0 else "Secondary"

        metaEvents = [
            {"name": "process_name", "ph": "M", "pid": pid, "tid": rank, "args": {"name" : name} },
            {"name": "thread_name", "ph": "M", "pid": pid, "tid": rank, "args": {"name" : rankName } }
        ]

        allmeta += metaEvents
        alltraces += tracesFor(pid, rank, content["events"])

    return {
        "traceEvents": allmeta + alltraces
    }

def traceCommand(filenames, outfile):
    trace = eventsToTraces(filenames)
    json_object = json.dumps(trace, indent=2)
    print(f"Writing to {outfile}")
    with open(outfile, "w") as outfile:
        outfile.write(json_object)

if __name__ == '__main__':
    traceCommand(sys.argv[1:], "trace.json")
