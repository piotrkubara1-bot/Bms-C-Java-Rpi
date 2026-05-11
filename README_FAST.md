# README FAST

To jest bardzo krótka instrukcja.

Jeśli chcesz pełną instrukcję, otwórz:

`README.md`

## 1. Wejdź do folderu projektu

```powershell
C:\sciezka\do\BmsManager
```

## 2. Jeśli nie masz `.env`, skopiuj go

```powershell
Copy-Item ".env.example" ".env"
```

Pliku `.env` nie ma na GitHubie celowo.
Na GitHubie jest tylko `.env.example`.
Ty masz zrobić własny `.env` lokalnie u siebie.

## 3. Uruchom cały serwer jedną komendą

```powershell
.\run_server_stack.bat
```

Ta komenda uruchamia:

- MySQL,
- bazę danych,
- backend,
- Web GUI

## 4. Otwórz Web GUI

Otwórz ten adres:

[http://127.0.0.1:8088/dashboard.html](http://127.0.0.1:8088/dashboard.html)

## 5. Jeśli nie masz podłączonego BMS

W Web GUI:

1. wejdź do `Cell Settings`
2. wybierz `SIMULATED`
3. kliknij `Save COM Port`
4. kliknij `Start UART`

## 6. Jeśli masz prawdziwy BMS

W Web GUI:

1. wejdź do `Cell Settings`
2. wybierz prawdziwy port, na przykład `COM3` albo `COM5`
3. kliknij `Save COM Port`
4. kliknij `Start UART`

## 7. Sprawdź, czy backend działa

```powershell
curl.exe http://127.0.0.1:8090/api/health
```

Szukaj:

```json
{"status":"ok","dbConnected":true}
```

## 8. Jeśli chcesz desktop viewer

Najpierw uruchom serwer:

```powershell
.\run_server_stack.bat
```

Potem uruchom viewer:

```powershell
.\run_desktop_web_gui.bat
```

## 9. Jeśli chcesz Raspberry Pi przez SSH

Na Windows uruchom serwer:

```powershell
.\run_server_stack.bat
```

Na Raspberry Pi zbuduj program C:

```bash
cd rpi
make
```

Wyślij dane do programu Java na Windows:

```bash
cd ..
PC_USER=twoj_user \
PC_HOST=192.168.1.50 \
PC_PROJECT_DIR=/c/Users/Piotrek/Desktop/Bms-C-Java-Rpi \
./rpi/stream_to_windows_java.sh
```

Więcej:

```text
docs/RPI_SSH_PIPELINE.md
```

## 10. Jak zatrzymać wszystko

```powershell
.\run_server_stack.bat stop
```

albo:

```powershell
.\stop_all.bat
```

## 11. Gdy po zatrzymaniu dalej cos dziala

Jezeli port jest zajety, Web GUI dziwnie dziala albo stare okno Java zostalo w tle, uzyj mocnego stopu:

```powershell
.\stop_all.bat
taskkill /F /IM java.exe
taskkill /F /IM javaw.exe
```

Potem uruchom od nowa:

```powershell
.\run_server_stack.bat
```

Komunikat `ERROR: The process "javaw.exe" not found.` jest OK. Oznacza tylko, ze nie bylo takiego procesu do zamkniecia.
