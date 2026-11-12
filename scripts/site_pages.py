#!/usr/bin/env python3
"""Regenerate the site pages that are built from the repositories.

Everything listed here is produced from source, so a new module, package or
guide document appears on the next run without editing HTML by hand:

  site/examples.html       every program in examples/ with the output the
                           portable runner prints for it;
  site/modules.html        the index of every standard module and every
                           public ziranlang package, with summary and status;
  site/std/NAME.html       the public declarations of each std/ module, as
                           `ziran api` reports them, with their comments;
  site/packages/NAME.html  each package's README and the declarations of
                           every module its ziran.toml exports;
  site/guide/NAME.html     docs/USAGE.md, docs/COMPILER_API.md and every
                           document they link to inside docs/;
  site/sitemap.xml         every page of the site.

Packages are read from --packages (default: the ziranlang/packages directory
next to this repository). A package whose GitHub repository is private is
left out. The standard-library release inventory records shipped modules, including in
source archives and shallow checkouts. Package status follows each package's
v* release tags; modules added between releases remain in progress.

    python3 scripts/site_pages.py --ziran build/bin/ziran [--check]

With --check, report pages that differ from what the sources produce now.
Package pages are checked only when the packages directory is present.
"""
import argparse
import html
import json
import os
import re
import subprocess
import sys
import tomllib
import urllib.error
import urllib.request

try:
    import markdown
except ImportError:  # pragma: no cover - reported at run time
    markdown = None

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SITE = os.path.join(REPO, 'site')
EXAMPLES = os.path.join(REPO, 'examples')
STD = os.path.join(REPO, 'std')
DOCS = os.path.join(REPO, 'docs')
ORIGIN = 'https://ziran-lang.org'
GITHUB = 'https://github.com/ziranlang/ziran'
# Guide pages start from these documents and follow their links inside docs/.
GUIDE_ROOTS = ['USAGE.md', 'COMPILER_API.md']
API_LIMITS = ['--max-symbols', '4096', '--max-bytes', '1048576']

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

NAV = [('get-started.html', 'Get started'), ('examples.html', 'Examples'),
       ('targets.html', 'Targets'),
       ('modules.html', 'Modules'), ('docs.html', 'Docs'), ('status.html', 'Status')]


def run(args, cwd=REPO):
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f'{" ".join(args)} failed:\n{result.stdout}{result.stderr}')
    return result.stdout


