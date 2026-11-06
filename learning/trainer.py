"""Experimental CPU residual learning. No model activation or pickle loading."""
import argparse
import copy
import datetime
import hashlib
import json
import math
import os
import platform
from pathlib import Path
import random
import re
import shutil
import sys
import tempfile
import uuid

SCOPE = 'oil-foil-stacked-ellipse-tyoke-dual-cylinder-50hz-75c'
ENGINE = 'sb20-default-sheet12-learning-v1'
METRICS = ['noLoadLoss_W','loadLoss_W','impedance_pct','oilTopRise_K','hvWindingRise_K','lvWindingRise_K']
UNITS = ['W','W','%','K','K','K']
# Explicit whitelist: electrical ratings, core/winding geometry, material flags.
FEATURES = ['capacity_kVA','hvRated_kV','lvRated_kV','coreDiameter_mm',
            'coreStraight_mm','stackFactor','steelThickness_mm','hvBareWidth_mm',
            'hvBareThick_mm','lvTurns','lvFoilThick_mm','lvFoilWidth_mm',
            'mainDuctWidth_mm','refFluxDens_T','hvCopperWire','lvCopperFoil']
SEED = 1729
# Only these explicitly nonphysical fields may vary outside the feature vector.
# Legacy text records are annotations, not the numeric core/yoke craft coefficients.
CONTEXT_EXCLUSIONS = frozenset([
    'materialPrices','steelPricePerKg','useCustomSteelPrice','steelPriceGrade',
    'performanceCriteria','craftConstraints',
    'legacyUnitLossRecordText','legacyCraftCoefRecordText'])


def fixed_context(inputs):
    return {k: copy.deepcopy(v) for k,v in inputs.items()
            if k not in FEATURES and k not in CONTEXT_EXCLUSIONS}


def check_context(actual, expected):
    require(isinstance(expected, dict), 'Model lacks fixedContext; retrain with applicability guard')
    missing = object()
    conflicts = sorted(k for k in actual.keys() | expected.keys()
                       if actual.get(k,missing) != expected.get(k,missing))
    require(not conflicts, 'Fixed physical context mismatch: '+', '.join(conflicts))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def number(v):
    return type(v) in (int, float) and math.isfinite(v)


def no_duplicates(pairs):
    result = {}
    for k, v in pairs:
        require(k not in result, 'Duplicate JSON key: '+k)
        result[k] = v
    return result


def read_json(path):
    with Path(path).open('rb') as f:
        raw = f.read(16*1024*1024+1)
    require(len(raw) <= 16*1024*1024, 'JSON exceeds 16 MiB')
    obj = json.loads(raw, object_pairs_hook=no_duplicates,
                     parse_constant=lambda x: (_ for _ in ()).throw(ValueError('Nonfinite JSON')))
    return obj, hashlib.sha256(raw).hexdigest()


def schema_check(value, schema):
    require(isinstance(schema, dict), 'Invalid input schema')
    kind = schema.get('type')
    if kind == 'object':
        props = schema.get('properties'); required = schema.get('required')
        require(isinstance(value, dict) and isinstance(props, dict) and isinstance(required, list), 'Expected complete input object')
        require(schema.get('additionalProperties') is False and set(required) <= set(value) <= set(props), 'Missing/unknown input fields')
        for k, v in value.items():
            schema_check(v, props[k])
    elif kind == 'array':
        items = schema.get('itemsByIndex')
        require(isinstance(value, list) and isinstance(items, list) and len(value) == schema.get('length') == len(items), 'Input array shape mismatch')
        for v, s in zip(value, items): schema_check(v, s)
    elif kind == 'number':
        require(number(value) and abs(value) <= 1000000, 'Input must be finite numeric within framework bounds')
    elif kind == 'boolean': require(type(value) is bool, 'Expected boolean input')
    elif kind == 'string': require(isinstance(value, str), 'Expected text input')
    else: raise ValueError('Unsupported schema type')


