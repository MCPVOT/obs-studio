#!/usr/bin/env -S uv run --script
"""Local ccache mechanism probe, not a hosted Qt performance comparison."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

parser=argparse.ArgumentParser(description='Exercise the actual Qt cache scripts with two local C objects.')
parser.add_argument('--output',type=Path,required=True)
root=parser.parse_args().output.resolve()
root.mkdir()
cache=root/'cache'
cache.mkdir()
env={**os.environ,'CCACHE_DIR':str(cache),'CCACHE_CONFIGPATH':str(cache/'ccache.conf'),'CCACHE_MAXSIZE':'1200M'}
workflow=Path('.github/workflows/build-project.yaml').read_text()
job=workflow.split('  linux-qt-frontend:\n',1)[1].split('\n  rust-tier2:',1)[0]
def script(name):
 step=job.split('- name: '+name,1)[1].split('\n      - ',1)[0]
 return '\n'.join(line[10:] for line in step.split('        run: |\n',1)[1].splitlines())
def run(args):
 return subprocess.run(args,env=env,check=True,capture_output=True,text=True).stdout
def compile_file(name):
 run(['ccache','cc','-c',str(root/(name+'.c')),'-o',str(root/(name+'.o'))])
for name in ('old','used'):
 (root/(name+'.c')).write_text('int '+name+'(void) { return 7; }\n')
 compile_file(name)
for path in cache.rglob('*'):
 if path.is_file():os.utime(path,(time.time()-3600,)*2)
output=root/'outputs'
output.touch()
env['GITHUB_OUTPUT']=str(output)
run(['bash','-euo','pipefail','-c',script('Set Qt Cache Limit')])
compile_file('used')
before=json.loads(run(['ccache','--print-stats','--format=json']))
assert before['direct_cache_hit']==1 and before['cache_miss']==0
before_files=sorted(str(p.relative_to(cache)) for p in cache.rglob('*') if p.is_file())
env['QT_CACHE_EPOCH']=output.read_text().strip().split('=',1)[1]
log=run(['bash','-euo','pipefail','-c',script('Report Qt Cache')])
after_files=sorted(str(p.relative_to(cache)) for p in cache.rglob('*') if p.is_file())
run(['ccache','--zero-stats'])
compile_file('used')
used=json.loads(run(['ccache','--print-stats','--format=json']))
assert used['direct_cache_hit']==1 and used['cache_miss']==0
compile_file('old')
old=json.loads(run(['ccache','--print-stats','--format=json']))
assert old['direct_cache_hit']==1 and old['cache_miss']==1
result={'ccache_version':run(['ccache','--version']).splitlines()[0], 'compiler':run(['cc','--version']).splitlines()[0], 'workflow_scripts_sha256':{name:hashlib.sha256(script(name).encode()).hexdigest() for name in ('Set Qt Cache Limit','Report Qt Cache')}, 'before_files_count':len(before_files),'after_files_count':len(after_files),'removed_files':sorted(set(before_files)-set(after_files)),'report_log':log,'used_after_prune':used,'old_after_prune':old,'limitations':['Two tiny C objects, local ccache 4.13.6; not the hosted Qt working set or its ccache 4.12.3.','Shows retained warm hit and evicted stale miss using the actual workflow scripts; no CI speedup or archive retention claim.']}
(root/'report.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'ccache_version':result['ccache_version'],'files_before':len(before_files),'files_after':len(after_files),'used_hit':used['direct_cache_hit'],'used_miss':used['cache_miss'],'old_miss':old['cache_miss']}))