def git(directory, *args):
    result = subprocess.run(['git', '-C', directory, *args], capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else ''


def code(text):
    return html.escape(text, quote=False)


def attr(text):
    return html.escape(text, quote=True)


def first_sentence(text):
    match = re.match(r'(.+?[.!?])(\s|$)', text)
    return match.group(1) if match else text


# ---------------------------------------------------------------- page shell

def shell(path, title, description, current, body):
    """A complete page at site/PATH with the shared header and footer."""
    up = '../' * path.count('/')
    canonical = ORIGIN + '/' + ('' if path == 'index.html' else path[:-5])
    nav = ''.join(f'<a href="{up}{href}"' + (' aria-current="page"' if href == current else '')
                  + f'>{label}</a>' for href, label in NAV)
    footer = ''.join(f'<a href="{up}{href}">{label}</a>' for href, label in NAV)
    brand = (f'<a class="brand" href="{up}index.html"><img class="brand-icon" src="{up}assets/logo-capybara.png" '
             'alt="" width="36" height="36"><span>Ziran</span></a>')
    return ('<!doctype html>\n<html lang="en">\n'
            '<head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">'
            f'<meta name="theme-color" content="#f7f3eb"><meta name="description" content="{attr(description)}">'
            f'<title>{code(title)}</title><link rel="canonical" href="{canonical}">'
            f'<link rel="icon" href="{up}assets/logo-capybara.png" type="image/png"><link rel="stylesheet" href="{up}styles.css">'
            f'<script defer src="{up}site.js"></script></head>\n'
            '<body><a class="skip-link" href="#main">Skip to content</a><header class="site-header">'
            f'<div class="site-header-inner">{brand}<button class="menu-button" type="button" aria-expanded="false" '
            'aria-controls="primary-nav">Menu</button><nav class="header-nav" id="primary-nav" aria-label="Primary">'
            f'{nav}<a href="{GITHUB}">GitHub ↗</a><button class="theme-toggle" type="button" '
            'aria-label="Switch color theme"></button></nav></div></header>\n'
            f'<main id="main" class="article-layout">{body}</main>\n'
            f'<footer><div class="footer-top">{brand}<nav aria-label="Footer">{footer}'
            f'<a href="{up}index.html#support">Support</a><a href="{GITHUB}">GitHub ↗</a></nav></div>'
            '<div class="footer-bottom"><p>A language for useful software.</p>'
            '<p>© 2026 Kryon Labs · <a href="https://waozi.xyz/">Made by Waozi</a></p></div></footer></body></html>\n')


def article(eyebrow, heading, intro, toc_title, toc, content, body_class='article-body'):
    return (f'<div class="article-head"><p class="eyebrow">{eyebrow}</p><h1>{heading}</h1>{intro}</div>\n'
            f'<div class="article-grid"><aside class="page-toc"><strong>{toc_title}</strong>{toc}</aside>'
            f'<div class="{body_class}">\n{content}</div></div>')


def badge(status):
    label = 'Stable' if status == 'stable' else 'In progress'
    return f'<span class="status-badge is-{status}">{label}</span>'


# ------------------------------------------------------------------ examples

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
    for number, name in enumerate(names, 1):
        source = open(os.path.join(EXAMPLES, name + '.zi')).read()
        comment, body = leading_comment(source.rstrip('\n').split('\n'))
        summary = EXAMPLE_TITLES.get(name, comment)
        title, _, description = summary.partition(': ')
        if not description:
            sys.exit(f'examples/{name}.zi needs a "// Title: description" first comment')
        entry = 'main' if re.search(r'^main :: \(\)', source, re.M) else 'Answer'
        path = f'examples/{name}.zi'
        options = ['--entry', f'{name}:{entry}'] if entry != 'main' else []
        output = run([ziran, 'run', *options, path])
        entry_option = f' --entry {name}:{entry}' if options else ''
        commands = (f'ziran run{entry_option} {name}.zi\n'
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
    return ''.join(sections), ''.join(toc), len(names)


# --------------------------------------------------------------- module APIs

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


def module_api(ziran, path, root, module_paths):
    """The module summary and public declarations of PATH, in source order."""
    name = os.path.basename(path)[:-3]
    data = json.loads(run([ziran, 'api', '--json', '--module', name] + API_LIMITS + ['--root', root]
                          + [arg for p in module_paths for arg in ('--module-path', p)] + [path]))
    if not data['truncation']['complete']:
        sys.exit(f'ziran api truncated {path}: {data["truncation"]}')
    source = os.path.relpath(path, root)
    module = next((m for m in data['modules'] if m['source'] == source), None)
    if module is None:
        sys.exit(f'ziran api did not report {source} for {path}')
    lines = open(path).read().split('\n')
    # The comment that opens a file describes the module.
    top = 0
    while top < len(lines) and lines[top].startswith('//'):
        top += 1
    summary = ' '.join(l[2:].strip() for l in lines[:top])
    # `#load` pulls declarations in from sibling files; each item reports its
    # own file and line, so its doc comment is read from that file.
    loaded = [m.group(1) for m in re.finditer(r'^#load "([^"]+)"', '\n'.join(lines), re.M)]
    order = {os.path.join(os.path.dirname(source), f): n for n, f in enumerate(loaded, 1)}
    order[source] = 0
    file_lines = {source: (lines, top)}

    def declaring_file(item):
        file = item.get('path', source)
        if file not in file_lines:
            text = open(os.path.join(root, file)).read().split('\n')
            start = 0
            while start < len(text) and text[start].startswith('//'):
                start += 1
            file_lines[file] = (text, start)
        return file_lines[file]
    entries = []
    uses_host = False
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
                uses_host = True
            if kind == 'functions' and item.get('must_use'):
                notes.append('result must be used')
            own_lines, own_top = declaring_file(item)
            entries.append(((order.get(item.get('path', source), len(order)), item['line']), signature,
                            doc_comment(own_lines, item['line'], own_top), notes))
    entries.sort()
    entries = [(line, *rest) for (_, line), *rest in entries]
    return summary, entries, uses_host


def api_list(entries):
    if not entries:
        return '<p>No public declarations.</p>'
    items = ''.join(
        f'<div><code>{code(signature)}</code>'
        + (f'<p>{code(doc)}</p>' if doc else '')
        + (f'<p class="api-note">{code("; ".join(notes).capitalize())}.</p>' if notes else '')
        + '</div>\n' for _, signature, doc, notes in entries)
    return f'<div class="api-list">\n{items}</div>'


def latest_release(directory):
    tags = git(directory, 'tag', '--list', 'v*', '--sort=-v:refname').split('\n')
    return tags[0] if tags and tags[0] else ''


def target_of(name, uses_host):
    """What building with the module needs: the target its name declares,
    or a host capability when `ziran api` marks one of its procedures."""
    for suffix, label in (('_linux', 'Native Linux'), ('_windows', 'Native Windows'), ('_go', 'Go target')):
        if name.endswith(suffix):
            return label
    return 'Host capability' if uses_host else ''


def std_modules(ziran):
    release = latest_release(REPO)
    inventory = os.path.join(SITE, 'stdlib-release.json')
    shipped_names = None
    if os.path.exists(inventory):
        with open(inventory) as stream:
            published = json.load(stream)
        version = open(os.path.join(REPO, 'VERSION')).read().strip()
        names = published.get('modules')
        if (published.get('version') != version or not re.fullmatch(r'\d+\.\d+\.\d+', version)
                or not isinstance(names, list) or not names or any(not isinstance(n, str) or not re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', n) for n in names)
                or len(names) != len(set(names))):
            sys.exit('invalid standard-library release inventory; run update_version.sh when releasing')
        release, shipped_names = 'v' + version, set(names)
    modules = []
    for file in sorted(f for f in os.listdir(STD) if f.endswith('.zi')):
        name = file[:-3]
        summary, entries, uses_host = module_api(ziran, os.path.join(STD, file), STD, [STD])
        shipped = name in shipped_names if shipped_names is not None else bool(release) and subprocess.run(
            ['git', '-C', REPO, 'cat-file', '-e', f'{release}:std/{file}'], capture_output=True).returncode == 0
        modules.append({'name': name, 'file': file, 'summary': summary, 'entries': entries,
                        'target': target_of(name, uses_host),
                        'status': 'stable' if shipped else 'in-progress'})
    return modules, release


def std_page(module, release):
    name, file = module['name'], module['file']
    status = (f'Shipped in release <a href="{GITHUB}/releases/tag/{release}">{release}</a>.'
              if module['status'] == 'stable' else
              f'Added after release {release or "none"}; available when building from source.')
    intro = ((f'<p>{code(module["summary"])}</p>' if module['summary'] else '')
             + f'<p class="module-meta">{badge(module["status"])} '
             + (f'<span class="module-target">Needs: {code(module["target"])}.</span> ' if module['target'] else '')
             + f'{status}</p>')
    toc = ''.join(f'<a href="#{attr(re.match(r"[A-Za-z_0-9]+", s).group(0))}">'
                  f'{code(re.match(r"[A-Za-z_0-9]+", s).group(0))}</a>' for _, s, _, _ in module['entries'])
    items = api_list(module['entries'])
    # Each declaration gets an anchor so other pages can link to it.
    items = re.sub(r'<div><code>([A-Za-z_0-9]+)', lambda m: f'<div id="{m.group(1)}"><code>{m.group(1)}', items)
    content = (f'<section><p>Import with <code>#import "std/{code(name)}"</code>.</p>{items}'
               f'<p><a class="text-link" href="{GITHUB}/blob/master/std/{code(file)}">Source ↗</a> · '
               f'<a class="text-link" href="../modules.html#std">All modules</a></p></section>\n')
    description = first_sentence(module['summary']) or f'API reference for the Ziran std/{name} module.'
    return shell(f'std/{name}.html', f'std/{name} · Ziran', description, 'modules.html',
                 article(f'<a href="../modules.html#std">Modules</a> / Standard library', f'std/{code(name)}', intro,
                         'Declarations', toc or '<span>None</span>', content, 'article-body api-body'))


# ------------------------------------------------------------------ packages

def github_url(remote):
    match = re.match(r'(?:https://github\.com/|git@github\.com:)([^/]+/[^/]+?)(?:\.git)?/?$', remote)
    return f'https://github.com/{match.group(1)}' if match else ''


def public(url, cache={}):
    """Whether an anonymous visitor can open the GitHub repository."""
    if url not in cache:
        request = urllib.request.Request(url, method='HEAD', headers={'User-Agent': 'ziran-site-pages'})
        try:
            with urllib.request.urlopen(request, timeout=20) as response:
                cache[url] = response.status == 200
        except urllib.error.HTTPError as error:
            if error.code != 404:
                sys.exit(f'cannot tell whether {url} is public: HTTP {error.code}')
            cache[url] = False
        except OSError as error:
            sys.exit(f'cannot tell whether {url} is public: {error}')
    return cache[url]


def render_markdown(text, link):
    """Markdown to HTML with GitHub-style heading ids; LINK rewrites each
    relative href and returns None to drop the link but keep its text."""
    if markdown is None:
        sys.exit('python3-markdown is required: apt install python3-markdown')
    def slug(value, separator):
        value = re.sub(r'<[^>]+>', '', value).strip().lower()
        return re.sub(r'\s', separator, re.sub(r'[^\w\s-]', '', value))
    md = markdown.Markdown(extensions=['fenced_code', 'tables', 'sane_lists', 'toc'],
                           extension_configs={'toc': {'slugify': slug}})
    body = md.convert(text)

    def rewrite(match):
        before, href, after, label = match.group(1), html.unescape(match.group(2)), match.group(3), match.group(4)
        if re.match(r'[a-z]+:|#', href):
            return match.group(0)
        target = link(href)
        if target is None:
            return label
        return f'<a{before}href="{attr(target)}"{after}>{label}</a>'
    body = re.sub(r'<a([^>]*?)href="([^"]*)"([^>]*)>(.*?)</a>', rewrite, body, flags=re.S)
    # Tables scroll on narrow screens instead of widening the page.
    body = body.replace('<table>', '<div class="table-scroll"><table>').replace('</table>', '</table></div>')
    return body, md.toc_tokens


def strip_markdown(text):
    text = re.sub(r'!\[[^\]]*\]\([^)]*\)', '', text)
    text = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', text)
    return re.sub(r'\s+', ' ', re.sub(r'[`*_]', '', text)).strip()


def readme_parts(text):
    """The README's title, its first paragraph as plain text, and the rest."""
    lines = text.split('\n')
    title = ''
    if lines and lines[0].startswith('# '):
        title, lines = lines[0][2:].strip(), lines[1:]
    paragraphs = re.split(r'\n\s*\n', '\n'.join(lines).strip())
    first = next((i for i, p in enumerate(paragraphs)
                  if p and not p.lstrip().startswith(('```', '#', '|', '-', '<'))), None)
    if first is None:
        return title, '', '\n\n'.join(paragraphs)
    # The summary leads the page, so the overview continues after it.
    return title, strip_markdown(paragraphs[first]), '\n\n'.join(paragraphs[:first] + paragraphs[first + 1:])


def package_modules(ziran, directory, manifest, packages):
    roots = [os.path.join(directory, r) for r in manifest['package'].get('module_roots', ['.'])]
    paths = list(roots)
    for name, dependency in manifest.get('dependencies', {}).items():
        requested = dependency.get('git', '')
        repo = requested.rstrip('/').removesuffix('.git').rsplit('/', 1)[-1]
        package_directory = os.path.join(packages, repo)
        other = os.path.join(package_directory, 'ziran.toml')
        if os.path.exists(other):
            origin = git(package_directory, 'remote', 'get-url', 'origin')
            requested_identity = github_url(requested) or requested.rstrip('/').removesuffix('.git')
            origin_identity = github_url(origin) or origin.rstrip('/').removesuffix('.git')
        if os.path.exists(other) and requested_identity and requested_identity == origin_identity:
            other_roots = tomllib.load(open(other, 'rb'))['package'].get('module_roots', ['.'])
            paths.append(f'{name}={os.path.join(package_directory, other_roots[0])}')
    modules = []
    for export, file in manifest.get('exports', {}).items():
        path = os.path.join(directory, file)
        root = next((r for r in roots if path.startswith(r + os.sep)), roots[0])
        summary, entries, _ = module_api(ziran, path, root, paths)
        modules.append({'name': export, 'file': file, 'summary': summary, 'entries': entries})
    return modules


def read_packages(ziran, packages):
    found, skipped = [], []
    for name in sorted(os.listdir(packages)):
        directory = os.path.join(packages, name)
        manifest_path = os.path.join(directory, 'ziran.toml')
        if not os.path.isfile(manifest_path):
            continue
        manifest = tomllib.load(open(manifest_path, 'rb'))
        url = github_url(git(directory, 'remote', 'get-url', 'origin'))
        if url and not public(url):
            skipped.append(f'{name} (private repository)')
            continue
        readme_path = os.path.join(directory, 'README.md')
        readme = open(readme_path).read() if os.path.exists(readme_path) else ''
        title, summary, rest = readme_parts(readme)
        docs_dir = os.path.join(directory, 'docs')
        docs = sorted(f for f in os.listdir(docs_dir) if f.endswith('.md')) if os.path.isdir(docs_dir) else []
        release = latest_release(directory)
        found.append({'name': name, 'title': title or name, 'summary': summary, 'readme': rest,
                      'url': url, 'docs': docs, 'release': release,
                      'status': 'stable' if release else 'in-progress',
                      'dependency': name.replace('-', '_'),
                      'modules': package_modules(ziran, directory, manifest, packages)})
    return found, skipped


def package_page(package):
    name, url = package['name'], package['url']

    def link(href):
        path, _, fragment = href.partition('#')
        if not url:
            return None
        return f'{url}/blob/HEAD/{path}' + (f'#{fragment}' if fragment else '')
    readme, _ = render_markdown(package['readme'], link) if package['readme'].strip() else ('', [])
    if package['status'] == 'stable':
        status = f'Latest release {code(package["release"])}.'
    else:
        status = 'No tagged release yet.'
    if not url:
        status += ' The repository is not public yet.'
    intro = ((f'<p>{code(package["summary"])}</p>' if package['summary'] else '')
             + f'<p class="module-meta">{badge(package["status"])} {status}</p>')
    sections = []
    if url:
        sections.append(
            f'<section id="install"><h2>Add the dependency</h2><p>In your project’s <code>ziran.toml</code>; '
            'pin <code>ref</code> to a commit for reproducible builds.</p>'
            f'<pre><code>[dependencies.{package["dependency"]}]\ngit = "{url}.git"\nref = "master"</code></pre></section>\n')
    if readme:
        sections.append(f'<section id="overview" class="prose"><h2>Overview</h2>{readme}</section>\n')
    for module in package['modules']:
        anchor = 'module-' + module['name']
        source = (f'<p><a class="text-link" href="{url}/blob/HEAD/{attr(module["file"])}">Source ↗</a></p>'
                  if url else '')
        sections.append(
            f'<section id="{attr(anchor)}"><p class="eyebrow">{code(name)}/{code(module["file"])}</p>'
            f'<h2>{code(module["name"])}</h2>'
            + (f'<p>{code(module["summary"])}</p>' if module['summary'] else '')
            + f'<p>Import with <code>#import "{code(package["dependency"])}/{code(module["name"])}"</code>.</p>'
            + api_list(module['entries']) + source + '</section>\n')
    if package['docs'] and url:
        docs = ''.join(f'<a href="{url}/blob/HEAD/docs/{attr(d)}">{code(d)}<span>GitHub ↗</span></a>'
                       for d in package['docs'])
        sections.append(f'<section id="documents"><h2>Documents</h2><div class="link-list">{docs}</div></section>\n')
    toc = (('<a href="#install">Install</a>' if url else '')
           + ('<a href="#overview">Overview</a>' if readme else '')
           + ''.join(f'<a href="#module-{attr(m["name"])}">{code(m["name"])}</a>' for m in package['modules'])
           + ('<a href="#documents">Documents</a>' if package['docs'] and url else ''))
    links = f'<p><a class="text-link" href="{url}">Repository ↗</a></p>' if url else ''
    description = first_sentence(package['summary']) or f'The {name} package for Ziran.'
    return shell(f'packages/{name}.html', f'{package["title"]} · Ziran package', description, 'modules.html',
                 article('<a href="../modules.html#packages">Modules</a> / Packages', code(package['title']),
                         intro + links, 'Contents', toc, ''.join(sections), 'article-body api-body'))


# -------------------------------------------------------------------- guide

def guide_slug(name):
    return name[:-3].lower().replace('_', '-')


def guide_documents():
    """GUIDE_ROOTS and every docs/*.md they link to, in discovery order."""
    order, queue = [], list(GUIDE_ROOTS)
    while queue:
        name = queue.pop(0)
        if name in order or not os.path.isfile(os.path.join(DOCS, name)):
            continue
        order.append(name)
        for target in re.findall(r'\]\(([^)#\s]+\.md)(?:#[^)]*)?\)', open(os.path.join(DOCS, name)).read()):
            if '/' not in target:
                queue.append(target)
    return order


def guide_link(source_dir, documents):
    def link(href):
        path, _, fragment = href.partition('#')
        resolved = os.path.normpath(os.path.join(source_dir, path))
        relative = os.path.relpath(resolved, REPO)
        if os.path.dirname(relative) == 'docs' and os.path.basename(relative) in documents:
            return guide_slug(os.path.basename(relative)) + '.html' + (f'#{fragment}' if fragment else '')
        kind = 'tree' if os.path.isdir(resolved) else 'blob'
        return f'{GITHUB}/{kind}/master/{relative}' + (f'#{fragment}' if fragment else '')
    return link


def guide_pages(documents):
    pages = {}
    for name in documents:
        text = open(os.path.join(DOCS, name)).read()
        title = name[:-3]
        match = re.match(r'#\s+(.+)\n', text)
        if match:
            title, text = match.group(1).strip(), text[match.end():]
        body, tokens = render_markdown(text, guide_link(DOCS, documents))
        toc = ''.join(f'<a href="#{attr(t["id"])}">{code(html.unescape(t["name"]))}</a>'
                      for level in tokens for t in ([level] if level['level'] <= 2 else []))
        intro = (f'<p>Rendered from <a href="{GITHUB}/blob/master/docs/{name}">docs/{name}</a>. '
                 'See the <a href="../docs.html#guide">documentation index</a> for the other guides.</p>')
        page = shell(f'guide/{guide_slug(name)}.html', f'{title} · Ziran guide',
                     f'{title}: the Ziran guide, rendered from docs/{name}.', 'docs.html',
                     article('<a href="../docs.html#guide">Guide</a>', code(title), intro, 'On this page',
                             toc or '<span>One section</span>', f'<section class="prose">{body}</section>\n'))
        pages[name] = (title, page)
    return pages


# ------------------------------------------------------------------- index

def module_links(package):
    return ', '.join(f'<a href="packages/{package["name"]}.html#module-{attr(m["name"])}">{code(m["name"])}</a>'
                     for m in package['modules'])


def modules_page(std, release, packages):
    rows = ''.join(
        f'<tr><th scope="row"><a href="std/{m["name"]}.html">{code(m["name"])}</a></th>'
        f'<td>{code(first_sentence(m["summary"])) or "<span class=muted>No module description yet.</span>"}</td>'
        f'<td>{code(m["target"]) or "<span class=muted>—</span>"}</td><td>{badge(m["status"])}</td></tr>\n' for m in std)
    package_rows = ''.join(
        f'<tr><th scope="row"><a href="packages/{p["name"]}.html">{code(p["name"])}</a></th>'
        f'<td>{code(first_sentence(p["summary"])) or "<span class=muted>No README summary yet.</span>"}</td>'
        f'<td>{module_links(p)}</td>'
        f'<td>{badge(p["status"])}</td></tr>\n' for p in packages)
    stable = sum(m['status'] == 'stable' for m in std)
    intro = (f'<p>{len(std)} standard modules and {len(packages)} packages, listed from the source on every '
             'site build. Each page shows the public declarations <code>ziran api</code> reports, with the '
             'comment written above them.</p>'
             f'<p class="module-meta">{badge("stable")} ships in the latest release'
             + (f', <a href="{GITHUB}/releases/tag/{release}">{release}</a>' if release else '')
             + f' (for a package: it has a tagged release). {badge("in-progress")} exists only on '
             '<code>master</code> or has no release yet.</p>')
    content = (
        f'<section id="std"><h2>Standard library</h2><p>Import a module with <code>#import "std/NAME"</code>. '
        'Modules ending in <code>_linux</code>, <code>_windows</code> or <code>_go</code> build only for that target. '
        'A module that needs a host capability runs in a portable bundle only when the program hosting the bundle '
        f'binds it. {stable} of {len(std)} modules '
        'shipped in the latest release.</p>'
        '<div class="table-scroll"><table class="module-table"><thead><tr><th scope="col">Module</th>'
        '<th scope="col">Summary</th><th scope="col">Needs</th><th scope="col">Status</th></tr></thead>'
        f'<tbody>\n{rows}</tbody></table></div></section>\n'
        '<section id="packages"><h2>Packages</h2><p>Packages live in their own repositories under '
        '<a href="https://github.com/ziranlang">github.com/ziranlang</a> and are added as '
        '<code>ziran.toml</code> dependencies; see the <a href="guide/packages.html">package guide</a>.</p>'
        '<div class="table-scroll"><table class="module-table"><thead><tr><th scope="col">Package</th>'
        '<th scope="col">Summary</th><th scope="col">Modules</th><th scope="col">Status</th></tr></thead>'
        f'<tbody>\n{package_rows}</tbody></table></div></section>\n')
    toc = '<a href="#std">Standard library</a><a href="#packages">Packages</a>'
    return shell('modules.html', 'Ziran modules and packages',
                 f'Every Ziran standard module and package: {len(std)} std modules and {len(packages)} packages '
                 'with API references generated from the source.', 'modules.html',
                 article('Reference / Modules', 'Modules and <em>packages.</em>', intro, 'Sections', toc, content))


def sitemap(paths):
    urls = ''.join(f'  <url><loc>{ORIGIN}/{"" if p == "index.html" else p[:-5]}</loc></url>\n' for p in paths)
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            f'<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n{urls}</urlset>\n')


# -------------------------------------------------------------------- main

def fill(page, parts):
    path = os.path.join(SITE, page)
    text = open(path).read()
    def replace(m):
        if m.group(2) not in parts:
            sys.exit(f'{page}: no content for generated:{m.group(2)}')
        return m.group(1) + parts[m.group(2)] + m.group(4)
    return GENERATED.sub(replace, text)


def shared_navigation(path, text):
    """Keep generated and hand-authored pages on the same navigation."""
    up = '../' * path.count('/')
    current = ('modules.html' if path.startswith(('std/', 'packages/')) else
               'docs.html' if path.startswith('guide/') else path)
    links = ''.join(f'<a href="{up}{href}"' +
                    (' aria-current="page"' if href == current else '') +
                    f'>{label}</a>' for href, label in NAV)
    header = (links + f'<a href="{GITHUB}">GitHub ↗</a>' +
              '<button class="theme-toggle" type="button" aria-label="Switch color theme"></button>')
    footer = (''.join(f'<a href="{up}{href}">{label}</a>' for href, label in NAV) +
              f'<a href="{up}index.html#support">Support</a><a href="{GITHUB}">GitHub ↗</a>')
    text = re.sub(r'(<nav\b[^>]*\bid="primary-nav"[^>]*>).*?(</nav>)',
                  lambda match: match[1] + header + match[2], text, flags=re.S)
    return re.sub(r'(<nav\b[^>]*\baria-label="Footer"[^>]*>).*?(</nav>)',
                  lambda match: match[1] + footer + match[2], text, flags=re.S)


def main():
    global REPO, SITE, STD
    parser = argparse.ArgumentParser()
    parser.add_argument('--ziran', required=True)
    parser.add_argument('--module-api', metavar='SOURCE', help='print a module\'s documentation data as JSON')
    parser.add_argument('--module-root', metavar='DIR')
    parser.add_argument('--stdlib-inventory', metavar='REPOSITORY', help='print a repository\'s standard-module release inventory as JSON')
    parser.add_argument('--packages', default=os.path.join(os.path.dirname(REPO), 'packages'))
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    ziran = os.path.abspath(args.ziran)
    if args.stdlib_inventory:
        # The same release-inventory path is used for normal site generation.
        REPO = os.path.abspath(args.stdlib_inventory)
        SITE = os.path.join(REPO, 'site')
        STD = os.path.join(REPO, 'std')
        print(json.dumps(std_modules(ziran)))
        return
    if args.module_api:
        source = os.path.abspath(args.module_api)
        root = os.path.abspath(args.module_root) if args.module_root else os.path.dirname(source)
        summary, entries, uses_host = module_api(ziran, source, root, [])
        print(json.dumps({'summary': summary, 'entries': entries, 'uses_host': uses_host}))
        return
    with_packages = os.path.isdir(args.packages)
    if not with_packages and not args.check:
        sys.exit(f'{args.packages}: no packages directory; pass --packages DIR')

    examples, example_toc, example_count = example_sections(ziran)
    std, release = std_modules(ziran)
    documents = guide_documents()
    guide = guide_pages(documents)
    guide_toc = ''.join(f'<a href="guide/{guide_slug(n)}.html">{code(t)}<span>docs/{code(n)}</span></a>'
                        for n, (t, _) in guide.items())

    outputs = {'examples.html': fill('examples.html', {'examples': examples, 'examples_toc': example_toc})}
    outputs['docs.html'] = fill('docs.html', {'std_count': str(len(std)), 'example_count': str(example_count),
                                              'guide': guide_toc})
    owned = {'std': {}, 'guide': {}, 'packages': {}}
    for module in std:
        owned['std'][f'{module["name"]}.html'] = std_page(module, release)
    for name, (_, page) in guide.items():
        owned['guide'][f'{guide_slug(name)}.html'] = page
    skipped = []
    if with_packages:
        packages, skipped = read_packages(ziran, args.packages)
        for package in packages:
            owned['packages'][f'{package["name"]}.html'] = package_page(package)
        outputs['modules.html'] = modules_page(std, release, packages)
        outputs['index.html'] = fill('index.html', {'module_counts':
                                     f'{len(std)} standard modules and {len(packages)} packages'})
    else:
        owned.pop('packages')
    for directory, pages in owned.items():
        for file, text in pages.items():
            outputs[f'{directory}/{file}'] = text
    if with_packages:
        top = sorted(f for f in os.listdir(SITE) if f.endswith('.html') and f != 'index.html')
        outputs['sitemap.xml'] = sitemap(['index.html'] + sorted(set(top) | {'modules.html'})
                                         + sorted(p for p in outputs if '/' in p))

    # Top-level pages keep their authored body but share the navigation used
    # by generated guides and references.
    for page in sorted(os.listdir(SITE)):
        if page.endswith('.html') and page not in outputs:
            outputs[page] = open(os.path.join(SITE, page)).read()
    outputs = {page: shared_navigation(page, text) if page.endswith('.html') else text
               for page, text in outputs.items()}

    stale = []
    for page, text in outputs.items():
        path = os.path.join(SITE, page)
        old = open(path).read() if os.path.exists(path) else None
        if old == text:
            continue
        if args.check:
            stale.append(page)
        else:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            open(path, 'w').write(text)
            print(f'updated site/{page}')
    # Pages for removed modules, packages or documents go with them.
    for directory, pages in owned.items():
        folder = os.path.join(SITE, directory)
        for file in sorted(os.listdir(folder)) if os.path.isdir(folder) else []:
            if file.endswith('.html') and file not in pages:
                if args.check:
                    stale.append(f'{directory}/{file}')
                else:
                    os.remove(os.path.join(folder, file))
                    print(f'removed site/{directory}/{file}')
    for entry in skipped:
        print(f'left out {entry}', file=sys.stderr)
    if stale:
        sys.exit('stale site pages: ' + ', '.join(stale) +
                 '\nrun: python3 scripts/site_pages.py --ziran build/bin/ziran')


if __name__ == '__main__':
    main()
