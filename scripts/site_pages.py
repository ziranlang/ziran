#!/usr/bin/env python3
"""Regenerate the site pages that are built from the repository.

site/examples.html lists every program in site/examples with the output the
portable runner prints for it, and site/std.html lists the public
declarations of every standard module as `ziran api` reports them. With
--check, report pages that differ from what the toolchain produces now.

    python3 scripts/site_pages.py --ziran build/bin/ziran [--check]
"""
import argparse
import html
import json
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SITE = os.path.join(REPO, 'site')
EXAMPLES = os.path.join(SITE, 'examples')
STD = os.path.join(REPO, 'std')

# Reading order for the examples page: first steps, then data, then
# complete programs. Examples not listed here follow in name order.
EXAMPLE_ORDER = ['hello', 'greet', 'values', 'variables', 'functions',
                 'conditions', 'loops', 'arrays', 'structs', 'enums',
                 'text_basics', 'defer', 'fizzbuzz', 'primes', 'score',
                 'text_demo']
EXAMPLE_TITLES = {'hello': 'Hello, World!: the smallest complete program.',
                  'greet': 'Print values: each % in a print format takes the next argument, and %% prints a percent sign.',
                  'conditions': 'Conditions: if and else chains, and an if-case that picks a branch by value.',
                  'defer': 'Defer: a deferred statement runs when its scope ends, last deferred first.',
                  'score': 'Model a score: define a record, pass it to a procedure, and return the answer.',
                  'text_demo': 'Use a standard module: import the text module and test a prefix with ASCII case folding.'}

GENERATED = re.compile(r'(<!-- generated:(\w+) -->)(.*?)(<!-- /generated:\2 -->)', re.S)


def run(args, cwd=REPO):
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f'{" ".join(args)} failed:\n{result.stdout}{result.stderr}')
    return result.stdout


def code(text):
    return html.escape(text, quote=False)


def leading_comment(lines):
    """Return the // block at the top of a file and the lines after it."""
    comment = []
    index = 0
    while index < len(lines) and lines[index].startswith('//'):
        comment.append(lines[index][2:].strip())
        index += 1
    while index < len(lines) and not lines[index].strip():
        index += 1
    return ' '.join(comment), lines[index:]


def example_sections(ziran):
    names = sorted(f[:-3] for f in os.listdir(EXAMPLES) if f.endswith('.zi'))
    names = [n for n in EXAMPLE_ORDER if n in names] + [n for n in names if n not in EXAMPLE_ORDER]
    sections, toc = [], []
    with tempfile.TemporaryDirectory() as work:
        for number, name in enumerate(names, 1):
            source = open(os.path.join(EXAMPLES, name + '.zi')).read()
            comment, body = leading_comment(source.rstrip('\n').split('\n'))
            summary = EXAMPLE_TITLES.get(name, comment)
            title, _, description = summary.partition(': ')
            if not description:
                sys.exit(f'site/examples/{name}.zi needs a "// Title: description" first comment')
            entry = 'main' if re.search(r'^main :: \(\)', source, re.M) else 'Answer'
            imports = re.search(r'^#import "', source, re.M) is not None
            module_path = ' --module-path std' if imports else ''
            path = f'site/examples/{name}.zi'
            bundle = os.path.join(work, name + '.zib')
            run([ziran, 'bundle', '--root', '.'] + (['--module-path', 'std'] if imports else [])
                + ['--entry', f'{name}:{entry}', '-o', bundle, path])
            output = run([ziran, 'run', bundle])
            commands = (f'build/bin/ziran bundle --root .{module_path} --entry {name}:{entry} '
                        f'-o {name}.zib {path}\nbuild/bin/ziran run {name}.zib\n'
                        + ''.join(f'# {line}\n' for line in output.rstrip('\n').split('\n')))
            anchor = name.replace('_', '-')
            description = description[:1].upper() + description[1:]
            sections.append(
                f'<section id="{anchor}"><p class="eyebrow">{number:02d} / {code(name)}.zi</p>'
                f'<h2>{code(title)}</h2><p>{code(description)}</p>'
                f'<pre><code>{code(chr(10).join(body))}</code></pre>'
                f'<pre><code>{code(commands.rstrip(chr(10)))}</code></pre>'
                f'<p><a class="text-link" href="examples/{name}.zi" download>Download {name}.zi ↓</a></p></section>\n')
            toc.append(f'<a href="#{anchor}">{code(title)}</a>')
    return ''.join(sections), ''.join(toc)


def spaced(params):
    """Write parameters as `name: Type`, however the checker spaced them."""
    return re.sub(r'(\w)\s*:\s*(?!:)', r'\1: ', re.sub(r'\s*,\s*', ', ', params.strip()))


