"""Check data fidelity, compiler bounds, and deterministic shard inventories."""
import tempfile,unittest
from pathlib import Path
from case_shards import CaseShards,PREAMBLE,file_size

def definition(i,payload=''):
 return [f'static const Setup case_cards_{i}[] = {{{{"source",0,1,91,0}}}};\n',
  f'static const Step case_steps_{i}[] = {{{{"{payload}","","","",0,0,0,0,0}}}};\n',
  f'static const Case case_{i} = {{"Case_{i}","Forest","id","text","empty_main",123,case_cards_{i},1,case_steps_{i},1}};\n']

class CaseShardsTest(unittest.TestCase):
 def test_case_statements_preserved_across_translation_units(self):
  with tempfile.TemporaryDirectory() as temporary:
   out=Path(temporary);writer=CaseShards(out,600);original=[]
   for i in range(6):parts=definition(i);original+=parts;writer.add(i,parts)
   declarations=writer.finish();assembled=''.join((out/name).read_text(encoding='utf8').removeprefix(PREAMBLE).replace('extern const Case','static const Case') for name in writer.names)
   self.assertEqual(assembled,''.join(original));self.assertEqual(declarations,''.join(f'extern const Case case_{i};\n' for i in range(6)))
   self.assertGreater(len(writer.names),1);self.assertTrue(all((out/name).stat().st_size<=600 for name in writer.names))
 def test_utf8_and_platform_newlines_count_toward_bound(self):
  with tempfile.TemporaryDirectory() as temporary:
   out=Path(temporary);size=file_size(PREAMBLE)+file_size(''.join(definition(0,'é'*25)).replace('static const Case','extern const Case'))
   writer=CaseShards(out,size)
   for i in range(2):writer.add(i,definition(i,'é'*25))
   writer.finish();self.assertEqual(len(writer.names),2);self.assertTrue(all((out/name).stat().st_size<=size for name in writer.names))
 def test_large_individual_case_is_not_split_or_lost(self):
  with tempfile.TemporaryDirectory() as temporary:
   out=Path(temporary);writer=CaseShards(out,200);writer.add(0,definition(0,'x'*600));writer.add(1,definition(1));writer.finish()
   first=(out/writer.names[0]).read_text();self.assertIn('case_cards_0',first);self.assertIn('case_steps_0',first);self.assertIn('case_0 =',first);self.assertNotIn('case_1 =',first)
 def test_stale_files_are_not_linked(self):
  with tempfile.TemporaryDirectory() as temporary:
   out=Path(temporary);stale=out/'duel_case_shard_9999.cpp';stale.write_text('unused');writer=CaseShards(out);writer.add(0,definition(0));writer.finish()
   cmake=(out/'duel_case_sources.cmake').read_text();self.assertNotIn(stale.name,cmake);self.assertIn('${duel_generated}/'+writer.names[0],cmake);self.assertEqual(stale.read_text(),'unused')
 def test_unchanged_data_shards_keep_timestamps(self):
  with tempfile.TemporaryDirectory() as temporary:
   out=Path(temporary);writer=CaseShards(out);writer.add(0,definition(0));writer.finish();path=out/writer.names[0];stamp=path.stat().st_mtime_ns
   writer=CaseShards(out);writer.add(0,definition(0));writer.finish();self.assertEqual(path.stat().st_mtime_ns,stamp)
 def test_empty_finish_and_invalid_bounds(self):
  with tempfile.TemporaryDirectory() as temporary:
   out=Path(temporary)
   for limit in (True,-1,0,file_size(PREAMBLE)):
    with self.assertRaises(ValueError):CaseShards(out,limit)
   writer=CaseShards(out);self.assertEqual(writer.finish(),'');self.assertEqual(writer.names,[])

if __name__=='__main__':unittest.main()
