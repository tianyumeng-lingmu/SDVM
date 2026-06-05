"""Python emulator for SDVM - load .dance and trace execution"""
import struct

class Value:
    VAL_NULL = 0
    VAL_INT = 1
    VAL_FLOAT = 2
    VAL_BOOL = 3
    VAL_STRING = 4
    
    def __init__(self, type=0, int_val=0, float_val=0.0, bool_val=False, str_idx=0):
        self.type = type
        self.int_val = int_val
        self.float_val = float_val
        self.bool_val = bool_val
        self.str_idx = str_idx

    def is_truthy(self):
        if self.type == self.VAL_NULL: return False
        if self.type == self.VAL_INT: return self.int_val != 0
        if self.type == self.VAL_FLOAT: return self.float_val != 0.0
        if self.type == self.VAL_BOOL: return self.bool_val
        if self.type == self.VAL_STRING: return True
        return False
    
    def __repr__(self):
        types = {0:'null',1:'int',2:'float',3:'bool',4:'str'}
        t = types.get(self.type, f'?{self.type}')
        if self.type == self.VAL_NULL: return 'null'
        if self.type == self.VAL_INT: return str(self.int_val)
        if self.type == self.VAL_FLOAT: return str(self.float_val)
        if self.type == self.VAL_BOOL: return 'true' if self.bool_val else 'false'
        if self.type == self.VAL_STRING: return f'str#{self.str_idx}'
        return f'?(type={self.type})'

OP_NAMES = {
    0x00:'NOP',0x01:'ICONST',0x02:'FCONST',0x03:'SCONST',
    0x04:'BCONST',0x05:'NULL',0x06:'DUP',0x07:'POP',
    0x10:'LOAD',0x11:'STORE',0x20:'ADD',0x21:'SUB',
    0x22:'MUL',0x23:'DIV',0x24:'MOD',0x25:'NEG',
    0x30:'EQ',0x31:'NE',0x32:'LT',0x33:'GT',
    0x34:'LE',0x35:'GE',0x38:'NOT',
    0x40:'JMP',0x41:'JIF',0x42:'BIF',0x43:'RET',0x44:'HALT',
    0x50:'CALL',0x51:'ANON',0x52:'CALLR',
}

def load_dance(path):
    with open(path, 'rb') as f:
        data = f.read()
    
    off = 4  # skip magic
    version = struct.unpack('<I', data[off:off+4])[0]; off += 4
    str_count = struct.unpack('<I', data[off:off+4])[0]; off += 4
    
    strings = []
    for _ in range(str_count):
        slen = struct.unpack('<I', data[off:off+4])[0]; off += 4
        s = data[off:off+slen].decode('utf-8'); off += slen
        strings.append(s)
    
    func_count = struct.unpack('<I', data[off:off+4])[0]; off += 4
    funcs = []
    for _ in range(func_count):
        name_idx = struct.unpack('<I', data[off:off+4])[0]; off += 4
        arg_count = struct.unpack('<I', data[off+0:off+4])[0]; off += 4
        local_count = struct.unpack('<I', data[off+0:off+4])[0]; off += 4
        code_offset = struct.unpack('<I', data[off+0:off+4])[0]; off += 4
        funcs.append({'name_idx': name_idx, 'arg_count': arg_count,
                      'local_count': local_count, 'code_offset': code_offset})
    
    code_size = struct.unpack('<I', data[off:off+4])[0]; off += 4
    code = data[off:off+code_size]
    
    return strings, funcs, code

