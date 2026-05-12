# BmsManager

To jest bardzo prosta instrukcja uruchomienia projektu na Windows.

Ten projekt ma kilka części:

- baza danych MySQL,
- backend API,
- Web GUI w przeglądarce,
- desktop viewer w JavaFX,
- program C na Raspberry Pi,
- most SSH z Raspberry Pi do programu Java na Windows.

Najważniejsze:

- serwer uruchamiasz jedną komendą: `.\run_server_stack.bat`
- Web GUI działa bez podłączonego BMS
- jeśli nie masz sprzętu, możesz użyć trybu `SIMULATED`
- dane z RPi możesz przesłać do Windows przez SSH i `DataManager.java`

## 1. Co musisz mieć zainstalowane

Na komputerze musisz mieć:

- Java JDK
- XAMPP w folderze `C:\xampp`
- PowerShell
- opcjonalnie Git Bash i OpenSSH Server, jeśli chcesz odbierać dane z Raspberry Pi przez SSH

## 2. W jakim folderze uruchamiać komendy

Wszystkie komendy uruchamiaj w folderze projektu.

Przykład:

```powershell
C:\sciezka\do\BmsManager
```

W PowerShell przed plikiem `.bat` zawsze pisz `.\`

Przykład:

```powershell
.\run_server_stack.bat
```

Nie tak:

```powershell
run_server_stack.bat
```

## 3. Pierwsze przygotowanie

### Krok 1. Skopiuj plik konfiguracyjny

Jeśli nie masz jeszcze pliku `.env`, skopiuj go z przykładu:

```powershell
Copy-Item ".env.example" ".env"
```

Jeśli plik `.env` już istnieje, nic nie rób.

Ważne:

- pliku `.env` nie ma na GitHubie celowo,
- ten plik zawiera lokalną konfigurację komputera,
- dlatego w repo jest tylko `.env.example`,
- Ty masz skopiować `.env.example` do `.env` u siebie na swoim komputerze

### Krok 2. Otwórz `.env`

Otwórz plik `.env` w folderze projektu.

Sprawdź, czy masz tam przynajmniej takie wartości:

```env
BMS_API_PORT=8090
WEB_UI_PORT=8088
DB_HOST=127.0.0.1
DB_PORT=3306
DB_NAME=bms
DB_USER=root
DB_PASSWORD=
SERIAL_PORT=COM5
SERIAL_BAUD=115200
BMS_API_INGEST_URL=http://127.0.0.1:8090/api/ingest
```

Jeśli nie masz podłączonego BMS i chcesz tylko testować program, ustaw:

```env
SERIAL_PORT=SIMULATED
```

## 4. Jak uruchomić cały serwer

Najprościej tak:

```powershell
.\run_server_stack.bat
```

Ten skrypt robi po kolei:

1. uruchamia MySQL z XAMPP,
2. czeka aż MySQL będzie gotowy,
3. importuje plik `bms_schema.sql`,
4. uruchamia backend API,
5. uruchamia Web GUI.

Nie musisz osobno uruchamiać bazy, backendu i Web GUI.

## 5. Jak sprawdzić, czy serwer działa

Po uruchomieniu wejdź na:

- [http://127.0.0.1:8090/api/health](http://127.0.0.1:8090/api/health)
- [http://127.0.0.1:8088/dashboard.html](http://127.0.0.1:8088/dashboard.html)

Możesz też sprawdzić to komendą:

```powershell
curl.exe http://127.0.0.1:8090/api/health
```

Poprawny wynik powinien wyglądać mniej więcej tak:

```json
{"status":"ok","dbConnected":true}
```

Najważniejsze są dwie rzeczy:

- `"status":"ok"`
- `"dbConnected":true`

## 6. Jak zatrzymać serwer

Zatrzymanie:

```powershell
.\run_server_stack.bat stop
```

albo:

```powershell
.\stop_all.bat
```

### Mocne zatrzymanie, gdy program zachowuje sie dziwnie

Jezeli po zatrzymaniu program dalej dziala, port jest zajety albo Web GUI/desktop viewer dziwnie sie odswieza, zrob pelne czyszczenie procesow Java:

```powershell
.\stop_all.bat
taskkill /F /IM java.exe
taskkill /F /IM javaw.exe
```

Co to robi:

- `.\stop_all.bat` zatrzymuje procesy nasluchujace na portach projektu,
- `taskkill /F /IM java.exe` zamyka pozostale procesy Java w konsoli,
- `taskkill /F /IM javaw.exe` zamyka pozostale procesy Java uruchomione jako okna.

Jezeli zobaczysz komunikat:

```text
ERROR: The process "javaw.exe" not found.
```

to nie jest blad projektu. To znaczy tylko, ze nie bylo procesu `javaw.exe` do zamkniecia.

Po takim czyszczeniu uruchom projekt od nowa:

```powershell
.\run_server_stack.bat
```

## 7. Jak używać Web GUI

Web GUI otwierasz tutaj:

[http://127.0.0.1:8088/dashboard.html](http://127.0.0.1:8088/dashboard.html)

W Web GUI możesz:

- oglądać dane,
- wybrać port COM,
- zapisać port do konfiguracji,
- uruchomić i zatrzymać UART,
- użyć trybu `SIMULATED`,
- wysłać komendy `Reset BMS`, `Clear Events`, `Clear Statistics`

## 8. Co zrobić, jeśli nie masz podłączonego BMS

Jeśli nie masz sprzętu, nadal możesz uruchomić cały projekt.

Zrób tak:

1. uruchom serwer:

```powershell
.\run_server_stack.bat
```

2. otwórz Web GUI:

[http://127.0.0.1:8088/dashboard.html](http://127.0.0.1:8088/dashboard.html)

3. przejdź do zakładki `Cell Settings`
4. wybierz port `SIMULATED`
5. kliknij `Save COM Port`
6. kliknij `Start UART`

Wtedy program nie używa prawdziwego portu COM.
Zamiast tego generuje sztuczne dane testowe.

To jest najlepszy sposób, żeby sprawdzić działanie programu bez urządzenia.

## 9. Co zrobić, jeśli masz prawdziwy BMS

Jeśli masz prawdziwy sprzęt, zrób tak:

1. podłącz BMS do komputera,
2. sprawdź w Menedżerze urządzeń, jaki ma numer portu, na przykład `COM3` albo `COM5`,
3. uruchom serwer:

```powershell
.\run_server_stack.bat
```

4. otwórz Web GUI,
5. przejdź do zakładki `Cell Settings`,
6. wybierz właściwy port COM,
7. kliknij `Save COM Port`,
8. kliknij `Start UART`

## 10. Jak ręcznie uruchomić UART sender

Możesz też uruchomić sender ręcznie z terminala.

Bez podawania portu:

```powershell
.\run_uart_sender.bat
```

Z konkretnym portem:

```powershell
.\run_uart_sender.bat COM3
```

Albo w trybie symulacji:

```powershell
.\run_uart_sender.bat SIMULATED
```

## 11. Desktop viewer

Desktop viewer to osobny program w JavaFX.
Wygląda jak Web GUI, ale działa jako osobne okno na komputerze.

Najpierw uruchom serwer:

```powershell
.\run_server_stack.bat
```

Potem uruchom desktop viewer:

```powershell
.\run_desktop_web_gui.bat
```

Ważne:

- najpierw serwer,
- dopiero potem desktop viewer

## 12. Raspberry Pi przez SSH do Java na Windows

Ten wariant jest dla układu:

```text
bateria -> BMS -> RPi -> program C -> SSH -> Windows Bash -> Java
```

Program C znajduje się w folderze `rpi`.

W tym trybie Web GUI tylko pokazuje dane. Nie wybierasz w przeglądarce portu COM i nie klikasz `Start UART`, bo BMS czyta Raspberry Pi przez `/dev/ttyUSB0`.

Na Raspberry Pi:

```bash
cd rpi
make
./tinybms_rpi --device /dev/ttyUSB0 --module 1 --no-print --bms-line read
```

Jeśli używasz czujnika temperatury DS18B20, sprawdź go osobno:

```bash
cd rpi
python3 temperatureSensor.py
```

Powinien wypisać jedną liczbę, np. `24.5625`. Jeśli nie chcesz teraz używać temperatury albo czujnik nie jest gotowy, wyłącz ją przy starcie całego potoku:

```bash
TEMPERATURE_COMMAND=
```

Jeśli jednorazowy odczyt działa, wróć do katalogu głównego projektu i uruchom ciągłe wysyłanie do PC.

Jeśli laptop udostępnia hotspot Windows, Raspberry Pi dostanie adres z sieci hotspotu. Na RPi możesz sprawdzić adres laptopa:

```bash
ip route
```

Adres po `default via` to zwykle adres laptopa. W komendach poniżej podstaw go jako `LAPTOP_IP`.

Przykład dla projektu sklonowanego na laptopie:

```bash
PC_USER=WINDOWS_USER \
PC_HOST=LAPTOP_IP \
TEMPERATURE_COMMAND= \
REMOTE_COMMAND='cd /d "C:\path\to\Bms-C-Java-Rpi" && bash -lc "bash scripts/windows_receive_from_ssh.sh"' \
./rpi/stream_to_windows_java.sh
```

Na Windows skrypt `scripts/windows_receive_from_ssh.sh` czyta dane ze standardowego wejścia i uruchamia `DataManager.java`, który wysyła dane do backendu API.

### Testy RPi -> laptop

Najpierw uruchom na laptopie:

```powershell
.\run_server_stack.bat
```

Backend powinien wypisać:

```text
[BmsApiServer] Listening on 0.0.0.0:8090
```

Sprawdź z RPi, czy laptop odpowiada:

```bash
ssh WINDOWS_USER@LAPTOP_IP 'curl.exe http://127.0.0.1:8090/api/health'
```

Wyślij ręcznie jedną linię testową:

```bash
printf "BMS,1,52.100,0.000,75000000,151,3260,3261,3262,3264\n" | ssh WINDOWS_USER@LAPTOP_IP 'curl.exe -X POST http://127.0.0.1:8090/api/ingest --data-binary @-'
```

Poprawna odpowiedź:

```json
{"accepted":1,"heartbeat":0,"rejected":0}
```

Potem sprawdź ostatnie dane:

```bash
ssh WINDOWS_USER@LAPTOP_IP 'curl.exe http://127.0.0.1:8090/api/latest'
```

`/api/ingest` służy do wysyłania danych przez `POST`. `/api/latest` służy do oglądania danych przez zwykły `GET`, bez `-X POST`.

Pełna instrukcja jest tutaj:

```text
docs/RPI_SSH_PIPELINE.md
```

## 13. Najczęstsze problemy

### Problem 1. `dbConnected:false`

To zwykle znaczy:

- MySQL nie działa,
- backend uruchomił się za wcześnie,
- dane bazy w `.env` są złe

Najprostsza naprawa:

```powershell
.\stop_all.bat
.\run_server_stack.bat
```

### Problem 2. `Failed to open port COMx`

To zwykle znaczy:

- wybrałeś zły port,
- port jest zajęty przez inny program,
- urządzenie nie jest podłączone

Jeśli nie masz sprzętu, użyj:

```text
SIMULATED
```

### Problem 3. Web GUI działa, ale nie ma danych

To zwykle znaczy:

- UART nie został uruchomiony,
- nie kliknąłeś `Start UART`,
- wybrany port COM jest zły,
- nie uruchomiłeś trybu `SIMULATED`

W trybie Raspberry Pi przez SSH nie klikaj `Start UART` w GUI. Zamiast tego sprawdź:

```powershell
curl.exe http://127.0.0.1:8090/api/latest
```

Jeśli zwraca `[]`, backend nie dostał danych. Najpierw wyślij test przez `/api/ingest`.

### Problem 4. PowerShell nie uruchamia `.bat`

Pisz tak:

```powershell
.\run_server_stack.bat
```

Nie tak:

```powershell
run_server_stack.bat
```

### Problem 5. `Connection refused` w `DataManager`

To znaczy, że `DataManager` nie może połączyć się z backendem API. Sprawdź na laptopie:

```powershell
curl.exe http://127.0.0.1:8090/api/health
```

Jeśli działa lokalnie, ale nie przez SSH, sprawdź czy skrypt pokazuje:

```text
[windows_receive_from_ssh] java command: java.exe
```

Jeśli pokazuje `java`, a nie `java.exe`, uruchomiła się linuxowa Java z WSL i może mieć inną sieć.

### Problem 6. `Connect timed out`

Najczęściej firewall Windows blokuje port `8090` albo backend nie nasłuchuje na zewnątrz. Backend powinien logować:

```text
[BmsApiServer] Listening on 0.0.0.0:8090
```

Jeśli trzeba, odblokuj port w PowerShell jako administrator:

```powershell
New-NetFirewallRule -Name BMSApi8090 -DisplayName "BMS API 8090" -Enabled True -Direction Inbound -Protocol TCP -Action Allow -LocalPort 8090
```

### Problem 7. `UnsupportedClassVersionError`

To znaczy, że `javac` i `java` są z różnych wersji. Sprawdź na laptopie:

```cmd
where java
where javac
java -version
javac -version
```

Pierwsza linia `where java` powinna wskazywać aktualny JDK, na przykład:

```text
C:\Program Files\Java\jdk-25\bin\java.exe
```

Jeśli pierwsza jest Java 8, przestaw `Path`, aby `C:\Program Files\Java\jdk-25\bin` było wyżej niż `C:\Program Files (x86)\Common Files\Oracle\Java\java8path`.

### Problem 8. `No module named w1thermsensor`

To znaczy, że Python na Raspberry Pi nie widzi biblioteki do czujnika DS18B20. Jeśli używasz aktywnego środowiska `venv`, zainstaluj ją w tym środowisku:

```bash
python3 -m pip install w1thermsensor
```

Jeśli używasz systemowego Pythona:

```bash
sudo apt update
sudo apt install python3-w1thermsensor
```

Sprawdź:

```bash
python3 -c "from w1thermsensor import W1ThermSensor; print('ok')"
python3 rpi/temperatureSensor.py
```

Dla DS18B20 musi być też włączone 1-Wire. W Raspberry Pi OS zwykle w pliku `/boot/firmware/config.txt` albo `/boot/config.txt` dodaj:

```text
dtoverlay=w1-gpio
```

Potem zrestartuj RPi:

```bash
sudo reboot
```

### Problem 9. `Temperature command produced no output`

Program C uruchomił komendę temperatury, ale skrypt nic nie wypisał. Najpierw testuj temperaturę osobno:

```bash
cd rpi
python3 temperatureSensor.py
```

Jeśli chcesz uruchomić cały BMS bez temperatury, dodaj puste `TEMPERATURE_COMMAND`:

```bash
TEMPERATURE_COMMAND= \
PC_USER=WINDOWS_USER \
PC_HOST=LAPTOP_IP \
REMOTE_COMMAND='cd /d "C:\path\to\Bms-C-Java-Rpi" && bash -lc "bash scripts/windows_receive_from_ssh.sh"' \
./rpi/stream_to_windows_java.sh
```

### Problem 10. `Read timed out` w `DataManager`

To znaczy, że Java połączyła się z backendem, ale odpowiedź nie wróciła na czas. Sprawdź, czy backend nie jest zawieszony:

```powershell
curl.exe http://127.0.0.1:8090/api/health
curl.exe http://127.0.0.1:8090/api/latest
```

Jeśli backend odpowiada wolno albo wcale, zrestartuj go:

```powershell
.\stop_all.bat
.\run_server_stack.bat
```

Jeśli używasz MySQL i API przycina się przy zapisie, sprawdź też czy XAMPP/MySQL działa poprawnie. Na próbę możesz wysłać ręcznie jedną linię przez `/api/ingest` i sprawdzić, czy zwraca `accepted:1`.

## 14. Najważniejsze pliki

- serwer: `run_server_stack.bat`
- backend + Web GUI: `run_full_stack.bat`
- UART sender: `run_uart_sender.bat`
- desktop viewer: `run_desktop_web_gui.bat`
- RPi program C: `rpi/tinybms_rpi.c`
- RPi wysyłka SSH: `rpi/stream_to_windows_java.sh`
- Windows odbiór SSH do Java: `scripts/windows_receive_from_ssh.sh`
- instrukcja RPi/SSH: `docs/RPI_SSH_PIPELINE.md`
- zatrzymanie: `stop_all.bat`
- konfiguracja: `.env.example`
