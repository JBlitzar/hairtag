## Method

Generate keys first (writes gitignored `src/keys.h` + one `<prefix>.keys.json` for `../findmy/`):

```bash
uv run --with cryptography scripts/keygen.py -n 96 -p MYTAG
```

For a fresh clone without keygen: `cp src/keys.h.template src/keys.h` (dummy key).

Click build (incremental build will always fail, pristine build mostly doesn't fail but sometimes does. It if fails, rebuild it lol)

Re-plug xiao, double click RST

copy build/firmware-nrf52840/zephyr/zephyr.uf2 to the XIAO drive

profit!
