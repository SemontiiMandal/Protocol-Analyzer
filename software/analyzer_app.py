import tkinter as tk
from tkinter import ttk, scrolledtext, filedialog
import serial
import serial.tools.list_ports
import threading
import time
import re
import csv
import sys
from collections import deque

# Colors per protocol, used consistently across the graph, the data log and legend
PROTOCOL_COLORS = {
    "SPI": "#00d9ff",
    "I2C": "#ffe135",
    "UART": "#ff66c4",
    "CAN": "#ffa500",
}

RE_SPI_BYTE = re.compile(r"^\[SPI\] MOSI: 0x([0-9A-Fa-f]{2}) \| MISO: 0x([0-9A-Fa-f]{2})$")
RE_SPI_CS = re.compile(r"^\[SPI\] CS (LOW|HIGH)")
RE_I2C_BYTE = re.compile(r"^\[I2C\] Byte: 0x([0-9A-Fa-f]{2}) \[(ACK|NACK)\]$")
RE_I2C_COND = re.compile(r"^\[I2C\] (START|STOP) Condition$")
RE_UART_BYTE = re.compile(r"^\[UART\] 0x([0-9A-Fa-f]{2}) \('(.+)'\)$")
RE_UART_ERR = re.compile(r"^\[UART\] ERROR")
RE_CAN_FRAME = re.compile(
    r"^\[CAN\] ID: 0x([0-9A-Fa-f]{3}) \| DLC: (\d+) \| "
    r"Rx CRC: 0x([0-9A-Fa-f]{4}) \| Calc CRC: 0x([0-9A-Fa-f]{4}) \[(VALID|ERROR)\]$"
)


