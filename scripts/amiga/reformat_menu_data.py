#!/usr/bin/env python3
"""Reformat C struct initializers for consistent style.

Rules:
- Leaf brace groups (no nested braces): content on one line
    {
        0, 31, 304, 116
    },
- Branch brace groups: children on separate lines
    {
        { ... },
        { ... },
        MENU_CMD_END
    },
- Standalone values (MENU_CMD_END, MENU_MARK_*, etc.): own line
"""

import sys


def tokenize_initializer(text):
    """Tokenize a C initializer into meaningful tokens."""
    tokens = []
    i = 0
    while i < len(text):
        c = text[i]

        # Skip whitespace
        if c in ' \t\n\r':
            i += 1
            continue

        # Block comment
        if c == '/' and i + 1 < len(text) and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            if j == -1:
                j = len(text)
            else:
                j += 2
            tokens.append(text[i:j])
            i = j
            continue

        # Line comment
        if c == '/' and i + 1 < len(text) and text[i + 1] == '/':
            j = text.find('\n', i)
            if j == -1:
                j = len(text)
            tokens.append(text[i:j])
            i = j
            continue

        # Simple single-char tokens
        if c in '{}=;':
            tokens.append(c)
            i += 1
            continue

        if c == ',':
            tokens.append(',')
            i += 1
            continue

        # & operator (address-of)
        if c == '&':
            # Merge & with following identifier
            i += 1
            while i < len(text) and text[i] in ' \t':
                i += 1
            if i < len(text) and (text[i].isalpha() or text[i] == '_'):
                j = i
                while j < len(text) and (text[j].isalnum() or text[j] == '_'):
                    j += 1
                tokens.append('&' + text[i:j])
                i = j
            else:
                tokens.append('&')
            continue

        # Cast expressions like (char *)
        if c == '(':
            j = i + 1
            depth = 1
            while j < len(text) and depth > 0:
                if text[j] == '(':
                    depth += 1
                elif text[j] == ')':
                    depth -= 1
                j += 1
            tok = text[i:j].strip()
            tokens.append(tok)
            i = j
            # If cast is followed by a value, merge them
            # e.g. (char *) 0x4 -> (char *)0x4
            while i < len(text) and text[i] in ' \t':
                i += 1
            if i < len(text) and (text[i].isdigit() or text[i] == '0' or
                                   text[i].isalpha() or text[i] == '_' or
                                   text[i] == '&' or text[i] == '-'):
                # Next token will be collected separately, let the cast stay separate
                pass
            continue

        # String literal
        if c == '"':
            j = i + 1
            while j < len(text) and text[j] != '"':
                if text[j] == '\\':
                    j += 1
                j += 1
            j += 1
            tokens.append(text[i:j])
            i = j
            continue

        # Number (possibly negative, hex, with suffixes)
        if c.isdigit() or (c == '-' and i + 1 < len(text) and text[i + 1].isdigit()):
            j = i
            if c == '-':
                j += 1
            if j + 1 < len(text) and text[j] == '0' and text[j + 1] in 'xX':
                j += 2
                while j < len(text) and text[j] in '0123456789abcdefABCDEF':
                    j += 1
            else:
                while j < len(text) and text[j].isdigit():
                    j += 1
            # Suffixes
            while j < len(text) and text[j] in 'lLuU':
                j += 1
            tokens.append(text[i:j])
            i = j
            continue

        # Identifier
        if c.isalpha() or c == '_':
            j = i
            while j < len(text) and (text[j].isalnum() or text[j] == '_'):
                j += 1
            tokens.append(text[i:j])
            i = j
            continue

        # Anything else
        tokens.append(c)
        i += 1

    return tokens


def find_matching_brace(tokens, start):
    """Find index of matching '}' for '{' at start."""
    depth = 0
    for i in range(start, len(tokens)):
        if tokens[i] == '{':
            depth += 1
        elif tokens[i] == '}':
            depth -= 1
            if depth == 0:
                return i
    return -1


def has_nested_braces(tokens, start, end):
    """Check if tokens[start:end] contain any braces."""
    for i in range(start, end):
        if tokens[i] in ('{', '}'):
            return True
    return False


