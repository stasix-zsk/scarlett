# Contributing

## Workflow
1. Fork and create a feature branch.
2. Keep changes focused and small.
3. Run local checks before opening a PR.

## Local Checks
```powershell
cmake --preset mingw
cmake --build --preset mingw
ctest --preset mingw
```

## Pull Requests
- Fill out the PR template completely.
- Ensure CI passes.
- Request review from code owners.
- Squash commits when merging.

## Commit Messages
Use clear, imperative messages (e.g. "Add ...", "Fix ...").

## Reporting Issues
Use issue templates and include reproduction steps.
