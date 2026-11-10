# Design direction

Owner-supplied direction for PS5 Package Sender, recorded 2026-11-10:

- **Style:** modern minimalist, clean, professional.
- **Brand soul:** comfort and ease.
- **Audience:** PS5 owners moving games, updates and extra content onto a
  jailbroken console over their home network.

Design read (antislop audit 001): "Reading this as: a desktop file-transfer
utility for PS5 owners, in a modern minimalist, clean, professional style,
dial ENERGY 1 / RHYTHM 2 / MOTION 1."

## Identity accent

The default accent is PlayStation blue `#0070d1`. The product exists to
serve a PlayStation 5, so the console family's blue is the one color this
tool has a native claim to (the swap-the-logo test from audit 001, finding
8). It passes WCAG AA as a white-text fill at 4.96:1.

Offered accent presets, each AA-safe as a fill and, through the
`Theme.accentText` token, as text in both themes:

| Preset | Value     |
|--------|-----------|
| Blue   | `#0070d1` |
| Purple | `#6a48e8` |
| Pink   | `#c73e64` |
| Amber  | `#b45309` |
| Green  | `#157f4e` |

## Accessibility floor

- Text contrast meets WCAG AA (4.5:1 normal, 3:1 large) in both themes for
  every offered accent. The values in `qml/Theme.qml` are verified
  programmatically; see `anti-slop/audit-001-2026-11-10.md` and its fix
  report.
- Every control is reachable and operable by keyboard with a visible focus
  ring.
- Badge and meta text floor: 11px. Icon button target floor: 28px.
- Icon-only controls carry a tooltip and an accessible name.
