# Server Installation
This guide is mostly intended for developers and/or package maintainers.

## Interactive Installation Script
The [`install.sh`](install.sh) script is currently the only supported way to install FIY.

Features:
- Development installation that installs files as symlinks to relevant project files and build targets.
- Makes it easy to install multiple instances on a single machine.

## config.ini
The install.sh script will walk you through the construction of config.ini file.

It should look like this. For a complete list of options, check the `Config.hpp`

```
# Where your instance will be accessible at
hostname=fiy.yourdomain.com

# Where FIY is installed
data_dir=/opt/fiy

# Additional entropy source
salt=your_random_salt_here_change_this

# Port for protocol server to listen on
# It's expected that you use a reverse proxy
port=8848

# Number of IO threads to use (default: `nproc`)
concurrency=4
```

## SSL
The server should be reverse-proxied through a server that adds ssl.

For example, this nginx.conf should work:
```nginx.conf
    server {
        listen 80;
        listen [::]:80;
        server_name fiy.to;
        server_name *.fiy.to;
        access_log /var/log/nginx/fiy.access.log;
        location / {
            proxy_pass http://127.0.0.1:8848;
            proxy_set_header Host $host;
        }
    }
    server {
        listen 443 ssl;
        listen [::]:443 ssl;
        server_name fiy.to;
        server_name *.fiy.to;
        access_log /var/log/nginx/fiy.access.log;
        ssl_certificate  /etc/letsencrypt/live/fiy.to-0001/fullchain.pem;
        ssl_certificate_key /etc/letsencrypt/live/fiy.to-0001/privkey.pem;
        location / {
            proxy_pass http://127.0.0.1:8848;
            proxy_set_header Host $host;
        }
    }
```
Run install.sh.

## Security
Consider running the FIY server as a dedicated user that only has access to files in the install dir.

## Notes
- Your system clock must be accurate in order to interact with other instances

## TODO: Paths
Eventually these will be accurate, for now everything is in a provided install directory
- `/usr/bin/fiy` - portal/protcol server
- `/usr/share/fiy/config.ini`
- `/usr/share/fiy/templates/*.html`
- `/var/lib/fiy/db.db3` - database