# Releasing

A `v*` tag runs `.github/workflows/release.yml`. It builds the CLI tarballs and the app zip, attests them and publishes a GitHub release.

Without the secrets below the artifacts are ad-hoc signed, as before. With them the workflow signs with a Developer ID Application certificate, notarizes, staples the app and re-zips it, then computes the checksums and attestations from the final files.

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

## Cutting a release

1. Set `JD_VERSION` in `src/common.h` to the new version and commit it.
2. `git tag vX.Y.Z && git push origin vX.Y.Z`. The workflow fails if the tag and `JD_VERSION` differ.
3. Update the cask: `scripts/bump-cask.sh X.Y.Z`.

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
