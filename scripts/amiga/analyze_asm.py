import re

with open('SWOS2.s', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Find ALL labels (line_idx, name)
all_labels = []
for i, l in enumerate(lines):
    m = re.match(r'^([A-Za-z]\w+):\s*$', l)
    if m:
        all_labels.append((i, m.group(1)))

# Build a set of names that are sub-labels (lowercase start, contains _ followed by lowercase)
# Real functions: CamelCase, ALL_CAPS constants, or known patterns

# Better approach: find functions by looking for labels that are referenced by xdef or bsr/jsr targets
# But simplest: filter to CamelCase labels (first letter uppercase) as main entry points

def is_main_function(name):
    """Heuristic: main function labels start with uppercase and are CamelCase"""
    if name[0].isupper():
        return True
    # Also include known patterns like j_start, nullsub_N
    if name.startswith('j_') or name.startswith('nullsub_'):
        return True
    return False

# Filter for main function labels
main_funcs = [(i, n) for i, n in all_labels if is_main_function(n)]

# For each main function, determine body size (lines until next main function label)
func_info = []
for idx, (line_idx, name) in enumerate(main_funcs):
    # Skip data prefixes
    if any(name.startswith(p) for p in ['BSS_', 'CODE_']):
        continue

    # Find end (next main function label)
    if idx + 1 < len(main_funcs):
        end_idx = main_funcs[idx + 1][0]
    else:
        end_idx = len(lines)

    # Check first instruction
    first_instr = None
    is_data = False
    code_lines = 0

    for j in range(line_idx + 1, min(end_idx, len(lines))):
        stripped = lines[j].strip()
        if not stripped or stripped.startswith(';') or stripped.startswith('*'):
            continue
        # Sub-label
        if re.match(r'^\w+:', stripped):
            continue
        # Data directive
        if re.match(r'(ds\.|dc\.|dcb\.|even|align|SECTION)', stripped):
            is_data = True
            break
        # Instruction
        if first_instr is None:
            first_instr = stripped
        code_lines += 1

    if is_data:
        continue

    if first_instr is None:
        continue

    func_info.append((line_idx + 1, name, first_instr, code_lines))

# Classify
ported = []
dead = []
alive = []

for line_no, name, first_instr, body_lines in func_info:
    if re.search(r'jmp\s+_', first_instr):
        ported.append((line_no, name))
    elif first_instr == 'rts' and body_lines <= 1:
        dead.append((line_no, name))
    else:
        alive.append((line_no, name, first_instr, body_lines))

print(f"Main function labels: {len(func_info)}")
print(f"Ported (jmp _XXX): {len(ported)}")
print(f"Dead (rts): {len(dead)}")
print(f"ALIVE (unported): {len(alive)}")

# Group alive by size
small = [f for f in alive if f[3] < 15]
medium = [f for f in alive if 15 <= f[3] < 50]
large = [f for f in alive if 50 <= f[3] < 150]
xlarge = [f for f in alive if f[3] >= 150]

print(f"\nSize breakdown: S(<15)={len(small)} M(15-49)={len(medium)} L(50-149)={len(large)} XL(150+)={len(xlarge)}")

print("\n=== LARGE + XLARGE FUNCTIONS ===")
for line_no, name, first_instr, body_lines in sorted(large + xlarge, key=lambda x: -x[3]):
    print(f"  Line {line_no:6d}: {name:<50s} ({body_lines:4d} lines)")

print("\n=== MEDIUM FUNCTIONS ===")
for line_no, name, first_instr, body_lines in sorted(medium, key=lambda x: x[0]):
    print(f"  Line {line_no:6d}: {name:<50s} ({body_lines:3d} lines)")

print("\n=== SMALL FUNCTIONS ===")
for line_no, name, first_instr, body_lines in sorted(small, key=lambda x: x[0]):
    print(f"  Line {line_no:6d}: {name:<50s} ({body_lines:3d} lines)")
