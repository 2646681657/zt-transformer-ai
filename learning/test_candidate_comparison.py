"""Candidate comparison contract exercised with temporary CPU models only."""
import argparse
import contextlib
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import uuid
from unittest.mock import patch

SCRIPT = Path(__file__).with_name('trainer.py')

def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value

source = module(SCRIPT.with_name('test_trainer.py'), 'comparison_fixtures')
KEYS = source.KEYS

class CandidateComparisonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        cls.bundle = source.context_fixture()
        # Train all six targets, using actual CPU optimization, never shipped data.
        for row in cls.bundle['rows']:
            for key in KEYS:
                row['measured'][key] = 110 + int(row['id'])
                row['residual'][key] = 10 + int(row['id'])
        path = cls.write('train.json', cls.bundle)
        cls.job = cls.root/'trained'
        r = cls.cli('train', '--bundle', path, '--output', cls.job,
                    '--epochs', 2, '--cancel-file', cls.root/'unused', '--test-only')
        if r.returncode: raise AssertionError(r.stdout+r.stderr)

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    @classmethod
    def write(cls, name, value):
        path = cls.root/name
        path.write_text(json.dumps(value, allow_nan=True), encoding='utf8')
        return path

    @classmethod
    def cli(cls, *args):
        return subprocess.run([sys.executable, '-I', '-B', str(SCRIPT), *map(str,args)],
                              capture_output=True, text=True, timeout=60)

    def snapshot(self):
        rows = []
        for order, index in enumerate([3, 2, 4], 1):
            sample = copy.deepcopy(self.bundle['rows'][index])
            for field in ['purpose', 'productGroupId', 'residual']: sample.pop(field)
            sample.update(source='optimization-candidate', measured=dict.fromkeys(KEYS))
            rows.append(dict(candidateId=f'candidate-{index}', schemeId=index+1,
                             originalOrder=order, materialCost=12.5*index,
                             costBasis='  原始成本 / kg\n依据  ', sample=sample))
        return dict(format='ZTBLD-CandidateComparisonInput', version=1,
                    usage='independent-comparison-only', runId=str(uuid.uuid4()), candidates=rows)

    def args(self, data=None, model=None):
        data = self.snapshot() if data is None else data
        path = self.write(str(uuid.uuid4())+'.json', data)
        return argparse.Namespace(model=str(model or self.job/'model.json'), input=str(path),
                                  output=str(self.root/data['runId']), cancel_file=None)

    def run_batch(self, args):
        extra = ['--cancel-file', args.cancel_file] if args.cancel_file else []
        return self.cli('predict-batch', '--model', args.model, '--input', args.input,
                        '--output', args.output, *extra)

    def success(self, args):
        r = self.run_batch(args)
        self.assertEqual(r.returncode, 0, r.stdout+r.stderr)
        return json.loads((Path(args.output)/'comparison.json').read_text(encoding='utf8'))

    def variant(self, change):
        folder = self.root/str(uuid.uuid4()); folder.mkdir()
        m = json.loads((self.job/'model.json').read_text())
        report = json.loads((self.job/'report.json').read_text())
        change(m)
        report['validation'] = {k:t['validation'] for k,t in m['models'].items()}
        self.write(str(folder/'report.json'), report)
        return self.write(str(folder/'model.json'), m)

    def test_cli_complete_exact_contract_order_hashes_and_no_training(self):
        data = self.snapshot(); args = self.args(data)
        before = {p:p.read_bytes() for p in [Path(args.input), self.job/'model.json', self.job/'report.json']}
        report = self.success(args)
        self.assertEqual(set(report), {'format','version','usage','status','runId','inputPath',
            'inputSha256','modelPath','modelSha256','reportSha256','modelVersion','scope',
            'engineVersion','testOnly','productionEligible','candidates','counts'})
        self.assertEqual(report['format'], 'ZTBLD-CandidateComparison')
        self.assertEqual(report['status'], 'completed')
        self.assertEqual(report['version'], 1)
        for field in ['usage','runId']: self.assertEqual(report[field], data[field])
        self.assertTrue(report['testOnly']); self.assertIs(report['productionEligible'], False)
        self.assertEqual(report['counts'], dict(candidates=3, successfulTargets=18, failedTargets=0))
        for prefix, path in [('input',Path(args.input)),('model',self.job/'model.json'),('report',self.job/'report.json')]:
            self.assertEqual(report[prefix+'Sha256'], hashlib.sha256(before[path]).hexdigest())
            self.assertEqual(path.read_bytes(), before[path])
            if prefix != 'report': self.assertEqual(report[prefix+'Path'], str(path.resolve()))
        model = json.loads(before[self.job/'model.json'])
        for key in ['modelVersion','scope','engineVersion']: self.assertEqual(report[key],model[key])
        for original, row in zip(data['candidates'], report['candidates']):
            self.assertEqual(set(row), {'candidateId','schemeId','originalOrder','materialCost','costBasis','baseline','predictions','failures'})
            for key in ['candidateId','schemeId','originalOrder','materialCost','costBasis']: self.assertEqual(row[key],original[key])
            self.assertEqual(row['baseline'],original['sample']['baseline'])
            self.assertEqual(set(row['predictions']),set(KEYS)); self.assertEqual(row['failures'],{})
            self.assertTrue(all(value > 100 for value in row['predictions'].values()))

    def test_partial_missing_bounds_and_entirely_failed_rows_are_retained(self):
        def change(m):
            del m['models'][KEYS[1]]
            m['models'][KEYS[2]]['bounds']['max'][0] = 125
        args = self.args(model=self.variant(change))
        data = json.loads(Path(args.input).read_text())
        data['candidates'][2]['sample']['input']['steelGrade'] = 'incompatible'
        self.write(str(args.input),data)
        report = self.success(args)
        self.assertEqual(report['counts'],dict(candidates=3,successfulTargets=9,failedTargets=9))
        for index, row in enumerate(report['candidates']):
            failed = set(KEYS) if index == 2 else {KEYS[1],KEYS[2]} if index == 0 else {KEYS[1]}
            self.assertEqual(set(row['failures']),failed)
            self.assertEqual({k for k,v in row['predictions'].items() if v is None},failed)
            self.assertTrue(all(isinstance(v,str) and v for v in row['failures'].values()))
        self.assertIn('bounds',report['candidates'][0]['failures'][KEYS[2]])
        self.assertIn('context',report['candidates'][2]['failures'][KEYS[0]])

    def test_invalid_snapshot_is_rejected_without_artifacts(self):
        mutations = [
            lambda d:d.update(format='other'), lambda d:d.update(version=True),
            lambda d:d.update(usage='training'), lambda d:d.update(runId='BAD'),
            lambda d:d.update(runId=d['runId'].upper()), lambda d:d.update(candidates=[]),
            lambda d:d['candidates'][1].update(candidateId=d['candidates'][0]['candidateId']),
            lambda d:d['candidates'][0].update(candidateId='  '),
            lambda d:d['candidates'][0].update(originalOrder=2),
            lambda d:d['candidates'][0].update(originalOrder=True),
            lambda d:d['candidates'][0].update(schemeId=0),
            lambda d:d['candidates'][0].update(schemeId=True),
            lambda d:d['candidates'][0].update(materialCost=-1),
            lambda d:d['candidates'][0].update(materialCost=float('inf')),
            lambda d:d['candidates'][0].update(costBasis=0),
            lambda d:d['candidates'][0]['sample'].update(source='laboratory'),
            lambda d:d['candidates'][0]['sample'].update(scope='other'),
            lambda d:d['candidates'][0]['sample']['baseline'].update({KEYS[0]:0}),
            lambda d:d['candidates'][0]['sample']['baseline'].update({KEYS[3]:-1}),
            lambda d:d['candidates'][0]['sample']['measured'].update({KEYS[0]:101}),
            lambda d:d['candidates'][0]['sample']['input'].pop('lvTurns')]
        for mutate in mutations:
            with self.subTest(mutation=mutations.index(mutate)):
                data=self.snapshot(); mutate(data); args=self.args(data)
                args.output=str(self.root/str(uuid.uuid4()))
                r=self.run_batch(args)
                self.assertEqual(r.returncode,1,r.stdout+r.stderr)
                self.assertEqual(json.loads(r.stdout)['event'],'error')
                self.assertFalse(Path(args.output).exists())

    def test_zero_temperature_allowed_but_zero_losses_and_impedance_fail(self):
        def change(m):
            for t in m['models'].values():
                t['weights']={k:self.zeros(v) for k,v in t['weights'].items()}
                t['normalization']['targetMean']=-100
        report=self.success(self.args(model=self.variant(change)))
        self.assertEqual(report['counts'],dict(candidates=3,successfulTargets=9,failedTargets=9))
        for row in report['candidates']:
            self.assertEqual(set(row['failures']),set(KEYS[:3]))
            for key in KEYS[:3]: self.assertIsNone(row['predictions'][key])
            for key in KEYS[3:]: self.assertEqual(row['predictions'][key],0)

    @staticmethod
    def zeros(value):
        return [CandidateComparisonTests.zeros(v) for v in value] if isinstance(value,list) else 0

    def test_cancel_and_existing_or_non_uuid_output(self):
        args=self.args(); flag=self.root/str(uuid.uuid4()); flag.touch(); args.cancel_file=str(flag)
        self.assertEqual(self.run_batch(args).returncode,2)
        self.assertFalse(Path(args.output).exists())
        args.cancel_file=None; Path(args.output).mkdir(); sentinel=Path(args.output)/'sentinel'; sentinel.write_text('keep')
        self.assertEqual(self.run_batch(args).returncode,1); self.assertEqual(sentinel.read_text(),'keep')
        args.output=str(self.root/'not-a-uuid')
        self.assertEqual(self.run_batch(args).returncode,1); self.assertFalse(Path(args.output).exists())

    def test_mid_loop_cancellation_and_evidence_tampering(self):
        # Inject changes at the inference boundary; run the real inference and publisher.
        for mode in ['cancel','input','model','report']:
            with self.subTest(mode=mode):
                args=self.args(model=self.variant(lambda m:None))
                flag=self.root/str(uuid.uuid4()); args.cancel_file=str(flag)
                backend=module(SCRIPT,'comparison_'+mode)
                original=backend.predict_sample
                changed=False
                def predict(*values,**kwargs):
                    nonlocal changed
                    result=original(*values,**kwargs)
                    if not changed:
                        changed=True
                        if mode=='cancel': flag.touch()
                        else:
                            path=Path(args.input if mode=='input' else args.model)
                            if mode=='report': path=path.with_name('report.json')
                            path.write_bytes(path.read_bytes()+b' ')
                    return result
                with patch.object(backend,'predict_sample',side_effect=predict),contextlib.redirect_stdout(io.StringIO()):
                    code=backend.main(['predict-batch','--model',args.model,'--input',args.input,
                                       '--output',args.output,'--cancel-file',args.cancel_file])
                self.assertEqual(code,2 if mode=='cancel' else 1)
                self.assertFalse(Path(args.output).exists())

    def test_limits_duplicate_json_and_cache_reuse(self):
        args=self.args(); Path(args.input).write_bytes(b' '*(16*1024*1024+1))
        self.assertEqual(self.run_batch(args).returncode,1); self.assertFalse(Path(args.output).exists())
        args=self.args(); raw=Path(args.input).read_text(); Path(args.input).write_text(raw.replace('"version": 1','"version": 1, "version": 1'))
        self.assertEqual(self.run_batch(args).returncode,1)
        data=self.snapshot(); data['candidates']=[data['candidates'][0]]*10001; args=self.args(data)
        self.assertEqual(self.run_batch(args).returncode,1)
        args=self.args(); backend=module(SCRIPT,'comparison_cache')
        original=backend.network
        with patch.object(backend,'network',wraps=original) as create,contextlib.redirect_stdout(io.StringIO()):
            code=backend.main(['predict-batch','--model',args.model,'--input',args.input,'--output',args.output])
        self.assertEqual(code,0); self.assertEqual(create.call_count,6)

    def test_staging_cancel_tamper_and_racing_output_leave_no_completion(self):
        for mode in ['cancel','input','model','report','output']:
            with self.subTest(mode=mode):
                args=self.args(model=self.variant(lambda m:None))
                args.cancel_file=str(self.root/str(uuid.uuid4()))
                backend=module(SCRIPT,'staging_'+mode)
                original=backend.os.fsync
                def fsync(fd):
                    original(fd)
                    if mode=='cancel': Path(args.cancel_file).touch()
                    elif mode=='output':
                        Path(args.output).mkdir()
                        (Path(args.output)/'sentinel').write_text('keep')
                    else:
                        path=Path(args.input if mode=='input' else args.model)
                        if mode=='report': path=path.with_name('report.json')
                        path.write_bytes(path.read_bytes()+b' ')
                with patch.object(backend.os,'fsync',side_effect=fsync),contextlib.redirect_stdout(io.StringIO()):
                    code=backend.main(['predict-batch','--model',args.model,'--input',args.input,
                                       '--output',args.output,'--cancel-file',args.cancel_file])
                self.assertEqual(code,2 if mode=='cancel' else 1)
                self.assertFalse((Path(args.output)/'comparison.json').exists())
                if mode=='output': self.assertEqual((Path(args.output)/'sentinel').read_text(),'keep')
                else: self.assertFalse(Path(args.output).exists())
                self.assertEqual(list(self.root.glob('.comparison-*')),[])

    def test_output_limit_rejects_expanded_failures_without_clipping(self):
        args=self.args(); backend=module(SCRIPT,'comparison_size')
        with patch.object(backend,'predict_sample',side_effect=ValueError('reason '*170000)),contextlib.redirect_stdout(io.StringIO()) as events:
            code=backend.main(['predict-batch','--model',args.model,'--input',args.input,'--output',args.output])
        self.assertEqual(code,1)
        self.assertIn('16 MiB',events.getvalue())
        self.assertFalse(Path(args.output).exists())
        self.assertEqual(list(self.root.glob('.comparison-*')),[])

    def test_real_flag_is_preserved_and_unsupported_structure_retains_all_rows(self):
        path=self.variant(lambda m:m.update(testOnly=False))
        sibling=json.loads(path.with_name('report.json').read_text()); sibling['testOnly']=False
        self.write(str(path.with_name('report.json')),sibling)
        data=self.snapshot()
        for row in data['candidates']: row['sample']['input']['coreShape']=3
        report=self.success(self.args(data,model=path))
        self.assertIs(report['testOnly'],False)
        self.assertIs(report['productionEligible'],False)
        self.assertEqual(report['counts'],dict(candidates=3,successfulTargets=0,failedTargets=18))
        self.assertEqual([r['candidateId'] for r in report['candidates']],['candidate-3','candidate-2','candidate-4'])

    def test_zero_original_temperature_is_valid_for_batch_and_legacy_predict(self):
        data=self.snapshot()
        for row in data['candidates']:
            for key in KEYS[3:]: row['sample']['baseline'][key]=0
        args=self.args(data)
        report=self.success(args)
        self.assertEqual(report['counts'],dict(candidates=3,successfulTargets=18,failedTargets=0))
        for row in report['candidates']:
            for key in KEYS[3:]:
                self.assertEqual(row['baseline'][key],0)
                self.assertGreater(row['predictions'][key],0)
        sample=self.write(str(uuid.uuid4())+'.json',data['candidates'][0]['sample'])
        r=self.cli('predict','--model',self.job/'model.json','--sample',sample)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        for key in KEYS[3:]: self.assertGreater(json.loads(r.stdout)['predictions'][key],0)

if __name__=='__main__': unittest.main()
