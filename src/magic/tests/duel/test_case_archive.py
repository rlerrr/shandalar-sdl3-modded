import json,struct,tempfile,unittest,zlib
from unittest.mock import patch
from pathlib import Path
from case_archive import CaseArchiveWriter,CaseArchiveReader,HEADER,INDEX,legacy_case_hash,write_text_if_changed

class CaseArchiveTest(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.path=Path(self.temp.name)/'cases.bin'
  self.metadata=('Example','Æther card','oracle-id','Pay {B}.\n“Text”','empty_main',123)
  self.cards=[('source',1,0,123,8),('witness',0,1,91,0)]
  self.steps=[('choose_target','source','witness','',1,-1,0,-1,0),('assert_card','source','','toughness',0,3,0,0,0)]
 def tearDown(self):self.temp.cleanup()
 def write(self,second=False):
  writer=CaseArchiveWriter(self.path);writer.add(self.metadata,self.cards,self.steps)
  if second:writer.add(('Second',*self.metadata[1:]),[],[])
  writer.finish()
 def mutate(self,offset,value):
  b=bytearray(self.path.read_bytes());b[offset]=value;self.path.write_bytes(b)
 def test_round_trip_preserves_utf8_negative_values_order_and_empty_cases(self):
  self.write(True);reader=CaseArchiveReader(self.path)
  try:
   self.assertEqual(reader.names,['Example','Second']);self.assertEqual(reader.load(0),(self.metadata,self.cards,self.steps));self.assertEqual(reader.load(1),(('Second',*self.metadata[1:]),[],[]))
  finally:reader.close()
 def test_header_is_portable_and_payload_is_independently_compressed(self):
  self.write();header=HEADER.unpack(self.path.read_bytes()[:HEADER.size]);self.assertEqual(header[:3],(b'DUELCASE',1,1));self.assertEqual(HEADER.size,48);self.assertEqual(INDEX.size,24)
  reader=CaseArchiveReader(self.path)
  try:
   _,offset,packed,raw,crc=reader.index[0];payload=self.path.read_bytes()[offset:offset+packed];data=zlib.decompress(payload);self.assertEqual(len(data),raw);self.assertEqual(zlib.crc32(data),crc)
  finally:reader.close()
 def test_unchanged_outputs_keep_timestamps(self):
  self.write();before=self.path.stat().st_mtime_ns;self.write();self.assertEqual(self.path.stat().st_mtime_ns,before)
  text=self.path.with_suffix('.inc');write_text_if_changed(text,'same\n');before=text.stat().st_mtime_ns;write_text_if_changed(text,'same\n');self.assertEqual(text.stat().st_mtime_ns,before)
 def test_rejects_wrong_version_truncation_and_trailing_bytes(self):
  self.write();good=self.path.read_bytes()
  for broken in (good[:12],good[:-1],good+b'x',good[:8]+struct.pack('<I',2)+good[12:]):
   self.path.write_bytes(broken)
   with self.assertRaises(ValueError):CaseArchiveReader(self.path)
 def test_metadata_checksums_are_required(self):
  self.write();good=self.path.read_bytes();header=HEADER.unpack(good[:HEADER.size])
  for offset in (header[3]+8,header[4]+4):
   broken=bytearray(good);broken[offset]^=1;self.path.write_bytes(broken)
   with self.assertRaisesRegex(ValueError,'checksum'):CaseArchiveReader(self.path)
 def test_loading_one_case_does_not_read_another_case_body(self):
  self.write(True);reader=CaseArchiveReader(self.path);offset=reader.index[0][1];reader.close();self.mutate(offset,0)
  reader=CaseArchiveReader(self.path)
  try:
   self.assertEqual(reader.load(1)[0][0],'Second')
   with self.assertRaises((ValueError,zlib.error)):reader.load(0)
  finally:reader.close()
 def test_invalid_record_shape_is_rejected_even_with_valid_checksums(self):
  self.write();b=self.path.read_bytes();h=list(HEADER.unpack(b[:HEADER.size]));entry=list(INDEX.unpack(b[h[4]:]));raw=bytearray(zlib.decompress(b[entry[1]:entry[1]+entry[2]]));struct.pack_into('<I',raw,24,999);packed=zlib.compress(raw,1);entry[2]=len(packed);entry[4]=zlib.crc32(raw)
  table=b[h[3]:h[4]];h[3]=HEADER.size+len(packed);h[4]=h[3]+len(table);index=INDEX.pack(*entry);h[5]=h[4]+len(index);h[7]=zlib.crc32(index);self.path.write_bytes(HEADER.pack(*h)+packed+table+index)
  reader=CaseArchiveReader(self.path)
  try:
   with self.assertRaisesRegex(ValueError,'shape'):reader.load(0)
  finally:reader.close()
 def test_fingerprint_is_independent_of_global_case_number(self):
  self.assertEqual(legacy_case_hash(0,self.metadata,self.cards,self.steps),legacy_case_hash(999,self.metadata,self.cards,self.steps))
  modified=[*self.steps[:-1],('assert_card','source','','toughness',0,4,0,0,0)]
  self.assertNotEqual(legacy_case_hash(0,self.metadata,self.cards,self.steps),legacy_case_hash(0,self.metadata,self.cards,modified))
 def test_embedded_nul_is_rejected(self):
  writer=CaseArchiveWriter(self.path)
  try:
   with self.assertRaises(ValueError):writer.add(('Bad\0name',*self.metadata[1:]),[],[])
  finally:writer.file.close()
 def test_behavior_edits_do_not_rewrite_compiled_driver_or_discovery_tables(self):
  from generate_cases import build
  data=Path(self.temp.name)/'data';out=Path(self.temp.name)/'generated';(data/'cases').mkdir(parents=True)
  source=Path(__file__).parent/'data'
  oracle=json.loads((source/'oracle.json').read_text(encoding='utf8'))
  (data/'oracle.json').write_text(json.dumps(dict(version=1,source=oracle['source'],cards={'Forest':oracle['cards']['Forest']})),encoding='utf8')
  for name in ('fixtures.json','oracle_aliases.json'):(data/name).write_bytes((source/name).read_bytes())
  path=data/'cases/format.json';doc=dict(version=1,card='Forest',csvid=91,cases=[dict(name='DataOnlyProbe',fixture='empty_main',steps=[dict(op='assert_life',player=0,value=20)])])
  path.write_text(json.dumps(doc));build(data,out)
  compiled=['duel_cases.inc','duel_handler_bindings.inc','duel_case_sources.cmake','cases.cmake','manifest.json','inventory.json'];before={n:((out/n).read_bytes(),(out/n).stat().st_mtime_ns) for n in compiled};archive=(out/'duel_cases.bin').read_bytes()
  doc['cases'][0]['steps'][0]['value']=19;path.write_text(json.dumps(doc));build(data,out)
  self.assertNotEqual((out/'duel_cases.bin').read_bytes(),archive)
  for name,(content,stamp) in before.items():self.assertEqual(((out/name).read_bytes(),(out/name).stat().st_mtime_ns),(content,stamp),name)
  reader=CaseArchiveReader(out/'duel_cases.bin')
  try:self.assertEqual(reader.load(0)[2][0][5],19)
  finally:reader.close()
 def test_subset_archive_preserves_full_case_records_and_marks_partial_manifest(self):
  from generate_cases import build
  data=Path(self.temp.name)/'data';full=Path(self.temp.name)/'full';subset=Path(self.temp.name)/'subset';(data/'cases/cards').mkdir(parents=True)
  source=Path(__file__).parent/'data';oracle=json.loads((source/'oracle.json').read_text(encoding='utf8'))
  (data/'oracle.json').write_text(json.dumps(dict(version=1,source=oracle['source'],cards={n:oracle['cards'][n] for n in ('Forest','Island')})),encoding='utf8')
  for name in ('fixtures.json','oracle_aliases.json'):(data/name).write_bytes((source/name).read_bytes())
  for name,csvid,names in (('Forest',91,['Alpha','Zulu']),('Island',126,['Beta'])):
   doc=dict(version=1,card=name,csvid=csvid,cases=[dict(name=n,fixture='empty_main',setup=[dict(ref='source',name=name,player=0,zone='hand')],steps=[dict(op='assert_life',player=0,value=20)]) for n in names])
   (data/f'cases/cards/{name.lower()}.json').write_text(json.dumps(doc))
  build(data,full,True);build(data,subset,True,csvids=[91])
  manifest=json.loads((subset/'manifest.json').read_text());complete=json.loads((full/'manifest.json').read_text())
  self.assertNotIn('selection',complete)
  self.assertEqual(manifest['selection'],dict(version=1,complete=False,source_files=['cases/cards/forest.json']))
  self.assertEqual(manifest['cases'],[c for c in complete['cases'] if c['tier']=='behavior' and c['csvid']==91])
  self.assertEqual(sorted(p.name for p in subset.iterdir()),['case_semantics_sha256.json','duel_cases.bin','inventory.json','manifest.json'])
  full_reader=CaseArchiveReader(full/'duel_cases.bin');subset_reader=CaseArchiveReader(subset/'duel_cases.bin')
  try:
   self.assertEqual(subset_reader.names,['Alpha','Zulu'])
   for i,name in enumerate(subset_reader.names):self.assertEqual(subset_reader.load(i),full_reader.load(full_reader.names.index(name)))
  finally:full_reader.close();subset_reader.close()
  hashes=json.loads((full/'case_semantics_sha256.json').read_text())
  self.assertEqual(json.loads((subset/'case_semantics_sha256.json').read_text()),{n:hashes[n] for n in ('Alpha','Zulu')})

 def driver_data(self):
  data=Path(self.temp.name)/'data';(data/'cases').mkdir(parents=True)
  source=Path(__file__).parent/'data';oracle=json.loads((source/'oracle.json').read_text(encoding='utf8'))
  (data/'oracle.json').write_text(json.dumps(dict(version=1,source=oracle['source'],cards={'Forest':oracle['cards']['Forest']})),encoding='utf8')
  for name in ('fixtures.json','oracle_aliases.json'):(data/name).write_bytes((source/name).read_bytes())
  (data/'cases/forest.json').write_text(json.dumps(dict(version=1,card='Forest',csvid=91,cases=[dict(name='DriverParity',fixture='empty_main',steps=[dict(op='assert_life',player=0,value=20)])])))
  return data

 def test_driver_only_outputs_match_full_compiled_inputs_without_case_expansion(self):
  from generate_cases import build
  data=self.driver_data();full=Path(self.temp.name)/'full';driver=Path(self.temp.name)/'driver'
  build(data,full)
  # Invalid and legacy behavior declarations cannot influence a reusable driver.
  (data/'cases/forest.json').write_text('not JSON');(data/'cases.json').write_text('not JSON')
  with patch('generate_cases.CaseStream',side_effect=AssertionError('behavior expansion')),patch('generate_cases.CaseArchiveWriter',side_effect=AssertionError('archive writer')):
   build(data,driver,driver_only=True)
  expected={'duel_case_sources.cmake','duel_cases.inc','duel_handler_bindings.inc'}
  self.assertEqual({p.name for p in driver.iterdir()},expected)
  for name in expected:self.assertEqual((driver/name).read_bytes(),(full/name).read_bytes(),name)
  stamps={name:(driver/name).stat().st_mtime_ns for name in expected};build(data,driver,driver_only=True)
  self.assertEqual({name:(driver/name).stat().st_mtime_ns for name in expected},stamps)

 def test_driver_only_does_not_modify_existing_author_archive_or_catalog(self):
  from generate_cases import build
  data=self.driver_data();output=Path(self.temp.name)/'driver';output.mkdir()
  names=('duel_cases.bin','manifest.json','inventory.json','cases.cmake','case_semantics_sha256.json')
  for name in names:(output/name).write_bytes(('keep '+name).encode())
  before={name:((output/name).read_bytes(),(output/name).stat().st_mtime_ns) for name in names}
  build(data,output,driver_only=True)
  for name in names:self.assertEqual(((output/name).read_bytes(),(output/name).stat().st_mtime_ns),before[name])

 def test_driver_only_rejects_behavior_selectors_and_fingerprints_before_writing(self):
  from generate_cases import build
  output=Path(self.temp.name)/'must-not-exist'
  for arguments in (dict(case_files=['forest.json']),dict(csvids=[91]),dict(legacy_fingerprints=True)):
   with self.assertRaisesRegex(ValueError,'driver-only'):
    build(Path(self.temp.name)/'missing-data',output,driver_only=True,**arguments)
   self.assertFalse(output.exists())

if __name__=='__main__':unittest.main()
