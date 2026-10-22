#!/usr/bin/env -S uv run --script
"""Recompute gate and job-wall cohort statistics from the accompanying archive."""
import base64
import collections
import datetime
import gzip
import hashlib
import json
import math
import statistics
import sys
from pathlib import Path

payload = json.loads(gzip.decompress(Path(sys.argv[1]).read_bytes()))
records = {}
for item in payload['json_records']:
    raw = base64.b64decode(item['base64'], validate=True)
    assert hashlib.sha256(raw).hexdigest() == item['sha256']
    assert len(raw) == item['bytes']
    records[item['path']] = json.loads(raw)

def timestamp(value):
    return datetime.datetime.fromisoformat(value.replace('Z', '+00:00')) if value and not value.startswith('0001-') else None

def elapsed(start, end):
    a, b = timestamp(start), timestamp(end)
    return (b-a).total_seconds() if a and b and b >= a else None

runs = [run for page in records['day-pull-runs.json'] for run in page['workflow_runs']
        if run['event'] in ('pull_request', 'merge_group')]
summary = {}
for event in ('pull_request', 'merge_group'):
    cohort = [run for run in runs if run['event'] == event]
    gates = []
    successful_workflow_gates = []
    wall = 0
    unknown = 0
    outcomes = collections.Counter()
    for original in cohort:
        run = records.get(f"day-run-rechecks/{original['id']}.json", original)
        outcomes[run['conclusion'] or run['status']] += 1
        jobs = [job for page in records[f"day-jobs/{run['id']}.json"] for job in page['jobs']]
        assert len({job['id'] for job in jobs}) == len(jobs)
        for job in jobs:
            value = elapsed(job.get('started_at'), job.get('completed_at'))
            if job['conclusion'] != 'skipped':
                if value is None:
                    unknown += 1
                else:
                    wall += value
        candidates = [job for job in jobs if job['name'] == 'CI Gate ✅']
        latest = max(candidates, key=lambda job: (job.get('run_attempt', 0), job['id'])) if candidates else None
        if latest and latest['conclusion'] == 'success':
            value = elapsed(run['created_at'], latest.get('completed_at'))
            if value is not None:
                gates.append(value)
                if run['conclusion'] == 'success':
                    successful_workflow_gates.append(value)
    def quantiles(values):
        values = sorted(values)
        return {'n': len(values), 'median_seconds': statistics.median(values),
                'p95_nearest_rank_seconds': values[math.ceil(.95*len(values))-1]}
    summary[event] = {'runs': len(cohort), 'observed_run_outcomes': dict(outcomes),
                      'successful_latest_gate': quantiles(gates),
                      'observed_successful_workflow_and_latest_gate': quantiles(successful_workflow_gates),
                      'known_all_attempt_job_wall_seconds': wall, 'unknown_job_duration_n': unknown}
expected = records['day-cohort-analysis.json']['summary']
for event, result in summary.items():
    for field in ('runs', 'observed_run_outcomes', 'observed_successful_workflow_and_latest_gate',
                  'known_all_attempt_job_wall_seconds', 'unknown_job_duration_n'):
        assert result[field] == expected[event][field], (event, field)
    assert result['successful_latest_gate']['n'] == expected[event]['successful_latest_gate_n']
    assert result['successful_latest_gate']['median_seconds'] == expected[event]['creation_to_successful_latest_gate_seconds']['median']
    assert result['successful_latest_gate']['p95_nearest_rank_seconds'] == expected[event]['creation_to_successful_latest_gate_seconds']['p95_nearest_rank']
print(json.dumps(summary, indent=2))
