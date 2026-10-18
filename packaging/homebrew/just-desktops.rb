# Template for n-seiji/homebrew-tap (Casks/just-desktops.rb).
# scripts/bump-cask.sh rewrites version and sha256 from a release.
# TODO: once a release is signed with a Developer ID and notarized (docs/releasing.md), drop the
# Gatekeeper and ad-hoc paragraphs from the caveats below.
cask "just-desktops" do
  version "0.1.0"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000"

  url "https://github.com/n-seiji/just-desktops/releases/download/v#{version}/just-desktops-#{version}.app.zip"
  name "just-desktops"
  desc "Minimal workspace switcher and focus border"
  homepage "https://github.com/n-seiji/just-desktops"

  depends_on macos: ">= :ventura"

  app "just-desktops.app"

  uninstall quit: "dev.n-seiji.just-desktops"

  zap trash: "~/.config/just-desktops"

  caveats <<~EOS
    First launch:
      1. Open just-desktops. macOS asks for Accessibility permission; allow it
         under System Settings > Privacy & Security > Accessibility.
         It starts working as soon as the permission is granted.
      2. Screen Recording is only needed for `switcher = square` in
         ~/.config/just-desktops/config. It is asked for at launch in that mode only.

    If this version is not notarized and macOS blocks it, open System Settings >
    Privacy & Security and choose "Open Anyway" for this app only. On
    macOS 15 and later right-click > Open no longer bypasses Gatekeeper. Do not
    disable Gatekeeper (spctl --master-disable) or run xattr -cr. Homebrew has
    already checked the download against the pinned sha256; the release also carries
    a build attestation (gh attestation verify <zip> --repo n-seiji/just-desktops).

    If this version is ad-hoc signed, macOS asks for the permissions again after
    every update. Grant them again, or remove the old entry and add the app.

    To start at login, add just-desktops under System Settings > General >
    Login Items. Do not run it together with the launchd service
    (`mise run service`); the second instance exits.
  EOS
end
