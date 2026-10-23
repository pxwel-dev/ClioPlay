# GitHub launch

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

The verifier checks source/binary correspondence, demo provenance, licences and
hashes, high-confidence credential patterns and the exact files Git would include.
Local caches, test reports, navigation, firmware and settings must stay ignored.
Use Git rather than uploading the entire local working directory as a ZIP.
GitHub's source ZIP after a clean commit contains the committed files only.

## First push

Create an empty **public** GitHub repository. From the publication folder:

```sh
git init -b main
git add .
git diff --cached --stat
git commit -m "Initial ClioPlay Global preview"
```

Add the new repository's actual URL as `origin`, then push `main`. No remote,
repository or release is created by the local checks. Do not add the private
phone website or local data to this Git history.

Enable Issues and private vulnerability reporting. After the first Actions run,
require **Host and publication checks** and **Windows launcher checks** for pull requests to the default branch.
The workflow has read-only repository permissions, a SHA-pinned checkout and no
publish/deploy job. It needs no secrets or paid external services. GitHub workflow
configuration and dependency updates follow the [GitHub Actions security guidance](https://docs.github.com/en/actions/reference/security/secure-use).

## Release status

Keep this labelled a **preview**. Record physical-unit results separately from
host results. Remaining navigation validation is described in the main README;
legacy helper source/correspondence limitations are in
[SOURCE_STATUS.md](../../SOURCE_STATUS.md). Resolve the documented corresponding-
source question before a public binary release containing the affected helper.
Do not describe the project as fully rebuildable while those gaps remain.

Generated root files are refreshed from `src/`; build instructions are in
[BUILDING.md](../BUILDING.md). Repository metadata sources live in `src/repository/`.
