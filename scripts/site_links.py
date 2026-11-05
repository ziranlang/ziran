#!/usr/bin/env python3
"""Check every local link in the website without a network.

Each href and src in site/**/*.html must name a file in site/, and each
#fragment must name an id (or anchor name) on the target page. Sitemap
entries must name pages. External links are counted, not fetched.

    python3 scripts/site_links.py [site]
"""
import html.parser
import os
import sys
import urllib.parse

ORIGIN = 'https://ziran-lang.org'


class Page(html.parser.HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.links, self.ids = [], set()

    def handle_starttag(self, tag, attrs):
        values = dict(attrs)
        for key in ('id', 'name') if tag == 'a' else ('id',):
            if values.get(key):
                self.ids.add(values[key])
        for key in ('href', 'src'):
            if values.get(key) is not None and not (tag == 'link' and values.get('rel') == 'canonical'):
                self.links.append((self.getpos()[0], values[key]))


def page_file(site, path):
    """The file Cloudflare Pages serves for PATH, or None."""
    path = urllib.parse.unquote(path)
    candidates = [path, path + '.html', os.path.join(path, 'index.html')] if not path.endswith('/') \
        else [os.path.join(path, 'index.html')]
    for candidate in candidates:
        full = os.path.normpath(os.path.join(site, candidate.lstrip('/')))
        if os.path.isfile(full):
            return full
    return None


def main():
    site = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', 'site'))
    pages = {}
    for root, _, files in os.walk(site):
        for file in files:
            if file.endswith('.html'):
                parser = Page()
                path = os.path.join(root, file)
                parser.feed(open(path, encoding='utf-8').read())
                pages[path] = parser
    errors, external, local = [], 0, 0
    for path, parser in sorted(pages.items()):
        name = os.path.relpath(path, site)
        for line, link in parser.links:
            parts = urllib.parse.urlsplit(link)
            if parts.scheme or link.startswith('//'):
                external += 1
                continue
            local += 1
            if parts.path:
                base = os.path.relpath(os.path.dirname(path), site)
                target = page_file(site, parts.path if parts.path.startswith('/')
                                   else os.path.join('/', base, parts.path))
                if target is None:
                    errors.append(f'{name}:{line}: missing {link}')
                    continue
            else:
                target = path
            fragment = urllib.parse.unquote(parts.fragment)
            if fragment and target in pages and fragment not in pages[target].ids:
                errors.append(f'{name}:{line}: no id "{fragment}" in {os.path.relpath(target, site)}')
    sitemap = os.path.join(site, 'sitemap.xml')
    entries = 0
    if os.path.exists(sitemap):
        import re
        for url in re.findall(r'<loc>([^<]+)</loc>', open(sitemap).read()):
            entries += 1
            if not url.startswith(ORIGIN) or page_file(site, url[len(ORIGIN):] or '/index.html') is None:
                errors.append(f'sitemap.xml: no page for {url}')
    for error in errors:
        print(error)
    print(f'{len(pages)} pages, {local} local links, {external} external links not fetched, '
          f'{entries} sitemap entries, {len(errors)} broken')
    sys.exit(1 if errors else 0)


if __name__ == '__main__':
    main()
