"""Portable, indexed test data. Only the case being executed is decompressed."""
import hashlib
import json
import os
import struct
import zlib
from pathlib import Path

MAGIC=b'DUELCASE'
VERSION=1
HEADER=struct.Struct('<8sIIQQQII')
INDEX=struct.Struct('<IQIII')
SETUP=struct.Struct('<Iiiii')
STEP=struct.Struct('<IIIIiiiii')
MAX_RECORD=64*1024*1024


def replace_if_changed(path,temporary):
    path,temporary=Path(path),Path(temporary)
    if path.exists() and path.stat().st_size==temporary.stat().st_size:
        identical=False
        with path.open('rb') as old,temporary.open('rb') as new:
            while True:
                a,b=old.read(1024*1024),new.read(1024*1024)
                if a!=b:break
                if not a:
                    identical=True
                    break
        if identical:
            temporary.unlink()
            return False
    os.replace(temporary,path)
    return True


def write_text_if_changed(path,text,encoding='utf8'):
    path=Path(path)
    if not path.exists() or path.read_text(encoding=encoding)!=text:
        path.write_text(text,encoding=encoding)


def legacy_case_hash(index,metadata,cards,steps):
    """Same fingerprint as the former C++ arrays, excluding global ID changes."""
    q=lambda x:json.dumps(x,ensure_ascii=True)
    row=lambda values:'{'+','.join(q(v) if isinstance(v,str) else str(v) for v in values)+'}'
    card_rows=cards or [('',0,0,0,0)]
    chunks=[f'static const Setup case_cards_{index}[] = {{'+','.join(map(row,card_rows))+'};\n',
            f'static const Step case_steps_{index}[] = {{'+','.join(map(row,steps))+'};\n',
            f'extern const Case case_{index} = {{'+','.join([*(q(v) for v in metadata[:5]),str(metadata[5]),f'case_cards_{index}',str(len(cards)),f'case_steps_{index}',str(len(steps))])+'};\n']
    import re
    text=''.join(re.sub(r'case_(?:(?:cards|steps)_)?\d+','CASE_ID',s) for s in chunks)
    return hashlib.sha256(text.encode()).hexdigest()


class CaseArchiveWriter:
    def __init__(self,path):
        self.path=Path(path);self.path.parent.mkdir(parents=True,exist_ok=True)
        self.temporary=self.path.with_suffix(self.path.suffix+'.tmp')
        self.file=self.temporary.open('w+b');self.file.write(bytes(HEADER.size))
        self.strings=[];self.ids={};self.index=[]

    def intern(self,value):
        if not isinstance(value,str) or '\0' in value:raise ValueError('archive strings must be NUL-free text')
        if value not in self.ids:self.ids[value]=len(self.strings);self.strings.append(value)
        return self.ids[value]

    def add(self,metadata,cards,steps):
        name,card,oracle_id,oracle_text,fixture,seed=metadata
        ids=[self.intern(v) for v in metadata[:5]]
        raw=bytearray(struct.pack('<8I',*ids,seed,len(cards),len(steps)))
        for ref,player,zone,csvid,state in cards:raw.extend(SETUP.pack(self.intern(ref),player,zone,csvid,state))
        for op,ref,other,prop,*numbers in steps:raw.extend(STEP.pack(*(self.intern(v) for v in (op,ref,other,prop)),*numbers))
        if len(raw)>MAX_RECORD:raise ValueError('case exceeds archive record bound: '+name)
        payload=zlib.compress(raw,1)
        self.index.append((ids[0],self.file.tell(),len(payload),len(raw),zlib.crc32(raw)))
        self.file.write(payload)

    def finish(self):
        strings_offset=self.file.tell();table=bytearray(struct.pack('<I',len(self.strings)))
        for value in self.strings:
            encoded=value.encode('utf8');table.extend(struct.pack('<I',len(encoded)));table.extend(encoded)
        self.file.write(table);index_offset=self.file.tell()
        index=b''.join(INDEX.pack(*entry) for entry in self.index);self.file.write(index);size=self.file.tell()
        self.file.seek(0);self.file.write(HEADER.pack(MAGIC,VERSION,len(self.index),strings_offset,index_offset,size,zlib.crc32(table),zlib.crc32(index)))
        self.file.close();replace_if_changed(self.path,self.temporary)


class CaseArchiveReader:
    """Independent Python decoder for format tests and complete migration checks."""
    def __init__(self,path):
        self.file=Path(path).open('rb')
        try:self._read_index()
        except BaseException:
            self.file.close()
            raise

    def _read_index(self):
        size=self.file.seek(0,2);self.file.seek(0)
        header=self.file.read(HEADER.size)
        if len(header)!=HEADER.size:raise ValueError('truncated archive header')
        magic,version,count,strings_offset,index_offset,total,strings_crc,index_crc=HEADER.unpack(header)
        if magic!=MAGIC or version!=VERSION or size!=total or not HEADER.size<=strings_offset<=index_offset<=size or size-index_offset!=count*INDEX.size:raise ValueError('invalid archive header')
        self.data_end=strings_offset;self.file.seek(strings_offset);table=self.file.read(index_offset-strings_offset);index=self.file.read()
        if zlib.crc32(table)!=strings_crc or zlib.crc32(index)!=index_crc:raise ValueError('archive metadata checksum mismatch')
        self.strings=[];cursor=4
        if len(table)<4:raise ValueError('truncated string table')
        for _ in range(struct.unpack_from('<I',table)[0]):
            if cursor+4>len(table):raise ValueError('truncated string length')
            n=struct.unpack_from('<I',table,cursor)[0];cursor+=4
            if cursor+n>len(table):raise ValueError('truncated string')
            value=table[cursor:cursor+n].decode('utf8');cursor+=n
            if '\0' in value:raise ValueError('NUL in archive string')
            self.strings.append(value)
        if cursor!=len(table):raise ValueError('trailing string data')
        self.index=[INDEX.unpack_from(index,i*INDEX.size) for i in range(count)]
        self.names=[self.string(entry[0]) for entry in self.index]
        if len(set(self.names))!=count:raise ValueError('duplicate archive case name')
        for _,offset,packed,raw,_ in self.index:
            if offset<HEADER.size or offset+packed>strings_offset or not 32<=raw<=MAX_RECORD or packed>MAX_RECORD:raise ValueError('invalid case bounds')

    def string(self,index):
        if not 0<=index<len(self.strings):raise ValueError('invalid archive string ID')
        return self.strings[index]

    def load(self,index):
        name_id,offset,packed,raw_size,crc=self.index[index];self.file.seek(offset);packed_data=self.file.read(packed)
        decoder=zlib.decompressobj();raw=decoder.decompress(packed_data,raw_size+1)
        if len(raw)!=raw_size or not decoder.eof or decoder.unused_data or zlib.crc32(raw)!=crc:raise ValueError('invalid compressed case/checksum')
        header=struct.unpack_from('<8I',raw);metadata=tuple(self.string(i) for i in header[:5])+(header[5],)
        if header[0]!=name_id or 32+header[6]*SETUP.size+header[7]*STEP.size!=len(raw):raise ValueError('invalid record shape')
        cards=[];steps=[];cursor=32
        for _ in range(header[6]):
            row=SETUP.unpack_from(raw,cursor);cursor+=SETUP.size;cards.append((self.string(row[0]),*row[1:]))
        for _ in range(header[7]):
            row=STEP.unpack_from(raw,cursor);cursor+=STEP.size;steps.append((*map(self.string,row[:4]),*row[4:]))
        return metadata,cards,steps

    def close(self):self.file.close()
