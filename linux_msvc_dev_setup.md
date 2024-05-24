# Developing MSVC binaries with a linux machine

### msvc-wine-git

Package that will install msvc on your machine. [Download it here](https://aur.archlinux.org/packages/msvc-wine-git)

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
