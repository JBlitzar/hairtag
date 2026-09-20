#!/usr/bin/env python3

# Generates N secp224r1 keypairs for pre-generated key rotation.
# Writes src/keys.h (gitignored) and one composite <prefix>.keys.json
# for the findmy fetch side.

import argparse
import base64
import hashlib
import json
import os
import random
import string

from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.asymmetric import ec

MAX_KEYS = 500
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(SCRIPT_DIR, "..", "src")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "output")


def sha256(data):
    digest = hashlib.new("sha256")
    digest.update(data)
    return digest.digest()


def to_c_array(b):
    return "{ " + ", ".join("0x%02x" % x for x in b) + " }"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-n", "--nkeys", help="number of keys to generate (1..500)", type=int, default=96
    )
    parser.add_argument("-p", "--prefix", help="name prefix for the key set")
    args = parser.parse_args()

    if args.nkeys < 1 or args.nkeys > MAX_KEYS:
        parser.error("nkeys out of range (1..%d)" % MAX_KEYS)

    prefix = args.prefix or "".join(
        random.choice(string.ascii_uppercase + string.digits) for _ in range(6)
    )

    keys = []
    while len(keys) < args.nkeys:
        priv = random.getrandbits(224)
        adv = (
            ec.derive_private_key(priv, ec.SECP224R1(), default_backend())
            .public_key()
            .public_numbers()
            .x
        )
        priv_b64 = base64.b64encode(priv.to_bytes(28, "big")).decode("ascii")
        adv_bytes = adv.to_bytes(28, "big")
        adv_b64 = base64.b64encode(adv_bytes).decode("ascii")
        s256_b64 = base64.b64encode(sha256(adv_bytes)).decode("ascii")
        if "/" in s256_b64[:7]:
            continue
        keys.append((priv_b64, adv_b64, s256_b64, adv_bytes))

    with open(os.path.join(SRC_DIR, "keys.h"), "w") as f:
        f.write("#define KEY_COUNT %d\n\n" % len(keys))
        f.write("static const uint8_t keys[KEY_COUNT][28] = {\n")
        for _, _, _, adv_bytes in keys:
            f.write("    %s,\n" % to_c_array(adv_bytes))
        f.write("};\n")

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    composite = {
        "name": prefix,
        "keys": [
            {"private": priv_b64, "advertisement": adv_b64, "hashed": s256_b64}
            for priv_b64, adv_b64, s256_b64, _ in keys
        ],
    }
    out_path = os.path.join(OUTPUT_DIR, prefix + ".keys.json")
    with open(out_path, "w") as f:
        json.dump(composite, f, indent=2)

    print("wrote %s" % os.path.join(SRC_DIR, "keys.h"))
    print("wrote %s (copy into software/findmy/)" % out_path)


if __name__ == "__main__":
    main()
