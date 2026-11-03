"""Independent evaluation through the public CLI, including isolated Python."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import uuid
import importlib.util
import contextlib
import io
from unittest.mock import patch

SCRIPT = Path(__file__).with_name('trainer.py')
spec = importlib.util.spec_from_file_location('fixture_source', SCRIPT.with_name('test_trainer.py'))
source = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source)
KEY = source.KEYS[0]


class EvaluationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        cls.bundle = source.fixture()
        cls.job = cls.root/'trained'
        cls.write('train.json', cls.bundle)
        r = cls.cli('train', '--bundle', cls.root/'train.json', '--output', cls.job,
                    '--epochs', 2, '--cancel-file', cls.root/'cancel', '--test-only')
        if r.returncode: raise AssertionError(r.stdout+r.stderr)

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    @classmethod
    def write(cls, name, value):
        p = cls.root/name
        p.write_text(json.dumps(value), encoding='utf8')
        return p

    @classmethod
    def cli(cls, *args):
        return subprocess.run([sys.executable, '-I', '-B', str(SCRIPT), *map(str,args)],
                              capture_output=True, text=True, timeout=60)

    def independent(self):
        b = copy.deepcopy(self.bundle)
        b['rows'] = b['rows'][2:4]
        for i,r in enumerate(b['rows']):
            r.update(id=f'test-{i}', productId=f' Independent {i} ', productGroupId=f'independent {i}',purpose='independent_test')
            r['measured'][KEY] = 100 if i == 0 else 104
            r['residual'][KEY] = 0 if i == 0 else 4
        return b

    def evaluate(self, b, model=None, extra=()):
        out = self.root/str(uuid.uuid4())
        path = self.write('evaluation-bundle.json', b)
        r = self.cli('evaluate', '--bundle', path, '--models', model or self.job/'model.json',
                     '--output', out, '--test-only', *extra)
        self.assertEqual(r.returncode, 0, r.stdout+r.stderr)
        return json.loads((out/'evaluation.json').read_text()), out

    def test_full_denominator_hashes_and_partial_coverage(self):
        b = self.independent()
        b['rows'][1]['input']['capacity_kVA'] = 999
        report, out = self.evaluate(b)
        self.assertEqual(report['format'], 'ZTBLD-IndependentEvaluation')
        self.assertTrue(report['testOnly'])
        m = report['models'][0]
        self.assertFalse(m['eligible'])
        for prefix, path in [('model', self.job/'model.json'), ('report', self.job/'report.json')]:
            self.assertEqual(m[prefix+'Path'], str(path.resolve()))
            self.assertEqual(m[prefix+'Sha256'], hashlib.sha256(path.read_bytes()).hexdigest())
        self.assertEqual(report['bundleSha256'], hashlib.sha256(Path(report['bundlePath']).read_bytes()).hexdigest())
        t = m['targets'][KEY]
        self.assertEqual((t['labelledSamples'],t['predictedSamples'],t['independentProducts'],t['coverage']), (2,1,2,.5))
        self.assertEqual(t['baseline']['MAE'],2.0)
        self.assertAlmostEqual(t['baseline']['RMSE'],8**.5)
        self.assertEqual(t['baseline']['maxAbsoluteError'],4.0)
        self.assertEqual(len(t['failures']), 1)
        self.assertIsNone(m['targets'][source.KEYS[1]]['model'])
        r = self.cli('evaluate','--bundle',report['bundlePath'],'--models',m['modelPath'],'--output',out,'--test-only')
        self.assertEqual(r.returncode,1)

    def test_leakage_in_any_target_and_missing_provenance(self):
        for mode in ['trainGroups','validationGroups','missing']:
            m = json.loads((self.job/'model.json').read_text())
            if mode == 'missing': m['models'][KEY].pop('trainGroups')
            else: m['models'][KEY][mode].append('independent 0')
            report,_ = self.evaluate(self.independent(), self.write('trained/variant.json',m))
            entry=report['models'][0]
            self.assertFalse(entry['eligible'])
            self.assertTrue(entry['failures'])
            self.assertEqual(entry['targets'][KEY]['predictedSamples'],0)

    def test_missing_target_context_and_cancel(self):
        b=self.independent()
        for row in b['rows']:
            row['measured'][source.KEYS[1]]=103; row['residual'][source.KEYS[1]]=3
            row['input']['coreShape']=3
        report,_=self.evaluate(b)
        for key in source.KEYS[:2]:
            t=report['models'][0]['targets'][key]
            self.assertEqual((t['labelledSamples'],t['predictedSamples'],len(t['failures'])),(2,0,2))
        flag=self.root/'stop'; flag.touch()
        out=self.root/str(uuid.uuid4())
        r=self.cli('evaluate','--bundle',self.root/'evaluation-bundle.json','--models',self.job/'model.json','--output',out,'--test-only','--cancel-file',flag)
        self.assertEqual(r.returncode,2,r.stdout)
        self.assertFalse(out.exists())

    def test_zero_baseline_multiple_models_and_model_test_flag(self):
        b=self.independent(); b['testOnly']=False
        for row in b['rows']:
            row['source']='laboratory'; row['measured'][KEY]=100; row['residual'][KEY]=0
        out=self.root/str(uuid.uuid4())
        r=self.cli('evaluate','--bundle',self.write('real-test.json',b),'--models',
                   self.job/'model.json',self.job/'model.json','--output',out)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        report=json.loads((out/'evaluation.json').read_text())
        self.assertTrue(report['testOnly'])
        self.assertEqual(len(report['models']),2)
        for m in report['models']:
            self.assertTrue(m['testOnly']); self.assertFalse(m['eligible'])
            self.assertEqual(m['targets'][KEY]['baseline'],dict(MAE=0,RMSE=0,maxAbsoluteError=0))

    def test_cross_target_leakage_blocks_all_predictions(self):
        m=json.loads((self.job/'model.json').read_text())
        m['models'][source.KEYS[1]]=copy.deepcopy(m['models'][KEY])
        m['models'][source.KEYS[1]]['validationGroups']=['independent 0']
        sibling=json.loads((self.job/'report.json').read_text())
        sibling['validation'][source.KEYS[1]]=m['models'][KEY]['validation']
        folder=self.root/str(uuid.uuid4()); folder.mkdir()
        path=self.write(str(folder/'model.json'),m)
        self.write(str(folder/'report.json'),sibling)
        report,_=self.evaluate(self.independent(),path)
        entry=report['models'][0]
        self.assertIn('leakage',entry['failures'][0])
        self.assertEqual(entry['targets'][KEY]['predictedSamples'],0)

    def mechanical_fixture(self):
        # NOT production proof: temporary copies of synthetic trained weights,
        # with test flags cleared ONLY to exercise the independent eligibility
        # gates. No production model is trained, exported, activated or retained.
        folder=self.root/str(uuid.uuid4()); folder.mkdir()
        m=json.loads((self.job/'model.json').read_text())
        sibling=json.loads((self.job/'report.json').read_text())
        m['testOnly']=sibling['testOnly']=False
        sibling['modelPath']=str(folder/'model.json')
        b=self.independent(); b['testOnly']=False
        for row in b['rows']: row['source']='temporary mechanical fixture'
        return folder,m,sibling,b

    def run_mechanical(self,folder,m,sibling,b):
        model=self.write(str(folder/'model.json'),m)
        report_path=self.write(str(folder/'report.json'),sibling)
        bundle=self.write(str(folder/'bundle.json'),b)
        out=folder/str(uuid.uuid4())
        # Deliberately omit --test-only; otherwise eligible=False is masked.
        r=self.cli('evaluate','--bundle',bundle,'--models',model,'--output',out)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        report=json.loads((out/'evaluation.json').read_text())
        entry=report['models'][0]
        self.assertFalse(report['testOnly']); self.assertFalse(entry['testOnly'])
        for key,path in [('model',model),('report',report_path)]:
            self.assertEqual(entry[key+'Sha256'],hashlib.sha256(path.read_bytes()).hexdigest())
            self.assertEqual(entry[key+'Path'],str(path.resolve()))
        self.assertEqual(report['bundleSha256'],hashlib.sha256(bundle.read_bytes()).hexdigest())
        return entry

    def test_mechanical_nominal_evidence_is_eligible_without_test_flag(self):
        entry=self.run_mechanical(*self.mechanical_fixture())
        self.assertTrue(entry['eligible'])
        self.assertEqual(entry['failures'],[])
        target=entry['targets'][KEY]
        self.assertEqual((target['labelledSamples'],target['predictedSamples'],target['coverage']),(2,2,1))
        self.assertEqual(target['failures'],[])

    def test_mechanical_leaks_and_missing_provenance_reject_without_test_flag(self):
        for mode in ['trainGroups','validationGroups','cross-target','missing']:
            with self.subTest(mode=mode):
                folder,m,sibling,b=self.mechanical_fixture()
                if mode=='missing': m['models'][KEY].pop('trainGroups')
                else:
                    key=KEY
                    field=mode
                    if mode=='cross-target':
                        key=source.KEYS[1]; field='validationGroups'
                        m['models'][key]=copy.deepcopy(m['models'][KEY])
                        sibling['validation'][key]=m['models'][key]['validation']
                    # Mixed case/whitespace productId still canonicalizes to this.
                    m['models'][key][field].append('independent 0')
                entry=self.run_mechanical(folder,m,sibling,b)
                self.assertFalse(entry['eligible'])
                self.assertIn('provenance' if mode=='missing' else 'leakage',entry['failures'][0])
                self.assertEqual(entry['targets'][KEY]['predictedSamples'],0)
                self.assertEqual(len(entry['targets'][KEY]['failures']),2)

    def test_mechanical_partial_bounds_and_missing_target_reject_without_test_flag(self):
        for mode in ['bounds','missing-target']:
            with self.subTest(mode=mode):
                folder,m,sibling,b=self.mechanical_fixture()
                if mode=='bounds': b['rows'][1]['input']['capacity_kVA']=999
                else:
                    for row in b['rows']:
                        row['measured'][source.KEYS[1]]=103
                        row['residual'][source.KEYS[1]]=3
                entry=self.run_mechanical(folder,m,sibling,b)
                self.assertFalse(entry['eligible'])
                self.assertEqual(entry['failures'],[])
                target=entry['targets'][KEY if mode=='bounds' else source.KEYS[1]]
                self.assertEqual(target['labelledSamples'],2)
                self.assertEqual(target['predictedSamples'],1 if mode=='bounds' else 0)
                self.assertEqual(target['coverage'],.5 if mode=='bounds' else 0)
                self.assertEqual(len(target['failures']),1 if mode=='bounds' else 2)
                self.assertIn('bounds' if mode=='bounds' else 'Missing model target',target['failures'][0])

    def test_cancellation_after_inference_and_before_publication(self):
        spec=importlib.util.spec_from_file_location('evaluation_cancel_backend',SCRIPT)
        backend=importlib.util.module_from_spec(spec); spec.loader.exec_module(backend)
        for phase in ['after-first-inference','staged-report']:
            with self.subTest(phase=phase):
                folder,m,sibling,b=self.mechanical_fixture()
                model=self.write(str(folder/'model.json'),m)
                self.write(str(folder/'report.json'),sibling)
                bundle=self.write(str(folder/'bundle.json'),b)
                out=folder/str(uuid.uuid4()); flag=folder/'cancel'
                original_check=backend.check_cancel
                calls=0; observed=[]
                def cancel_during_evaluation(path):
                    nonlocal calls
                    calls+=1
                    staged=list(folder.glob('.evaluation-*/evaluation.json'))
                    # Checks 1/2/3 precede loading/model/first sample; check 4
                    # occurs after the first real inference, before sample 2.
                    if (phase=='after-first-inference' and calls==4) or (phase=='staged-report' and staged):
                        observed.append(calls)
                        flag.touch()
                    original_check(path)
                output=io.StringIO()
                with patch.object(backend,'check_cancel',side_effect=cancel_during_evaluation), contextlib.redirect_stdout(output):
                    code=backend.main(['evaluate','--bundle',str(bundle),'--models',str(model),
                                       '--output',str(out),'--cancel-file',str(flag)])
                self.assertEqual(code,2,output.getvalue())
                self.assertEqual(len(observed),1)
                events=[json.loads(line) for line in output.getvalue().splitlines()]
                self.assertEqual(events[-1]['event'],'cancelled')
                self.assertFalse(any(e.get('percent')==100 for e in events))
                if phase=='staged-report':
                    self.assertTrue(any(e['event']=='progress' for e in events))
                self.assertFalse(out.exists())
                self.assertEqual(list(folder.glob('.evaluation-*')),[])


if __name__ == '__main__': unittest.main()
