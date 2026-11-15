# Publishing ClioPlay

Publish the **contents of `CLIOPLAY-LATEST-VERSION/`** as the public repository
root. Its `src/` is the canonical source. The enclosing local workspace, private
data folder and phone website sources are not part of this repository. The
README links to the independently deployed phone website; its source stays private.

## Check the exact Git payload

From the publication/repository root:

```sh
python3 src/tools/refresh_latest.py --check
python3 src/tools/verify_ci.py
```

The verifier checks source/binary correspondence, GIF provenance, licences and
hashes, high-confidence credential patterns and the exact files Git would include.
Local caches, test reports, navigation, firmware and settings must stay ignored.
Use Git rather than uploading the entire local working directory as a ZIP.
GitHub's source ZIP after a clean commit contains the committed files only.

## Update an existing checkout

Copy the verified release contents into the repository working tree and include
deletions for files retired by the new release. Review the changed files and
run the checks above from the checkout before committing and pushing.

## Commit and release

Review the diff and the files selected for commit. Commit and push from the
existing checkout when satisfied; the preparation and verification tools do not
stage files, create commits, push or publish releases.

For a GitHub release, use a tag matching `src/VERSION` and describe the actual
changes and validation limits. GitHub's source ZIP contains the committed files;
private navigation data, firmware and website sources must remain excluded.

No GitHub Actions workflow is included. Run the local host and Windows launcher
checks manually; branch protection should reference checks actually available in
the repository.

## Release status

The release number is in [VERSION](../VERSION). Record physical-unit results
separately from host results. Remaining navigation validation is described in the main README;
legacy helper source/correspondence limitations are in
[SOURCE_STATUS.md](../../SOURCE_STATUS.md). Resolve the documented corresponding-
source question before a public binary release containing the affected helper.
Do not describe the project as fully rebuildable while those gaps remain.

Generated root files are refreshed from `src/`; build instructions are in
[BUILDING.md](../BUILDING.md). Repository metadata sources live in `src/repository/`.
