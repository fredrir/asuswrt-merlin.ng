#!/usr/bin/env python3
"""Check shared port code against the upstream and validated IPQ53xx baselines.

This checks selected C tokens across feature profiles, with includes removed.
It does not type-check headers or replace a complete other-model firmware build.
Checks run without lazy fetching. --fetch-baselines explicitly provisions the
two baseline commits and the eighteen source blobs first, for shallow CI clones.
"""
import argparse
import hashlib
import itertools
import json
import os
from pathlib import Path
import re
import subprocess

FILES = ['release/src/router/libovpn/' + name for name in (
    'amvpn_routing.c', 'amvpn_routing.h', 'openvpn_config.c', 'openvpn_config.h',
    'openvpn_control.c', 'openvpn_control.h', 'openvpn_options.c', 'openvpn_setup.c',
)] + ['release/src/router/shared/scripts.c']
FEATURES = ('RTCONFIG_IPV6', 'RTCONFIG_MULTILAN_CFG', 'RTCONFIG_WIREGUARD', 'RTCONFIG_AUTO_WANPORT')
PLATFORMS = {
    'generic': [],
    'bcm-hnd': ['RTCONFIG_BCMARM', 'HND_ROUTER', 'RTCONFIG_HND_ROUTER'],
    'bcm-6756': ['RTCONFIG_BCMARM', 'HND_ROUTER', 'RTCONFIG_HND_ROUTER', 'RTCONFIG_HND_ROUTER_AX_6756'],
    'bcm-502': ['RTCONFIG_BCMARM', 'HND_ROUTER', 'RTCONFIG_HND_ROUTER', 'RTCONFIG_BCM_502L07P2'],
    'bcm-675x': ['RTCONFIG_BCMARM', 'HND_ROUTER', 'RTCONFIG_HND_ROUTER', 'RTCONFIG_HND_ROUTER_AX_675X'],
    'bcm-4916': ['RTCONFIG_BCMARM', 'HND_ROUTER', 'RTCONFIG_HND_ROUTER', 'RTCONFIG_HND_ROUTER_BE_4916'],
    'bcm-mfg': ['RTCONFIG_BCMARM', 'HND_ROUTER', 'RTCONFIG_HND_ROUTER', 'RTCONFIG_BCM_MFG'],
    'ipq53xx': ['RTCONFIG_SOC_IPQ53XX'],
}
TOKEN = re.compile(r'''(?:u8|[LuU])?"(?:\\.|[^"\\])*"|(?:[LuU])?'(?:\\.|[^'\\])*'|[A-Za-z_]\w*|\d+(?:\.\d*)?|>>=|<<=|\.\.\.|\+\+|--|->|&&|\|\||<=|>=|==|!=|\+=|-=|\*=|/=|%=|&=|\|=|\^=|<<|>>|\S''')


def git_source(repo, revision, name, fetch=False):
    return subprocess.check_output(
        ['git', 'show', revision + ':' + name], cwd=repo,
        env=dict(os.environ, GIT_NO_LAZY_FETCH='0' if fetch else '1'), text=True)


def tokens(source, definitions, compiler):
    source = re.sub(r'^\s*#\s*include\b[^\n]*', '', source, flags=re.M)
    command = [compiler, '-E', '-P', '-undef', '-nostdinc', '-x', 'c', '-', '-D__LINE__=0']
    command += ['-D' + name + '=1' for name in definitions]
    result = subprocess.run(command, input=source, text=True, capture_output=True, check=True)
    return TOKEN.findall(result.stdout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', default='920b77f5f92db14717a27abd5c8e1b06ae6c8ec1')
    parser.add_argument('--qca-base', default='2db9b885dec71cb4971b54edf7ad31d00063d842')
    parser.add_argument('--compiler', default='cc')
    parser.add_argument('--fetch-baselines', action='store_true',
                        help='Fetch pinned baseline objects from origin before offline comparisons')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    if args.fetch_baselines:
        for revision, prefix in ((args.base, ''),
                                 (args.qca_base, 'release/src-qca-ipq53xx/source-overlay/')):
            if not re.fullmatch(r'[0-9a-f]{40}', revision):
                parser.error('--fetch-baselines requires full commit IDs')
            subprocess.run(['git', 'fetch', '--no-tags', '--depth=1', '--filter=blob:none',
                            'origin', revision], cwd=repo, check=True)
            for name in FILES:
                git_source(repo, revision, prefix + name, fetch=True)
    results = []
    for name in FILES:
        upstream = git_source(repo, args.base, name)
        original_hash = hashlib.sha256(upstream.encode()).hexdigest()
        adjustments = []
        if name.endswith('/openvpn_options.c'):
            # The old format has two %d placeholders but only one argument.
            # This existing port fix is valid on all architectures.
            before = '"Options error: Residual parse state (%d) in %d", line_num'
            after = '"Options error: Residual parse state in %d", line_num'
            if upstream.count(before) != 1:
                raise ValueError('Unexpected upstream parser diagnostic; review the baseline')
            upstream = upstream.replace(before, after)
            adjustments.append('Correct the residual-state diagnostic format/argument mismatch')
        qca = git_source(repo, args.qca_base, 'release/src-qca-ipq53xx/source-overlay/' + name)
        current = (repo / name).read_text()
        checks = 0
        for platform, definitions in PLATFORMS.items():
            for flags in itertools.product((False, True), repeat=len(FEATURES)):
                selected = definitions + [name for name, enabled in zip(FEATURES, flags) if enabled]
                expected = qca if platform == 'ipq53xx' else upstream
                if tokens(current, selected, args.compiler) != tokens(expected, selected, args.compiler):
                    raise ValueError('Conditional code differs: %s (%s)' % (name, ','.join(selected) or 'generic'))
                checks += 1
        results.append({'file': name, 'profiles_checked': checks,
                        'upstream_sha256': original_hash, 'upstream_adjustments': adjustments})
    print(json.dumps({'base': args.base, 'qca_base': args.qca_base,
                      'files': results, 'comparisons_passed': sum(r['profiles_checked'] for r in results),
                      'scope': 'C tokens with includes removed; not a full other-model compile'}, indent=2))


if __name__ == '__main__':
    main()
