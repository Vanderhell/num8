# Troubleshooting

- `NUM8_STATUS_CORRUPTED` -> file is damaged, CRC or consistency validation failed.
- `NUM8_STATUS_READ_ONLY` -> engine was opened in read-only mode.
- `NUM8_STATUS_INVALID_NUMBER` -> numeric input is greater than `99999999`.
- `NUM8_STATUS_INVALID_FORMAT` -> string input is not exactly 8 digits.
- Build fails on Linux -> verify CMake version is 3.16+ and a C99 compiler is available.