import customtkinter as ctk
from tkinter import filedialog
from tkinter import messagebox

from translations import translate, available_languages
from arduino_comm import ArduinoComm
from scoring import score_session
from storage import (
    save_session,
    get_all_users,
    get_user_sessions,
    load_compression_rows_for_session,
    export_csvs_to_folder,
    delete_session,
)


class SmartCPRApp(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.hand_correcting = False
        self.last_hand_prompt = None
        self.hp_enter_threshold = 12.0
        self.hp_exit_threshold = 8.0

        self.compression_rows = []
        self.recent_compression_rows = []
        self.compression_window_size = 3

        self.current_language = "English"
        self.current_side_mode = "Right"
        self.selected_time_option = "30 secs"
        self.session_time = 30

        self.timer_running = False
        self.time_left = self.session_time

        self.geometry("800x480")
        self.title("SMART CPR")

        ctk.set_appearance_mode("light")
        ctk.set_default_color_theme("dark-blue")

        self.arduino = ArduinoComm()
        self.arduino_connected = False
        self.arduino_ready = False


        self.current_depth = 55.0
        self.current_rate = 110.0
        self.current_lean = 0.0
        self.current_horizontal = 0.0
        self.current_vertical = 0.0
        self.current_force = 0.0

        self.session_rows = []

        self.build_start_screen()

        self.connect_to_arduino()

    def connect_to_arduino(self):
        try:
            self.arduino.connect()
            self.arduino_connected = True
            self.status_label.configure(text=self.t("warming_up"))
            self.check_arduino_ready_loop()
        except Exception as e:
            self.arduino_connected = False
            self.arduino_ready = False

            self.status_label.configure(text=self.t("not_connected"))
            self.start_button.configure(state="disabled")

            print(f"Arduino connection failed: {e}")

    def check_arduino_ready_loop(self):
        if not self.arduino_connected:
            return

        if self.arduino.check_ready():
            self.arduino_ready = True
            self.status_label.configure(text=self.t("ready"))
            self.start_button.configure(state="normal")
            return

        self.after(100, self.check_arduino_ready_loop)

    def t(self, key):
        return translate(self.current_language, key)

    def build_start_screen(self):
        self.start_frame = ctk.CTkFrame(self)
        self.start_frame.pack(fill="both", expand=True)

        self.username_entry = ctk.CTkEntry(
            self.start_frame,
            placeholder_text=self.t("enter_name")
        )
        self.username_entry.pack(pady=10)

        self.welcome_label = ctk.CTkLabel(
            self.start_frame,
            text=self.t("welcome"),
            font=("Times", 28, "bold")
        )
        self.welcome_label.pack(pady=20)

        self.status_label = ctk.CTkLabel(
            self.start_frame,
            text="System warming up...",
            font=("Times", 16)
        )
        self.status_label.pack(pady=5)

        self.language_menu = ctk.CTkOptionMenu(
            self.start_frame,
            values=available_languages(),
            command=self.change_language
        )
        self.language_menu.pack(pady=5)
        self.language_menu.set(self.current_language)

        self.side_menu = ctk.CTkOptionMenu(
            self.start_frame,
            values=["Right", "Left"],
            command=self.change_side_mode
        )
        self.side_menu.pack(pady=5)
        self.side_menu.set(self.current_side_mode)

        self.time_label = ctk.CTkLabel(
            self.start_frame,
            text=self.t("time"),
            font=("Times", 16)
        )
        self.time_label.pack(pady=5)

        self.time_menu = ctk.CTkOptionMenu(
            self.start_frame,
            values=["30 secs", "1 min", "5 min", "10 min", "Other"],
            command=self.change_time
        )
        self.time_menu.pack(pady=2)
        self.time_menu.set(self.selected_time_option)

        self.custom_entry = ctk.CTkEntry(
            self.start_frame,
            placeholder_text=self.t("custom")
        )

        self.countdown_label = ctk.CTkLabel(
            self.start_frame,
            text="",
            font=("Times", 48, "bold")
        )
        self.countdown_label.pack(pady=10)

        self.start_button = ctk.CTkButton(
            self.start_frame,
            text=self.t("start"),
            command=self.start_pressed,
            state="disabled"
        )
        self.start_button.pack(pady=10)

        self.stats_button = ctk.CTkButton(
            self.start_frame,
            text=self.t("view_stats"),
            command=self.stats_pressed
        )
        self.stats_button.pack(pady=10)

        self.export_button = ctk.CTkButton(
            self.start_frame,
            text=self.t("export_data"),
            command=self.export_pressed
        )
        self.export_button.pack(pady=10)

    def change_language(self, choice):
        self.current_language = choice

        self.username_entry.configure(placeholder_text=self.t("enter_name"))
        self.welcome_label.configure(text=self.t("welcome"))
        self.time_label.configure(text=self.t("time"))
        self.custom_entry.configure(placeholder_text=self.t("custom"))
        self.start_button.configure(text=self.t("start"))
        self.stats_button.configure(text=self.t("view_stats"))
        self.export_button.configure(text=self.t("export_data"))

    def change_side_mode(self, choice):
        self.current_side_mode = choice

    def change_time(self, choice):
        self.selected_time_option = choice

        if choice == "30 secs":
            self.session_time = 30
            self.custom_entry.pack_forget()
        elif choice == "1 min":
            self.session_time = 60
            self.custom_entry.pack_forget()
        elif choice == "5 min":
            self.session_time = 300
            self.custom_entry.pack_forget()
        elif choice == "10 min":
            self.session_time = 600
            self.custom_entry.pack_forget()
        else:
            self.custom_entry.pack(pady=5)

    def start_pressed(self):
        self.set_username()
        self.start_countdown()

    def stats_pressed(self):
        self.show_stats_screen()

    def export_pressed(self):
        destination = filedialog.askdirectory(title="Select export destination")

        if not destination:
            return

        success = export_csvs_to_folder(destination)

        if success:
            self.status_label.configure(text=self.t("export_complete"))
        else:
            self.status_label.configure(text=self.t("export_failed"))

    def set_username(self):
        name = self.username_entry.get().strip()
        self.current_user = name if name else "Guest"

    def start_countdown(self):
        values = self.t("countdown")
        self.show_countdown(values, 0)

    def show_countdown(self, values, i):
        if i < len(values):
            self.countdown_label.configure(text=values[i])
            self.after(700, lambda: self.show_countdown(values, i + 1))
        else:
            self.countdown_label.configure(text="")
            self.switch_to_main()

    def build_main_screen(self):
        self.main_frame = ctk.CTkFrame(self)

        self.title_label = ctk.CTkLabel(
            self.main_frame,
            text=self.t("title"),
            font=("Times", 22, "bold")
        )
        self.title_label.pack()

        self.timer_label = ctk.CTkLabel(
            self.main_frame,
            text="00:00",
            font=("Consolas", 20)
        )
        self.timer_label.pack()

        self.back_button = ctk.CTkButton(
            self.main_frame,
            text=self.t("back"),
            command=self.back_to_menu
        )
        self.back_button.pack(pady=5)

        self.panel = ctk.CTkFrame(self.main_frame)
        self.panel.pack(fill="both", expand=True)

        self.output_label = ctk.CTkLabel(
            self.panel,
            text="",
            font=("Times", 100, "bold")
        )
        self.output_label.pack(pady=10)

        self.arrow_label = ctk.CTkLabel(
            self.panel,
            text="",
            font=("Arial", 180, "bold")
        )
        self.arrow_label.pack()

    def switch_to_main(self):
        self.start_frame.pack_forget()

        if not hasattr(self, "main_frame"):
            self.build_main_screen()

        self.main_frame.pack(fill="both", expand=True)

        # RESET VISUAL STATE
        self.main_frame.configure(fg_color="white")
        self.panel.configure(fg_color="white")
        self.title_label.configure(text_color="black")
        self.timer_label.configure(text_color="black")
        self.back_button.configure(fg_color="#808080", text_color="white")
        self.output_label.configure(text="", font=("Times", 100, "bold"))
        self.arrow_label.configure(text="")

        if self.arduino_connected:
            self.arduino.start_session(self.session_time)

        self.reset_session_state()

        self.start_timer()
        self.read_data_loop()


    def back_to_menu(self):
        self.timer_running = False

        if self.arduino_connected:
            self.arduino.stop_session()

        self.main_frame.pack_forget()
        self.start_frame.pack(fill="both", expand=True)

    def start_timer(self):
        self.timer_running = True
        self.time_left = self.session_time
        self.update_timer()

    def update_timer(self):
        if not self.timer_running:
            return

        mins = self.time_left // 60
        secs = self.time_left % 60
        self.timer_label.configure(text=f"{mins:02d}:{secs:02d}")

        if self.time_left > 0:
            self.time_left -= 1
            self.after(1000, self.update_timer)
        else:
            self.end_session()

    def end_session(self):
        self.timer_running = False

        summary = score_session(self.compression_rows)
        print("\n=== SESSION SUMMARY ===")
        for key, value in summary.items():
            print(f"{key}: {value}")
        print("=======================\n")

        save_session(
            self.current_user,
            self.session_rows,
            self.compression_rows,
            summary
        )

        if self.arduino_connected:
            self.arduino.stop_session()
            self.arduino.send_result(summary["passed"])

        result_text = self.t("pass") if summary["passed"] else self.t("fail")

        self.output_label.configure(
            text=(
                f"{result_text}\n"
                f"Depth: {summary['avg_depth']:.1f} mm\n"
                f"Rate: {summary['avg_rate']:.1f} CPM\n"
                f"Recoil: {summary['avg_lean']:.1f} mm\n"
                f"Hands: {summary['avg_hp_offset']:.1f} mm"
            ),
            text_color="black",
            font=("Times", 42, "bold")
        )

        self.arrow_label.configure(text="")

    def read_data_loop(self):
        if not self.timer_running:
            return

        row = None

        if self.arduino_connected:
            row = self.arduino.read_data_row()

            if row == "SESSION_COMPLETE":
                self.end_session()
                return
        else:
            return

        if isinstance(row, dict):
            row_type = row.get("row_type")

            if row_type == "DATA":
                self.session_rows.append(row)

            elif row_type == "COMP":
                self.handle_compression_row(row)

        self.after(33, self.read_data_loop)

    def apply_data_row(self, row):
        self.current_depth = row.get("depth_mm", 0.0)
        self.current_rate = row.get("rate_cpm", 0.0)
        self.current_lean = row.get("lean_mm", 0.0)
        self.current_horizontal = row.get("hp_x", 0.0)
        self.current_vertical = row.get("hp_y", 0.0)
        self.current_force = row.get("force_N", 0.0)

    def get_hand_feedback(self):
        h = self.current_horizontal
        v = self.current_vertical

        abs_h = abs(h)
        abs_v = abs(v)

        outside_enter = abs_h > self.hp_enter_threshold or abs_v > self.hp_enter_threshold
        inside_exit = abs_h < self.hp_exit_threshold and abs_v < self.hp_exit_threshold

        if not self.hand_correcting:
            if outside_enter:
                self.hand_correcting = True
                self.last_hand_prompt = self.determine_hand_prompt(h, v)
            else:
                self.last_hand_prompt = None
        else:
            if inside_exit:
                self.hand_correcting = False
                self.last_hand_prompt = None
            elif outside_enter:
                self.last_hand_prompt = self.determine_hand_prompt(h, v)

        return self.last_hand_prompt

    def determine_hand_prompt(self, h, v):
        """
        Convert manikin-centered hp_x / hp_y into practical user-facing directions.

        Left side mode:
        - negative hp_x = farther away from trainee -> MOVE DOWN
        - positive hp_x = closer to trainee -> MOVE UP

        Right side mode:
        - positive hp_x = farther away from trainee -> MOVE DOWN
        - negative hp_x = closer to trainee -> MOVE UP
        """

        if abs(h) > abs(v):
            if self.current_side_mode == "Left":
                if h < 0:
                    return self.t("down")
                else:
                    return self.t("up")
            else:  # Right side
                if h > 0:
                    return self.t("down")
                else:
                    return self.t("up")

        else:
            if self.current_side_mode == "Left":
                if v < 0:
                    return self.t("left")
                else:
                    return self.t("right")
            else:  # Right side
                if v < 0:
                    return self.t("right")
                else:
                    return self.t("left")

    def update_system_display(self):
        arrow = ""

        hand = self.get_hand_feedback()

        if hand:
            status, color = hand, "#000000"

            if hand == self.t("left"):
                arrow = "←"
            elif hand == self.t("right"):
                arrow = "→"
            elif hand == self.t("up"):
                arrow = "↑"
            elif hand == self.t("down"):
                arrow = "↓"

        elif self.current_depth < 50:
            status, color = self.t("deeper"), "#1f8cb5"

        elif self.current_rate < 100:
            status, color = self.t("faster"), "#cf431f"

        elif self.current_rate >= 120:
            status, color = self.t("slower"), "#ff8c00"

        elif self.current_lean > 20:
            status, color = self.t("full_recoil"), "#4B0082"

        elif self.current_depth >= 60:
            status, color = self.t("shallower"), "#05437e"

        else:
            status, color = self.t("good"), "#1e9648"

        if status == self.t("good"):
            self.main_frame.configure(fg_color="#00A651")
            self.panel.configure(fg_color="#00A651")

            self.title_label.configure(text_color="white")
            self.timer_label.configure(text_color="white")
            self.back_button.configure(fg_color="#ffffff", text_color="black")

            self.output_label.configure(text=status, text_color="white")
            self.arrow_label.configure(text=arrow, text_color="white")

        else:
            self.main_frame.configure(fg_color="white")
            self.panel.configure(fg_color="white")

            self.title_label.configure(text_color="black")
            self.timer_label.configure(text_color="black")
            self.back_button.configure(fg_color="#808080", text_color="white")

            self.output_label.configure(text=status, text_color=color)
            self.arrow_label.configure(text=arrow, text_color="black")

    def handle_compression_row(self, row):
        # Save full history
        self.compression_rows.append(row)

        # Maintain sliding window for feedback
        self.recent_compression_rows.append(row)

        if len(self.recent_compression_rows) > self.compression_window_size:
            self.recent_compression_rows.pop(0)

        avg = self.get_average_compression()

        self.apply_compression_average(avg)
        self.update_system_display()

    def get_average_compression(self):
        rows = self.recent_compression_rows

        def avg(key):
            return sum(r[key] for r in rows) / len(rows)

        return {
            "depth_mm": avg("peak_depth_mm"),
            "rate_cpm": avg("rate_cpm"),
            "lean_mm": avg("lean_mm"),
            "hp_x": avg("hp_x"),
            "hp_y": avg("hp_y"),
            "force_N": avg("force_N"),
        }

    def apply_compression_average(self, avg):
        self.current_depth = avg["depth_mm"]
        self.current_rate = avg["rate_cpm"]
        self.current_lean = avg["lean_mm"]
        self.current_horizontal = avg["hp_x"]
        self.current_vertical = avg["hp_y"]
        self.current_force = avg["force_N"]

    def reset_session_state(self):
        self.session_rows = []
        self.compression_rows = []
        self.recent_compression_rows = []

        self.current_depth = 0.0
        self.current_rate = 0.0
        self.current_lean = 0.0
        self.current_horizontal = 0.0
        self.current_vertical = 0.0
        self.current_force = 0.0

        self.hand_correcting = False
        self.last_hand_prompt = None

    def build_stats_screen(self):
        self.stats_frame = ctk.CTkFrame(self)

        self.stats_title = ctk.CTkLabel(
            self.stats_frame,
            text=self.t("user_stats"),
            font=("Times", 26, "bold")
        )
        self.stats_title.pack(pady=10)

        self.user_menu = ctk.CTkOptionMenu(
            self.stats_frame,
            values=[],
            command=self.user_selected
        )
        self.user_menu.pack(pady=5)

        self.session_menu = ctk.CTkOptionMenu(
            self.stats_frame,
            values=[],
            command=self.session_selected
        )
        self.session_menu.pack(pady=5)

        self.stats_text = ctk.CTkLabel(
            self.stats_frame,
            text="",
            font=("Times", 18),
            justify="left"
        )
        self.stats_text.pack(pady=15)

        self.delete_button = ctk.CTkButton(
            self.stats_frame,
            text=self.t("delete_selected"),
            fg_color="#aa3333",
            command=self.delete_selected_session
        )
        self.delete_button.pack(pady=5)

        self.stats_back_button = ctk.CTkButton(
            self.stats_frame,
            text=self.t("back"),
            command=self.back_to_menu_from_stats
        )
        self.stats_back_button.pack(pady=10)

    def show_stats_screen(self):
        self.start_frame.pack_forget()

        if not hasattr(self, "stats_frame"):
            self.build_stats_screen()

        self.stats_frame.pack(fill="both", expand=True)
        self.load_users_into_stats()

    def load_users_into_stats(self):
        users = get_all_users()

        if not users:
            self.user_menu.configure(values=[])
            self.session_menu.configure(values=[])
            self.stats_text.configure(text=self.t("no_data"))
            return

        self.user_menu.configure(values=users)
        self.user_menu.set(users[0])
        self.user_selected(users[0])

    def user_selected(self, user):
        self.current_stats_user = user
        sessions = get_user_sessions(user)

        if not sessions:
            self.session_menu.configure(values=[])
            self.stats_text.configure(text=self.t("no_data"))
            return

        session_values = [
            f"{i} | {session['date']}"
            for i, session in enumerate(sessions)
        ]

        self.session_menu.configure(values=session_values)
        self.session_menu.set(session_values[-1])
        self.session_selected(session_values[-1])

    def session_selected(self, selected):
        if not selected:
            return

        index = int(selected.split("|")[0].strip())
        sessions = get_user_sessions(self.current_stats_user)

        if not (0 <= index < len(sessions)):
            return

        session = sessions[index]
        compression_rows = load_compression_rows_for_session(session)
        summary = score_session(compression_rows)

        hand_description = self.describe_hand_position(summary["avg_hp_offset"])

        result_text = self.t("pass") if summary["passed"] else self.t("fail")

        text = (
            f"User: {self.current_stats_user}\n"
            f"Date: {session['date']}\n"
            f"Result: {result_text}\n\n"
            f"Avg Depth: {summary['avg_depth']:.1f} mm\n"
            f"Avg Rate: {summary['avg_rate']:.1f} CPM\n"
            f"Avg Recoil/Lean: {summary['avg_lean']:.1f} mm\n"
            f"Avg Hand Offset: {summary['avg_hp_offset']:.1f} mm\n"
            f"Hand Placement: {hand_description}\n"
            f"Compressions: {summary['total_compressions']}"
        )

        self.stats_text.configure(text=text)

    def describe_hand_position(self, offset):
        if offset < 8:
            return "Centered"
        elif offset < 15:
            return "Slightly off center"
        elif offset < 25:
            return "Moderately off center"
        else:
            return "Far off center"

    def back_to_menu_from_stats(self):
        self.stats_frame.pack_forget()
        self.start_frame.pack(fill="both", expand=True)

    def delete_selected_session(self):
        selected = self.session_menu.get()

        if not selected:
            return

        confirm = messagebox.askyesno(
            "Confirm Delete",
            "Are you sure you want to delete this session?"
        )

        if not confirm:
            return

        index = int(selected.split("|")[0].strip())

        delete_session(self.current_stats_user, index)
        self.user_selected(self.current_stats_user)
