import ctypes, pathlib, subprocess, tempfile
root=pathlib.Path(__file__).parent
with tempfile.TemporaryDirectory() as d:
 so=pathlib.Path(d)/'p.so'; subprocess.run(['cc','-shared','-fPIC',str(root/'protocol.c'),'-o',str(so)],check=True)
 p=ctypes.CDLL(str(so)); U8=ctypes.c_uint8
 out=(U8*16)(); assert p.lw_encode(out,1,42,9,1234)==0
 k=ctypes.c_uint8(); ident=ctypes.c_uint16(); seq=ctypes.c_uint32(); val=ctypes.c_int32()
 assert p.lw_decode(out,16,ctypes.byref(k),ctypes.byref(ident),ctypes.byref(seq),ctypes.byref(val))==0
 assert (k.value,ident.value,seq.value,val.value)==(1,42,9,1234)
 damaged=(U8*16).from_buffer_copy(bytes(out)); damaged[6]^=1
 assert p.lw_decode(damaged,16,ctypes.byref(k),ctypes.byref(ident),ctypes.byref(seq),ctypes.byref(val))==-2
 assert p.lw_decode(out,15,ctypes.byref(k),ctypes.byref(ident),ctypes.byref(seq),ctypes.byref(val))==-1
 print('protocol round-trip, CRC rejection, and truncation checks passed')
