#!/usr/bin/env python3
"""
Strip dead code bodies from SWOS2.s after ported function stubs.

SAFE version: checks that no labels in a dead code region are referenced
from outside that region before removing it.
"""
import re
import sys

def main():
    with open('SWOS2.s', 'r') as f:
        lines = f.readlines()

    # Build index: for each label, list of line numbers where it's referenced
    # (not where it's defined)
    label_ref_lines = {}  # label -> set of line numbers
    label_def_lines = {}  # label -> line number of definition

    ident_re = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')
    mnemonics = frozenset([
        'dc', 'ds', 'dcb', 'move', 'movem', 'moveq', 'movea', 'lea',
        'jsr', 'jmp', 'bsr', 'bra', 'rts', 'rte', 'tst', 'cmp', 'cmpi',
        'cmpa', 'add', 'addi', 'addq', 'adda', 'sub', 'subi', 'subq',
        'suba', 'and', 'andi', 'or', 'ori', 'eor', 'eori', 'not', 'neg',
        'clr', 'ext', 'swap', 'exg', 'asl', 'asr', 'lsl', 'lsr', 'rol',
        'ror', 'roxl', 'roxr', 'bne', 'beq', 'bgt', 'bge', 'blt', 'ble',
        'bhi', 'bls', 'bcc', 'bcs', 'bpl', 'bmi', 'bvc', 'bvs', 'btst',
        'bset', 'bclr', 'bchg', 'dbf', 'dbra', 'dbne', 'dbeq', 'mulu',
        'muls', 'divu', 'divs', 'pea', 'link', 'unlk', 'trap', 'nop',
        'xdef', 'xref', 'section', 'include', 'even', 'end',
        'sp', 'a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7',
        'd0', 'd1', 'd2', 'd3', 'd4', 'd5', 'd6', 'd7',
        'sr', 'pc', 'ccr', 'usp', 'b', 'w', 'l', 's',
        'equ', 'set', 'scc', 'sne', 'seq', 'sgt', 'sge', 'slt', 'sle',
        'shi', 'sls', 'spl', 'smi', 'sf', 'st', 'addx', 'subx', 'negx',
    ])

    for lineno, line in enumerate(lines):
        s = line.strip()
        if not s or s.startswith(';'):
            continue

        # Check for label definition
        m = re.match(r'^([A-Za-z_][A-Za-z0-9_]*):', s)
        if m:
            label_def_lines[m.group(1)] = lineno
            s = s.split(':', 1)[1].strip()

        # Find all identifier references in the rest of the line
        for m in ident_re.finditer(s):
            ident = m.group(0)
            if ident.lower() in mnemonics:
                continue
            if ident not in label_ref_lines:
                label_ref_lines[ident] = set()
            label_ref_lines[ident].add(lineno)

    def labels_in_range(start, end):
        """Get labels defined in lines[start:end]"""
        labels = set()
        for k in range(start, end):
            m = re.match(r'^([A-Za-z_][A-Za-z0-9_]*):', lines[k])
            if m:
                labels.add(m.group(1))
        return labels

    def has_external_refs(labels, start, end):
        """Check if any labels are referenced from outside [start, end)"""
        range_set = set(range(start, end))
        for label in labels:
            refs = label_ref_lines.get(label, set())
            external = refs - range_set
            if external:
                return True
        return False

    def is_section_header(line):
        return line.startswith('; =====')

    def is_function_label(line):
        if not line or line[0] in ' \t':
            return False
        return bool(re.match(r'^[A-Z][A-Za-z0-9_]+:', line))

    def is_jmp_to_c(line):
        s = line.strip()
        return bool(re.match(r'jmp\s+_\w+', s))

    def is_dead_code_marker(line):
        s = line.strip().lower()
        return 'dead code' in s and s.startswith(';')

    data_decl_re = re.compile(r'^\s*(dc\.[bwl]|ds\.[bwl]|dcb\.[bwl])\b', re.IGNORECASE)

    def range_has_data_decls(start, end):
        """Check if any line in [start, end) contains a data declaration."""
        for k in range(start, end):
            if data_decl_re.match(lines[k]):
                return True
        return False

    # Process file
    output = []
    i = 0
    total_removed = 0

    while i < len(lines):
        line = lines[i]

        # Pattern 1: explicit dead code marker
        if is_dead_code_marker(line):
            j = i + 1
            while j < len(lines):
                if is_section_header(lines[j]) or is_function_label(lines[j]):
                    break
                j += 1

            labels = labels_in_range(i, j)
            if not has_external_refs(labels, i, j) and not range_has_data_decls(i, j):
                removed = j - i
                total_removed += removed
                i = j
                continue

            output.append(line)
            i += 1
            continue

        # Pattern 2: jmp _FunctionName followed by dead body
        if is_jmp_to_c(line.lstrip()):
            j = i + 1
            while j < len(lines) and lines[j].strip() == '':
                j += 1

            if j < len(lines):
                next_s = lines[j].strip()
                # Not dead code if next is header, data, label, xdef, or comment
                if is_section_header(lines[j]) or is_function_label(lines[j]) or \
                   next_s.startswith('xdef') or next_s.startswith(';') or \
                   re.match(r'(dc\.[bwl]|ds\.[bwl]|dcb\.[bwl]|even)\b', next_s):
                    output.append(line)
                    i += 1
                    continue

                # Find extent of dead code
                dead_start = j
                while j < len(lines):
                    if is_section_header(lines[j]) or is_function_label(lines[j]):
                        break
                    j += 1

                if j > dead_start:
                    labels = labels_in_range(dead_start, j)
                    if not has_external_refs(labels, dead_start, j) and \
                       not range_has_data_decls(dead_start, j):
                        removed = j - dead_start
                        if removed > 0:
                            total_removed += removed
                            output.append(line)
                            for k in range(i + 1, dead_start):
                                output.append(lines[k])
                            i = j
                            continue

            output.append(line)
            i += 1
            continue

        output.append(line)
        i += 1

    print(f"Removed {total_removed} lines of dead code", file=sys.stderr)

    with open('SWOS2.s', 'w') as f:
        f.writelines(output)

if __name__ == '__main__':
    main()
