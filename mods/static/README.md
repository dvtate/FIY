# HTTP File Server
Serve static files.

## Module.json settings
Add a `mod_settings` field containing an object that has the following properties.
### `mod_settings` . `root` (required)
Path that the static files are stored in.
- example: `"/var/www/public"`

### `mod_settings` . `index` (optional)
Array of file names to check for if a folder is requested.
- default: `["index.html","index.htm","index.xhtml"]`

### `mod_settings` . `list_directories` (optional)
For directories without an index file, should the user be shown a list of files in the directory (`true`) or be given
a 404 page (`false`)?
- default: `true`

### `mod_settings` . `cache` (optional)
Should the client cache these files?
- default: `false`

### Tips
- The `id` field should be globally unique, the easiest way to ensure this is to use reverse domain naming convention 
  starting with the instance's domain, thus making giving it a local id.
- do not use the same module.so file via symlink for multiple static mods
  - ie - mods/blog and mods/static.root should each have their own copies of module.so
- Also note that the `"access"` field can be used to restrict access to the mod.

## Problem: multi-path
Users will likely want to have multiple statically hosted paths (eg. /, /blog, /wiki, etc.)
- Solution: Duplicate the mod
  - For each statically hosted path, have a separate mod.
  - Each mod needs:
    - distinct module.json, and copy/symlink to module.so and icons 
    - distinct app id (does it?)
    - distinct path
  - Downsides:
    - More admin configuration and not very intuitive
