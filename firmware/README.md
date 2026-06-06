# Firmware

This directory contains everything needed to turn an ESP32-C3 into an Apple Find My tracker and query its location from your machine.

Think of it this way: I basically got five half-broken implementations off of github, all at least two years old, tried things until it worked, and merged them together into one cursed readme here.

This was one of the hardest installation processes I've ever documented (or done), probably superseded in difficulty only to that one arm64 rabbit r1 rooting project.

This will likely only work on a mac, because apple is apple.

For the sake of reproducibility, here's what to do:



## How it works

1. `esp32c3-openhaystack/` runs on the ESP32-C3 and broadcasts BLE advertisements that Apple devices interpret as a Find My accessory.
2. `findmy/` runs on your computer, authenticates with Apple, and fetches/decrypts location reports from the Find My network.

## Prerequisites

- macOS (special apple things)
- [Docker Desktop](https://www.docker.com/products/docker-desktop/) running
- [uv](https://docs.astral.sh/uv/) package manager
- An Apple ID with SMS 2FA enabled
- The flashing setup as described in [README.md](/README.md#wiring-diagram-for-flashing)

## Anisette server

```bash
docker network create mh-network
docker run -d --restart always --name anisette -p 6969:6969 \
  --volume anisette-v3_data:/home/Alcoholic/.config/anisette-v3 \
  --network mh-network \
  dadoum/anisette-v3-server
```

Verify it's running:

```bash
curl http://localhost:6969
```

## Generate keys

See instructions in the esp32c3 README: [`esp32c3-openhaystack/README.md`](esp32c3-openhaystack/README.md)

```bash
cd esp32c3-openhaystack/
uv sync
uv run scripts/keygen.py
```

This creates a `.keys` file (e.g. `XXXXX.keys`) in the `scripts/output/` directory containing your private key, advertisement key, and hashed advertisement key. Copy the .keys file to `findmy`.

## Flash the firmware

Follow the instructions in [`esp32c3-openhaystack/README.md`](esp32c3-openhaystack/README.md). In short:

```bash
cd esp32c3-openhaystack/
# Build and flash firmware
pio run -t upload
# Write your keys to the device (copy your .keys file to scripts/input/ first)
python3 scripts/keywriter.py
```

After flashing and resetting, the device starts advertising immediately.

## Create the reports database

```bash
cd findmy/
sqlite3 reports.db 'CREATE TABLE reports (id_short TEXT, timestamp INTEGER, datePublished INTEGER, payload TEXT, id TEXT, statusCode INTEGER, PRIMARY KEY(id_short,timestamp))'
```

## Authenticate with Apple

Disclaimer: I'm not responsible if something happens to your apple account. Prefer an alt account.

My personal advice is that it's probably fine though.

```bash
cd findmy/
uv run request_reports.py --regen
```

You'll be prompted for your Apple ID, password, and a 2FA code.

Now, you won't actually get a 2fa code texted to you. For that, go to icloud.com, sign in, it will give send an approval code to your phone. But don't use that either. You click, "can't use this device" or whatever, then select the option to send an *SMS* code via text. Then paste that sms code (not the initial approval code) into the terminal (not icloud.com)

On success, the script saves `auth.json` in the `findmy/` directory. Subsequent runs reuse it without prompting.

## Query locations

```bash
cd findmy/
./locate.sh
```

Output looks like:

```
200: 8 reports received.
8 reports used.
{'lat': 37.7841, 'lon': -122.4194, 'conf': 1, 'status': 0, 'timestamp': 1749234567, 'isodatetime': '2026-06-06T14:42:47', 'key': 'XXXXX', 'goog': 'https://maps.google.com/maps?q=37.7841,-122.4460'}
found:   ['XXXXX']
missing: []
```

The `goog` URL opens the location in Google Maps.

a TODO for the future is to make this a nice TUI. 

### Options

| Flag | Default | Description |
|------|---------|-------------|
| `--hours` | 24 | Look back this many hours for reports |
| `--regen` | off | Force re-authentication (needed when token expires) |
| `--prefix` | (none) | Only query `.keys` files starting with this prefix |
| `--trusteddevice` | off | Use trusted device 2FA instead of SMS |

## Auth token expiry

idk, sometimes it expires.

```bash
uv run request_reports.py --regen
```

The anisette Docker container must be running for this to work.

## On apple IDs

You could make a burner apple id if you want I guess, it's kind of a pain though, what with the android apple music bypass and whatnot.


## Licensing

I did not write this myself. I copied these two repositories, patched them so that they worked, and put them together.

Thank you, @biemster, @timbeh, and of course seemoo lab. 

https://github.com/biemster/FindMy

https://github.com/timbeh/esp32c3-openhaystack
