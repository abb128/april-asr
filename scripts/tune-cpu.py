#!/usr/bin/env python3
"""Measure encoder thread settings on the user's actual PC and recording."""
import argparse
import json
import os
import statistics
import subprocess
from pathlib import Path
os.environ['ORT_DISABLE_TELEMETRY'] = '1'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('audio', type=Path)
p.add_argument('model', type=Path)
p.add_argument('--runner', type=Path, default=Path(__file__).resolve().parents[1] / 'build/main')
p.add_argument('--profile', choices=['balanced', 'strict', 'legacy'], default='balanced')
p.add_argument('--repeats', type=int, default=3)
p.add_argument('--output', type=Path)
a = p.parse_args()
if a.repeats < 1:
    p.error('--repeats must be positive')
results = []
for threads in [1, 2, 4, 8]:
    values = []
    for repeat in range(a.repeats + 1):
        run = subprocess.run([str(a.runner.resolve()), str(a.audio), str(a.model), '--profile', a.profile,
                              '--threads', str(threads), '--benchmark'], text=True, capture_output=True, check=True)
        measured = next(json.loads(line) for line in run.stdout.splitlines() if line.startswith('{'))
        if repeat: values.append(measured)
    row = {'threads': threads, 'rtf': statistics.median(x['rtf'] for x in values),
           'feed_p95_ms': statistics.median(x['feed_p95_ms'] for x in values),
           'transcripts_agree': len({x['transcript'] for x in values}) == 1,
           'transcript': values[0]['transcript']}
    results.append(row)
    print(f"{threads} threads: RTF {row['rtf']:.3f}, p95 feed {row['feed_p95_ms']:.2f} ms", flush=True)
best = min(results, key=lambda x: x['rtf'])
print(f"Measured choice: --threads {best['threads']} (RTF below 1 processes faster than real time).")
print('These are processing measurements, not capture-to-caption latency or accuracy scores.')
if a.output:
    a.output.write_text(json.dumps(results, indent=2) + '\n')
