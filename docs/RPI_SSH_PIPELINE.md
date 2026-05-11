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

Na Windows musi dzialac SSH server. Jezeli uzywasz Git Bash jako powloki po SSH, na RPi wystarczy ustawic `PC_PROJECT_DIR`.

Test lokalny odbiornika:

```bash
printf "BMS,1,52.100,0.000,75000000,151,3260,3261,3262,3263\n" | bash scripts/windows_receive_from_ssh.sh
```

Ten skrypt kompiluje `DataManager.java`, czyta dane ze standardowego wejscia i wysyla je do `BMS_API_INGEST_URL`, domyslnie `http://127.0.0.1:8090/api/ingest`.

## 2. Raspberry Pi

Zbuduj program C:

```bash
cd rpi
make
```

Jednorazowy odczyt w formacie dla Javy:

```bash
./tinybms_rpi --device /dev/ttyUSB0 --module 1 --no-print --bms-line read
```

Ciagly odczyt i wysylka do PC:

```bash
cd ..
PC_USER=twoj_user \
PC_HOST=192.168.1.50 \
PC_PROJECT_DIR=/c/Users/Piotrek/Desktop/Bms-C-Java-Rpi \
./rpi/stream_to_windows_java.sh
```

Jezeli Windows OpenSSH nie startuje od razu w Git Bash, ustaw pelna komende zdalna:

```bash
REMOTE_COMMAND='bash -lc "cd /c/Users/Piotrek/Desktop/Bms-C-Java-Rpi && bash scripts/windows_receive_from_ssh.sh"' \
PC_USER=twoj_user \
PC_HOST=192.168.1.50 \
./rpi/stream_to_windows_java.sh
```

## 3. Bez hasla przy kazdym polaczeniu

Na Raspberry Pi:

```bash
ssh-keygen
ssh-copy-id twoj_user@192.168.1.50
```

Potem `stream_to_windows_java.sh` moze dzialac bez wpisywania hasla przy kazdym starcie.

## Format danych

`tinybms_rpi --bms-line` wypisuje:

```text
BMS,module_id,pack_voltage_v,current_a,soc_raw,status_decimal,cell_1_mv,cell_2_mv,...
```

Ten format jest zgodny z parserem `BmsApiServer`.
