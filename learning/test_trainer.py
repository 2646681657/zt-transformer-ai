import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).with_name('trainer.py')
KEYS = ['noLoadLoss_W','loadLoss_W','impedance_pct','oilTopRise_K','hvWindingRise_K','lvWindingRise_K']

def fixture():
    x=dict(capacity_kVA=100,hvRated_kV=10,lvRated_kV=.4,coreDiameter_mm=200,coreStraight_mm=100,stackFactor=.96,steelThickness_mm=.23,hvBareWidth_mm=4,hvBareThick_mm=2,lvTurns=20,lvFoilThick_mm=1,lvFoilWidth_mm=400,mainDuctWidth_mm=20,refFluxDens_T=1.6,hvCopperWire=True,lvCopperFoil=True,coreShape=2,yokeShape=1,hvCoilFormIdx=1)
    schema=dict(type='object',properties={k:dict(type='boolean' if type(v) is bool else 'number') for k,v in x.items()},required=list(x),additionalProperties=False,id='ztbld-calcinput-learning-v1')
    rows=[dict(id=str(i),productId=f' Product {i} ',productGroupId=f'product {i}',source='synthetic',schemeName='fixture',testedOn='2026-10-23',scope='oil-foil-stacked-ellipse-tyoke-dual-cylinder-50hz-75c',engineVersion='sb20-sheet11-learning-v1',input=dict(x,capacity_kVA=100+i*10),baseline=dict.fromkeys(KEYS,100),measured={k:110+i if k==KEYS[0] else None for k in KEYS},residual={k:10+i if k==KEYS[0] else None for k in KEYS}) for i in range(8)]
    for row in rows: row['purpose']='training'
    return dict(format='ZTBLD-ResidualTrainingBundle',version=1,scope=rows[0]['scope'],engineVersion=rows[0]['engineVersion'],targets=[dict(key=k,unit=u) for k,u in zip(KEYS,['W','W','%','K','K','K'])],datasetSha256='a'*64,inputSchema=schema,splitGroupKey='productGroupId',residualDefinition='measured-baseline',rows=rows,testOnly=True)

def context_fixture():
    b=fixture()
    extra=dict(steelGrade='23ZH90',hvTurnsPerLayer=100,steelPricePerKg=17,
               useCustomSteelPrice=False,steelPriceGrade='23ZH90',
               materialPrices=dict(custom=False,copper=80),
               craftConstraints=dict(enabled=False,minimumMainDuct_mm=0,source=''),
               legacyUnitLossRecordText='',legacyCraftCoefRecordText='',
               hvDuctWidthSide=[1,2],yokeSteps_mm=[10,20])
    def schema(v):
        if isinstance(v,dict): return dict(type='object',properties={k:schema(x) for k,x in v.items()},required=list(v),additionalProperties=False)
        if isinstance(v,list): return dict(type='array',length=len(v),itemsByIndex=[schema(x) for x in v])
        return dict(type='boolean' if type(v) is bool else 'string' if isinstance(v,str) else 'number')
    for r in b['rows']: r['input'].update(copy.deepcopy(extra))
    b['inputSchema']=dict(schema(b['rows'][0]['input']),id='ztbld-calcinput-learning-v1')
    # Criteria are optional in the actual Qt schema.
    b['inputSchema']['properties']['performanceCriteria']=schema(dict(mode='builtin'))
    return b

