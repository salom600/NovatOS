#!/usr/bin/env python3
"""
NovatOS Panel — custom GTK3 taskbar
=====================================
A modern, lightweight taskbar built with Python + GTK3.
Features:
  - Start button (opens app launcher)
  - Running windows (click to focus/minimize)
  - Workspaces
  - System tray: volume, network, battery, clock
  - Power button (shutdown/reboot/suspend)
  - Dark NovatOS theme
"""

import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Gdk', '3.0')
from gi.repository import Gtk, Gdk, GLib, GObject
import subprocess
import os
import time
import threading

class NovatOSPanel(Gtk.Window):
    def __init__(self):
        super().__init__(title="NovatOS Panel")
        
        # Window properties — bottom of screen, full width, no decorations
        self.set_type_hint(Gdk.WindowTypeHint.DOCK)
        self.set_decorated(False)
        self.set_skip_taskbar_hint(True)
        self.set_skip_pager_hint(True)
        self.set_keep_above(True)
        
        # Get screen dimensions
        screen = Gdk.Screen.get_default()
        monitor = screen.get_monitor_geometry(0)
        panel_height = 44
        
        # Position at bottom of screen
        self.move(0, monitor.height - panel_height)
        self.set_size_request(monitor.width, panel_height)
        
        # Dark theme
        self.set_name("novatos-panel")
        self.apply_css()
        
        # Create UI
        self.create_ui()
        
        # Start clock update
        GLib.timeout_add_seconds(1, self.update_clock)
        self.update_clock()
        
        # Start window list update
        GLib.timeout_add_seconds(2, self.update_windows)
        
        # Reshow
        self.show_all()
    
    def apply_css(self):
        css = b"""
        #novatos-panel {
            background-color: rgba(15, 17, 23, 0.92);
            border-top: 1px solid rgba(76, 194, 255, 0.2);
        }
        #novatos-panel .start-button {
            background-color: rgba(76, 194, 255, 0.1);
            color: #4CC2FF;
            border: none;
            border-radius: 8px;
            padding: 6px 16px;
            font-weight: bold;
            font-size: 13px;
        }
        #novatos-panel .start-button:hover {
            background-color: rgba(76, 194, 255, 0.25);
        }
        #novatos-panel .task-button {
            background-color: transparent;
            color: #9DB7E0;
            border: none;
            border-radius: 6px;
            padding: 4px 10px;
            font-size: 12px;
        }
        #novatos-panel .task-button:hover {
            background-color: rgba(255, 255, 255, 0.1);
            color: #FFFFFF;
        }
        #novatos-panel .task-button.active {
            background-color: rgba(76, 194, 255, 0.15);
            color: #4CC2FF;
        }
        #novatos-panel .tray-label {
            color: #9DB7E0;
            font-size: 12px;
            padding: 0 8px;
        }
        #novatos-panel .clock-label {
            background-color: rgba(76, 194, 255, 0.1);
            color: #4CC2FF;
            border-radius: 8px;
            padding: 4px 12px;
            font-weight: 600;
            font-size: 13px;
        }
        #novatos-panel .power-button {
            color: #FF6B6B;
            background-color: transparent;
            border: none;
            border-radius: 8px;
            padding: 4px 12px;
            font-size: 16px;
        }
        #novatos-panel .power-button:hover {
            background-color: rgba(255, 107, 107, 0.2);
        }
        """
        provider = Gtk.CssProvider()
        provider.load_from_data(css)
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(), provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )
    
    def create_ui(self):
        # Main horizontal box
        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=4)
        hbox.set_margin_start(8)
        hbox.set_margin_end(8)
        hbox.set_margin_top(2)
        hbox.set_margin_bottom(2)
        self.add(hbox)
        
        # ─── Left: Start button ───
        self.start_btn = Gtk.Button(label="  Start  ")
        self.start_btn.set_name("start-button")
        self.start_btn.connect("clicked", self.on_start_clicked)
        hbox.pack_start(self.start_btn, False, False, 0)
        
        # Separator
        hbox.pack_start(Gtk.VSeparator(), False, False, 4)
        
        # ─── Center: Running windows ───
        self.task_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=2)
        hbox.pack_start(self.task_box, True, True, 0)
        
        # ─── Right: System tray ───
        
        # Volume
        vol_btn = Gtk.Button(label="♪")
        vol_btn.set_name("task-button")
        vol_btn.connect("clicked", lambda w: subprocess.Popen(["pavucontrol"]))
        hbox.pack_start(vol_btn, False, False, 0)
        
        # Network
        net_btn = Gtk.Button(label="NET")
        net_btn.set_name("task-button")
        net_btn.connect("clicked", lambda w: subprocess.Popen(["nm-connection-editor"]))
        hbox.pack_start(net_btn, False, False, 0)
        
        # Battery
        self.batt_label = Gtk.Label(label="BAT")
        self.batt_label.set_name("tray-label")
        hbox.pack_start(self.batt_label, False, False, 0)
        
        # Separator
        hbox.pack_start(Gtk.VSeparator(), False, False, 4)
        
        # Clock
        self.clock_label = Gtk.Label()
        self.clock_label.set_name("clock-label")
        hbox.pack_start(self.clock_label, False, False, 0)
        
        # Power button
        power_btn = Gtk.Button(label="⏻")
        power_btn.set_name("power-button")
        power_btn.connect("clicked", self.on_power_clicked)
        hbox.pack_start(power_btn, False, False, 0)
        
        # Start battery update
        GLib.timeout_add_seconds(10, self.update_battery)
        self.update_battery()
    
    def on_start_clicked(self, widget):
        # Launch app launcher
        subprocess.Popen(["novatos-launcher"])
    
    def on_power_clicked(self, widget):
        # Power menu
        menu = Gtk.Menu()
        
        suspend = Gtk.MenuItem(label="Suspend")
        suspend.connect("activate", lambda w: subprocess.Popen(["systemctl", "suspend"]))
        menu.append(suspend)
        
        reboot = Gtk.MenuItem(label="Restart")
        reboot.connect("activate", lambda w: subprocess.Popen(["systemctl", "reboot"]))
        menu.append(reboot)
        
        shutdown = Gtk.MenuItem(label="Shut Down")
        shutdown.connect("activate", lambda w: subprocess.Popen(["systemctl", "poweroff"]))
        menu.append(shutdown)
        
        menu.show_all()
        menu.popup_at_widget(widget, Gdk.Gravity.NORTH_EAST, Gdk.Gravity.SOUTH_EAST, None)
    
    def update_clock(self):
        now = time.strftime("%H:%M")
        self.clock_label.set_text(now)
        return True
    
    def update_battery(self):
        try:
            with open("/sys/class/power_supply/BAT0/capacity") as f:
                cap = f.read().strip()
            with open("/sys/class/power_supply/BAT0/status") as f:
                status = f.read().strip()
            if status == "Charging":
                self.batt_label.set_text(f"⚡{cap}%")
            else:
                self.batt_label.set_text(f"BAT {cap}%")
        except:
            self.batt_label.set_text("")
        return True
    
    def update_windows(self):
        # Clear existing
        for child in self.task_box.get_children():
            self.task_box.remove(child)
        
        # Get window list via wmctrl
        try:
            result = subprocess.run(["wmctrl", "-l"], capture_output=True, text=True, timeout=2)
            for line in result.stdout.strip().split('\n'):
                if line:
                    parts = line.split(None, 3)
                    if len(parts) >= 4:
                        title = parts[3][:30]
                        btn = Gtk.Button(label=title)
                        btn.set_name("task-button")
                        btn.connect("clicked", lambda w, wid=parts[0]: self.focus_window(wid))
                        self.task_box.pack_start(btn, False, False, 0)
            self.task_box.show_all()
        except:
            pass
        return True
    
    def focus_window(self, window_id):
        subprocess.run(["wmctrl", "-i", "-a", window_id])

