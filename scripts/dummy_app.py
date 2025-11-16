#!/usr/bin/env python3
#
# This script creates a minimal GTK app with the given app ID and displays the
# app ID, window pointer, and title inside the window.
#
# Note: This file contains LLM generated code.

import json
import os
import socket
import sys

import gi

gi.require_version("Gtk", "4.0")
from gi.repository import GLib, Gtk  # pyright: ignore[reportAttributeAccessIssue] # noqa: E402

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} <app_id> [title]")
    exit(1)

app_id = sys.argv[1]
title = None
if len(sys.argv) == 3:
    title = sys.argv[2]
GLib.set_prgname(app_id)
app = Gtk.Application()


def get_window_addr(app_id_to_find):
    sock_path = f"{os.environ['XDG_RUNTIME_DIR']}/hypr/{os.environ['HYPRLAND_INSTANCE_SIGNATURE']}/.socket.sock"

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(sock_path)
    sock.send(b"j/clients")

    response = b""
    while True:
        data = sock.recv(4096)
        if not data:
            break
        response += data
    sock.close()

    for client in reversed(json.loads(response.decode())):
        if client["class"] == app_id_to_find:
            return client["address"]


def on_activate(app):
    win = Gtk.ApplicationWindow(application=app)
    if title:
        win.set_title(title)
    win.set_decorated(False)
    win.set_default_size(400, 300)

    title_label = Gtk.Label()
    title_label.set_margin_top(10)
    title_label.set_markup(
        f"<span font='{16}' font_family='monospace'>{app_id}::{title}</span>"
    )
    title_label.set_hexpand(True)
    title_label.set_vexpand(True)

    addr_label = Gtk.Label()
    addr_label.set_hexpand(True)
    addr_label.set_vexpand(True)

    box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
    box.set_vexpand(True)
    box.set_valign(Gtk.Align.CENTER)
    box.append(title_label)
    box.append(addr_label)
    win.set_child(box)

    def on_map(_):
        GLib.timeout_add(150, lambda: (
            addr_label.set_markup(
                f"<span font_family='monospace' font='{14}'>{get_window_addr(app_id)}</span>"
            )
            or False)
        )

    win.connect("map", on_map)
    win.present()


app.connect("activate", on_activate)
app.run(None)