def leaf_content_to_line(tokens):
    """Join leaf brace content tokens into a single line."""
    parts = []
    i = 0
    while i < len(tokens):
        tok = tokens[i]
        if tok == ',':
            if parts:
                parts[-1] += ','
            i += 1
            continue
        # Handle cast + value: (char *) followed by value
        if tok.startswith('(') and tok.endswith(')') and i + 1 < len(tokens) and tokens[i + 1] != ',':
            parts.append(tok + tokens[i + 1])
            i += 2
            continue
        parts.append(tok)
        i += 1
    return ' '.join(parts)


def format_brace_group(tokens, indent_level):
    """Format tokens that are inside a brace group (between { and })."""
    indent = '    ' * indent_level
    lines = []
    i = 0

    while i < len(tokens):
        tok = tokens[i]

        if tok == ',':
            # Trailing comma - attach to previous line
            if lines and not lines[-1].rstrip().endswith(','):
                lines[-1] = lines[-1].rstrip() + ',\n'
            i += 1
            continue

        if tok == '{':
            match = find_matching_brace(tokens, i)
            if match == -1:
                lines.append(indent + tok + '\n')
                i += 1
                continue

            inner_tokens = tokens[i + 1:match]

            if not has_nested_braces(tokens, i + 1, match):
                # Leaf: single line { content }
                content = leaf_content_to_line(list(inner_tokens))
                lines.append(indent + '{ ' + content + ' }')

                i = match + 1
                if i < len(tokens) and tokens[i] == ',':
                    lines[-1] += ','
                    i += 1
                lines[-1] += '\n'
            else:
                # Branch: recurse
                lines.append(indent + '{\n')
                inner_formatted = format_brace_group(inner_tokens, indent_level + 1)
                lines.append(inner_formatted)
                lines.append(indent + '}')

                i = match + 1
                if i < len(tokens) and tokens[i] == ',':
                    lines[-1] += ','
                    i += 1
                lines[-1] += '\n'
        else:
            # Standalone value — emit ONE value per line
            lines.append(indent + tok + ',\n')
            i += 1

    return ''.join(lines)


def format_initializer(init_text):
    """Format a complete initializer (everything between outer { and })."""
    tokens = tokenize_initializer(init_text)

    if not tokens or tokens[0] != '{':
        return init_text

    match = find_matching_brace(tokens, 0)
    if match == -1 or match != len(tokens) - 1:
        return init_text

    inner = tokens[1:-1]
    result = '{\n'
    result += format_brace_group(inner, 1)
    result += '}'
    return result


def process_file(filepath):
    """Process a C file and reformat all struct initializers."""
    with open(filepath, 'r') as f:
        content = f.read()

    output = []
    pos = 0

    while pos < len(content):
        # Find next " = {"
        eq_brace = content.find(' = {', pos)
        if eq_brace == -1:
            output.append(content[pos:])
            break

        # Find the start of the declaration line
        line_start = content.rfind('\n', 0, eq_brace)
        if line_start == -1:
            line_start = 0
        else:
            line_start += 1

        # Check this looks like a struct init
        decl_part = content[line_start:eq_brace].strip()
        if not decl_part or decl_part.startswith('//') or decl_part.startswith('/*'):
            output.append(content[pos:eq_brace + 4])
            pos = eq_brace + 4
            continue

        # Output everything before " = "
        output.append(content[pos:eq_brace + 3])  # includes " = "

        # Find matching } for the { at eq_brace + 3
        brace_start = eq_brace + 3
        depth = 0
        i = brace_start
        while i < len(content):
            if content[i] == '{':
                depth += 1
            elif content[i] == '}':
                depth -= 1
                if depth == 0:
                    break
            elif content[i] == '"':
                i += 1
                while i < len(content) and content[i] != '"':
                    if content[i] == '\\':
                        i += 1
                    i += 1
            elif content[i] == '/' and i + 1 < len(content) and content[i + 1] == '*':
                j = content.find('*/', i + 2)
                if j != -1:
                    i = j + 1
            i += 1

        if depth != 0:
            output.append(content[brace_start:])
            pos = len(content)
            continue

        brace_end = i

        # Extract and format the initializer
        init_text = content[brace_start:brace_end + 1]
        formatted = format_initializer(init_text)
        output.append(formatted)

        pos = brace_end + 1

    return ''.join(output)


if __name__ == '__main__':
    filepath = sys.argv[1] if len(sys.argv) > 1 else 'src/menu_data.c'
    result = process_file(filepath)
    with open(filepath, 'w') as f:
        f.write(result)
    print(f"Reformatted {filepath}")
