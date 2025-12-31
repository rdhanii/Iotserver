import requests
import time
import os
import re
from datetime import datetime

# ================== KONFIGURASI ==================
BASE_PATH = r"X:\AviMet\History"
API_KEY = "I6OFXOW30RGKI1QE"
THINGSPEAK_URL = "https://api.thingspeak.com/update"
INTERVAL = 15  # detik
# =================================================


def get_file_his():
    """
    Membentuk path otomatis:
    X:\AviMet\History\YYYY\Mon\WIND_METGARDEN_DD.his
    """
    now = datetime.now()
    year = now.strftime("%Y")      # 2025
    month = now.strftime("%b")     # Jan..Dec
    day = now.strftime("%d")       # 01..31
    filename = f"WIND_METGARDEN_{day}.his"
    return os.path.join(BASE_PATH, year, month, filename)


def ambil_data_his():
    """
    Mengambil data terakhir dari file .his:
    - dt         -> datetime (dipakai untuk created_at ThingSpeak)
    - timestamp  -> UNIX timestamp (field1)
    - wsins      -> field2
    - wdins      -> field3
    """
    path_file = get_file_his()

    if not os.path.exists(path_file):
        raise FileNotFoundError(f"File tidak ditemukan: {path_file}")

    with open(path_file, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()

    # Ambil baris terakhir yang valid
    last_line = None
    for line in reversed(lines):
        if line.strip():
            last_line = line.strip()
            break

    if not last_line:
        raise ValueError("File kosong atau tidak ada data valid")

    # ================== CREATEDATE ==================
    # Ambil tanggal & jam dari awal baris (aman untuk TAB / spasi)
    m = re.match(
        r"^(\d{4}-\d{2}-\d{2})\s+(\d{2}:\d{2}:\d{2})",
        last_line
    )
    if not m:
        raise ValueError(f"Format tanggal tidak dikenali: {repr(last_line)}")

    date_str = m.group(1)
    time_str = m.group(2)

    dt = datetime.strptime(
        f"{date_str} {time_str}",
        "%Y-%m-%d %H:%M:%S"
    )

    timestamp = int(dt.timestamp())

    # ================== WSINS & WDINS ==================
    # Split whitespace supaya kebal TAB / spasi campuran
    cols = re.split(r"\s+", last_line)

    if len(cols) <= 10:
        raise ValueError(
            f"Jumlah kolom tidak cukup ({len(cols)}): {repr(last_line)}"
        )

    wsins = float(cols[3])     # WSINS (kolom 3)
    wdins = float(cols[10])    # WDINS (kolom 10)

    return dt, timestamp, wsins, wdins


def kirim_ke_thingspeak(dt, timestamp, wsins, wdins):
    """
    created_at DIPAKSA mengikuti waktu data (.his)
    """
    payload = {
        "api_key": API_KEY,
        "created_at": dt.isoformat(),   # ⬅ KUNCI AGAR WAKTU PERSIS
        "field1": timestamp,
        "field2": wsins,
        "field3": wdins
    }

    r = requests.post(
        THINGSPEAK_URL,
        data=payload,
        timeout=10
    )
    return r.text


# ================== LOOP UTAMA ==================
if __name__ == "__main__":
    print("ThingSpeak AviMet uploader STARTED")

    while True:
        try:
            dt, createdate, wsins, wdins = ambil_data_his()

            result = kirim_ke_thingspeak(
                dt, createdate, wsins, wdins
            )

            if result != "0":
                print(
                    f"OK | {dt} | "
                    f"WSINS={wsins} m/s | WDINS={wdins}°"
                )
            else:
                print("GAGAL | ThingSpeak menolak data")

        except Exception as e:
            print("ERROR:", e)

        time.sleep(INTERVAL)
