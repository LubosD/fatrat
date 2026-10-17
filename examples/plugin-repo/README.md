# An example FatRat plugin repository

FatRat installs plugins from repositories you add yourself; there is no
central index. A repository is a git repository -- or just a directory --
with a `fatrat-plugins.json` manifest at its root listing the plugins it
offers.

## Trying this one

In FatRat: **Settings → Extensions → Repositories → Add**, pick this
directory, then install `dummy` from the list and restart FatRat. Nothing is
downloaded: a local repository is used where it lies, so editing
`dummy/dummy.py` and restarting is the whole development cycle.

## What is in here

```
fatrat-plugins.json      the repository manifest: which plugins, and where
dummy/                   a download plugin: fetch, countdown, captcha, final check
extras/                  the other kinds: extractor, upload, search, account status
feedwatch/               a plugin with a PyPI dependency, and how that is confirmed
```

Each plugin directory holds a `plugin.json` manifest (name, version, entry
module, the settings page it wants) next to the module itself.

## Publishing your own

Copy this directory, replace `dummy/` with your plugin, push it to any git
host, and give people the clone URL. FatRat pins the commit it installed, so
an update is something the user chooses, never something that happens to
them.

A plugin runs with your full user privileges, exactly like anything else you
install from a git repository. Say in your README what your plugin does and
what it talks to.