class ProtocolAnalyzerUI:
    def __init__(self, root):
        self.root = root
        self.root.title("8-Bit Protocol Analyzer")
        self.root.geometry("1000x650")
        self.root.configure(bg="#1e1e1e")

        self.serial_port = None
        self.is_connected = False
        self.read_thread = None

        self.total_bytes = 0
        self.packet_counts = {"SPI": 0, "I2C": 0, "UART": 0, "CAN": 0}
        self.error_count = 0
        self.capture_start_time = None

        # Graph state: decoded bytes expanded into digital bit samples
        # Each entry is (protocol, bit). This is a digital-style visualization
        # of the decoded data. It is NOT a timing-accurate raw logic capture
        # because the current serial format does not contain timestamps yet, need to iterate further on this
        self.recent_bits = deque(maxlen=1600)

        # Data log rows, kept for CSV export
        self.log_rows = []

        self.setup_ui()
        self.refresh_ports()
        self.tick_stats()

    # UI construction
    def setup_ui(self):
        style = ttk.Style()
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass
        style.configure("TNotebook", background="#1e1e1e", borderwidth=0)
        style.configure("TNotebook.Tab", background="#2a2a2a", foreground="white", padding=(12, 6))
        style.map("TNotebook.Tab", background=[("selected", "#3a3a3a")])
        style.configure("Treeview", background="#141414", fieldbackground="#141414",
                         foreground="#e0e0e0", rowheight=22)
        style.configure("Treeview.Heading", background="#2a2a2a", foreground="white")

        # Top: connection & mode controls
        control_frame = tk.Frame(self.root, pady=8, padx=10, bg="#1e1e1e")
        control_frame.pack(fill=tk.X)

        tk.Label(control_frame, text="COM Port:", bg="#1e1e1e", fg="white").pack(side=tk.LEFT)
        self.port_combo = ttk.Combobox(control_frame, width=15)
        self.port_combo.pack(side=tk.LEFT, padx=5)

        tk.Button(control_frame, text="Refresh", command=self.refresh_ports).pack(side=tk.LEFT, padx=5)
        self.btn_connect = tk.Button(control_frame, text="Connect", command=self.toggle_connection, bg="lightgray")
        self.btn_connect.pack(side=tk.LEFT, padx=15)

        tk.Label(control_frame, text="Protocol Mode:", bg="#1e1e1e", fg="white").pack(side=tk.LEFT, padx=(20, 0))
        self.mode_var = tk.StringVar()
        self.mode_combo = ttk.Combobox(control_frame, textvariable=self.mode_var, state="readonly", width=20)
        self.mode_combo["values"] = (
            "IDLE (0)",
            "SPI Only (S)",
            "I2C Only (I)",
            "UART Only (U)",
            "CAN Only (C)",
            "Dual SPI+I2C (1)",
            "SPI+CAN+UART (2)",
            "I2C+CAN+UART (3)",
        )
        self.mode_combo.current(0)
        self.mode_combo.pack(side=tk.LEFT, padx=5)
        tk.Button(control_frame, text="Set Mode", command=self.send_mode).pack(side=tk.LEFT, padx=5)

        # --- Capture controls ---
        capture_frame = tk.Frame(self.root, pady=4, padx=10, bg="#1e1e1e")
        capture_frame.pack(fill=tk.X)

        self.btn_start = tk.Button(capture_frame, text="\u25b6 START CAPTURE (G)",
                                    command=lambda: self.send_command("G"),
                                    bg="lightgreen", state=tk.DISABLED)
        self.btn_start.pack(side=tk.LEFT, padx=5)

        self.btn_stop = tk.Button(capture_frame, text="\u23f8 STOP CAPTURE (H)",
                                   command=lambda: self.send_command("H"),
                                   bg="salmon", state=tk.DISABLED)
        self.btn_stop.pack(side=tk.LEFT, padx=5)

        tk.Button(capture_frame, text="Clear All", command=self.clear_all).pack(side=tk.LEFT, padx=15)
        tk.Button(capture_frame, text="Export Log to CSV", command=self.export_csv).pack(side=tk.LEFT, padx=5)

        # --- Notebook: Live View / Data Log / Raw Terminal ---
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        self._build_live_view_tab()
        self._build_data_log_tab()
        self._build_raw_terminal_tab()

        # --- Status bar ---
        self.status_var = tk.StringVar(value="Not connected.")
        status_bar = tk.Label(self.root, textvariable=self.status_var, bd=1, relief=tk.SUNKEN,
                               anchor=tk.W, bg="#141414", fg="#a0a0a0")
        status_bar.pack(fill=tk.X, side=tk.BOTTOM)

    def _build_live_view_tab(self):
        live_tab = tk.Frame(self.notebook, bg="#1e1e1e")
        self.notebook.add(live_tab, text="Live View")

        # Graph on the left
        graph_frame = tk.Frame(live_tab, bg="#1e1e1e")
        graph_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 8))

        tk.Label(
            graph_frame,
            text="Digital Trace — decoded bits (MSB → LSB; timing is not available)",
            bg="#1e1e1e",
            fg="#a0a0a0",
            anchor="w"
        ).pack(fill=tk.X)
        self.graph_canvas = tk.Canvas(graph_frame, bg="#101010", highlightthickness=0)
        self.graph_canvas.pack(fill=tk.BOTH, expand=True)
        self.graph_canvas.bind("<Configure>", lambda e: self.draw_graph())

        # Stats sidebar on the right
        stats_frame = tk.Frame(live_tab, bg="#141414", width=220)
        stats_frame.pack(side=tk.RIGHT, fill=tk.Y)
        stats_frame.pack_propagate(False)

        tk.Label(stats_frame, text="STATS", bg="#141414", fg="white",
                 font=("Consolas", 11, "bold")).pack(pady=(10, 5))

        self.stat_vars = {
            "total_bytes": tk.StringVar(value="Total bytes: 0"),
            "rate": tk.StringVar(value="Rate: 0.0 B/s"),
            "SPI": tk.StringVar(value="SPI packets: 0"),
            "I2C": tk.StringVar(value="I2C packets: 0"),
            "UART": tk.StringVar(value="UART packets: 0"),
            "CAN": tk.StringVar(value="CAN packets: 0"),
            "errors": tk.StringVar(value="Errors: 0"),
        }
        for key in ["total_bytes", "rate"]:
            tk.Label(stats_frame, textvariable=self.stat_vars[key], bg="#141414",
                     fg="#e0e0e0", anchor="w", font=("Consolas", 10)).pack(fill=tk.X, padx=12, pady=2)

        ttk.Separator(stats_frame, orient="horizontal").pack(fill=tk.X, padx=10, pady=8)

        for proto in ["SPI", "I2C", "UART", "CAN"]:
            row = tk.Frame(stats_frame, bg="#141414")
            row.pack(fill=tk.X, padx=12, pady=2)
            swatch = tk.Frame(row, bg=PROTOCOL_COLORS[proto], width=10, height=10)
            swatch.pack(side=tk.LEFT, padx=(0, 6))
            tk.Label(row, textvariable=self.stat_vars[proto], bg="#141414",
                     fg="#e0e0e0", anchor="w", font=("Consolas", 10)).pack(side=tk.LEFT)

        ttk.Separator(stats_frame, orient="horizontal").pack(fill=tk.X, padx=10, pady=8)
        tk.Label(stats_frame, textvariable=self.stat_vars["errors"], bg="#141414",
                 fg="#ff6666", anchor="w", font=("Consolas", 10)).pack(fill=tk.X, padx=12, pady=2)

    def _build_data_log_tab(self):
        log_tab = tk.Frame(self.notebook, bg="#1e1e1e")
        self.notebook.add(log_tab, text="Data Log")

        columns = ("time", "protocol", "detail")
        self.log_tree = ttk.Treeview(log_tab, columns=columns, show="headings")
        self.log_tree.heading("time", text="Time (s)")
        self.log_tree.heading("protocol", text="Protocol")
        self.log_tree.heading("detail", text="Detail")
        self.log_tree.column("time", width=90, anchor="w")
        self.log_tree.column("protocol", width=80, anchor="w")
        self.log_tree.column("detail", width=500, anchor="w")

        vsb = ttk.Scrollbar(log_tab, orient="vertical", command=self.log_tree.yview)
        self.log_tree.configure(yscrollcommand=vsb.set)
        self.log_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        vsb.pack(side=tk.RIGHT, fill=tk.Y)

        for proto, color in PROTOCOL_COLORS.items():
            self.log_tree.tag_configure(proto, foreground=color)
        self.log_tree.tag_configure("ERR", foreground="#ff6666")

    def _build_raw_terminal_tab(self):
        raw_tab = tk.Frame(self.notebook, bg="#1e1e1e")
        self.notebook.add(raw_tab, text="Raw Terminal")

        tk.Button(raw_tab, text="Clear Terminal", command=lambda: self.raw_console.delete(1.0, tk.END)).pack(
            anchor="e", padx=5, pady=5)
        self.raw_console = scrolledtext.ScrolledText(raw_tab, wrap=tk.WORD, bg="black", fg="lime",
                                                       font=("Consolas", 10))
        self.raw_console.pack(fill=tk.BOTH, expand=True, padx=5, pady=(0, 5))

    # Serial connection handling
    def refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        self.port_combo["values"] = [port.device for port in ports]
        if self.port_combo["values"]:
            self.port_combo.current(0)

    def toggle_connection(self):
        if not self.is_connected:
            port = self.port_combo.get()
            if not port:
                self.log_raw("Please select a COM port.")
                return
            try:
                self.serial_port = serial.Serial(port, 115200, timeout=1)
                self.is_connected = True
                self.btn_connect.config(text="Disconnect", bg="salmon")
                self.btn_start.config(state=tk.NORMAL)
                self.btn_stop.config(state=tk.NORMAL)
                self.status_var.set(f"Connected to {port} at 115200 baud.")
                self.log_raw(f"Connected to {port} at 115200 baud.")

                self.read_thread = threading.Thread(target=self.read_from_serial, daemon=True)
                self.read_thread.start()
            except Exception as e:
                self.log_raw(f"Connection failed: {e}")
        else:
            self.is_connected = False
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
            self.btn_connect.config(text="Connect", bg="lightgray")
            self.btn_start.config(state=tk.DISABLED)
            self.btn_stop.config(state=tk.DISABLED)
            self.status_var.set("Not connected.")
            self.log_raw("Disconnected.")

    def send_mode(self):
        mode_str = self.mode_var.get()
        cmd_char = mode_str[mode_str.find("(") + 1: mode_str.find(")")]
        self.send_command(cmd_char)

    def send_command(self, char):
        if self.is_connected and self.serial_port and self.serial_port.is_open:
            self.serial_port.write(char.encode("utf-8"))
            self.log_raw(f">>> Sent Command: {char}")
            if char == "G":
                self.capture_start_time = time.time()
        else:
            self.log_raw("Cannot send command. Not connected.")

    def read_from_serial(self):
        while self.is_connected and self.serial_port and self.serial_port.is_open:
            try:
                if self.serial_port.in_waiting > 0:
                    line = self.serial_port.readline().decode("utf-8", errors="ignore").strip()
                    if line:
                        self.root.after(0, self.handle_line, line)
            except Exception as e:
                self.root.after(0, self.log_raw, f"Serial Read Error: {e}")
                break

    # Line handling: raw log + decoded parsing
    def handle_line(self, line):
        self.log_raw(line)
        self.total_bytes += len(line)
        self.parse_decoded_line(line)

    def log_raw(self, text):
        self.raw_console.insert(tk.END, text + "\n")
        self.raw_console.see(tk.END)

    def elapsed(self):
        if self.capture_start_time is None:
            return 0.0
        return time.time() - self.capture_start_time

    def add_log_row(self, protocol, detail, tag=None):
        t = f"{self.elapsed():.3f}"
        row_id = self.log_tree.insert("", tk.END, values=(t, protocol, detail), tags=(tag or protocol,))
        self.log_tree.see(row_id)
        self.log_rows.append((t, protocol, detail))

    def _append_byte_bits(self, protocol, value):
        """Expand one decoded byte into eight digital samples, MSB first.

        The firmware currently sends decoded bytes over UART rather than raw
        timestamped logic samples, so these bits represent the decoded data
        sequence, not the electrical timing of the bus.
        """
        for shift in range(7, -1, -1):
            bit = (value >> shift) & 1
            self.recent_bits.append((protocol, bit))

    def _append_bit(self, protocol, bit):
        """Append a single digital sample for future protocol events."""
        self.recent_bits.append((protocol, 1 if bit else 0))

    def parse_decoded_line(self, line):
        m = RE_SPI_BYTE.match(line)
        if m:
            mosi_val = int(m.group(1), 16)
            miso_val = int(m.group(2), 16)
            self.packet_counts["SPI"] += 1
            self._append_byte_bits("SPI", mosi_val)
            self.add_log_row("SPI", f"MOSI: 0x{m.group(1)} | MISO: 0x{m.group(2)}")
            self.draw_graph()
            return

        m = RE_SPI_CS.match(line)
        if m:
            self.add_log_row("SPI", f"CS {m.group(1)}")
            return

        m = RE_I2C_BYTE.match(line)
        if m:
            val = int(m.group(1), 16)
            self.packet_counts["I2C"] += 1
            self._append_byte_bits("I2C", val)
            self.add_log_row("I2C", f"Byte: 0x{m.group(1)} [{m.group(2)}]")
            self.draw_graph()
            return

        m = RE_I2C_COND.match(line)
        if m:
            self.add_log_row("I2C", f"{m.group(1)} Condition")
            return

        m = RE_UART_BYTE.match(line)
        if m:
            val = int(m.group(1), 16)
            self.packet_counts["UART"] += 1
            self._append_byte_bits("UART", val)
            self.add_log_row("UART", f"0x{m.group(1)} ('{m.group(2)}')")
            self.draw_graph()
            return

        m = RE_UART_ERR.match(line)
        if m:
            self.error_count += 1
            self.add_log_row("UART", line, tag="ERR")
            return

        m = RE_CAN_FRAME.match(line)
        if m:
            status = m.group(5)
            self.packet_counts["CAN"] += 1
            if status == "ERROR":
                self.error_count += 1
            # Scale the CAN ID into a 0-255 range just for graph placement
            val = int(m.group(1), 16) % 256
            self._append_byte_bits("CAN", val)
            self.add_log_row(
                "CAN",
                f"ID: 0x{m.group(1)} | DLC: {m.group(2)} | CRC {m.group(3)}/{m.group(4)} [{status}]",
                tag=None if status == "VALID" else "ERR",
            )
            self.draw_graph()
            return

    # Graph
    def draw_graph(self):
        """Draw a Saleae-style digital waveform from the decoded bit stream."""
        c = self.graph_canvas
        c.delete("all")

        w = c.winfo_width()
        h = c.winfo_height()
        if w <= 1 or h <= 1:
            return

        # One horizontal lane per protocol.
        protocols = ["SPI", "I2C", "UART", "CAN"]
        lane_h = max(55, h // len(protocols))
        trace_top = 24

        # Number of bits that fit horizontally.
        bit_w = 8
        max_bits = max(1, w // bit_w)
        bits = list(self.recent_bits)[-max_bits:]

        # Group the bits by protocol. A decoded stream does not preserve
        # physical timing, so every decoded bit gets the same display width.
        lanes = {proto: [] for proto in protocols}
        for proto, bit in bits:
            lanes.setdefault(proto, []).append(bit)

        for lane_index, proto in enumerate(protocols):
            y_top = trace_top + lane_index * lane_h
            y_mid = y_top + lane_h // 2
            y_high = y_top + 14
            y_low = y_top + lane_h - 14

            # Lane separator
            c.create_line(0, y_top, w, y_top, fill="#252525")

            # Protocol label
            c.create_text(
                8, y_top + 7,
                text=proto,
                fill=PROTOCOL_COLORS[proto],
                anchor="w",
                font=("Consolas", 9, "bold")
            )

            proto_bits = lanes.get(proto, [])
            if not proto_bits:
                continue

            # Right-align each protocol's most recent bits.
            x_start = max(70, w - len(proto_bits) * bit_w)

            # Draw waveform
            previous = proto_bits[0]
            x = x_start
            y = y_high if previous else y_low

            c.create_line(x, y, x + bit_w, y,
                          fill=PROTOCOL_COLORS[proto], width=2)

            for bit in proto_bits[1:]:
                next_y = y_high if bit else y_low

                if bit != previous:
                    # Vertical transition at the bit boundary.
                    c.create_line(
                        x + bit_w, y,
                        x + bit_w, next_y,
                        fill=PROTOCOL_COLORS[proto],
                        width=2
                    )

                c.create_line(
                    x + bit_w, next_y,
                    x + 2 * bit_w, next_y,
                    fill=PROTOCOL_COLORS[proto],
                    width=2
                )

                previous = bit
                y = next_y
                x += bit_w

            # Light bit-center ticks make the decoded bit boundaries visible.
            for i in range(len(proto_bits)):
                tick_x = x_start + i * bit_w
                c.create_line(
                    tick_x, y_low + 4,
                    tick_x, y_low + 8,
                    fill="#333333"
                )

        # Bottom border.
        c.create_line(0, h - 1, w, h - 1, fill="#333333")

        # Legend / explanation.
        lx = 10
        for proto in protocols:
            color = PROTOCOL_COLORS[proto]
            c.create_rectangle(lx, 5, lx + 10, 15, fill=color, outline="")
            c.create_text(
                lx + 15, 10,
                text=proto,
                fill="white",
                anchor="w",
                font=("Consolas", 8)
            )
            lx += 65

    # Stats
    def tick_stats(self):
        elapsed = self.elapsed()
        rate = (self.total_bytes / elapsed) if elapsed > 0 else 0.0

        self.stat_vars["total_bytes"].set(f"Total bytes: {self.total_bytes}")
        self.stat_vars["rate"].set(f"Rate: {rate:.1f} B/s")
        for proto in ["SPI", "I2C", "UART", "CAN"]:
            self.stat_vars[proto].set(f"{proto} packets: {self.packet_counts[proto]}")
        self.stat_vars["errors"].set(f"Errors: {self.error_count}")

        self.root.after(500, self.tick_stats)

    def clear_all(self):
        self.total_bytes = 0
        self.packet_counts = {"SPI": 0, "I2C": 0, "UART": 0, "CAN": 0}
        self.error_count = 0
        self.recent_bits.clear()
        self.log_rows.clear()
        for item in self.log_tree.get_children():
            self.log_tree.delete(item)
        self.raw_console.delete(1.0, tk.END)
        self.draw_graph()
        self.tick_stats()

    def export_csv(self):
        if not self.log_rows:
            self.log_raw("Nothing to export yet.")
            return
        path = filedialog.asksaveasfilename(defaultextension=".csv",
                                             filetypes=[("CSV files", "*.csv")])
        if not path:
            return
        with open(path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["Time (s)", "Protocol", "Detail"])
            writer.writerows(self.log_rows)
        self.log_raw(f"Exported {len(self.log_rows)} rows to {path}")


if __name__ == "__main__":
    root = tk.Tk()
    app = ProtocolAnalyzerUI(root)

    def on_closing():
        app.is_connected = False
        if app.serial_port and app.serial_port.is_open:
            app.serial_port.close()
        root.destroy()
        sys.exit()

    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()