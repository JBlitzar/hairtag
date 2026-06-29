#!/usr/bin/env python3

import base64
import datetime
import glob
import hashlib
import os
import sqlite3
import struct

from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from flask import Flask, jsonify, render_template, request

app = Flask(__name__)

BASE_DIR = os.path.dirname(os.path.realpath(__file__))
DB_PATH = os.path.join(BASE_DIR, "reports.db")


def load_privkeys():
    """Return {hashed_adv_b64: (priv_b64, short_name)} for all .keys files."""
    keys = {}
    for keyfile in glob.glob(os.path.join(BASE_DIR, "*.keys")):
        with open(keyfile) as f:
            priv = hashed = ""
            name = os.path.basename(keyfile)[:-5]
            for line in f:
                k = line.rstrip("\n").split(": ")
                if k[0] == "Private key":
                    priv = k[1]
                elif k[0] == "Hashed adv key":
                    hashed = k[1]
            if priv and hashed:
                keys[hashed] = (priv, name)
    return keys


def sha256(data):
    d = hashlib.new("sha256")
    d.update(data)
    return d.digest()


def decrypt_payload(payload_b64, priv_b64):
    priv = int.from_bytes(base64.b64decode(priv_b64), "big")
    data = base64.b64decode(payload_b64)
    if len(data) > 88:
        data = data[:4] + data[5:]

    eph_key = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP224R1(), data[5:62])
    shared_key = ec.derive_private_key(
        priv, ec.SECP224R1(), default_backend()
    ).exchange(ec.ECDH(), eph_key)
    sym_key = sha256(shared_key + b"\x00\x00\x00\x01" + data[5:62])
    dec_key = sym_key[:16]
    iv = sym_key[16:]
    enc_data = data[62:72]
    auth_tag = data[72:]

    dec = Cipher(
        algorithms.AES(dec_key), modes.GCM(iv, auth_tag), default_backend()
    ).decryptor()
    decrypted = dec.update(enc_data) + dec.finalize()

    lat = struct.unpack(">i", decrypted[0:4])[0] / 10000000.0
    lon = struct.unpack(">i", decrypted[4:8])[0] / 10000000.0
    conf = int.from_bytes(decrypted[8:9], "big")
    status = int.from_bytes(decrypted[9:10], "big")
    return lat, lon, conf, status


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/tags")
def api_tags():
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("SELECT DISTINCT id_short FROM reports ORDER BY id_short")
    tags = [row[0] for row in cur.fetchall()]
    conn.close()
    return jsonify(tags)


@app.route("/api/reports")
def api_reports():
    hours = request.args.get("hours", 24, type=int)
    cutoff = int(datetime.datetime.now().timestamp()) - hours * 3600

    privkeys = load_privkeys()

    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()
    cur.execute(
        "SELECT id_short, timestamp, payload, id FROM reports WHERE timestamp >= ? ORDER BY timestamp ASC",
        (cutoff,),
    )
    rows = cur.fetchall()
    conn.close()

    results = []
    for row in rows:
        hashed_adv = row["id"]
        if hashed_adv not in privkeys:
            continue
        priv_b64, name = privkeys[hashed_adv]
        try:
            lat, lon, conf, status = decrypt_payload(row["payload"], priv_b64)
        except Exception:
            continue

        results.append(
            {
                "key": name,
                "lat": lat,
                "lon": lon,
                "conf": conf,
                "status": status,
                "timestamp": row["timestamp"],
                "isodatetime": datetime.datetime.fromtimestamp(
                    row["timestamp"]
                ).isoformat(),
            }
        )

    return jsonify(results)


if __name__ == "__main__":
    import socket

    app.run(host="0.0.0.0", port=5369, debug=False)
