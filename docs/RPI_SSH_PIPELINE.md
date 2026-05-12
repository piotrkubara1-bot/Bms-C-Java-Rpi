# Raspberry Pi -> SSH -> Windows Bash -> Java

Docelowy przeplyw:

```text
bateria -> BMS -> Raspberry Pi -> rpi/tinybms_rpi -> SSH -> Windows Bash -> DataManager.java -> BmsApiServer -> GUI
```

## 1. Windows PC

Uruchom backend:

```powershell
.\run_server_stack.bat
```

Backend powinien pokazac:

```text
[BmsApiServer] Listening on 0.0.0.0:8090
```

Na Windows musi dzialac SSH server. Jezeli SSH uruchamia WSL/bash, skrypt `scripts/windows_receive_from_ssh.sh` uzyje `java.exe`, zeby laczyc sie przez siec Windows.

Test lokalny odbiornika:

```bash
printf "BMS,1,52.100,0.000,75000000,151,3260,3261,3262,3263\n" | bash scripts/windows_receive_from_ssh.sh
```

Ten skrypt kompiluje `DataManager.java`, czyta dane ze standardowego wejscia i wysyla je do `BMS_API_INGEST_URL`, domyslnie `http://127.0.0.1:8090/api/ingest`.

Sprawdz wersje Java na laptopie:

```cmd
where java
where javac
java -version
javac -version
```

`java` i `javac` musza byc z tej samej glownej wersji. Jezeli `where java` jako pierwsze pokazuje Jave 8, a `javac` pokazuje JDK 25, popraw `Path` w Windows.

## 2. Raspberry Pi

Zbuduj program C:

```bash
cd rpi
make
```

Sensor DS18B20 jest czytany przez Python. W katalogu `rpi` jest gotowy plik:

```bash
python3 temperatureSensor.py
```

Powinien wypisac jedna liczbe, np. `24.5625`. Skrypt `stream_to_windows_java.sh` uruchamia go domyslnie i dopisuje wynik do linii BMS jako `TEMP_C`.

Jezeli nie chcesz uzywac temperatury albo czujnik nie jest jeszcze gotowy, wylacz odczyt temperatury:

```bash
TEMPERATURE_COMMAND= \
PC_USER=piotrek \
PC_HOST=192.168.137.1 \
REMOTE_COMMAND='cd /d "C:\Users\Piotrek\Desktop\Bms-C-Java-Rpi-main" && bash -lc "bash scripts/windows_receive_from_ssh.sh"' \
./rpi/stream_to_windows_java.sh
```

Jezeli Python pokazuje `No module named w1thermsensor`, zainstaluj biblioteke w tym samym Pythonie, ktory uruchamia skrypt. Dla aktywnego `venv`:

```bash
python3 -m pip install w1thermsensor
```

Dla systemowego Pythona:

```bash
sudo apt update
sudo apt install python3-w1thermsensor
```

Sprawdz import i odczyt:

```bash
python3 -c "from w1thermsensor import W1ThermSensor; print('ok')"
python3 rpi/temperatureSensor.py
```

Dla DS18B20 musi byc wlaczone 1-Wire. W Raspberry Pi OS zwykle dodaj do `/boot/firmware/config.txt` albo `/boot/config.txt`:

```text
dtoverlay=w1-gpio
```

Potem zrestartuj RPi:

```bash
sudo reboot
```

Jednorazowy odczyt w formacie dla Javy:

```bash
./tinybms_rpi --device /dev/ttyUSB0 --module 1 --temperature-command "python3 ./temperatureSensor.py" --no-print --bms-line read
```

Ciagly odczyt i wysylka do PC:

```bash
cd ..
PC_USER=piotrek \
PC_HOST=192.168.137.1 \
REMOTE_COMMAND='cd /d "C:\Users\Piotrek\Desktop\Bms-C-Java-Rpi-main" && bash -lc "bash scripts/windows_receive_from_ssh.sh"' \
./rpi/stream_to_windows_java.sh
```

Jezeli projekt jest w innym folderze, zmien tylko sciezke w `REMOTE_COMMAND`.

## 3. Hotspot Windows

Gdy laptop udostepnia hotspot, jego IP zwykle jest:

