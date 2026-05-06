import csv
import json
import os
import shutil
from datetime import datetime


BASE_DIR = os.path.dirname(os.path.abspath(__file__))

DATA_DIR = os.path.join(BASE_DIR, "Smart_CPR_Data")
CSV_DIR = os.path.join(DATA_DIR, "csv_exports")
DB_FILE = os.path.join(DATA_DIR, "sessions.json")
COMP_CSV_DIR = os.path.join(DATA_DIR, "compression_exports")



def ensure_data_folders():
    os.makedirs(DATA_DIR, exist_ok=True)
    os.makedirs(CSV_DIR, exist_ok=True)
    os.makedirs(COMP_CSV_DIR, exist_ok=True)

    if not os.path.exists(DB_FILE):
        with open(DB_FILE, "w") as f:
            json.dump({}, f, indent=4)


def clean_user_name(user_name):
    user_name = user_name.strip() if user_name else "Guest"

    # Keep filenames/folders safe
    for ch in r'<>:"/\|?*':
        user_name = user_name.replace(ch, "_")

    return user_name if user_name else "Guest"


def load_database():
    ensure_data_folders()

    try:
        with open(DB_FILE, "r") as f:
            return json.load(f)
    except:
        return {}


def save_database(db):
    ensure_data_folders()

    with open(DB_FILE, "w") as f:
        json.dump(db, f, indent=4)


def save_session(user_name, rows, compression_rows, summary):
    """
    Saves one CPR session:
    - continuous raw CSV
    - per-compression CSV
    - JSON summary record
    """

    ensure_data_folders()

    user_name = clean_user_name(user_name)
    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")

    user_csv_dir = os.path.join(CSV_DIR, user_name)
    os.makedirs(user_csv_dir, exist_ok=True)

    raw_csv_filename = f"{timestamp}_raw.csv"
    raw_csv_path = os.path.join(user_csv_dir, raw_csv_filename)

    # Save continuous raw CSV
    if rows:
        fieldnames = list(rows[0].keys())

        with open(raw_csv_path, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)
    else:
        with open(raw_csv_path, "w", newline="") as f:
            f.write("No raw data recorded\n")

    # Save per-compression CSV
    compression_csv_file = save_compression_csv(user_name, compression_rows, timestamp)

    # Save summary to JSON
    db = load_database()

    if user_name not in db:
        db[user_name] = []

    session_record = {
        "date": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "raw_csv_file": os.path.relpath(raw_csv_path, DATA_DIR),
        "compression_csv_file": compression_csv_file,
        **summary,
    }

    db[user_name].append(session_record)
    save_database(db)

    return session_record


def get_user_sessions(user_name):
    user_name = clean_user_name(user_name)
    db = load_database()
    return db.get(user_name, [])


def get_all_users():
    db = load_database()
    return list(db.keys())


def delete_session(user_name, index):
    user_name = clean_user_name(user_name)
    db = load_database()

    sessions = db.get(user_name, [])

    if not (0 <= index < len(sessions)):
        return False

    session = sessions.pop(index)

    # Delete raw CSV
    raw_csv = session.get("raw_csv_file")
    if raw_csv:
        raw_path = os.path.join(DATA_DIR, raw_csv)
        if os.path.exists(raw_path):
            try:
                os.remove(raw_path)
            except:
                pass

    # Delete compression CSV
    comp_csv = session.get("compression_csv_file")
    if comp_csv:
        comp_path = os.path.join(DATA_DIR, comp_csv)
        if os.path.exists(comp_path):
            try:
                os.remove(comp_path)
            except:
                pass

    db[user_name] = sessions
    save_database(db)

    return True


def export_csvs_to_folder(destination_folder):
    """
    Copies all SMART CPR data into a selected folder or USB folder.
    Includes:
    - raw continuous CSVs
    - per-compression CSVs
    - sessions.json
    """

    ensure_data_folders()

    if not os.path.exists(destination_folder):
        return False

    export_destination = os.path.join(destination_folder, "Smart_CPR_Data_Export")

    if os.path.exists(export_destination):
        shutil.rmtree(export_destination)

    shutil.copytree(DATA_DIR, export_destination)

    return True

def save_compression_csv(user_name, compression_rows, timestamp):
    ensure_data_folders()

    user_name = clean_user_name(user_name)

    user_dir = os.path.join(COMP_CSV_DIR, user_name)
    os.makedirs(user_dir, exist_ok=True)

    filename = f"{timestamp}_comp.csv"
    path = os.path.join(user_dir, filename)

    if not compression_rows:
        with open(path, "w", newline="") as f:
            f.write("No compression data\n")
        return os.path.relpath(path, DATA_DIR)

    fieldnames = [
        "compression_index",
        "session_time_ms",
        "time_s",
        "peak_depth_mm",
        "lean_mm",
        "rate_cpm",
        "hp_x",
        "hp_y",
        "hp_s",
        "force_N",
    ]

    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

        for i, row in enumerate(compression_rows, start=1):
            writer.writerow({
                "compression_index": i,
                "session_time_ms": row.get("session_time_ms"),
                "time_s": row.get("time_s"),
                "peak_depth_mm": row.get("peak_depth_mm"),
                "lean_mm": row.get("lean_mm"),
                "rate_cpm": row.get("rate_cpm"),
                "hp_x": row.get("hp_x"),
                "hp_y": row.get("hp_y"),
                "hp_s": row.get("hp_s"),
                "force_N": row.get("force_N"),
            })

    return os.path.relpath(path, DATA_DIR)

def load_compression_rows_for_session(session):
    """
    Loads the per-compression CSV rows for a saved session.
    session: one session record from sessions.json
    """

    ensure_data_folders()

    compression_csv = session.get("compression_csv_file")

    if not compression_csv:
        return []

    path = os.path.join(DATA_DIR, compression_csv)

    if not os.path.exists(path):
        return []

    rows = []

    try:
        with open(path, "r", newline="") as f:
            reader = csv.DictReader(f)

            for row in reader:
                rows.append({
                    "compression_index": int(row.get("compression_index", 0)),
                    "session_time_ms": int(float(row.get("session_time_ms", 0))),
                    "time_s": float(row.get("time_s", 0.0)),
                    "peak_depth_mm": float(row.get("peak_depth_mm", 0.0)),
                    "lean_mm": float(row.get("lean_mm", 0.0)),
                    "rate_cpm": float(row.get("rate_cpm", 0.0)),
                    "hp_x": float(row.get("hp_x", 0.0)),
                    "hp_y": float(row.get("hp_y", 0.0)),
                    "hp_s": float(row.get("hp_s", 0.0)),
                    "force_N": float(row.get("force_N", 0.0)),
                })

    except Exception as e:
        print(f"Failed to load compression rows: {e}")
        return []

    return rows