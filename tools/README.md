# EMGOR universe tools

Zero-dependency Node >= 18 scripts. No npm install, no package.json needed.
The data contract lives in `docs/universe-schema.md` — read it first.

## Build the galaxy

```sh
node tools/build-galaxy.mjs
```

Walks `universe/**`, parses frontmatter from every `_index.md` and leaf `.md`,
validates the tree, and writes `galaxy.json` at the repo root. Then:

```sh
git add galaxy.json universe/
git commit -m "universe update"
git push        # Netlify deploys automatically
```

Options:

- `--check` — validate only, write nothing (use in CI or before committing).
- `--root DIR` — build against a different tree (used for testing with fixtures).

The build **fails (exit 1)** on: duplicate ids, a `parent` that doesn't exist,
`downloads` pointing at files that aren't on disk, nodes unreachable from the
root `emgor`, or missing required frontmatter (`id`, `title`, `blurb`, and
`parent` below the top level). It **warns** (but still builds) on missing
recommended fields (`updated`, `tags`, `source`), oversize downloads (>25MB),
and parent/filepath mismatches. `draft: true` nodes are skipped.

## Find new work

```sh
node tools/sync.mjs            # report only
node tools/sync.mjs --draft    # also write draft stubs to universe/_drafts/
```

Scans `_EMGOR_SYNTH` (**read-only** — the script contains no delete/move/rename
codepaths) for significant project folders (`.amxd`/`.csd`/`.ck`/`.ino`, or 3+
`.md`), compares against the `source:` fields already declared in
`universe/**.md`, and reports folders not yet represented on the site.

`--draft` writes proposed planet stubs (with `draft: true`) into
`universe/_drafts/` — it never overwrites existing stubs and never publishes
anything. Edit a stub, fix its `id`/`parent`, move it into place under
`universe/`, flip `draft: false`, then rebuild.

Options: `--root DIR` (repo root override), `--synth DIR` (scan-root override,
for testing).
