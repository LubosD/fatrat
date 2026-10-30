# An example FatRat plugin repository

FatRat installs plugins from repositories you add yourself; there is no
central index. A repository is a git repository -- or just a directory --
with a `fatrat-plugins.json` manifest at its root listing the plugins it
offers.

## Trying this one

In FatRat: **Settings → Extensions → Repositories → Add**, pick this
directory, then install `dummy` from the list; it is loaded at once. Nothing is
downloaded: a local repository is used where it lies, so editing
`dummy/dummy.py` and then switching the plugin off and on in Settings →
Extensions (or `POST /api/v1/plugins/dummy/reload`) is the whole development
cycle.

## What is in here

```
fatrat-plugins.json      the repository manifest: which plugins, and where
dummy/                   a download plugin: fetch, countdown, captcha, final check
extras/                  the other kinds: extractor, upload, search, account status
feedwatch/               a plugin with a PyPI dependency, and how that is confirmed
```

Each plugin directory holds a `plugin.json` manifest (name, version, entry
module, the settings page it wants) next to the module itself.

## Trying plugins without FatRat

`fatrat_devkit` (in `data/python`, installed to `share/fatrat/data/python`)
runs plugins from the command line under plain Python 3.11+:

```
export PYTHONPATH=/path/to/fatrat/data/python
python3 -m fatrat_devkit check .                      # validate the manifests, import every plugin
python3 -m fatrat_devkit match . dummy://example/x    # which plugin takes this URL?
python3 -m fatrat_devkit download dummy dummy://127.0.0.1:8099/f/1 --captcha abc
```

It also drives plugins from unit tests with canned responses; see
"Testing a plugin without FatRat" in `doc/plugins.html`. Its own tests use
this repository: `python3 -m unittest discover -s tests/python`.

## Publishing your own

Copy this directory, replace `dummy/` with your plugin, push it to any git
host, and give people the clone URL. FatRat pins the commit it installed, so
an update is something the user chooses, never something that happens to
them.

A plugin runs with your full user privileges, exactly like anything else you
install from a git repository. Say in your README what your plugin does and
what it talks to.
