"""Current Global navigation delivery layout."""
import json
import re
from pathlib import Path

REGIONAL = ('map.cpm', 'route.cpr', 'route.qadj')
SHARED = ('regions.cpg', 'timezones.cpt', 'fonts.cpf', 'cameras.cpc')
NAMES = (*REGIONAL, *SHARED)


def bundle_names(manifest):
    builder = manifest.get('builder')
    if builder not in ('global-navigation-2', 'global-atlas-2'):
        raise ValueError('not a Global bundle manifest')
    if builder.startswith('global-navigation-'):
        return (*REGIONAL, *SHARED)
    regions = manifest.get('regions', [])
    if not 1 <= len(regions) <= 256:
        raise ValueError('invalid regional atlas size')
    paths = [r['path'] for r in regions]
    if len(paths) != len(set(paths)) or any(not re.fullmatch('[a-z0-9_-]{1,23}', n) for n in paths):
        raise ValueError('invalid atlas region path')
    return ('atlas.cpa', *SHARED, *(region+'/'+name for region in paths for name in REGIONAL))


def remove_obsolete(output, names):
    """Remove only known generated files superseded by a successful rebuild."""
    old = output/'build.json'
    prior = ()
    if old.is_file():
        try:
            prior = bundle_names(json.loads(old.read_text()))
        except (ValueError, KeyError, TypeError):
            pass
    known = set(prior) | set(names) | {'search.cps'}
    # Older atlas outputs also carried unlisted child diagnostics/shared copies.
    directories = {Path(name).parent for name in known} - {Path('.')}
    for directory in directories:
        known.update((directory/name).as_posix() for name in
                     (*REGIONAL, *SHARED, 'search.cps', 'route.cpn',
                      'atlas-region.json', 'build.json'))
    for name in known:
        path = output/name
        if name not in names:
            path.unlink(missing_ok=True)
        # Diagnostic sidecars belong to scratch, not the installable bundle.
        path.with_suffix(path.suffix+'.json').unlink(missing_ok=True)