class BackendTests(unittest.TestCase):
    def setUp(self):
        t=tempfile.TemporaryDirectory(); self.addCleanup(t.cleanup); self.root=Path(t.name)
    def module(self):
        self.assertTrue(SCRIPT.exists(),'Python backend is not implemented')
        s=importlib.util.spec_from_file_location('trainer',SCRIPT); m=importlib.util.module_from_spec(s); s.loader.exec_module(m); return m
    def write(self,name,data):
        p=self.root/name; p.write_text(json.dumps(data),encoding='utf8'); return p
    def cli(self,*args):
        return subprocess.run([sys.executable,'-B',str(SCRIPT),*map(str,args)],capture_output=True,text=True)
    def test_partial_labels(self):
        b=self.module().load_bundle(self.write('b.json',fixture()),True)
        self.assertIsNone(b['rows'][0]['residual'][KEYS[1]])
    def test_training_requires_explicit_training_purpose(self):
        for purpose in [None,'unassigned','independent_test','excluded','unknown']:
            b=fixture()
            if purpose is None: b['rows'][0].pop('purpose')
            else: b['rows'][0]['purpose']=purpose
            with self.subTest(purpose=purpose), self.assertRaisesRegex(ValueError,'purpose'):
                self.module().load_bundle(self.write('purpose.json',b),True)
    def test_evaluation_requires_reserved_test_purpose(self):
        b=fixture()
        for row in b['rows']: row['purpose']='independent_test'
        self.module().load_bundle(self.write('test-role.json',b),True,evaluation=True)
        for purpose in ['training','unassigned','excluded']:
            b['rows'][0]['purpose']=purpose
            with self.subTest(purpose=purpose), self.assertRaisesRegex(ValueError,'purpose'):
                self.module().load_bundle(self.write('test-role.json',b),True,evaluation=True)
    def test_invalid_bundle(self):
        m=self.module()
        mutations=[lambda b:b['rows'][0]['residual'].update({KEYS[0]:999}),lambda b:b['rows'][0].update(productGroupId='other'),lambda b:b['rows'].append(copy.deepcopy(b['rows'][0])),lambda b:b.update(datasetSha256='bad'),lambda b:b['rows'][0]['input'].update(capacity_kVA=float('nan')),lambda b:b['rows'][0]['input'].pop('lvTurns'),lambda b:b.update(engineVersion='unknown')]
        for mutate in mutations:
            b=fixture(); mutate(b)
            with self.assertRaises(ValueError): m.load_bundle(self.write('b.json',b),True)
    def test_testonly_guard(self):
        with self.assertRaises(ValueError): self.module().load_bundle(self.write('b.json',fixture()),False)
    def test_real_training_reload_and_train_only_statistics(self):
        self.module(); b=fixture(); out=self.root/'job'
        out.mkdir(); (out/'job.json').write_text('{"status":"running"}')
        r=self.cli('train','--bundle',self.write('b.json',b),'--output',out,'--epochs',4,'--cancel-file',self.root/'cancel','--test-only')
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertTrue((out/'job.json').exists())
        model=json.loads((out/'model.json').read_text()); report=json.loads((out/'report.json').read_text())
        import platform
        import torch
        import numpy as np
        expected=dict(pythonVersion=platform.python_version(),torchVersion=torch.__version__,numpyVersion=np.__version__,device='cpu')
        self.assertEqual(model.get('runtime'),expected)
        self.assertEqual(report.get('runtime'),expected)
        self.assertEqual(report['status'],'completed'); self.assertTrue(model['testOnly']); self.assertFalse(report['productionEligible'])
        t=model['models'][KEYS[0]]
        self.assertFalse(set(t['trainGroups']) & set(t['validationGroups']))
        xs=[r['input']['capacity_kVA'] for r in b['rows'] if r['productGroupId'] in t['trainGroups']]
        self.assertAlmostEqual(t['normalization']['mean'][0],sum(xs)/len(xs))
        self.assertEqual(set(model['models']),{KEYS[0]}); self.assertIn('baselineMAE',report['validation'][KEYS[0]])
        s=copy.deepcopy(b['rows'][2]); s.pop('residual'); s.pop('productGroupId')
        r=self.cli('predict','--model',out/'model.json','--sample',self.write('s.json',s))
        self.assertEqual(r.returncode,0,r.stdout); self.assertGreater(json.loads(r.stdout)['predictions'][KEYS[0]],100)
        s['input']['capacity_kVA']=999
        self.assertNotEqual(self.cli('predict','--model',out/'model.json','--sample',self.write('s.json',s)).returncode,0)
        # Rectangular but wrong tensor shape reaches our shape validation.
        for row in model['models'][KEYS[0]]['weights']['0.weight']: row.append(0)
        r=self.cli('predict','--model',self.write('job/bad.json',model),'--sample',self.write('s.json',b['rows'][2]))
        self.assertEqual(r.returncode,1,r.stdout)
        self.assertEqual(json.loads(r.stdout)['message'],'Invalid weight tensor')
    def test_insufficient_and_cancel_no_artifacts(self):
        self.module()
        for cancel in [False,True]:
            b=fixture(); flag=self.root/'cancel'; out=self.root/str(cancel)
            if cancel: flag.touch()
            else: b['rows']=b['rows'][:4]
            r=self.cli('train','--bundle',self.write('b.json',b),'--output',out,'--epochs',3,'--cancel-file',flag,'--test-only')
            self.assertEqual(r.returncode,2 if cancel else 1,r.stdout)
            self.assertFalse((out/'model.json').exists()); self.assertFalse((out/'report.json').exists())

    def test_repeated_product_does_not_meet_minimum(self):
        self.module(); b=fixture()
        for i,r in enumerate(b['rows']):
            r['productId']=' SAME '; r['productGroupId']='same'; r['testedOn']=f'2026-10-{i+1:02d}'
        r=self.cli('train','--bundle',self.write('b.json',b),'--output',self.root/'job','--epochs',2,'--cancel-file',self.root/'cancel','--test-only')
        self.assertEqual(r.returncode,1,r.stdout)

    def test_constant_features_and_constant_residual(self):
        self.module(); b=fixture()
        for r in b['rows']:
            r['input']=copy.deepcopy(b['rows'][0]['input'])
            r['measured'][KEYS[0]]=106; r['residual'][KEYS[0]]=6
        out=self.root/'job'
        r=self.cli('train','--bundle',self.write('b.json',b),'--output',out,'--epochs',200,'--cancel-file',self.root/'cancel','--test-only')
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        m=json.loads((out/'model.json').read_text())
        self.assertEqual(m['models'][KEYS[0]]['normalization']['scale'],[1]*16)
        self.assertLess(m['models'][KEYS[0]]['validation']['modelMAE'],.1)

    def test_cancellation_during_epochs(self):
        self.module(); flag=self.root/'cancel'; out=self.root/'job'
        p=subprocess.Popen([sys.executable,'-B',str(SCRIPT),'train','--bundle',str(self.write('b.json',fixture())),'--output',str(out),'--epochs','100000','--cancel-file',str(flag),'--test-only'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            line=p.stdout.readline()
            self.assertEqual(json.loads(line)['event'],'progress',line)
            flag.touch()
            stdout,stderr=p.communicate(timeout=30)
            self.assertEqual(p.returncode,2,stdout+stderr)
            self.assertIn('cancelled',stdout)
            self.assertFalse((out/'report.json').exists()); self.assertFalse((out/'model.json').exists())
        finally:
            if p.poll() is None: p.kill(); p.communicate()

    def test_cli_error_is_machine_readable(self):
        self.module(); r=self.cli('train','--bundle','missing')
        self.assertEqual(r.returncode,1)
        self.assertEqual(json.loads(r.stdout)['event'],'error')

    def test_bundle_rejects_mixed_unrepresented_physics(self):
        m=self.module()
        for key,value in [('steelGrade','different'),('hvTurnsPerLayer',101),('hvDuctWidthSide',[1,3]),('yokeSteps_mm',[10,21])]:
            with self.subTest(key=key):
                b=context_fixture(); b['rows'][-1]['input'][key]=value
                with self.assertRaisesRegex(ValueError,key): m.load_bundle(self.write('b.json',b),True)

    def test_fixed_context_prediction_and_price_metadata_exclusions(self):
        self.module(); b=context_fixture(); out=self.root/'job'
        b['rows'][-1]['input']['steelPricePerKg']=30
        b['rows'][-1]['input']['materialPrices']['copper']=99
        r=self.cli('train','--bundle',self.write('b.json',b),'--output',out,'--epochs',2,'--cancel-file',self.root/'cancel','--test-only')
        self.assertEqual(r.returncode,0,r.stdout)
        model=json.loads((out/'model.json').read_text())
        self.assertEqual(model.get('fixedContext',{}).get('steelGrade'),'23ZH90')
        for key,value in [('steelGrade','different'),('hvTurnsPerLayer',101),('hvDuctWidthSide',[1,3]),('yokeSteps_mm',[10,21])]:
            s=copy.deepcopy(b['rows'][2]); s['input'][key]=value
            r=self.cli('predict','--model',out/'model.json','--sample',self.write('s.json',s))
            self.assertEqual(r.returncode,1,r.stdout); self.assertIn(key,json.loads(r.stdout)['message'])
        s=copy.deepcopy(b['rows'][2]); s.update(source='edited report',schemeName='renamed',productId='other id')
        s['input'].update(materialPrices=dict(custom=True,copper=90),steelPricePerKg=40,useCustomSteelPrice=True,steelPriceGrade='binding',performanceCriteria=dict(mode='custom'),legacyUnitLossRecordText='record',legacyCraftCoefRecordText='record',craftConstraints=dict(enabled=True,minimumMainDuct_mm=10,source='report'))
        r=self.cli('predict','--model',out/'model.json','--sample',self.write('s.json',s))
        self.assertEqual(r.returncode,0,r.stdout)
        model.pop('fixedContext',None)
        r=self.cli('predict','--model',self.write('job/old.json',model),'--sample',self.write('s.json',s))
        self.assertEqual(r.returncode,1,r.stdout)
        self.assertEqual(json.loads(r.stdout)['message'],'Model lacks fixedContext; retrain with applicability guard')

    def test_prediction_requires_matching_completed_sibling_report(self):
        self.module(); b=fixture(); out=self.root/'job'
        r=self.cli('train','--bundle',self.write('b.json',b),'--output',out,'--epochs',2,'--cancel-file',self.root/'cancel','--test-only')
        self.assertEqual(r.returncode,0,r.stdout)
        sample=self.write('s.json',b['rows'][2]); report_path=out/'report.json'
        report=json.loads(report_path.read_text())
        report_path.unlink()
        r=self.cli('predict','--model',out/'model.json','--sample',sample)
        self.assertEqual(r.returncode,1,'Prediction accepted orphan model: '+r.stdout)
        mutations=[lambda r:r.update(status='running'),lambda r:r.update(modelVersion='other'),lambda r:r.update(testOnly=False),lambda r:r.update(productionEligible=True),lambda r:r.update(validation={}),lambda r:r['validation'].update({KEYS[1]:r['validation'][KEYS[0]]})]
        for mutate in mutations:
            bad=copy.deepcopy(report); mutate(bad); report_path.write_text(json.dumps(bad))
            r=self.cli('predict','--model',out/'model.json','--sample',sample)
            self.assertEqual(r.returncode,1,'Prediction accepted invalid report: '+r.stdout)
        report_path.write_text(json.dumps(report))
        r=self.cli('predict','--model',out/'model.json','--sample',sample)
        self.assertEqual(r.returncode,0,r.stdout)

if __name__=='__main__': unittest.main()