class Emulator:
    def __init__(self, strings, funcs, code):
        self.strings = strings
        self.funcs = funcs
        self.code = code
        self.stack = [Value() for _ in range(4096)]
        self.sp = -1
        self.ip = 0
        self.locals = [Value() for _ in range(256)]
        self.local_count = 0
        
        # Call stack
        self.call_stack = []
        
        self.verbose = True
        self.insn_count = 0
    
    def push(self, v):
        self.sp += 1
        self.stack[self.sp] = v
    
    def pop(self):
        v = self.stack[self.sp]
        self.sp -= 1
        return v
    
    def peek(self):
        return self.stack[self.sp]
    
    def read_u8(self):
        v = self.code[self.ip]
        self.ip += 1
        return v
    
    def read_i32(self):
        v = struct.unpack('<i', self.code[self.ip:self.ip+4])[0]
        self.ip += 4
        return v
    
    def read_u32(self):
        v = struct.unpack('<I', self.code[self.ip:self.ip+4])[0]
        self.ip += 4
        return v
    
    def read_f64(self):
        v = struct.unpack('<d', self.code[self.ip:self.ip+8])[0]
        self.ip += 8
        return v
    
    def trace(self, op_name):
        if not self.verbose:
            return
        print(f"[{self.ip-1:04d}] {op_name}", end="")
    
    def run(self):
        self.sp = -1
        self.ip = 0
        self.insn_count = 0
        max_insn = 10000
        
        # Init locals for func 0
        if len(self.funcs) > 0:
            lc = self.funcs[0]['local_count']
            for i in range(lc):
                self.locals[i] = Value()
            self.local_count = lc
        
        while self.ip < len(self.code):
            self.insn_count += 1
            if self.insn_count > max_insn:
                print(f"\n*** INSTRUCTION LIMIT ({max_insn}) REACHED - INFINITE LOOP? ***")
                return -1
            
            op = self.read_u8()
            op_name = OP_NAMES.get(op, f'UNK({op:#04x})')
            
            if op == 0x00:  # NOP
                self.trace(op_name); print()
            elif op == 0x01:  # ICONST
                v = self.read_i32()
                self.trace(op_name); print(f" {v}")
                self.push(Value(Value.VAL_INT, int_val=v))
            elif op == 0x02:  # FCONST
                v = self.read_f64()
                self.trace(op_name); print(f" {v}")
                self.push(Value(Value.VAL_FLOAT, float_val=v))
            elif op == 0x03:  # SCONST
                idx = self.read_u32()
                self.trace(op_name); print(f" #{idx} ('{self.strings[idx]}')")
                self.push(Value(Value.VAL_STRING, str_idx=idx))
            elif op == 0x04:  # BCONST
                v = self.read_u8()
                self.trace(op_name); print(f" {v}")
                self.push(Value(Value.VAL_BOOL, bool_val=(v != 0)))
            elif op == 0x05:  # NULL
                self.trace(op_name); print()
                self.push(Value())
            elif op == 0x06:  # DUP
                self.trace(op_name); print()
                self.push(self.peek())
            elif op == 0x07:  # POP
                self.trace(op_name); print()
                self.pop()
            elif op == 0x10:  # LOAD
                idx = self.read_u8()
                self.trace(op_name); print(f" r{idx} = {self.locals[idx]}")
                self.push(self.locals[idx])
            elif op == 0x11:  # STORE
                idx = self.read_u8()
                val = self.pop()
                self.trace(op_name); print(f" r{idx} = {val}")
                self.locals[idx] = val
            elif op == 0x20:  # ADD
                self.trace(op_name); print()
                b = self.pop(); a = self.pop()
                if a.type == Value.VAL_INT and b.type == Value.VAL_INT:
                    self.push(Value(Value.VAL_INT, int_val=a.int_val + b.int_val))
                else:
                    print(f"  *** ADD type mismatch: {a.type}, {b.type} ***")
                    return -1
            elif op == 0x21:  # SUB
                self.trace(op_name); print()
                b = self.pop(); a = self.pop()
                self.push(Value(Value.VAL_INT, int_val=a.int_val - b.int_val))
            elif op == 0x22:  # MUL
                self.trace(op_name); print()
                b = self.pop(); a = self.pop()
                self.push(Value(Value.VAL_INT, int_val=a.int_val * b.int_val))
            elif op == 0x23:  # DIV
                self.trace(op_name); print()
                b = self.pop(); a = self.pop()
                self.push(Value(Value.VAL_INT, int_val=a.int_val // b.int_val))
            elif op == 0x31:  # NE
                self.trace(op_name); print()
                b = self.pop(); a = self.pop()
                self.push(Value(Value.VAL_BOOL, bool_val=(a.int_val != b.int_val)))
            elif op == 0x32:  # LT
                self.trace(op_name); print()
                b = self.pop(); a = self.pop()
                result = 1 if a.int_val < b.int_val else 0
                self.push(Value(Value.VAL_INT, int_val=result))
            elif op == 0x33:  # GT
                self.trace(op_name); print()
                b = self.pop(); a = self.pop()
                result = 1 if a.int_val > b.int_val else 0
                self.push(Value(Value.VAL_INT, int_val=result))
            elif op == 0x38:  # NOT
                self.trace(op_name); print()
                a = self.pop()
                self.push(Value(Value.VAL_BOOL, bool_val=(not a.is_truthy())))
            elif op == 0x40:  # JMP
                offset = self.read_i32()
                target = self.ip + offset
                self.trace(op_name); print(f" {offset:+d} (-> {target:04d})")
                self.ip = target
            elif op == 0x41:  # JIF
                offset = self.read_i32()
                cond = self.pop()
                target = self.ip + offset
                self.trace(op_name); print(f" {offset:+d} (cond={cond}, truthy={cond.is_truthy()}, target={target:04d})")
                if not cond.is_truthy():
                    self.ip = target
            elif op == 0x42:  # BIF
                bif_idx = self.read_u8()
                argc = self.read_u8()
                bif_names = {0:'see',1:'int',2:'float',3:'str',4:'bool',5:'type'}
                bname = bif_names.get(bif_idx, f'bif{bif_idx}')
                self.trace(op_name); print(f" {bname}({argc})")
                
                if bif_idx == 0:  # see
                    args = []
                    for _ in range(argc):
                        args.insert(0, self.pop())
                    for a in args:
                        if a.type == Value.VAL_STRING:
                            print(self.strings[a.str_idx], end='')
                        elif a.type == Value.VAL_INT:
                            print(a.int_val, end='')
                        elif a.type == Value.VAL_FLOAT:
                            print(a.float_val, end='')
                        elif a.type == Value.VAL_BOOL:
                            print('true' if a.bool_val else 'false', end='')
                        else:
                            print('null', end='')
                else:
                    for _ in range(argc):
                        self.pop()
            elif op == 0x43:  # RET
                self.trace(op_name); print()
                ret_val = self.pop() if self.sp >= 0 else Value()
                if self.call_stack:
                    saved = self.call_stack.pop()
                    self.locals = saved['locals']
                    self.local_count = saved['local_count']
                    self.sp = saved['return_sp']
                    self.ip = saved['return_ip']
                    self.push(ret_val)
                else:
                    return 0
            elif op == 0x44:  # HALT
                self.trace(op_name); print()
                return 0
            elif op == 0x50:  # CALL
                func_idx = self.read_u32()
                arg_count = self.read_u8()
                self.trace(op_name); print(f" func[{func_idx}] args={arg_count}")
                
                # Save state
                saved_locals = list(self.locals)
                saved_local_count = self.local_count
                return_sp = self.sp - arg_count
                return_ip = self.ip
                
                # Get target function
                func = self.funcs[func_idx]
                
                # Copy args
                new_locals = [Value() for _ in range(256)]
                base = self.sp - arg_count + 1
                for i in range(min(arg_count, func['local_count'])):
                    new_locals[i] = self.stack[base + i]
                for i in range(arg_count, func['local_count']):
                    new_locals[i] = Value()
                
                # Pop args
                self.sp -= arg_count
                
                # Push to call stack
                self.call_stack.append({
                    'locals': saved_locals,
                    'local_count': saved_local_count,
                    'return_sp': return_sp,
                    'return_ip': return_ip,
                })
                
                # Jump to function
                self.locals = new_locals
                self.local_count = func['local_count']
                self.ip = func['code_offset']
            elif op == 0x51:  # ANON
                fi = self.read_u32()
                self.trace(op_name); print(f" func[{fi}]")
                # Push anonymous function reference (simplified: push null)
                self.push(Value())
            elif op == 0x52:  # CALLR
                ac = self.read_u8()
                self.trace(op_name); print(f" args={ac}")
                # Inline method call - simplified
            else:
                print(f"UNKNOWN OPCODE: {op:#04x} at {self.ip-1:04d}")
                return -1
            
            # Debug: check sp
            if self.sp > 100:
                print(f"\n*** STACK GROWTH: sp={self.sp} ***")
        
        return 0

if __name__ == '__main__':
    import sys
    log_path = r'D:\stay\SDVM\emulate_log.txt'
    with open(log_path, 'w', encoding='utf-8') as log:
        sys.stdout = log
        strings, funcs, code = load_dance(r'D:\stay\SDVM\test_debug.dance')
        print(f"Strings: {len(strings)}, Funcs: {len(funcs)}, Code: {len(code)}B")
        emu = Emulator(strings, funcs, code)
        result = emu.run()
        print(f"\nResult: {result}, insn_count: {emu.insn_count}")
    sys.stdout = sys.__stdout__
    print(f"Emulation complete. See {log_path}")