def features(sample, schema):
    require(sample.get('scope') == SCOPE and sample.get('engineVersion') == ENGINE, 'Sample scope/engineVersion mismatch')
    x = sample.get('input')
    schema_check(x, schema)
    require(x.get('coreShape') == 2 and x.get('yokeShape') == 1 and x.get('hvCoilFormIdx') == 1, 'Unsupported structure')
    result = []
    for key in FEATURES:
        require(key in x, 'Missing feature: '+key)
        v = x[key]
        require(type(v) is bool if key in FEATURES[-2:] else number(v), 'Invalid feature: '+key)
        result.append(float(v))
    return result


def metric_object(obj, name):
    require(isinstance(obj, dict) and set(obj) == set(METRICS), 'Incomplete '+name)


def validate_sample(row, schema, labelled=True, applicability=True):
    if applicability: features(row, schema)
    else: schema_check(row.get('input'), schema)
    for key in ['id','productId','source','schemeName','testedOn']:
        require(isinstance(row.get(key), str), 'Missing sample text: '+key)
    require(row['id'].strip() and row['productId'].strip(), 'Empty sample/product ID')
    datetime.date.fromisoformat(row['testedOn'])
    metric_object(row.get('baseline'), 'baseline')
    metric_object(row.get('measured'), 'measured')
    if labelled: metric_object(row.get('residual'), 'residual')
    for i, k in enumerate(METRICS):
        base, label = row['baseline'][k], row['measured'][k]
        require(number(base) and (base > 0 if i < 3 else base >= 0), 'Invalid baseline')
        require(label is None or (number(label) and (label > 0 if i < 3 else label >= 0)), 'Invalid measured value')
        if label is not None: require(row['source'].strip(), 'Missing label provenance')
        if labelled:
            residual = row['residual'][k]
            require(residual is None if label is None else number(residual) and math.isclose(residual, label-base, rel_tol=1e-12, abs_tol=1e-7), 'Residual evidence inconsistent')


def load_bundle(path, test_only=False, evaluation=False):
    b, sha = read_json(path)
    require(isinstance(b, dict) and b.get('format') == 'ZTBLD-ResidualTrainingBundle' and b.get('version') == 1, 'Unsupported bundle format/version')
    require(b.get('scope') == SCOPE and b.get('engineVersion') == ENGINE, 'Bundle scope/engineVersion mismatch')
    require(b.get('splitGroupKey') == 'productGroupId' and b.get('residualDefinition') == 'measured-baseline', 'Invalid bundle semantics')
    require(b.get('targets') == [dict(key=k,unit=u) for k,u in zip(METRICS,UNITS)], 'Target metadata mismatch')
    require(isinstance(b.get('datasetSha256'), str) and re.fullmatch('[0-9a-f]{64}',b['datasetSha256']), 'Invalid source hash')
    require(isinstance(b.get('inputSchema'), dict) and b['inputSchema'].get('id') == 'ztbld-calcinput-learning-v1', 'Unknown input schema')
    require(type(b.get('testOnly', False)) is bool, 'Invalid testOnly flag')
    synthetic = b.get('testOnly',False) or b.get('synthetic',False) or any('synthetic' in str(r.get('source','')).casefold() or r.get('testOnly') is True or r.get('synthetic') is True for r in b.get('rows',[]) if isinstance(r,dict))
    require(not synthetic or test_only, 'Synthetic/testOnly bundle requires --test-only')
    require(isinstance(b.get('rows'),list) and b['rows'], 'Empty bundle')
    ids, tests = set(), set()
    context = None
    for row in b['rows']:
        require(isinstance(row,dict), 'Invalid row')
        expected_purpose = 'independent_test' if evaluation else 'training'
        require(row.get('purpose') == expected_purpose,
                'Sample purpose must be explicitly '+expected_purpose+'; assign product usage before export')
        validate_sample(row,b['inputSchema'],applicability=not evaluation)
        current = fixed_context(row['input'])
        if context is None: context = current
        elif not evaluation: check_context(current,context)
        group = row['productId'].strip().casefold()
        require(row.get('productGroupId') == group, 'Noncanonical productGroupId')
        require(row['id'] not in ids, 'Duplicate sample ID')
        test = (group,row['testedOn'])
        require(test not in tests, 'Duplicate product/date')
        ids.add(row['id']); tests.add(test)
        require(any(v is not None for v in row['measured'].values()), 'Unlabelled bundle row')
    b['_evidence'] = dict(sourceDatasetSha256=b['datasetSha256'], bundleFileSha256=sha,
                          trustedProducer='SelfLearning::trainingBundle (Qt/C++ validated export)',
                          sourceHashVerification='recorded, not recomputed: Qt compact JSON encoding differs from Python',
                          baselineVerification='trusted C++ full-engine recalculation; Python validates finite self-contained residual consistency')
    b['_testOnly'] = bool(test_only or synthetic)
    b['_fixedContext'] = context
    return b


