# NUM8 DLL API Summary (Production)

Tento dokument je stručný produkčný popis pre používanie `num8.dll`.

## 1. Čo je v DLL

Exportované API je definované v:
- [num8.h](C:/Users/vande/Desktop/NUM8-DENSE/num8.h)

Knižnica:
- [num8.dll](C:/Users/vande/Desktop/NUM8-DENSE/build/Release/num8.dll)
- import lib: [num8.lib](C:/Users/vande/Desktop/NUM8-DENSE/build/Release/num8.lib)

## 2. Dátový súbor

Engine pracuje so súborom `.num8`:
- fixný header: `64 bytes`
- fixný payload: `12,500,000 bytes`
- celkom: `12,500,064 bytes`

Doména hodnôt:
- `0..99,999,999`
- string forma musí byť presne 8 číslic (`"00000000"` až `"99999999"`).

## 3. Core API

- `num8_create(path, &engine)`
  - vytvorí nový `.num8` súbor a otvorí engine v RW režime.
- `num8_open(path, mode, &engine)`
  - otvorí existujúci súbor a validuje header.
- `num8_close(&engine)`
  - zavrie engine; v RW režime najprv flushne zmeny.

- `num8_exists_u32(...)` / `num8_exists_str(...)`
  - kontrola prítomnosti čísla.
- `num8_add_u32(...)` / `num8_add_str(...)`
  - pridanie čísla.
- `num8_remove_u32(...)` / `num8_remove_str(...)`
  - odobratie čísla.

- `num8_count(...)`
  - vráti aktuálny `set_count`.
- `num8_clear_all(...)`
  - vynuluje celý payload a nastaví count na 0.
- `num8_flush(...)`
  - zapíše payload + header commit.
- `num8_validate(...)`
  - overí integritu.

## 4. Return status (najdôležitejšie)

Úspech/flow:
- `NUM8_STATUS_OK`
- `NUM8_STATUS_ADDED`
- `NUM8_STATUS_REMOVED`
- `NUM8_STATUS_ALREADY_EXISTS`
- `NUM8_STATUS_NOT_FOUND`

Vstup:
- `NUM8_STATUS_INVALID_ARGUMENT`
- `NUM8_STATUS_INVALID_NUMBER`
- `NUM8_STATUS_INVALID_FORMAT`

I/O a integrita:
- `NUM8_STATUS_IO_ERROR`
- `NUM8_STATUS_OPEN_FAILED`
- `NUM8_STATUS_CREATE_FAILED`
- `NUM8_STATUS_FLUSH_FAILED`
- `NUM8_STATUS_CORRUPTED`
- `NUM8_STATUS_HEADER_CRC_MISMATCH`
- `NUM8_STATUS_PAYLOAD_CRC_MISMATCH`
- `NUM8_STATUS_SIZE_MISMATCH`
- `NUM8_STATUS_UNSUPPORTED_VERSION`

Režim/stav:
- `NUM8_STATUS_READ_ONLY`
- `NUM8_STATUS_NOT_OPEN`
- `NUM8_STATUS_OUT_OF_MEMORY`

## 5. Typický produkčný flow

1. `num8_open(...)` (alebo `num8_create(...)`).
2. `num8_add_*` / `num8_remove_*` / `num8_exists_*`.
3. Pri zápisoch volať `num8_flush(...)` v commit bodoch.
4. Pred ukončením `num8_close(...)`.

## 6. Krátky C príklad

```c
#include "num8.h"

int main(void)
{
    num8_engine_t e;
    int exists = 0;

    if (num8_open("customers.num8", NUM8_OPEN_READ_WRITE, &e) != NUM8_STATUS_OK)
        return 1;

    num8_add_str(&e, "12345678");
    num8_exists_u32(&e, 12345678u, &exists);

    if (num8_flush(&e) != NUM8_STATUS_OK) {
        num8_close(&e);
        return 2;
    }

    num8_close(&e);
    return 0;
}
```

## 7. Poznámky pre produkciu

- Engine nie je implicitne thread-safe.
- Odporúčaný model je single-writer, multi-reader.
- Prípona dátového súboru má byť `.num8`.
- Builder utility: [num8_builder.c](C:/Users/vande/Desktop/NUM8-DENSE/num8_builder.c)
- Self-test utility: [num8_selftest.c](C:/Users/vande/Desktop/NUM8-DENSE/num8_selftest.c)