class AppLauncher(Gtk.Window):
    """App launcher — Start menu replacement"""
    def __init__(self):
        super().__init__(title="NovatOS Launcher")
        self.set_type_hint(Gdk.WindowTypeHint.POPUP_MENU)
        self.set_decorated(False)
        self.set_skip_taskbar_hint(True)
        self.set_size_request(400, 500)
        
        self.apply_css()
        
        # Main container
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        vbox.set_margin_top(16)
        vbox.set_margin_bottom(16)
        vbox.set_margin_start(16)
        vbox.set_margin_end(16)
        self.add(vbox)
        
        # Search entry
        self.search = Gtk.SearchEntry()
        self.search.set_placeholder_text("Search applications...")
        self.search.connect("changed", self.on_search_changed)
        self.search.connect("activate", self.on_search_activate)
        vbox.pack_start(self.search, False, False, 0)
        
        # App list
        self.listbox = Gtk.ListBox()
        self.listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        self.listbox.connect("row-activated", self.on_app_activated)
        
        scroll = Gtk.ScrolledWindow()
        scroll.add(self.listbox)
        scroll.set_min_content_height(350)
        vbox.pack_start(scroll, True, True, 0)
        
        # Bottom: power buttons
        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        
        terminal_btn = Gtk.Button(label="Terminal")
        terminal_btn.connect("clicked", lambda w: self.launch("lxterminal"))
        hbox.pack_start(terminal_btn, False, False, 0)
        
        files_btn = Gtk.Button(label="Files")
        files_btn.connect("clicked", lambda w: self.launch("pcmanfm"))
        hbox.pack_start(files_btn, False, False, 0)
        
        hbox.pack_end(Gtk.Label(), True, True, 0)
        
        suspend_btn = Gtk.Button(label="Suspend")
        suspend_btn.connect("clicked", lambda w: subprocess.Popen(["systemctl", "suspend"]))
        hbox.pack_start(suspend_btn, False, False, 0)
        
        reboot_btn = Gtk.Button(label="Restart")
        reboot_btn.connect("clicked", lambda w: subprocess.Popen(["systemctl", "reboot"]))
        hbox.pack_start(reboot_btn, False, False, 0)
        
        poweroff_btn = Gtk.Button(label="Shut Down")
        poweroff_btn.connect("clicked", lambda w: subprocess.Popen(["systemctl", "poweroff"]))
        hbox.pack_start(poweroff_btn, False, False, 0)
        
        vbox.pack_start(hbox, False, False, 0)
        
        # Load apps
        self.apps = []
        self.load_apps()
        self.populate_list("")
        
        # Position near bottom-left (above taskbar)
        screen = Gdk.Screen.get_default()
        monitor = screen.get_monitor_geometry(0)
        self.move(20, monitor.height - 500 - 50)
        
        # Focus search
        self.search.grab_focus()
        
        # Close on focus loss
        self.connect("focus-out-event", lambda w, e: self.close_launcher())
    
    def apply_css(self):
        css = b"""
        NovatOSLauncher, window {
            background-color: #161922;
            border: 1px solid #4CC2FF44;
            border-radius: 16px;
        }
        entry {
            background-color: #0F1117;
            border: 2px solid #2A2F3D;
            border-radius: 10px;
            padding: 10px;
            color: #FFFFFF;
            font-size: 14px;
        }
        entry:focus { border-color: #4CC2FF; }
        list { background-color: transparent; }
        row {
            padding: 8px 12px;
            border-radius: 8px;
            color: #FFFFFF;
        }
        row:selected {
            background-color: rgba(76, 194, 255, 0.2);
            color: #4CC2FF;
        }
        button {
            background-color: transparent;
            border: 1px solid #2A2F3D;
            border-radius: 8px;
            padding: 6px 12px;
            color: #9DB7E0;
            font-size: 12px;
        }
        button:hover {
            background-color: rgba(255, 255, 255, 0.1);
            color: #FFFFFF;
        }
        """
        provider = Gtk.CssProvider()
        provider.load_from_data(css)
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(), provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )
    
    def load_apps(self):
        import glob
        app_dirs = [
            "/usr/share/applications",
            "/usr/local/share/applications",
            os.path.expanduser("~/.local/share/applications")
        ]
        
        for app_dir in app_dirs:
            for desktop_file in glob.glob(os.path.join(app_dir, "*.desktop")):
                try:
                    name = ""
                    exec_cmd = ""
                    no_display = False
                    
                    with open(desktop_file) as f:
                        for line in f:
                            if line.startswith("Name=") and not name:
                                name = line[5:].strip()
                            elif line.startswith("Exec=") and not exec_cmd:
                                exec_cmd = line[5:].strip().split()[0]
                            elif line.startswith("NoDisplay=true"):
                                no_display = True
                                break
                    
                    if not no_display and name and exec_cmd:
                        self.apps.append((name, exec_cmd))
                except:
                    pass
        
        self.apps.sort(key=lambda x: x[0].lower())
    
    def populate_list(self, query):
        # Clear
        for child in self.listbox.get_children():
            self.listbox.remove(child)
        
        query_lower = query.lower()
        for name, exec_cmd in self.apps:
            if not query or query_lower in name.lower():
                row = Gtk.ListBoxRow()
                label = Gtk.Label(label=name, xalign=0)
                label.set_margin_start(8)
                row.add(label)
                row.exec_cmd = exec_cmd
                self.listbox.add(row)
        
        self.listbox.show_all()
    
    def on_search_changed(self, widget):
        self.populate_list(widget.get_text())
    
    def on_search_activate(self, widget):
        row = self.listbox.get_selected_row()
        if row:
            self.launch(row.exec_cmd)
    
    def on_app_activated(self, listbox, row):
        self.launch(row.exec_cmd)
    
    def launch(self, cmd):
        subprocess.Popen([cmd])
        self.close_launcher()
    
    def close_launcher(self):
        self.destroy()
        Gtk.main_quit()

def run_panel():
    win = NovatOSPanel()
    win.connect("destroy", Gtk.main_quit)
    Gtk.main()

def run_launcher():
    win = AppLauncher()
    win.show_all()
    Gtk.main()

if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1 and sys.argv[1] == "launcher":
        run_launcher()
    else:
        run_panel()
