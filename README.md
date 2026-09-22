# FIY -- Federate It Yourself
A federation protocol and platform that makes it easy to make and host federated apps.

## Building
### Install Build Dependencies
- cmake
- npm
- boost
- nlohmann json
- sqlite3
- git
- libgit2
- libcurl
- openssl
- libcrypto

```
$ # Arch Linux
$ sudo pacman -S cmake npm git boost sqlite nlohmann-json libgit2 curl openssl crypto++
```
<!-- TODO commands for other common distros -->

### Download Sources
Clone this repo and submodules.

```
git clone https://fiy.to/git/fiy/fiy --recurse-submodules
```

### Build
Run `build.sh`.

### Install
- Run `install.sh`.
- See [INSTALL.md](INSTALL.md).