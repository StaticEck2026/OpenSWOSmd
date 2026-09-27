# matchmaker-admin

A tiny password-protected web page with six buttons (`git pull`, rebuild, and
the four `swos-matchmaker-service.sh` actions) plus a live output pane, for
maintaining the matchmaker server without an SSH session.

## Why it's split into three pieces

The web-facing part (`main.py`) runs as an **unprivileged** user and can only
ever write a small, named file into a queue directory - it cannot run git,
cmake, or systemctl itself, and never will. A **root** cron job
(`admin-cron-worker.sh`) is the only thing that actually executes anything,
polling that queue once a minute. This way a bug or a leaked password in the
web part can, at worst, queue one of six fixed actions - never arbitrary code.

```
browser --(auth_basic, HTTPS)--> nginx --(127.0.0.1:8899)--> matchmaker-admin (unprivileged)
                                                                     |
                                                       writes /srv/swos-matchmaker/admin/requests/<ts>-<action>.req
                                                                     |
                                                     root cron, once a minute -> admin-cron-worker.sh
                                                                     |
                                                    git pull / build (as redhair) or systemctl (as root)
                                                                     |
                                          /srv/swos-matchmaker/admin/processed/<ts>-<action>.log + latest.log
```

## One-time setup on the server

1. **Install the gateway service:**
   ```
   tools/matchmaker-admin/matchmaker-admin-service.sh install
   ```
   (installs+starts it as a systemd service, bound to
   127.0.0.1:8899 only - see that script for the unit file it writes.)

2. **Root crontab** (`sudo crontab -e`, as root):
   ```
   * * * * * /home/redhair/git/OpenSWOS/tools/matchmaker-admin/admin-cron-worker.sh >/dev/null 2>&1
   ```

3. **nginx: password + reverse proxy.** Create the password file once:
   ```
   sudo apt-get install -y apache2-utils   # if htpasswd isn't already there
   sudo htpasswd -c /etc/nginx/.htpasswd-matchmaker-admin redhair
   ```
   Then add this location block to the `us.swos.eu` HTTPS server block
   (alongside the `/reports/` one, if that's already there):
   ```nginx
   location /cc/ {
       auth_basic "matchmaker admin";
       auth_basic_user_file /etc/nginx/.htpasswd-matchmaker-admin;

       proxy_pass http://127.0.0.1:8899/;
       proxy_set_header Host $host;
   }
   ```
   Test and reload: `sudo nginx -t && sudo systemctl reload nginx`.

Afterwards, `https://us.swos.eu/cc/` asks for the password and shows the
button page.

## Files

- `main.py` - the gateway (stdlib only, no dependencies to install - run
  directly with `python3 main.py`, or via `matchmaker-admin-service.sh`).
- `admin-cron-worker.sh` - the root worker; also documents the request-file
  naming scheme and the action allowlist (must match `main.py`'s own list).
- `matchmaker-admin-service.sh` - installs/restarts/removes/status's the
  gateway's own systemd service (same `install|restart|remove|status`
  convention as `scripts/swos-matchmaker-service.sh`).
