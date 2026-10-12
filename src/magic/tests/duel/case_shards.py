"""Bound generated case-data translation units without altering case definitions."""
import os
MAX_CASE_SHARD_BYTES=8*1024*1024
PREAMBLE='// Generated semantic case data.\n#include "duel_case_types.h"\n'

def file_size(text):
 return len(text.encode('utf8'))+text.count('\n')*int(os.linesep=='\r\n')

class CaseShards:
 def __init__(self,output,limit=MAX_CASE_SHARD_BYTES):
  if type(limit) is not int or limit<=file_size(PREAMBLE):raise ValueError('positive usable shard byte limit required')
  self.output=output;self.limit=limit;self.parts=[];self.size=file_size(PREAMBLE);self.names=[];self.declarations=[];self.output.mkdir(parents=True,exist_ok=True)
 def add(self,index,parts):
  text=''.join(parts).replace(f'static const Case case_{index} =',f'extern const Case case_{index} =',1)
  size=file_size(text)
  if self.parts and self.size+size>self.limit:self.flush()
  self.parts.append(text);self.size+=size;self.declarations.append(f'extern const Case case_{index};\n')
 def flush(self):
  if not self.parts:return
  name=f'duel_case_shard_{len(self.names):04d}.cpp';text=PREAMBLE+''.join(self.parts)
  path=self.output/name
  if not path.exists() or path.read_text(encoding='utf8')!=text:path.write_text(text,encoding='utf8')
  self.names.append(name);self.parts=[];self.size=file_size(PREAMBLE)
 def finish(self):
  self.flush()
  lines=['set(duel_generated_case_sources\n']+[f'  "${{duel_generated}}/{name}"\n' for name in self.names]+[')\n']
  (self.output/'duel_case_sources.cmake').write_text(''.join(lines),encoding='utf8')
  return ''.join(self.declarations)
