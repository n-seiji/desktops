# Releasing

A `v*` tag on a commit in `main` runs `.github/workflows/release.yml`. The `build` job builds the CLI tarballs and the app zip and stores them in a draft release. After you approve the `release` environment, the `publish` job verifies the checksums, attests the files and publishes the release.

Without the secrets below the artifacts are ad-hoc signed, as before. With them the workflow signs with a Developer ID Application certificate, notarizes, staples the app and re-zips it, then computes the checksums from the final files. The attestations are made from those same files after approval.

## One-time setup

You need an Apple Developer Program membership.

### 1. Developer ID Application certificate

1. Keychain Access > Certificate Assistant > Request a Certificate From a Certificate Authority. Save the CSR to disk.
2. developer.apple.com > Certificates, IDs & Profiles > Certificates > + > Developer ID Application. Choose the G2 Sub-CA, upload the CSR, download the `.cer` and open it so it lands in your login keychain.
3. In Keychain Access > My Certificates, export "Developer ID Application: ..." as `cert.p12` with a password.
4. `base64 -i cert.p12 | pbcopy`, or keep the file for the `gh` command below.

The certificate carries your legal name, and it is shown in the Gatekeeper dialog and in `codesign -dvv`.

### 2. App Store Connect API key

1. appstoreconnect.apple.com > Users and Access > Integrations > App Store Connect API > Team Keys > +. Give it the Developer role.
2. Download `AuthKey_<KEYID>.p8`. Apple lets you download it once.
3. Note the Key ID and the Issuer ID shown on that page.

### 3. Repository secrets

Run these from the repository, with the real files and values. Nothing is echoed.

```sh
base64 -i cert.p12 | gh secret set DEVELOPER_ID_P12_BASE64
gh secret set DEVELOPER_ID_P12_PASSWORD          # prompts, input hidden
base64 -i AuthKey_ABC123DEFG.p8 | gh secret set ASC_KEY_P8_BASE64
gh secret set ASC_KEY_ID                         # prompts: the Key ID
gh secret set ASC_ISSUER_ID                      # prompts: the Issuer ID
```

Delete `cert.p12` and the `.p8` afterwards, or store them in a password manager.

Setting only some of a group fails the release before building. Notarization without the certificate secrets fails as well.

### 4. Release environment

```sh
scripts/setup-release-env.sh
```

Run it once. It creates the `release` environment with you as the required reviewer and allows deployments from `v*` tags only. It is safe to re-run. Set `REVIEWER` or pass a login to change the reviewer, and `REPO` to change the repository.

Without it GitHub creates the environment on first use with no reviewers, and `publish` runs unapproved. Check it:

```sh
gh api repos/n-seiji/just-desktops/environments/release -q '.protection_rules'
gh api repos/n-seiji/just-desktops/environments/release/deployment-branch-policies -q '.branch_policies'
```

## Cutting a release

1. Set `JD_VERSION` in `src/common.h` to the new version, merge it to `main`.
2. Tag the merged commit: `git tag vX.Y.Z && git push origin vX.Y.Z`. `build` fails if the tag and `JD_VERSION` differ, or if the tagged commit is not on `main`.
3. `build` signs, notarizes and uploads a draft release. Review the draft under Releases: assets and notes. Optionally download it and run the checks below.
4. In the Actions run, open "Review deployments" and approve `release`. `publish` verifies the checksums, attests the files and publishes the release.
5. Update the cask: `scripts/bump-cask.sh X.Y.Z`. Do this after approval, since it verifies the attestation.

### What is public before approval

Public: the tag, and the Actions logs. The workflow masks the signer's name and Team ID in them; they become public with the release, inside the signed app.
Not public: the binaries, the attestation and the release page. The draft is visible only to people with write access.

### Reject, re-run, expiry

- Reject: reject the run in "Review deployments" (or cancel it), then `gh release delete vX.Y.Z --yes --cleanup-tag` to remove the draft and the tag. A run left waiting holds the tag's concurrency group, so a new run for the same tag would queue behind it.
- "Re-run all jobs" fails in `build` because the draft already exists. Re-run only the failed `publish` job, or delete the draft first.
- A run waiting for approval expires after 30 days. The draft stays.
- A run uses the `release.yml` from the tag. If `publish` is broken, delete the draft and tag and tag again after the fix.
- `publish` uses `GITHUB_TOKEN`, so the release does not trigger workflows on `release: published`.
- The attestation is made on the ubuntu runner after approval, not on the macOS runner that built the files. `publish` first checks the draft's files against the checksums `build` passed along, so it attests exactly what `build` made.

## Checking the result

```sh
gh release download vX.Y.Z --pattern '*.app.zip' --dir /tmp/jd && cd /tmp/jd
ditto -x -k just-desktops-X.Y.Z.app.zip .
codesign --verify --deep --strict --verbose=2 just-desktops.app
xcrun stapler validate just-desktops.app
spctl --assess --type execute --verbose=4 just-desktops.app   # source=Notarized Developer ID
```

The workflow runs the same checks and fails if the app is not accepted as notarized.

## Good to know

- The CLI binaries are signed and notarized but cannot carry a ticket, so Gatekeeper checks them online on first run.
- If the membership lapses, releases that are already notarized keep working. You cannot sign or notarize new ones.
- The designated requirement of a Developer ID app is based on the Team ID and bundle identifier, not the code hash. Accessibility and Screen Recording grants therefore persist across updates.
- To notarize by hand: `ASC_KEY_ID=... ASC_ISSUER_ID=... ASC_KEY_P8=path/to/AuthKey.p8 scripts/notarize.sh dist/just-desktops-X.Y.Z.app.zip`.
