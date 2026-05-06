import serial
import time
import platform


def get_default_port():
    if platform.system() == "Windows":
        return "COM5"
    return "/dev/ttyACM0"

class ArduinoComm:


    def __init__(self, port=None, baud=115200, timeout=0.1):
        self.port = port or get_default_port()
        self.baud = baud
        self.timeout = timeout
        self.arduino = None
        self.ready = False

    def connect(self):
        self.arduino = serial.Serial(self.port, self.baud, timeout=self.timeout)
        self.clear_buffer()
        time.sleep(2)

    def is_connected(self):
        return self.arduino is not None and self.arduino.is_open

    def clear_buffer(self):
        if self.is_connected():
            self.arduino.reset_input_buffer()

    def close(self):
        if self.is_connected():
            self.arduino.close()

    def send_command(self, command):
        if not self.is_connected():
            return False

        if not command.endswith("\n"):
            command += "\n"

        self.arduino.write(command.encode())
        return True

    def start_session(self, seconds):
        return self.send_command(f"START,{seconds}")

    def stop_session(self):
        return self.send_command("STOP")

    def send_result(self, passed):
        result_value = 1 if passed else 0
        return self.send_command(f"RESULT,{result_value}")

    def read_line(self):
        if not self.is_connected():
            return ""

        try:
            return self.arduino.readline().decode(errors="ignore").strip()
        except:
            return ""

    def check_ready(self):
        line = self.read_line()

        if line == "ARDUINO_READY":
            self.ready = True
            return True

        return False

    def read_data_row(self):
        line = self.read_line()

        if not line:
            return None

        if line == "ARDUINO_READY":
            self.ready = True
            return None

        if line == "SESSION_COMPLETE":
            return "SESSION_COMPLETE"

        if line.startswith("COMP,"):
            return parse_compression_summary_line(line)

        return parse_arduino_csv_line(line)


def parse_arduino_csv_line(line):
    parts = line.split(",")

    if len(parts) != 15:
        return None

    try:
        return {
            "row_type": "DATA",

            "time_ms": int(parts[0]),
            "session_time_ms": int(parts[1]),
            "time_s": int(parts[1]) / 1000.0,

            "raw_depth_mm": float(parts[2]),
            "depth_mm": float(parts[3]),
            "peak_depth_mm": float(parts[4]),
            "lean_mm": float(parts[5]),
            "rate_cpm": float(parts[6]),

            "compression_started": int(parts[7]),
            "compression_completed": int(parts[8]),
            "motion_state": int(parts[9]),

            "hp_x": float(parts[10]),
            "hp_y": float(parts[11]),
            "hp_s": float(parts[12]),

            "force_N": float(parts[13]),
            "motor_on": int(parts[14]),
        }


    except ValueError:
        return None
def parse_compression_summary_line(line):
    """
    Expected Arduino row:

    COMP,session_time_ms,peak_depth_mm,lean_mm,rate_cpm,hp_x,hp_y,hp_s,force_N
    """

    parts = line.split(",")

    if len(parts) != 9:
        return None

    try:
        return {
            "row_type": "COMP",
            "session_time_ms": int(parts[1]),
            "time_s": int(parts[1]) / 1000.0,

            "peak_depth_mm": float(parts[2]),
            "lean_mm": float(parts[3]),
            "rate_cpm": float(parts[4]),

            "hp_x": float(parts[5]),
            "hp_y": float(parts[6]),
            "hp_s": float(parts[7]),

            "force_N": float(parts[8]),
        }

    except ValueError:
        return None