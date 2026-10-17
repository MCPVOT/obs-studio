"""Per-lane evidence must separate source cohorts and retain missing data."""
import unittest
from cache_metrics import lane_summaries

class CacheLaneCohortsTest(unittest.TestCase):
    def record(self, cohort, duration, step_conclusion='success'):
        return {'event':'pull_request','cohort':cohort,'conclusion':'success','jobs':[
            {'name':'Fast Gate','conclusion':'success','started_at':'2026-10-17T00:00:00Z',
             'completed_at':f'2026-10-17T00:00:{duration:02}Z','steps':[
                 {'name':'Set Up soldr','conclusion':step_conclusion,'started_at':'2026-10-17T00:00:00Z','completed_at':'2026-10-17T00:00:03Z'}]}]}

    def test_docs_and_code_do_not_share_a_lane_median(self):
        result = lane_summaries([self.record('code', 30), self.record('docs', 5)])
        self.assertEqual(result['pull_request/code/Fast Gate']['median_job_seconds'], 30)
        self.assertEqual(result['pull_request/docs/Fast Gate']['median_job_seconds'], 5)
        self.assertEqual(result['pull_request/code/Fast Gate']['steps']['Set Up soldr']['median_seconds'], 3)

    def test_skipped_and_missing_step_times_do_not_become_zero_samples(self):
        missing=self.record('code', 30)
        missing['jobs'][0]['steps'][0]['completed_at']=None
        skipped=self.record('code', 30, 'skipped')
        result=lane_summaries([missing,skipped])['pull_request/code/Fast Gate']
        self.assertEqual(result['successful_job_samples'], 2)
        self.assertEqual(result['steps'], {})

    def test_successful_jobs_from_canceled_runs_are_disclosed(self):
        record=self.record('code', 30)
        record['conclusion']='cancelled'
        result=lane_summaries([record])['pull_request/code/Fast Gate']
        self.assertEqual(result['parent_run_outcomes'], {'cancelled':1})

if __name__ == '__main__':
    unittest.main()
