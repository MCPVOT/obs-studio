#!/usr/bin/env -S uv run --script
# /// script
# dependencies = ["matplotlib==3.11.2"]
# ///
import argparse
from pathlib import Path
import json
import statistics
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

parser=argparse.ArgumentParser(description='Plot the recorded isolated libobs build trials.')
parser.add_argument('--preview',type=Path,help='Optional PNG preview path')
args=parser.parse_args()
root=Path(__file__).resolve().parent
data={name:json.loads((root/f'{name}.json').read_text()) for name in ('before-cold','current-cold','before-cached','current-cached')}
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10,'svg.fonttype':'none','svg.hashsalt':'obs-libobs-2026-10-17','axes.spines.top':False,'axes.spines.right':False})
fig,axes=plt.subplots(1,3,figsize=(12,5.5))
colors=['#64748b','#0d9488']
panels=[('Uncached CPU','cpu_seconds','before-cold','current-cold',False,84),('Uncached wall time','wall_seconds','before-cold','current-cold',False,12),('Warm cache wall time','wall_seconds','before-cached','current-cached',True,2)]
for ax,(title,metric,before,after,warm,top) in zip(axes,panels):
    for x,name in enumerate((before,after)):
        runs=data[name]['runs'][1:] if warm else data[name]['runs']
        values=[r[metric] for r in runs]
        assert len(values)==3 and all(r['returncode']==0 for r in runs)
        median=statistics.median(values)
        ax.bar(x,median,color=colors[x],width=.56,alpha=.88,zorder=2)
        ax.errorbar(x,median,yerr=[[median-min(values)],[max(values)-median]],fmt='none',color='#172033',capsize=5,lw=1.3,zorder=4)
        ax.scatter([x-.035,x,x+.035],values,s=22,color='#172033',zorder=5)
        ax.text(x,max(values)+top*.035,f'{median:.3f} s',ha='center',va='bottom',fontweight='bold',fontsize=11)
    ax.set_title(title,fontweight='bold',pad=14)
    ax.set_xticks([0,1],['Before','Grouped'])
    ax.set_ylim(0,top)
    ax.set_ylabel('Seconds')
    ax.grid(axis='y',alpha=.18,zorder=0)
    if metric=='cpu_seconds':
        ax.text(.5,.92,'Median CPU: 26.6% lower',transform=ax.transAxes,ha='center',color='#0f766e',fontsize=10)
    elif not warm:
        ax.text(.5,.92,'Observed ranges overlap',transform=ax.transAxes,ha='center',color='#475569',fontsize=10)
fig.suptitle('Isolated libobs build trials',x=.06,ha='left',fontsize=20,fontweight='bold',y=.96)
fig.text(.06,.875,'GCC 15.2 · RelWithDebInfo · -j8 · three measured trials per condition',fontsize=11,color='#475569')
fig.subplots_adjust(left=.065,right=.975,top=.76,bottom=.26,wspace=.35)
fig.text(.06,.14,'Bars: medians   •   Dots: individual trials   •   Whiskers: min–max range',fontsize=10,color='#334155')
fig.text(.06,.09,'Before: be611227   |   Grouped cold: d145635   |   Grouped warm: 93d8609 (initial seed build excluded)',fontsize=9,color='#475569')
fig.text(.06,.045,'Revisions and observed host loads differ. Attribution to unity/PCH alone and queue latency gains remain unproven.',fontsize=9,color='#475569')
if args.preview:
    fig.savefig(args.preview,dpi=150,facecolor='white')
fig.savefig(root/'build-trials.svg',facecolor='white',metadata={'Date':None,'Title':'Isolated libobs build trials','Description':'Three-trial medians, raw observations and min-max ranges; scoped measurements, not a queue latency benchmark.'})
print('matplotlib',matplotlib.__version__)
