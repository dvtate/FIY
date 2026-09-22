# TODO

## CLI
Convert protocol/main.cpp into a CLI with more functionality (or make a bash wrapper).

### Commands
These are just some ideas
- Run server
  - specify/override config
  - this is current functionality
- Install mod from store (by id)
- View and edit mod and host configs
- Run admin web interface

## Protocol Server Changes
- FIY endpoints get their own path prefix, ie: /peer/key -> /fiy/peer/key
- Either move portal to /fiy/portal or make it its own app?
- Make landing page use static server?
- Subdomain mods redirect to path-based alternatives

## Micro-refactoring
- Replace SQLiteCpp
  - The library is badly designed
  - Maybe I should make my own sqlite wrapper or find a generic SQL library
- Replace std::stoll, atoi, etc. with std::from_chars
  - especially w/ std::string_view's
- Use `std::string_view` where it makes sense to reduce copies
- Members designed to be public should not start with `m_`
- Replace nlohmann JSON with [glaze](https://github.com/stephenberry/glaze)

## Portal+Protocol Server Features
- [minify](https://www.npmjs.com/package/minify) html+css+js?
- encrypt peer handshake
- DLL Mods: shared request task queue + thread pool
  - if task queue gets big, warn user

## Shared library mod to server mod wrapper
Currently developing all mods as .so's as this would give best performance on low-traffic systems.
This wrapper would enable some degree of horizontal scaling without having to rewrite the mods. 

## Logging system
- Something auditable?
- Log to file
- Module log files
- Maybe write logs in a different format?
- Maybe good for new users

## Much later
- How to handle websockets/webrtc/ssh?
  - Websockets should be easy
  - IDK anything about WebRTC but probably hard
  - SSH basically impossible
  - Mods could start their own servers, but admin would have to set up

- i18n:
  - backend: Boost.Locale ? GNU gettext ?
  - frontend: javascript ?
  - Problem: locale specific caches?
  
- Disable federation? Mod specific?
  - lots of settings
