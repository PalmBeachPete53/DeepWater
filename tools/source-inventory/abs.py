#!/usr/bin/env python3
"""Australian Bureau of Statistics: list of dataflows, then dimensions, codelist sizes and availability of the ones
that aren't census cross-tabs. Writes docs/sources/catalog/abs.tsv and abs_structures.jsonl. Structure only, no values.
Sequential, one call per second, User-Agent Deepwater/1.0. Run with python3 -I."""
import json, os, re, sys, time, urllib.request, urllib.error
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'docs', 'sources', 'catalog')
BASE = 'https://data.api.abs.gov.au/rest/'
ACCEPT = 'application/vnd.sdmx.structure+json'
LOG = sys.argv[1] if len(sys.argv) > 1 else None

def get(path):
    time.sleep(1.0)
    req = urllib.request.Request(BASE + path, headers={'User-Agent': 'Deepwater/1.0', 'Accept': ACCEPT})
    try:
        with urllib.request.urlopen(req, timeout=120) as r:
            b = r.read(); st = r.status
    except urllib.error.HTTPError as e:
        b = e.read(); st = e.code
    except Exception as e:
        b = b''; st = type(e).__name__
    if LOG:
        with open(LOG, 'a') as f:
            f.write(json.dumps({'url': BASE + path, 'status': st, 'size': len(b)}) + '\n')
    return st, b

st, b = get('dataflow/ABS?detail=allstubs')
flows = json.loads(b)['data']['dataflows']
with open(os.path.join(OUT, 'abs.tsv'), 'w') as f:
    f.write('id\tversion\tname\n')
    for x in flows:
        f.write('%s\t%s\t%s\n' % (x['id'], x['version'], re.sub(r'\s+', ' ', x['name'])))
census = re.compile(r'^(C\d|ABS_C|ABS_\d|CENSUS|SA[1-4]_)')
keep = [x for x in flows if not census.match(x['id'])]
print(len(flows), len(keep))
with open(os.path.join(OUT, 'abs_structures.jsonl'), 'w') as out:
    for x in keep:
        rec = {'id': x['id'], 'version': x['version'], 'name': x['name']}
        st, b = get('dataflow/ABS/%s/%s?references=all' % (x['id'], x['version']))
        rec['status'] = st
        try:
            d = json.loads(b)['data']
            sizes = {c['id']: len(c.get('codes', [])) for c in d.get('codelists', [])}
            ds = d['dataStructures'][0]['dataStructureComponents']
            dims = []
            for dim in ds['dimensionList']['dimensions']:
                enum = (dim.get('localRepresentation') or {}).get('enumeration', '')
                m = re.search(r'=\w+:(\w+)\(', enum)
                dims.append({'id': dim['id'], 'codelist': m.group(1) if m else '', 'codes': sizes.get(m.group(1)) if m else None})
            rec['dimensions'] = dims
            rec['attributes'] = [a['id'] for a in ds.get('attributeList', {}).get('attributes', [])]
        except Exception as e:
            rec['error'] = str(e)[:100]
        st, b = get('availableconstraint/ABS,%s,%s/all/all/all?mode=exact' % (x['id'], x['version']))
        rec['availability_status'] = st
        try:
            c = json.loads(b)['data']['contentConstraints'][0]
            for a in c.get('annotations', []):
                if a.get('id') == 'obs_count':
                    rec['observations'] = int(a['title'])
            for kv in c['cubeRegions'][0]['keyValues']:
                if kv['id'] == 'TIME_PERIOD':
                    tr = kv.get('timeRange') or {}
                    rec['start'] = (tr.get('startPeriod') or {}).get('period') or (kv.get('values') or [None])[0]
                    rec['end'] = (tr.get('endPeriod') or {}).get('period') or (kv.get('values') or [None])[-1]
                elif kv['id'] == 'FREQ':
                    rec['freq'] = kv.get('values')
                rec.setdefault('available', {})[kv['id']] = len(kv.get('values', []))
        except Exception as e:
            rec['availability_error'] = str(e)[:100]
        out.write(json.dumps(rec, ensure_ascii=False) + '\n')
        out.flush()
print('done')