class Cancelled(Exception): pass


def check_cancel(path):
    if path and Path(path).exists(): raise Cancelled('Operation cancelled')


def event(kind, message, **extra):
    print(json.dumps(dict(event=kind,message=message,**extra),allow_nan=False),flush=True)


def runtime():
    import numpy as np
    import torch
    torch.set_num_threads(1)
    return np, torch


def network(torch):
    return torch.nn.Sequential(torch.nn.Linear(len(FEATURES),32),torch.nn.ReLU(),
                               torch.nn.Linear(32,16),torch.nn.ReLU(),torch.nn.Linear(16,1))


def train(args):
    require(1 <= args.epochs <= 100000, 'epochs must be 1..100000')
    check_cancel(args.cancel_file)
    b = load_bundle(args.bundle,args.test_only)
    np, torch = runtime()
    torch.manual_seed(SEED); torch.use_deterministic_algorithms(True)
    out = Path(args.output).absolute()
    require(not out.exists() or out.is_dir(), 'Output must be a directory')
    require(not (out/'model.json').exists() and not (out/'report.json').exists(), 'Job artifacts already exist')
    model = dict(format='ZTBLD-ResidualModel',version=1,status='experimental',productionEligible=False,
                 testOnly=b['_testOnly'],modelVersion=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+str(uuid.uuid4()),
                 scope=SCOPE,engineVersion=ENGINE,featureOrder=FEATURES,inputSchema=b['inputSchema'],
                 fixedContext=b['_fixedContext'],
                 runtime=dict(pythonVersion=platform.python_version(),torchVersion=torch.__version__,numpyVersion=np.__version__,device='cpu'),
                 seed=SEED,architecture=[len(FEATURES),32,16,1],evidence=b['_evidence'],models={})
    validation, skipped = {}, {}
    for index,k in enumerate(METRICS):
        rows = [r for r in b['rows'] if r['residual'][k] is not None]
        groups = sorted({r['productGroupId'] for r in rows})
        if len(groups) < 5:
            skipped[k] = dict(reason='requires at least 5 unique labelled products',labelledProducts=len(groups)); continue
        random.Random(SEED).shuffle(groups)
        vg = set(groups[:max(1,len(groups)//5)])
        tr = [r for r in rows if r['productGroupId'] not in vg]
        va = [r for r in rows if r['productGroupId'] in vg]
        x = np.array([features(r,b['inputSchema']) for r in tr],dtype=np.float64)
        vx = np.array([features(r,b['inputSchema']) for r in va],dtype=np.float64)
        y = np.array([r['residual'][k] for r in tr],dtype=np.float64)
        vy = np.array([r['residual'][k] for r in va],dtype=np.float64)
        mean, scale = x.mean(0), x.std(0); scale[scale < 1e-12] = 1
        ym, ys = float(y.mean()), float(y.std()); ys = ys if ys >= 1e-12 else 1.0
        tx = torch.tensor((x-mean)/scale,dtype=torch.float32)
        ty = torch.tensor(((y-ym)/ys)[:,None],dtype=torch.float32)
        tvx = torch.tensor((vx-mean)/scale,dtype=torch.float32)
        net=network(torch); optimizer=torch.optim.Adam(net.parameters(),lr=.005)
        best, state, best_epoch = math.inf, None, 0
        for epoch in range(args.epochs):
            check_cancel(args.cancel_file)
            net.train(); optimizer.zero_grad(); loss=torch.nn.functional.mse_loss(net(tx),ty)
            require(math.isfinite(loss.item()), 'Nonfinite training loss')
            loss.backward(); optimizer.step(); net.eval()
            with torch.no_grad(): pred=net(tvx).numpy().ravel().astype(float)*ys+ym
            score=float(np.mean(np.abs(pred-vy)))
            require(math.isfinite(score), 'Nonfinite validation loss')
            if score < best: best, state, best_epoch = score, copy.deepcopy(net.state_dict()), epoch+1
            event('progress',f'{k}: epoch {epoch+1}/{args.epochs}',percent=int(99*(index+(epoch+1)/args.epochs)/len(METRICS)))
        check_cancel(args.cancel_file)
        net.load_state_dict(state)
        with torch.no_grad(): err=net(tvx).numpy().ravel().astype(float)*ys+ym-vy
        validation[k]=dict(baselineMAE=float(np.mean(np.abs(vy))),modelMAE=float(np.mean(np.abs(err))),RMSE=float(np.sqrt(np.mean(err**2))),bestEpoch=best_epoch,validationSamples=len(va))
        model['models'][k]=dict(weights={name:value.tolist() for name,value in state.items()},
            normalization=dict(mean=mean.tolist(),scale=scale.tolist(),targetMean=ym,targetScale=ys),
            bounds=dict(min=x.min(0).tolist(),max=x.max(0).tolist()),
            trainGroups=sorted(set(groups)-vg),validationGroups=sorted(vg),
            trainSampleIds=[r['id'] for r in tr],validationSampleIds=[r['id'] for r in va],labelledSamples=len(rows),validation=validation[k])
    require(model['models'], 'No target has five independent labelled products')
    check_cancel(args.cancel_file)
    report=dict(status='completed',testOnly=model['testOnly'],productionEligible=False,
                modelVersion=model['modelVersion'],modelPath=str(out/'model.json'),validation=validation,skipped=skipped,evidence=b['_evidence'],runtime=model['runtime'])
    # Report is the completion marker. Preserve Qt-owned bundle/job/cancel files.
    out.parent.mkdir(parents=True,exist_ok=True)
    stage=Path(tempfile.mkdtemp(prefix='.dnn-',dir=out.parent))
    published = []
    try:
        for name,obj in [('model.json',model),('report.json',report)]:
            with (stage/name).open('w',encoding='utf8') as f:
                json.dump(obj,f,allow_nan=False,indent=2); f.flush(); os.fsync(f.fileno())
        check_cancel(args.cancel_file)
        if not out.exists():
            stage.rename(out)
        else:
            for name in ['model.json','report.json']:
                check_cancel(args.cancel_file)
                (stage/name).rename(out/name)
                published.append(out/name)
    except BaseException:
        for path in reversed(published): path.unlink(missing_ok=True)
        raise
    finally:
        if stage.exists(): shutil.rmtree(stage)
    event('progress','Training completed; experimental preview only',percent=100)


def load_model(path):
    m,sha=read_json(path)
    require(m.get('format')=='ZTBLD-ResidualModel' and m.get('version')==1 and m.get('status')=='experimental' and m.get('productionEligible') is False and type(m.get('testOnly')) is bool, 'Invalid model metadata')
    report,rsha=read_json(Path(path).with_name('report.json'))
    require(isinstance(report,dict) and report.get('status')=='completed', 'Sibling report.json is not completed')
    require(isinstance(m.get('modelVersion'),str) and m['modelVersion'] and report.get('modelVersion')==m['modelVersion'], 'Sibling report modelVersion mismatch')
    require(type(report.get('testOnly')) is bool and report['testOnly']==m['testOnly'] and report.get('productionEligible') is False, 'Sibling report eligibility/testOnly mismatch')
    require(isinstance(m.get('models'),dict) and m['models'] and set(m['models']) <= set(METRICS),'Invalid model targets')
    require(isinstance(report.get('validation'),dict) and set(report['validation'])==set(m['models']), 'Sibling report trained target validation set mismatch')
    for k,t in m['models'].items():
        v=report['validation'][k]
        require(isinstance(v,dict) and v==t.get('validation') and all(number(v.get(key)) and v[key]>=0 for key in ['baselineMAE','modelMAE','RMSE']), 'Sibling report validation mismatch: '+k)
    require(m.get('scope')==SCOPE and m.get('engineVersion')==ENGINE and m.get('featureOrder')==FEATURES and m.get('architecture')==[len(FEATURES),32,16,1], 'Model scope/version/architecture mismatch')
    require(m['inputSchema'].get('id')=='ztbld-calcinput-learning-v1','Model schema mismatch')
    return m,sha,report,rsha


def predict(args):
    m,_,_,_=load_model(args.model); sample,_=read_json(args.sample)
    result=predict_sample(m,sample,{})
    print(json.dumps(dict(predictions=result),allow_nan=False),flush=True)


def predict_sample(m,sample,cache,targets=None):
    validate_sample(sample,m['inputSchema'],False)
    check_context(fixed_context(sample['input']),m.get('fixedContext'))
    np,torch=runtime(); x=np.array(features(sample,m['inputSchema']),dtype=float)
    result={}
    for k,t in m['models'].items():
        if targets is not None and k not in targets: continue
        arrays={}
        for key in ['mean','scale']:
            arrays[key]=np.asarray(t['normalization'][key],dtype=float)
        lo=np.asarray(t['bounds']['min'],dtype=float); hi=np.asarray(t['bounds']['max'],dtype=float)
        require(all(a.shape==(len(FEATURES),) and np.isfinite(a).all() for a in [*arrays.values(),lo,hi]),'Invalid normalization/bounds')
        require((arrays['scale']>0).all() and (lo<=hi).all() and (x>=lo).all() and (x<=hi).all(),'Features outside training bounds')
        ym=t['normalization']['targetMean']; ys=t['normalization']['targetScale']
        require(number(ym) and number(ys) and ys>0,'Invalid target normalization')
        if k not in cache:
            net=network(torch); expected=net.state_dict(); weights=t['weights']
            require(set(weights)==set(expected),'Weight keys mismatch')
            state={}
            for key,v in expected.items():
                a=np.asarray(weights[key],dtype=float)
                require(a.shape==tuple(v.shape) and np.isfinite(a).all(),'Invalid weight tensor')
                state[key]=torch.tensor(a,dtype=torch.float32)
                require(torch.isfinite(state[key]).all().item(),'Weights overflow float32')
            net.load_state_dict(state); net.eval(); cache[k]=net
        net=cache[k]
        with torch.no_grad(): residual=net(torch.tensor((x-arrays['mean'])/arrays['scale'],dtype=torch.float32)).item()*ys+ym
        corrected=sample['baseline'][k]+residual
        require(number(corrected) and corrected>=0,'Invalid corrected prediction')
        result[k]=corrected
    return result


def error_stats(errors):
    if not errors: return None
    # hypot avoids overflowing intermediate squares for valid finite inputs.
    return dict(MAE=math.fsum(abs(e)/len(errors) for e in errors),
                RMSE=math.hypot(*errors)/math.sqrt(len(errors)),
                maxAbsoluteError=max(map(abs,errors)))


def evaluate(args):
    check_cancel(args.cancel_file)
    out=Path(args.output).resolve()
    require(not out.exists(), 'Evaluation output already exists')
    require(re.search(r'[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}',out.name), 'Evaluation output directory must contain a UUID')
    bundle_path=Path(args.bundle).resolve()
    b=load_bundle(bundle_path,args.test_only,evaluation=True)
    result=dict(format='ZTBLD-IndependentEvaluation',version=1,status='completed',
                testOnly=b['_testOnly'],bundlePath=str(bundle_path),
                bundleSha256=b['_evidence']['bundleFileSha256'],models=[])
    groups={r['productId'].strip().casefold() for r in b['rows']}
    for index,path in enumerate(args.models):
        check_cancel(args.cancel_file)
        path=Path(path).resolve(); report_path=path.with_name('report.json')
        entry=dict(modelPath=str(path),modelSha256=None,reportPath=str(report_path),
                   reportSha256=None,modelVersion=None,scope=None,engineVersion=None,
                   testOnly=True,eligible=False,failures=[],targets={})
        m=None; raw=None; cache={}
        try:
            raw,entry['modelSha256']=read_json(path)
            _,entry['reportSha256']=read_json(report_path)
            m,_,_,_=load_model(path)
            entry.update({k:m[k] for k in ['modelVersion','scope','engineVersion']})
            entry['testOnly']=bool(b['_testOnly'] or m['testOnly'])
            used=set()
            for key,t in m['models'].items():
                partitions=[]
                for field in ['trainGroups','validationGroups']:
                    values=t.get(field)
                    require(isinstance(values,list) and values and all(isinstance(v,str) and v.strip() and v==v.strip().casefold() for v in values), 'Missing/noncanonical group provenance: '+key+'/'+field)
                    require(len(set(values))==len(values),'Duplicate group provenance: '+key)
                    partitions.append(set(values)); used.update(values)
                require(not partitions[0]&partitions[1], 'Training/validation group overlap: '+key)
            require(not groups&used, 'Independent product leakage: '+', '.join(sorted(groups&used)))
        except Exception as exc:
            entry['failures'].append(str(exc))
        # Conservative propagation even if model validation failed.
        if isinstance(raw,dict):
            entry['testOnly']=bool(entry['testOnly'] or raw.get('testOnly') or raw.get('synthetic'))
        result['testOnly']=bool(result['testOnly'] or entry['testOnly'])
        for key in METRICS:
            rows=[r for r in b['rows'] if r['measured'][key] is not None]
            errors=[]; failures=[]
            for row in rows:
                check_cancel(args.cancel_file)
                try:
                    require(not entry['failures'], '; '.join(entry['failures']))
                    require(key in m['models'], 'Missing model target: '+key)
                    value=predict_sample(m,row,cache,[key])[key]
                    errors.append(value-row['measured'][key])
                except Exception as exc:
                    failures.append(row['id']+': '+str(exc))
            entry['targets'][key]=dict(labelledSamples=len(rows),
                independentProducts=len({r['productGroupId'] for r in rows}),
                predictedSamples=len(errors),coverage=len(errors)/len(rows) if rows else 0,
                failures=failures,baseline=error_stats([r['baseline'][key]-r['measured'][key] for r in rows]),
                model=error_stats(errors))
        entry['eligible']=bool(not entry['testOnly'] and not entry['failures'] and
                               any(t['labelledSamples'] for t in entry['targets'].values()) and
                               all(not t['failures'] for t in entry['targets'].values()))
        result['models'].append(entry)
        event('progress','Evaluated model '+str(path),percent=int(99*(index+1)/len(args.models)))
    check_cancel(args.cancel_file)
    # Detect changes while evaluating: hashes must bind the exact frozen inputs.
    require(read_json(bundle_path)[1]==result['bundleSha256'],'Bundle changed during evaluation')
    for entry in result['models']:
        for prefix in ['model','report']:
            digest=entry[prefix+'Sha256']
            if digest is not None:
                require(read_json(entry[prefix+'Path'])[1]==digest,'Artifact changed during evaluation')
    out.parent.mkdir(parents=True,exist_ok=True)
    stage=Path(tempfile.mkdtemp(prefix='.evaluation-',dir=out.parent))
    try:
        with (stage/'evaluation.json').open('w',encoding='utf8') as f:
            json.dump(result,f,allow_nan=False,indent=2); f.flush(); os.fsync(f.fileno())
        check_cancel(args.cancel_file)
        require(not out.exists(),'Evaluation output already exists')
        stage.rename(out)
    finally:
        if stage.exists(): shutil.rmtree(stage)
    event('progress','Independent evaluation completed',percent=100)


MAX_COMPARISON_BYTES = 16 * 1024 * 1024
MAX_COMPARISON_ROWS = 10000


def canonical_uuid(value):
    require(isinstance(value, str) and str(uuid.UUID(value)) == value,
            'Expected canonical UUID')


def predict_batch(args):
    """Read-only corrections; every target failure stays attached to its row."""
    check_cancel(args.cancel_file)
    out = Path(args.output).absolute()
    canonical_uuid(out.name)
    require(not os.path.lexists(out), 'Comparison output already exists')
    input_path = Path(args.input).resolve()
    model_path = Path(args.model).resolve()
    report_path = model_path.with_name('report.json')
    snapshot, input_sha = read_json(input_path)
    require(isinstance(snapshot, dict) and
            snapshot.get('format') == 'ZTBLD-CandidateComparisonInput' and
            type(snapshot.get('version')) is int and snapshot['version'] == 1 and
            snapshot.get('usage') == 'independent-comparison-only',
            'Invalid candidate comparison metadata')
    canonical_uuid(snapshot.get('runId'))
    rows = snapshot.get('candidates')
    require(isinstance(rows, list) and 1 <= len(rows) <= MAX_COMPARISON_ROWS,
            'Candidate count must be 1..10000; no clipping')
    m, model_sha, _, report_sha = load_model(model_path)
    ids = set()
    for order, row in enumerate(rows, 1):
        check_cancel(args.cancel_file)
        require(isinstance(row, dict), 'Invalid candidate row')
        cid = row.get('candidateId')
        require(isinstance(cid, str) and cid.strip() and cid not in ids,
                'Empty/duplicate candidateId')
        ids.add(cid)
        require(type(row.get('schemeId')) is int and row['schemeId'] > 0,
                'schemeId must be a positive integer')
        require(type(row.get('originalOrder')) is int and row['originalOrder'] == order,
                'originalOrder must follow array order 1..N')
        require(number(row.get('materialCost')) and row['materialCost'] >= 0,
                'Invalid materialCost')
        require(isinstance(row.get('costBasis'), str), 'costBasis must be text')
        sample = row.get('sample')
        require(isinstance(sample, dict) and sample.get('source') == 'optimization-candidate',
                'Expected optimization-candidate sample')
        require(sample.get('scope') == SCOPE and sample.get('engineVersion') == ENGINE,
                'Sample scope/engineVersion mismatch')
        # Validate the entire encoded snapshot, but defer physical applicability
        # to each prediction so unsupported candidates remain visible.
        validate_sample(sample, m['inputSchema'], labelled=False, applicability=False)
        require(all(v is None for v in sample['measured'].values()),
                'Candidate measured values must all be null')
    result = dict(format='ZTBLD-CandidateComparison', version=1,
                  usage=snapshot['usage'], status='completed', runId=snapshot['runId'],
                  inputPath=str(input_path), inputSha256=input_sha,
                  modelPath=str(model_path), modelSha256=model_sha, reportSha256=report_sha,
                  modelVersion=m['modelVersion'], scope=m['scope'], engineVersion=m['engineVersion'],
                  testOnly=m['testOnly'], productionEligible=False, candidates=[],
                  counts=dict(candidates=len(rows), successfulTargets=0, failedTargets=0))
    cache = {}
    for index, row in enumerate(rows):
        check_cancel(args.cancel_file)
        entry = {k:row[k] for k in ['candidateId','schemeId','originalOrder','materialCost','costBasis']}
        entry.update(baseline=copy.deepcopy(row['sample']['baseline']),
                     predictions=dict.fromkeys(METRICS), failures={})
        for target in METRICS:
            check_cancel(args.cancel_file)
            try:
                require(target in m['models'], 'Missing model target: '+target)
                value = predict_sample(m, row['sample'], cache, [target])[target]
                require(number(value) and (value > 0 if target in METRICS[:3] else value >= 0),
                        'Invalid corrected prediction for '+target)
                entry['predictions'][target] = value
                result['counts']['successfulTargets'] += 1
            except Cancelled:
                raise
            except Exception as exc:
                entry['failures'][target] = str(exc) or type(exc).__name__
                result['counts']['failedTargets'] += 1
        result['candidates'].append(entry)
        event('progress', 'Compared candidate '+str(index+1)+'/'+str(len(rows)),
              percent=int(99*(index+1)/len(rows)))
    check_cancel(args.cancel_file)
    payload = json.dumps(result, allow_nan=False, ensure_ascii=False, indent=2).encode('utf8')
    require(len(payload) <= MAX_COMPARISON_BYTES, 'Comparison output exceeds 16 MiB')

    def recheck():
        check_cancel(args.cancel_file)
        for path, sha in [(input_path,input_sha),(model_path,model_sha),(report_path,report_sha)]:
            require(read_json(path)[1] == sha, 'Artifact changed during comparison: '+str(path))
        check_cancel(args.cancel_file)
        require(not os.path.lexists(out), 'Comparison output already exists')

    recheck()
    out.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.comparison-', dir=out.parent))
    try:
        with (stage/'comparison.json').open('wb') as f:
            f.write(payload); f.flush(); os.fsync(f.fileno())
        # Recheck after staging as well: neither cancellation nor evidence
        # changes during serialization/fsync may publish a completion marker.
        recheck()
        stage.rename(out)
    finally:
        if stage.exists(): shutil.rmtree(stage)
    event('progress', 'Candidate comparison completed; experimental display only', percent=100)


class Parser(argparse.ArgumentParser):
    def error(self,message): raise ValueError(message)


def main(argv=None):
    try:
        parser=Parser(); sub=parser.add_subparsers(dest='command',required=True,parser_class=Parser)
        sub.add_parser('probe')
        p=sub.add_parser('train'); p.add_argument('--bundle',required=True); p.add_argument('--output',required=True)
        p.add_argument('--epochs',type=int,required=True); p.add_argument('--cancel-file',required=True); p.add_argument('--test-only',action='store_true')
        p=sub.add_parser('predict'); p.add_argument('--model',required=True); p.add_argument('--sample',required=True)
        p=sub.add_parser('predict-batch'); p.add_argument('--model',required=True); p.add_argument('--input',required=True)
        p.add_argument('--output',required=True); p.add_argument('--cancel-file')
        p=sub.add_parser('evaluate'); p.add_argument('--bundle',required=True); p.add_argument('--models',nargs='+',required=True)
        p.add_argument('--output',required=True); p.add_argument('--test-only',action='store_true'); p.add_argument('--cancel-file')
        args=parser.parse_args(argv)
        if args.command=='probe':
            np,torch=runtime(); event('probe','CPU runtime ready',available=True,torchVersion=torch.__version__,numpyVersion=np.__version__,device='cpu')
        elif args.command=='train': train(args)
        elif args.command=='evaluate': evaluate(args)
        elif args.command=='predict-batch': predict_batch(args)
        else: predict(args)
        return 0
    except Cancelled as e:
        event('cancelled',str(e)); return 2
    except Exception as e:
        event('error',str(e)); return 1


if __name__=='__main__': sys.exit(main())