def doc_comment(lines, line, top):
    """The // block directly above a declaration, past its attribute lines.
    The comment that opens the file belongs to the module, not to this."""
    index = line - 2
    while index >= 0 and lines[index].startswith('#') and not lines[index].startswith('#import'):
        index -= 1
    block = []
    while index >= top and lines[index].startswith('//'):
        block.insert(0, lines[index][2:].strip())
        index -= 1
    return ' '.join(block)


def std_reference(ziran):
    modules = []
    for file in sorted(f for f in os.listdir(STD) if f.endswith('.zi')):
        name = file[:-3]
        data = json.loads(run([ziran, 'api', '--json', '--root', STD, '--module-path', STD,
                               os.path.join(STD, file)]))
        module = next(m for m in data['modules'] if m['source'] == file)
        lines = open(os.path.join(STD, file)).read().split('\n')
        # The comment that opens a file describes the module.
        top = 0
        while top < len(lines) and lines[top].startswith('//'):
            top += 1
        summary = ' '.join(l[2:].strip() for l in lines[:top])
        entries = []
        for kind in ('types', 'constants', 'globals', 'functions'):
            for item in module[kind]:
                if kind == 'types' and item.get('procedure'):
                    result = f' -> {item["return_type"]}' if item.get('return_type') not in ('', 'void', None) else ''
                    signature = f'{item["name"]} :: #type ({spaced(item.get("body", ""))}){result}'
                elif kind == 'types':
                    names = [n.strip() for n in item.get('type_parameters', '').split(',') if n.strip()]
                    params = '(' + ', '.join(f'${n}: Type' for n in names) + ')' if names else ''
                    shape = 'enum' if item.get('enum') else 'struct'
                    fields = [f.strip() for f in re.split(r'[;\n]', item.get('body', '')) if f.strip()]
                    body = ' '.join(f + ';' for f in fields)
                    signature = f'{item["name"]} :: {shape}{params} {{ {body} }}' if body else f'{item["name"]} :: {shape}{params}'
                elif kind == 'functions':
                    result = '' if item['return_type'] in ('', 'void') else f' -> {item["return_type"]}'
                    signature = f'{item["name"]} :: ({spaced(item["parameters"])}){result}'
                elif kind == 'constants':
                    signature = f'{item["name"]} :: {item.get("value", "")}'.rstrip(': ')
                else:
                    signature = f'{item["name"]}: {item.get("type", "")}'
                notes = []
                if kind == 'functions' and item.get('uses_host'):
                    notes.append('needs a host capability')
                if kind == 'functions' and item.get('must_use'):
                    notes.append('result must be used')
                entries.append((item['line'], signature, doc_comment(lines, item['line'], top), notes))
        entries.sort()
        modules.append((name, file, summary, entries))
    toc = ''.join(f'<a href="#{n}">{code(n)}</a>' for n, _, _, _ in modules)
    sections = []
    for number, (name, file, summary, entries) in enumerate(modules, 1):
        items = ''.join(
            f'<div><code>{code(signature)}</code>'
            + (f'<p>{code(doc)}</p>' if doc else '')
            + (f'<p class="api-note">{code("; ".join(notes).capitalize())}.</p>' if notes else '')
            + '</div>\n' for _, signature, doc, notes in entries)
        sections.append(
            f'<section id="{name}"><p class="eyebrow">{number:02d} / std/{code(file)}</p>'
            f'<h2>{code(name)}</h2>'
            + (f'<p>{code(summary)}</p>' if summary else '')
            + f'<p>Import with <code>#import "{code(name)}"</code> and pass <code>--module-path std</code>.</p>'
            + (f'<div class="api-list">\n{items}</div>' if items else '<p>No public declarations.</p>')
            + f'<p><a class="text-link" href="https://github.com/ziranlang/ziran/blob/master/std/{code(file)}">Source ↗</a></p></section>\n')
    return ''.join(sections), toc


def fill(page, parts):
    path = os.path.join(SITE, page)
    text = open(path).read()
    def replace(m):
        if m.group(2) not in parts:
            sys.exit(f'{page}: no content for generated:{m.group(2)}')
        return m.group(1) + parts[m.group(2)] + m.group(4)
    return path, text, GENERATED.sub(replace, text)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--ziran', required=True)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    ziran = os.path.abspath(args.ziran)
    examples, example_toc = example_sections(ziran)
    std, std_toc = std_reference(ziran)
    stale = []
    for page, parts in (('examples.html', {'examples': examples, 'examples_toc': example_toc}),
                        ('std.html', {'std': std, 'std_toc': std_toc})):
        path, old, new = fill(page, parts)
        if old == new:
            continue
        if args.check:
            stale.append(page)
        else:
            open(path, 'w').write(new)
            print(f'updated site/{page}')
    if stale:
        sys.exit('stale site pages: ' + ', '.join(stale) +
                 '\nrun: python3 scripts/site_pages.py --ziran build/bin/ziran')


if __name__ == '__main__':
    main()
