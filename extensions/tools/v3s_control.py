"""Small Windows serial console and Peanut GBC controller (Python + pyserial)."""
import codecs
import queue
import re
import threading
import tkinter as tk
from tkinter import ttk, messagebox
from tkinter.scrolledtext import ScrolledText

import serial
from serial.tools import list_ports


class Controller:
    KEYS = {"Up": "w", "Down": "s", "Left": "a", "Right": "d",
            "z": "z", "x": "x", "Return": "e", "BackSpace": "r"}

    def __init__(self, root):
        self.root = root
        self.port = None
        self.reader = None
        self.events = queue.Queue()
        self.held = {}
        self.session = 0
        self.pending_cr = False
        root.title("V3s 串口控制台")
        root.geometry("880x680")
        root.minsize(700, 560)
        self.port_name = tk.StringVar(value="COM15")
        self.baud = tk.StringVar(value="115200")
        self.game = tk.BooleanVar(value=False)
        self.status = tk.StringVar(value="未连接")
        top = ttk.Frame(root, padding=10)
        top.pack(fill="x")
        ttk.Label(top, text="串口").pack(side="left")
        self.ports = ttk.Combobox(top, textvariable=self.port_name, width=14)
        self.ports.pack(side="left", padx=5)
        ttk.Button(top, text="刷新", command=self.refresh).pack(side="left")
        ttk.Label(top, text="波特率").pack(side="left", padx=(15, 0))
        ttk.Combobox(top, textvariable=self.baud, values=[9600, 38400, 57600, 115200, 230400, 460800, 921600], width=10).pack(side="left", padx=5)
        self.connect_button = ttk.Button(top, text="连接", command=self.toggle)
        self.connect_button.pack(side="left", padx=5)
        ttk.Label(top, textvariable=self.status).pack(side="left", padx=8)
        self.console = ScrolledText(root, wrap="char", font=("Consolas", 10), state="disabled", background="#15202b", foreground="#dce8ef", insertbackground="white")
        self.console.pack(fill="both", expand=True, padx=10)
        line = ttk.Frame(root, padding=10)
        line.pack(fill="x")
        self.command = ttk.Entry(line)
        self.command.pack(side="left", fill="x", expand=True)
        self.command.bind("<Return>", self.send_command)
        ttk.Button(line, text="发送命令 ↵", command=self.send_command).pack(side="left", padx=5)
        ttk.Button(line, text="Ctrl+C", command=lambda: self.send(b"\x03")).pack(side="left")
        ttk.Button(line, text="清屏", command=self.clear).pack(side="left", padx=5)
        panel = ttk.LabelFrame(root, text="游戏控制（适配当前 Peanut GBC 串口前端）", padding=10)
        panel.pack(fill="x", padx=10, pady=(0, 10))
        ttk.Checkbutton(panel, text="启用键盘控制：方向键 / Z / X / Enter / Backspace", variable=self.game, command=self.focus_game).pack(anchor="w")
        ttk.Label(panel, text="点击此面板后操作；按住方向持续移动。命令输入框始终用于打字。当前设备协议每次支持一个按键。", wraplength=800).pack(anchor="w", pady=5)
        buttons = ttk.Frame(panel)
        buttons.pack()
        for label, code, row, col in [("↑", "w", 0, 1), ("←", "a", 1, 0), ("↓", "s", 1, 1), ("→", "d", 1, 2), ("A / Z", "z", 0, 4), ("B / X", "x", 1, 4), ("Start", "e", 0, 5), ("Select", "r", 1, 5)]:
            button = ttk.Button(buttons, text=label, width=10, takefocus=False)
            button.grid(row=row, column=col, padx=3, pady=2)
            button.bind("<ButtonPress-1>", lambda e, c=code: self.mouse_down(c))
            button.bind("<ButtonRelease-1>", lambda e, c=code: self.release("mouse:" + c))
        ttk.Button(buttons, text="退出游戏 Q", command=lambda: self.send(b"q")).grid(row=0, column=6, rowspan=2, padx=12)
        panel.bind("<Button-1>", lambda e: root.focus_set())
        root.bind("<KeyPress>", self.key_down)
        root.bind("<KeyRelease>", self.key_up)
        root.bind("<FocusOut>", lambda e: self.held.clear())
        root.protocol("WM_DELETE_WINDOW", self.close)
        self.refresh()
        self.poll()
        self.repeat()

    def refresh(self):
        self.ports["values"] = [p.device for p in list_ports.comports()]

    def toggle(self):
        if self.port:
            self.disconnect()
            return
        try:
            port = serial.Serial(port=None, baudrate=int(self.baud.get()), timeout=0.1, write_timeout=0.5)
            port.port = self.port_name.get().strip()
            port.dtr = False
            port.rts = False
            port.open()
        except (ValueError, serial.SerialException, OSError) as exc:
            messagebox.showerror("连接失败", f"{exc}\n\n请检查串口编号，并关闭其他占用串口的终端。")
            return
        self.port = port
        self.session += 1
        self.pending_cr = False
        self.ansi_buffer = ""
        self.status.set(f"已连接 {port.port} · {port.baudrate} · 8N1")
        self.connect_button.config(text="断开")
        self.reader = threading.Thread(target=self.read_loop, args=(port, self.session), daemon=True)
        self.reader.start()

    def disconnect(self):
        self.session += 1
        port, self.port = self.port, None
        self.held.clear()
        if port:
            if self.reader:
                self.reader.join(timeout=0.3)
            port.close()
        self.reader = None
        self.connect_button.config(text="连接")
        self.status.set("未连接")

    def read_loop(self, port, session):
        decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")
        try:
            while port.is_open and self.port is port:
                data = port.read(max(1, min(port.in_waiting, 4096)))
                if data:
                    self.events.put((session, "text", decoder.decode(data)))
        except (serial.SerialException, OSError, TypeError, AttributeError) as exc:
            self.events.put((session, "error", str(exc)))

    def send(self, data):
        if not self.port:
            self.status.set("未连接：请先连接串口")
            return
        try:
            self.port.write(data)
        except (serial.SerialException, OSError) as exc:
            self.disconnect()
            self.status.set(f"串口错误：{exc}")

    def send_command(self, event=None):
        if self.port:
            self.send((self.command.get() + "\r").encode("utf-8"))
            self.command.delete(0, "end")
        else:
            self.status.set("未连接：请先连接串口")
        return "break"

    def press(self, token, code):
        if token not in self.held:
            self.held[token] = code
            self.send(code.encode("ascii"))

    def release(self, token):
        self.held.pop(token, None)

    def mouse_down(self, code):
        self.root.focus_set()
        self.press("mouse:" + code, code)

    def key_down(self, event):
        if not self.game.get() or isinstance(self.root.focus_get(), (ttk.Entry, ttk.Combobox)):
            return
        code = self.KEYS.get(event.keysym)
        if code:
            self.press(event.keysym, code)
            return "break"

    def key_up(self, event):
        self.release(event.keysym)

    def focus_game(self):
        self.held.clear()
        self.root.focus_set()

    def repeat(self):
        # Device holds each received character for eight display frames (~134ms).
        if self.held and self.port:
            self.send(list(self.held.values())[-1].encode("ascii"))
        self.root.after(90, self.repeat)

    def append(self, text):
        # A log view, not a full VT terminal. Keep split ANSI sequences buffered.
        self.ansi_buffer += text
        text = self.ansi_buffer
        self.ansi_buffer = ""
        start = text.rfind("\x1b")
        if start >= 0 and re.fullmatch(r"\x1b(?:\[[0-?]*[ -/]*)?", text[start:]):
            self.ansi_buffer = text[start:]
            text = text[:start]
        text = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", text)
        if self.pending_cr and text.startswith("\n"):
            text = text[1:]
        self.pending_cr = text.endswith("\r")
        text = text.replace("\r\n", "\n").replace("\r", "\n")
        self.console.config(state="normal")
        self.console.insert("end", text)
        if int(self.console.index("end-1c").split(".")[0]) > 5000:
            self.console.delete("1.0", "1000.0")
        self.console.see("end")
        self.console.config(state="disabled")

    ansi_buffer = ""

    def poll(self):
        for _ in range(100):
            try:
                session, kind, text = self.events.get_nowait()
            except queue.Empty:
                break
            if session != self.session:
                continue
            if kind == "error":
                self.disconnect()
                self.status.set(f"串口断开：{text}")
            else:
                self.append(text)
        self.root.after(50, self.poll)

    def clear(self):
        self.console.config(state="normal")
        self.console.delete("1.0", "end")
        self.console.config(state="disabled")

    def close(self):
        self.disconnect()
        self.root.destroy()


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM15")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--connect", action="store_true")
    args = parser.parse_args()
    window = tk.Tk()
    controller = Controller(window)
    controller.port_name.set(args.port)
    controller.baud.set(str(args.baud))
    if args.connect:
        window.after(100, controller.toggle)
    window.mainloop()