```text
192.168.137.1
```

Raspberry Pi ma wtedy adres podobny do:

```text
192.168.137.218
```

Na RPi sprawdz adres laptopa:

```bash
ip route
```

Adres po `default via` to zwykle adres laptopa. Uzywaj go jako `PC_HOST`.

## 4. Testy bez BMS

Sprawdz backend przez SSH:

```bash
ssh piotrek@192.168.137.1 'curl.exe http://127.0.0.1:8090/api/health'
```

Wyslij jedna linie testowa do backendu:

```bash
printf "BMS,1,52.100,0.000,75000000,151,3260,3261,3262,3264\n" | ssh piotrek@192.168.137.1 'curl.exe -X POST http://127.0.0.1:8090/api/ingest --data-binary @-'
```

Poprawna odpowiedz:

```json
{"accepted":1,"heartbeat":0,"rejected":0}
```

Sprawdz ostatnie dane:

```bash
ssh piotrek@192.168.137.1 'curl.exe http://127.0.0.1:8090/api/latest'
```

Wazne:

- `/api/ingest` jest do wysylania danych metoda `POST`.
- `/api/latest` jest do odczytu danych metoda `GET`.
- Nie rob `POST` na `/api/latest`.

## 5. Bez hasla przy kazdym polaczeniu

Na Raspberry Pi:

```bash
ssh-keygen
ssh-copy-id piotrek@192.168.137.1
```

Potem `stream_to_windows_java.sh` moze dzialac bez wpisywania hasla przy kazdym starcie.

## 6. Rozwiazywanie problemow

### `Connection refused`

Backend nie dziala albo `DataManager` laczy sie z niewlasciwym adresem. Sprawdz:

```bash
ssh piotrek@192.168.137.1 'curl.exe http://127.0.0.1:8090/api/health'
```

### `Connect timed out`

Najczesciej firewall Windows blokuje port `8090`. W PowerShell jako administrator:

```powershell
New-NetFirewallRule -Name BMSApi8090 -DisplayName "BMS API 8090" -Enabled True -Direction Inbound -Protocol TCP -Action Allow -LocalPort 8090
```

### `UnsupportedClassVersionError`

`javac` skompilowal klase nowsza Java niz ta, ktora uruchamia `java`. Na laptopie:

```cmd
where java
where javac
java -version
javac -version
```

Pierwsze `where java` powinno wskazywac aktualny JDK, np.:

```text
C:\Program Files\Java\jdk-25\bin\java.exe
```

### Web GUI nic nie pokazuje

Sprawdz najpierw API:

```bash
ssh piotrek@192.168.137.1 'curl.exe http://127.0.0.1:8090/api/latest'
```

Jesli zwraca dane, odswiez Web GUI przez `Ctrl + F5`.
Jesli zwraca `[]`, backend jeszcze nie dostal danych.

### `Temperature command produced no output`

Program C uruchomil skrypt temperatury, ale skrypt nic nie wypisal. Najpierw testuj:

```bash
python3 rpi/temperatureSensor.py
```

Jesli czujnik nie jest potrzebny, uruchom caly potok z:

```bash
TEMPERATURE_COMMAND=
```

### `Read timed out` w `DataManager`

To znaczy, ze polaczenie HTTP do API zostalo otwarte, ale backend nie oddal odpowiedzi w czasie. Sprawdz na laptopie:

```powershell
curl.exe http://127.0.0.1:8090/api/health
curl.exe http://127.0.0.1:8090/api/latest
```

Jesli backend odpowiada wolno albo wcale, zrestartuj:

```powershell
.\stop_all.bat
.\run_server_stack.bat
```

Jesli problem pojawia sie przy zapisie do bazy, sprawdz czy XAMPP/MySQL dziala poprawnie.

## Format danych

`tinybms_rpi --bms-line` wypisuje:

```text
BMS,module_id,pack_voltage_v,current_a,soc_raw,status_decimal,cell_1_mv,cell_2_mv,...,TEMP_C,temperature_c
```

Pole `TEMP_C` jest opcjonalne. Stare linie bez temperatury nadal sa zgodne z parserem `BmsApiServer`.
