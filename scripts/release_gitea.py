#!/usr/bin/env python3
"""Publish a tested Ziran version from its Gitea master push."""
import json
import os
from pathlib import Path
import re
import subprocess
from urllib.error import HTTPError
from urllib.request import HTTPRedirectHandler, ProxyHandler, Request, build_opener

REPOSITORY = 'ziranlang/ziran'
REVISION = re.compile('[0-9a-f]{40}')
VERSION = re.compile(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)')


class NoRedirects(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def api_client(origin, token):
    if not re.fullmatch(r'http://127\.0\.0\.1:[1-9][0-9]{0,4}', origin) or not token:
        raise ValueError('Expected the managed Build API and Actions job credential')
    endpoint = f'{origin}/api/v1/repos/{REPOSITORY}'
    opener = build_opener(ProxyHandler({}), NoRedirects)

    def request(method, path, data=None):
        headers = {'Authorization': 'token '+token, 'Content-Type': 'application/json'}
        target = Request(endpoint+path, method=method, headers=headers,
                         data=json.dumps(data).encode() if data is not None else None)
        with opener.open(target, timeout=15) as response:
            return json.load(response)

    return request


def get_optional(api, path):
    try:
        return api('GET', path)
    except HTTPError as error:
        if error.code == 404:
            return None
        raise


def release_notes(changelog, version):
    headings = list(re.finditer(r'(?m)^## \[([^\]]+)\].*$', changelog))
    for index, heading in enumerate(headings):
        if heading[1] == version:
            end = headings[index+1].start() if index+1 < len(headings) else len(changelog)
            notes = changelog[heading.end():end].strip()
            if notes:
                return notes
    raise ValueError('The new VERSION needs a populated CHANGELOG.md section')


def publish(api, previous, version, revision, changelog):
    if not VERSION.fullmatch(previous) or not VERSION.fullmatch(version) or not REVISION.fullmatch(revision):
        raise ValueError('Expected a numeric release version and an exact source commit')
    if version == previous:
        return None
    if tuple(map(int, version.split('.'))) <= tuple(map(int, previous.split('.'))):
        raise ValueError('VERSION must advance to publish a release')
    notes = release_notes(changelog, version)
    if api('GET', '/branches/master')['commit']['id'] != revision:
        return None
    tag = 'v'+version
    tag_path = '/tags/'+tag
    release_path = '/releases/tags/'+tag
    existing_tag = get_optional(api, tag_path)
    if existing_tag is not None and existing_tag['commit']['sha'] != revision:
        raise ValueError(f'{tag} is already attached to a different commit')
    release = get_optional(api, release_path)
    if release is None:
        try:
            api('POST', '/releases', {'tag_name': tag, 'target_commitish': revision, 'name': 'Ziran '+version,
                'body': notes, 'draft': False, 'prerelease': False, 'tag_message': 'Ziran '+version})
        except HTTPError as error:
            if error.code != 409:
                raise
        release = api('GET', release_path)
    if (api('GET', tag_path)['commit']['sha'] != revision or release['tag_name'] != tag
            or release['draft'] or release['prerelease']):
        raise ValueError('Gitea did not return a published release for the tested commit')
    return tag


def checked_git(*args):
    return subprocess.check_output(['git', *args], text=True, timeout=15).strip()


def main():
    env = os.environ
    if (env.get('GITHUB_REPOSITORY'), env.get('GITHUB_EVENT_NAME'), env.get('GITHUB_REF')) != (
            REPOSITORY, 'push', 'refs/heads/master'):
        raise ValueError('Release requires the Ziran master push job')
    revision = env['GITHUB_SHA']
    before = env['ZIRAN_RELEASE_BEFORE']
    if not REVISION.fullmatch(before) or before == '0'*40 or checked_git('rev-parse', 'HEAD') != revision:
        raise ValueError('Release requires the checked master push and its preceding commit')
    previous = checked_git('show', before+':VERSION')
    version = Path('VERSION').read_text().strip()
    if previous == version:
        print('VERSION is unchanged.')
        return
    subprocess.run(['git', 'merge-base', '--is-ancestor', before, revision], check=True, timeout=15)
    api = api_client(env['BUILD_NATIVE_GIT_ORIGIN'], env.pop('ZIRAN_RELEASE_TOKEN'))
    tag = publish(api, previous, version, revision, Path('CHANGELOG.md').read_text())
    if tag:
        print(f'Published and verified https://git.waozi.xyz/{REPOSITORY}/releases/tag/{tag}')
    else:
        print('A newer master commit is already at Gitea.')


if __name__ == '__main__':
    main()
