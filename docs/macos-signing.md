# macOS signing policy

How OBS-Studio-Rust release builds for macOS are signed and notarized, and
the rules that keep the credentials safe. Background: #2383, #2424, #2430.

## What gets signed

- **Releases only.** `release-rust.yaml` (tags and `verify_only`
  dispatches on `master`) signs with the Developer ID and notarizes.
  PR, fork and merge-queue builds are ad-hoc signed (`-`) and never see the
  secrets.
- **Everything inside the bundle.** Xcode's sign-on-copy signs only the top
  level of each embedded framework. `.github/scripts/.package.zsh` signs the
  nested code of prebuilt frameworks (Sparkle's helpers, CEF's
  `Libraries/*.dylib`) from the inside out with `--timestamp --options
  runtime`, reseals the app, and requires
  `codesign --verify --strict --deep` before it builds the DMG. A new
  prebuilt framework with nested code gets a line there.
- **Notarize, then staple.** The DMG goes to `notarytool submit --wait`
  and `stapler staple`. On failure the script prints Apple's notary log, so
  the reason is in the CI output.

## An unsigned release is a failed release

A tagged release whose macOS jobs do not report `codesign: true` and
`notarize: true`, or whose DMG fails `spctl --assess` or
`stapler validate`, is not published. Do not ship it with a "right-click,
Open" or `xattr -d com.apple.quarantine` workaround in the notes.

## Secrets

Repository secrets on `obs-rust/obs-studio`, read only by
`.github/actions/setup-macos-codesigning`:

| Secret | What |
| --- | --- |
| `MACOS_SIGNING_IDENTITY` | `Developer ID Application: <name> (<team>)` |
| `MACOS_SIGNING_CERT` | base64 of the `.p12` (exported with `-legacy` so `security import` reads it) |
| `MACOS_SIGNING_CERT_PASSWORD` | the `.p12` password |
| `MACOS_NOTARIZATION_USERNAME` | Apple ID that notarizes |
| `MACOS_NOTARIZATION_PASSWORD` | app-specific password for that Apple ID |

- Set them with `gh secret set` reading from stdin or a prompt. Never paste
  a value into an issue, PR, chat or log.
- The private key and `.p12` are kept offline by the account holder. The
  app-specific password is not stored anywhere but the secret; if it is
  lost, make a new one.
- A value that was ever shown in plain text (a chat, a log, a screenshot)
  is compromised: revoke it and set a new one the same day.

## Rotation and expiry

- **Developer ID certificate:** valid five years (the current one expires
  2027-11-24). Make the replacement at least 30 days before, from a new CSR,
  and update `MACOS_SIGNING_CERT`, `MACOS_SIGNING_CERT_PASSWORD` and
  `MACOS_SIGNING_IDENTITY` together. Builds already notarized stay valid
  after the certificate expires.
- **App-specific password:** rotate yearly, and whenever someone with
  access leaves.
- **Apple Developer Program membership:** renews yearly. If it lapses,
  notarization stops and releases fail by the rule above.
- After any change, run `release-rust.yaml` with `verify_only=true` and
  check both macOS jobs report `Accepted` before the next tag.
