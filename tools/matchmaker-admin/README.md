# matchmaker-admin

A tiny password-protected web page with six buttons (`git pull`, rebuild, and
the four `swos-matchmaker-service.sh` actions) plus a live output pane, for
maintaining the matchmaker server without an SSH session.

## Two tiers, by whether the action actually needs root

`pull`, `build` and `status` need no elevated privilege at all - a plain git
pull, a plain build, and reading a systemd unit's status are all fine as the
same unprivileged user the gateway already runs as. Those three run
**immediately**, right inside the already-running gateway process (a
background thread), no queue, no cron, no delay.

`install`, `remove` and `restart` genuinely need root (`systemctl enable/
disable/start/stop --now`, and writing the unit file into `/etc/systemd`).
The gateway (`main.py`) must never have that, so for these three it only ever
writes a small, named, empty file into a queue directory - never runs
anything itself. A **root** cron job (`admin-cron-worker.sh`) is the only
thing that actually executes those, polling the queue once a minute. This way
a bug or a leaked password in the web part can, at worst, trigger one of six
fixed actions - never arbitrary code, and never with more privilege than the
action actually needs.

```
                                     pull / build / status (no root needed)
                                                     |
browser --(auth_basic, HTTPS)--> nginx --(127.0.0.1:8899)--> matchmaker-admin (unprivileged)
                                                                     |
                                                     install / remove / restart (needs root)
                                                                     |
                                       writes /srv/swos-matchmaker/admin/requests/<ts>-<action>.req
                                                                     |
                                                     root cron, once a minute -> admin-cron-worker.sh
                                                                     |
                                                              systemctl (as root)
```

Either way, output lands in `/srv/swos-matchmaker/admin/processed/<ts>-<action>.log`
and `latest.log` - the page polls `latest.log` regardless of which tier ran.

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
- `admin-cron-worker.sh` - the root worker for `install`/`remove`/`restart`
  only; also documents the request-file naming scheme (must match `main.py`'s
  own `QUEUED_ACTIONS`).
- `matchmaker-admin-service.sh` - installs/restarts/removes/status's the
  gateway's own systemd service (same `install|restart|remove|status`
  convention as `scripts/swos-matchmaker-service.sh`).
