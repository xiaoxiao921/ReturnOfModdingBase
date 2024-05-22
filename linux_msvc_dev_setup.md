# Developing MSVC binaries with a linux machine

### msvc-wine-git

Package that will install msvc on your machine. [Download it here](https://aur.archlinux.org/packages/msvc-wine-git)

If the package above download VS 17.10 and that you end up with MSVC 14.40.33807 (or around that), it's likely that the project won't compile properly because of regressions introduced by the Microsoft team. Edit the PKGBUILD so that it download something like 17.7 / 14.37 instead.

### Debug builds require the optional dependency from the msvc-wine-git package.

```sh
sudo apt install winbind
sudo service winbind start
# OR for archlinux, do this:
pacman -S libwbclient samba
```

### Building the project

```sh
# Prepare with cmake
msvc-x64-cmake -D CMAKE_BUILD_TYPE=Debug -S. -Bbuild -G Ninja

# Build the binary
msvc-x64-cmake --build ./build --config Debug --target ReturnOfModdingBase --
```
