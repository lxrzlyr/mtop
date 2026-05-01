# Migration from asitop

The old Python-based `asitop` prototype has been frozen under [old/](../old).

The new project differs in several ways:

- new name: `mtop`
- new implementation language: `C++ / Objective-C++`
- new default execution model: non-root first
- root-only metrics are optional enhancements rather than the primary path
- UI direction follows compact `htop` and `nvtop` patterns rather than the original boxed Python dashboard

The old code is preserved only for reference while the new implementation replaces it phase by phase.
