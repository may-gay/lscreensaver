# lscreensaver

a lightweight screensaver that for both X11 and Wayland!

lscreensaver is a tiny, standalone screen-blanking daemon for Linux that works under both X11 and Wayland.

# Features
```
• Configurable idle timeout (in seconds).
• Exempt app list: if any listed process is running, the screen will not blank.
```
# Requirements
```
• libinput
• SDL2
• membership of the “input” group (so libinput can read /dev/input/event*).
```
# Installation (from source)

    Install dependencies:

    sudo pacman -S libinput sdl2

    Clone or download this repo:

    git clone https://github.com/may-gay/lscreensaver.git
    cd lscreensaver

    Build:

    gcc main.c -o lscreensaver 
    $(pkg-config --cflags --libs libudev libinput sdl2) -D_GNU_SOURCE

    Install somewhere on your $PATH:

    sudo install -Dm755 lscreensaver /usr/local/bin/lscreensaver

# Configuration

Copy or create ~/.config/lscreensaver/config with two settings:

    # inactivity_time in seconds
    inactivity_time=180

    # exempt_processes: comma-separated process names; if any is running, will not blank
    exempt_processes=jellyfinmediaplayer

# Usage

Just run:

    lscreensaver

You can run it in the foreground, or daemonize it, e.g. via a systemd user service:
```
[Unit]
Description=Lightweight screen blanker

[Service]
ExecStart=/usr/local/bin/lscreensaver
Restart=on-failure

[Install]
WantedBy=default.target
```
Enable & start:
```
systemctl --user enable --now lscreensaver.service
```
