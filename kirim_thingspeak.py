import requests
import time
import os
from datetime import datetime

BASE_PATH = r"X:\AviMet\History"
API_KEY = "T4SSZQ9PPXG9C3EJ"
URL = "https://api.thingspeak.com/update"

def get_file_his():
    now = datetime.now()
    year = now.strftime("%Y")
    month = now.strftime("%b")
    day = now.strftime("%d")
    filename = f"WIND_METGARDEN_{day}.his"
    return os.path.join(BASE_PATH, year, month, filename)

def ambil_data_his():
    path_file = get_file_his()

    if not os.path.exists(path_file):
        raise FileNotFoundError(f"File tidak ditemukan: {path_file}")

    with open(path_file, "r") as file:
        lines = file.readlines()

    for line in reversed(lines):
        if line.strip():
            data = line.strip().split("\t")
            break
            
    wsins = float(data[3])     # kolom 3
    wdins = float(data[10])    # kolom 10
    return wsins, wdins


while True:
    try:
        wsins, wdins = ambil_data_his()

        payload = {
            "api_key": API_KEY,
            "field2": wsins,
            "field3": wdins
        }

        r = requests.post(URL, data=payload)

        if r.text != "0":
            print(f"Terkirim | {get_file_his()} | WSINS={wsins} WDINS={wdins}")
        else:
            print("Gagal kirim")

    except Exception as e:
        print("Error:", e)

    time.sleep(15)
