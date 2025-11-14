#!/usr/bin/env python3
#
# This script creates a minimal GTK app with the given app ID and displays the
# window pointer inside the window.
#
# Note: This file contains LLM generated code.

import gi
import sys
import os
import json
import socket

gi.require_version("Gtk", "4.0")
from gi.repository import Gtk, GObject, GLib # pyright: ignore[reportAttributeAccessIssue] # noqa: E402

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <app_id>")
    exit(1)

app_id = sys.argv[1]
GLib.set_prgname(app_id)
app = Gtk.Application()


def get_window_address(app_id_to_find):
    signature = os.environ["HYPRLAND_INSTANCE_SIGNATURE"]
    xdg_runtime_dir = os.environ["XDG_RUNTIME_DIR"]
    sock_path = f"{xdg_runtime_dir}/hypr/{signature}/.socket.sock"

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

    clients = json.loads(response.decode("utf-8"))

    for client in reversed(clients):
        if client.get("class") == app_id_to_find:
            return client["address"]

    return "Error: Not Found"


def on_activate(app):
    win = Gtk.ApplicationWindow(application=app)
    win.set_decorated(False)
    win.set_default_size(200, 150)

    label = Gtk.Label()
    label.set_hexpand(True)
    label.set_vexpand(True)
    win.set_child(label)

    def update_title_from_ipc():
        title = get_window_address(app_id)
        label.set_markup(f"<span font='{20}'>{app_id}: {title}</span>")
        return False

    def on_map(_):
        GObject.timeout_add(150, update_title_from_ipc)

    win.connect("map", on_map)
    win.present()


app.connect("activate", on_activate)
app.run(None)
