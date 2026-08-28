# Downloading and testing the Switch artifact

The chunk milestone is blocked until the NRO downloaded from GitHub Actions is tested. A local cross-compile is not sufficient.

## Download on a phone

1. Open the repository on GitHub and select **Actions**.
2. Open the successful **Build Switch** run for the commit shown on the game screen.
3. In **Artifacts**, download **VoxelGame-Switch**.
4. Extract the ZIP. It must contain `voxelgame.nro`, `build-info.txt`, and `romfs/build-marker.txt`.
5. Open `voxelgame.nro` in a Switch emulator that supports homebrew NRO files.
6. Confirm that the M0 screen appears and that its short commit hash matches `commit=` in `build-info.txt`.
7. Record the emulator name/version, result, commit, and an optional screenshot in the test report.

Do not treat a successful workflow or a structurally valid NRO as an emulator test. On real hardware, copy the package to the SD card under `/switch/voxelgame/` and start it through the Homebrew Menu.

## Verify after downloading on Windows

Before opening an emulator, verify the package and NRO header:

```powershell
./scripts/verify-switch-artifact.ps1 -ArtifactDirectory ./VoxelGame-Switch -ExpectedCommit <full-commit-hash>
```

