# AtlasCube

*[English](README.md) | Polski*

[![Build firmware](https://github.com/marcinozog/AtlasCube/actions/workflows/build.yml/badge.svg)](https://github.com/marcinozog/AtlasCube/actions/workflows/build.yml)

Hobbystyczne radio internetowe i inteligentny zegar na uniwersalnej płytce (tymczasowo) z ESP32-S3 (AtlasCube). Gra radio z sieci, pokazuje godzinę, pilnuje przypomnień i ma web UI do konfiguracji. Wszystko działa lokalnie — żadnej chmury.

🌐 **[atlascube.net](https://atlascube.net)** — strona projektu

<table>
  <tr>
    <td>➡️ <b><a href="https://atlascube.net/demo">Demo web UI</a></b></td>
    <td>Interaktywny podgląd interfejsu w przeglądarce — poklikasz bez sprzętu</td>
  </tr>
  <tr>
    <td>⚡ <b><a href="https://atlascube.net/flash">Flash z przeglądarki</a></b></td>
    <td>Gotowy firmware przez USB z przeglądarki Chromium — bez ESP-IDF, bez esptool, bez konsoli</td>
  </tr>
  <tr>
    <td>🔧 <b><a href="#build">Zbuduj ze źródeł</a></b></td>
    <td>Inny wyświetlacz albo własny układ pinów? Zbuduj pod własny sprzęt jedną komendą</td>
  </tr>
  <tr>
    <td>📱 <b><a href="https://github.com/marcinozog/AtlasCube-Remote/">Aplikacja Android</a></b></td>
    <td>Pilot na telefon — odtwarzanie, EQ, wydarzenia, edytor layoutu (beta)</td>
  </tr>
</table>

---

## Zrzuty ekranu

### Ekrany urządzenia

<table>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/scr_clock_d.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_clock_d.jpg" width="200"></a><br><sub>Zegar — ciemny</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_clock_l.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_clock_l.jpg" width="200"></a><br><sub>Zegar — jasny</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_radio_d.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_radio_d.jpg" width="200"></a><br><sub>Radio — ciemny</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_radio_l.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_radio_l.jpg" width="200"></a><br><sub>Radio — jasny</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/scr_clock_d_2.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_clock_d_2.jpg" width="200"></a><br><sub>Zegar — ciemny</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_clock_l_2.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_clock_l_2.jpg" width="200"></a><br><sub>Zegar — jasny</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_radio_d_2.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_radio_d_2.jpg" width="200"></a><br><sub>Radio — ciemny</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_radio_l_2.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_radio_l_2.jpg" width="200"></a><br><sub>Radio — jasny</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/scr_playlist_d.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_playlist_d.jpg" width="200"></a><br><sub>Playlista — ciemny</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_playlist_l.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_playlist_l.jpg" width="200"></a><br><sub>Playlista — jasny</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_settings_d.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_settings_d.jpg" width="200"></a><br><sub>Ustawienia — ciemny</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_settings_l.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_settings_l.jpg" width="200"></a><br><sub>Ustawienia — jasny</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/scr_event_noti_d.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_event_noti_d.jpg" width="200"></a><br><sub>Wydarzenie — ciemny</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_event_noti_l.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_event_noti_l.jpg" width="200"></a><br><sub>Wydarzenie — jasny</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_eq_l.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_eq_l.jpg" width="200"></a><br><sub>Korektor</sub></td>
    <td align="center"><a href="https://atlascube.net/images/scr_bt_d.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/scr_bt_d.jpg" width="200"></a><br><sub>Bluetooth</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/diagram_ili9341.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/diagram_ili9341.jpg" width="200"></a><br><sub>Schemat z ILI9341</sub></td>
    <td align="center"><a href="https://atlascube.net/images/diagram_co5300.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/diagram_co5300.jpg" width="200"></a><br><sub>Schemat z CO5300</sub></td>
    <td align="center"><a href="https://atlascube.net/images/diagram_ssd1322.jpg" target="_blank" rel="noopener"><img src="https://atlascube.net/images/diagram_ssd1322.jpg" width="200"></a><br><sub>Schemat z SSD1322</sub></td>
  </tr>
</table>

### Web UI

<table>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_index.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_index.png" width="400"></a><br><sub>Radio</sub></td>
    <td align="center"><a href="https://atlascube.net/images/www_bt.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_bt.png" width="400"></a><br><sub>Bluetooth</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_layouts.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_layouts.png" width="400"></a><br><sub>Edytor layoutów</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_settings_disp.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_settings_disp.png" width="400"></a><br><sub>Wyświetlacz</sub></td>
    <td align="center"><a href="https://atlascube.net/images/www_settings_mqtt.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_settings_mqtt.png" width="400"></a><br><sub>MQTT</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_settings_ss.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_settings_ss.png" width="400"></a><br><sub>Wygaszacze</sub></td>
    <td align="center"><a href="https://atlascube.net/images/www_settings_theme.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_settings_theme.png" width="400"></a><br><sub>Motyw</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_settings_tools.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_settings_tools.png" width="400"></a><br><sub>Narzędzia</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_editor.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_editor.png" width="400"></a><br><sub>Edytor</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_playlist.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_playlist.png" width="400"></a><br><sub>Playlista</sub></td>
    <td align="center"><a href="https://atlascube.net/images/www_events.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_events.png" width="400"></a><br><sub>Wydarzenia</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="https://atlascube.net/images/www_eq.png" target="_blank" rel="noopener"><img src="https://atlascube.net/images/www_eq.png" width="400"></a><br><sub>Korektor</sub></td>
    <td align="center"></td>
  </tr>
</table>

---

## Co potrafi

**Audio**
- Strumieniowe radio internetowe — MP3, AAC, FLAC (przez [esp-adf](https://github.com/espressif/esp-adf))
- Metadane ICY — nazwa stacji i aktualny utwór widoczne na ekranie oraz w web UI
- 10-pasmowy EQ parametryczny + miękka regulacja głośności (własny element DSP, rdzeń 1)
- Playlista — do 50 stacji, trzymana w SPIFFS
- Bluetooth audio — A2DP sink i HFP hands-free (zewnętrzny moduł QCC5125, Bluetooth 5.1); kodeki: LDAC, aptX HD, aptX LL, aptX, SBC, AAC
- Odtwarzacz muzyki z karty SD — granie plików MP3 / WAV / FLAC / AAC wprost z folderu na microSD. Przeglądanie podfolderów, kolejka z losowaniem i powtarzaniem (brak / cała / utwór), pauza/wznowienie i auto-przejście — wszystko ze strony `/sd-player.html`. Trzecie źródło dźwięku obok radia i Bluetooth (jedno aktywne naraz); współdzieli EQ i głośność z wyjściem radia
- Sprzętowe przełączanie źródła I2S — multiplekser 74HC157D wpina do DAC-a albo ESP32-S3, albo QCC5125, jednym GPIO
- Automatyczne wznawianie po utracie strumienia
- Wznawianie po restarcie — opcjonalnie odtwarza ostatnią stację po ponownym uruchomieniu, jeśli radio grało w chwili wyłączenia (opcja włączana w ustawieniach przez Web UI)

**UI**
- GUI na LVGL — obsługuje ILI9341 320×240 (SPI), ST7796U 480×320 (SPI), ILI9488 480×320 (SPI, 18-bit), CO5300 240×296 (okrągły AMOLED, QSPI) i SSD1322 256×64 (mono OLED, SPI); wybór jednym `#define`
- Ekrany: zegar, playlista, korektor, ustawienia, Bluetooth, wydarzenia, WiFi AP
- Nawigacja enkoderem (obrót + klik)
- Dotyk pojemnościowy — CST816D (okrągły AMOLED) albo FT6336U (ST7796U), oba po I2C; działa równolegle z enkoderem
- Gesty swipe — w poziomie przeskakują między zegar ↔ radio ↔ bt, w górę otwierają ustawienia (z zegara) albo playlistę (z radia); rozpoznawane przez LVGL na zwykłym indevie wskaźnika, bez kombinowania per kontroler
- Nakładka sterująca — tap w ekran mediów otwiera krzyż 5 przycisków (play/pauza, vol±, prev/next), znika po chwili bezczynności
- Konfigurowalny layout (pozycje widgetów w JSON-ie)
- Wygaszacze — startują po zadanym czasie bezczynności; do wyboru wskazówki zegara, pole gwiazd, fajerwerki, plazma, gra w życie Conwaya, czarny ekran (przyjazny AMOLED-om), **Dashboard** albo **Fotoramka** (niżej)

**Wygaszacz Dashboard**
- Ambientowy ekran, który odpytuje dowolny endpoint JSON po HTTP/HTTPS i pokazuje jedną wartość
- Konfiguracja w web UI w ustawieniach: **tytuł**, **URL**, **ścieżka JSON** (kropki i nawiasy, np. `rates[0].mid` albo `main.temp`), **suffix** (np. ` PLN`, `°C`) i **interwał** (≥ 5 s)
- HTTPS działa od ręki — ESP-IDF dostarcza pakiet certyfikatów, więc publiczne API bez auth łyka się bez kombinacji
- Domyślnie ustawione na kurs USD/PLN z NBP; po podmianie pól pokaże praktycznie cokolwiek z JSON-a (pogoda, krypto, smart home, statystyki GitHuba…)
- Odpytywanie chodzi w osobnym tasku FreeRTOS tylko gdy wygaszacz jest na ekranie — gdy widać inny ekran, ruchu w tle nie ma

**Wygaszacz Fotoramka**
- Zamienia urządzenie w cyfrową ramkę na zdjęcia — przewija obrazy z karty microSD
- Zdjęcia są wcześniej konwertowane do binarnego formatu RGB565 LVGL w rozmiarze panelu (przez aplikację na Androida albo skrypt [`scripts/img2lvgl.py`](scripts/img2lvgl.py)) i wgrywane na kartę — na urządzeniu **nie ma dekodera JPG/PNG**, więc nawet duże zdjęcia nie kosztują dodatkowego RAM-u firmware'u
- Konfiguracja z web UI albo z aplikacji na Androida: **folder źródłowy**, **kolejność** (po kolei / losowo), **czas na slajd**, **efekt** i **prędkość** odsłaniania
- Wolne wczytywanie z SD jest tu zamienione w przejście: każde nowe zdjęcie **„wywołuje się" na poprzednim** wybranym efektem — **od góry**, **od boku**, **dissolve**, **interlaced** (retro: blokowy podgląd → ostrość) albo **losowo na slajd**
- Renderuje do dwóch pełnoekranowych buforów w PSRAM i odświeża tylko zmieniony fragment w każdym ticku, więc jest lekkie nawet przy grającym radiu
- Zmiany ustawień działają na żywo — zmiana efektu/kolejności/czasu aktualizuje trwający pokaz w ciągu jednego slajdu, bez wychodzenia z wygaszacza
- Slajdami zarządzasz skąd chcesz: przeglądanie / upload / zmiana nazwy / usuwanie przez **menedżer plików SD** w web UI (Ustawienia → Narzędzia) albo aplikację na Androida, która od razu konwertuje i wysyła zdjęcia z telefonu
- Wymaga karty microSD podpiętej do pinów SDMMC danego wariantu (tryb 1-bit)

**Wydarzenia i przypomnienia**
- Urodziny, imieniny, rocznice, zwykłe przypomnienia, alarmy (z radiem), powiadomienia głosowe
- Cykliczne: dziennie / co tydzień / miesiąc / rok
- W momencie wyzwolenia: pełnoekranowe powiadomienie i melodia z buzzera; melodie zaszyte w firmware (webowy edytor w planach)
- Tryb alarm — zamiast buzzera włącza wybraną stację z playlisty; strumień gra dalej po zamknięciu powiadomienia, aż ręcznie nie zatrzymasz radia
- Tryb powiadomienia głosowego — w momencie wyzwolenia odtwarza nagranie z karty microSD, na chwilę przerywając i potem przywracając źródło (radio/Bluetooth). Aplikacja na Androida syntezuje mowę na telefonie (TTS) i wgrywa ją; każde nagranie to pojedynczy plik nazwany czytelnym slugiem tytułu wydarzenia (np. `voice/pobudka-a3f9c1.wav`), dzięki czemu karta pozostaje przejrzysta w menedżerze plików. Edytor webowy i aplikacja pozwalają odsłuchać nagranie
- CRUD przez web UI

**Łączność**
- WiFi STA z fallbackiem do AP (pierwsze uruchomienie pod 192.168.4.1)
- Serwer HTTP + WebSocket do synchronizacji stanu na żywo
- mDNS — w trybie STA dostępne pod `<nazwa>.local` (domyślnie `atlascube-xxxx` z adresu MAC, edytowalne w Ustawienia → WiFi); ogłasza usługę `_http._tcp` z rekordem TXT niosącym nazwę `.local` dla klientów discovery (np. NsdManager w aplikacji Android)
- NTP z konfigurowalną strefą czasową
- Web UI z SPIFFS (po wgraniu internet nie jest potrzebny)
- Klient MQTT — zdalne sterowanie radiem (play/stop/głośność/stacja) plus do 6 konfigurowalnych widgetów (toggle / slider / label) na osobnym ekranie, do sterowania zewnętrznym sprzętem MQTT (Tasmota, zigbee2mqtt, Home Assistant…); szczegóły w [MQTT](#mqtt) niżej
- Aktualizacja OTA — nowy obraz firmware wgrywasz prosto z Web UI (Ustawienia → Tools); trafia do nieaktywnego slotu, jest walidowany i urządzenie restartuje się do niego, z rollbackiem bootloadera gdy nowy obraz nie wstanie. Przycisk eksportu pobiera najpierw bieżący firmware. Web UI i ustawienia leżą na osobnych partycjach flasha, więc aktualizacja OTA aplikacji nigdy ich nie nadpisuje. Szczegóły w [Aktualizacje OTA](#aktualizacje-ota) niżej

**Pamięć**
- Opcjonalna karta microSD po SDMMC (tryb 1-bit), podpięta do pinów SDMMC danego wariantu
- Webowy **menedżer plików SD** (Ustawienia → Narzędzia) — przeglądanie folderów, tworzenie katalogów, upload, zmiana nazwy i usuwanie plików prosto z przeglądarki; pliki może wysyłać też aplikacja Android
- Webowy **backup/restore SPIFFS ⇄ SD** (Ustawienia → Narzędzia) — osobny dwupanelowy menedżer kopiujący pliki między SPIFFS urządzenia a kartą SD: kopia konfiguracji / web UI na kartę i przywrócenie później. Po stronie klienta, tylko kopiowanie
- Obsługuje slajdy fotoramki, nagrania powiadomień głosowych oraz lokalną muzykę dla odtwarzacza SD; więcej zawartości z karty (np. logo stacji) jest na liście planów

**Aplikacja Android** *(beta)*
- Pilot do odtwarzania, zmiany stacji i głośności
- Wzorowana na YoRadio Remote, plus rzeczy specyficzne dla AtlasCube: wydarzenia, korektor, edytor layoutu
- Osobne repozytorium z własnym README: [AtlasCube-Remote](https://github.com/marcinozog/AtlasCube-Remote/)

---

## Sprzęt

| Komponent | Szczegóły |
|---|---|
| MCU | ESP32-S3, 240 MHz, dwurdzeniowy |
| Płytka | Atlas Hub (autorska) |
| Flash | 16 MB |
| PSRAM | OctoSPI, 80 MHz |
| Wyświetlacz | ILI9341 320×240 (SPI), ST7796U 480×320 (SPI), ILI9488 480×320 (SPI, 18-bit), CO5300 240×296 AMOLED (QSPI) albo SSD1322 256×64 mono OLED (SPI) — wybór przy kompilacji |
| Dotyk | Kontroler pojemnościowy CST816D albo FT6336U (I2C) — gesty po stronie LVGL na zwykłym indevie wskaźnika |
| Wejście | Enkoder z przyciskiem + dotyk (swipe i tap nakładki sterującej) |
| I2S mux | 74HC157D — sprzętowy przełącznik między wyjściem I2S z ESP32-S3 a QCC5125; sterowany GPIO |
| Wyjście audio | DAC / wzmacniacz I2S (za 74HC157D) |
| Bluetooth | Moduł QCC5125, Bluetooth 5.1, A2DP + HFP |
| Mikrofon | Wbudowany, do HFP hands-free |
| Buzzer | Generator tonów na LEDC PWM |

---

## Szybki start — gotowy firmware

Żeby wgrać AtlasCube nie trzeba ESP-IDF ani toolchaina. Otagowane wydania publikują gotowe obrazy, a instalator masz w przeglądarce.

### Najprościej: flash z przeglądarki

Wchodzisz na **[atlascube.net/flash](https://atlascube.net/flash/)** w Chrome / Edge / Operze / Bravie, wybierasz wariant wyświetlacza, podpinasz urządzenie po USB, klikasz Install. Zero CLI, zero instalek. (Firefox i Safari WebSeriala nie ogarniają.)

> Pierwszy flash: przytrzymaj **BOOT** przy podpinaniu USB i puść — ESP32-S3 wejdzie w tryb download. Jest to konieczne, bo działający firmware sam obsługuje natywne USB-CDC i ignoruje auto-reset.

### Albo: z konsoli przez esptool

**1. Wybierz wariant:**

| Plik | Wyświetlacz | Dotyk |
|---|---|---|
| `AtlasCube-ili9341.bin` | ILI9341 320×240 (SPI) | FT6336U |
| `AtlasCube-st7796.bin`  | ST7796U 480×320 (SPI) | FT6336U |
| `AtlasCube-ili9488.bin` | ILI9488 480×320 (SPI, 18-bit) | FT6336U |
| `AtlasCube-co5300.bin`  | CO5300 240×296 (QSPI AMOLED) | CST816D |
| `AtlasCube-ssd1322.bin` | SSD1322 256×64 (mono OLED, SPI) | — (enkoder) |

**2. Pobierz** odpowiedni `.bin` z [najnowszego Release'a](https://github.com/marcinozog/AtlasCube/releases/latest).

**3. Wgraj** przez [esptool](https://github.com/espressif/esptool) (raz `pip install esptool`):

```bash
esptool.py --chip esp32s3 -p <PORT> write_flash 0x0 AtlasCube-<variant>.bin
```

`<PORT>` podmień na swój port (`/dev/ttyUSB0`, `COM3`, …).

**4. Pierwsze uruchomienie:** urządzenie wystawia AP o nazwie `AtlasCube-XXXXXX`. Łączysz się, wchodzisz na `192.168.4.1` i konfigurujesz Wi-Fi.

Tyle — bez ESP-IDF, bez ESP-ADF, bez patchy. Reszta README to build deweloperski, potrzebny tylko gdy chcesz w firmware grzebać.

---

## Build

**Wymagania**

- [ESP-IDF v5.5.4](https://github.com/espressif/esp-idf)
- [ESP-ADF v2.8](https://github.com/espressif/esp-adf)

**Build jedną komendą (zalecane)**

> Pełne przepisy krok po kroku: [docs/build-windows.md](docs/build-windows.md) (ESP-IDF Installation Manager) oraz [docs/build-linux.md](docs/build-linux.md) (`install.sh` + `export.sh`). Oba kończą się na tym samym `scripts/build.py`.

Zainstaluj [ESP-IDF v5.5.4](https://github.com/espressif/esp-idf) (na Windows oficjalny instalator to jedyny ręczny krok), otwórz środowisko ESP-IDF i z katalogu repo odpal:

```bash
python scripts/build.py co5300       # albo ili9341 / st7796 / ili9488 / ssd1322
python scripts/build.py              # interaktywne menu wariantu
```

`build.py` to wieloplatformowy punkt wejścia do setupu/release (Windows, Linux, CI). Klonuje ESP-ADF v2.8 jeśli go nie ma, ustawia wariant w `defines.h`, aplikuje wszystkie patche ESP-ADF/ESP-IDF, kompresuje web UI, buduje i produkuje gotowy do wgrania `build/AtlasCube-<wariant>.bin`. Jest idempotentny — można puszczać wielokrotnie. Przydatne flagi: `--skip-build` (tylko setup), `--adf-path <ścieżka>`. Do budowania i wgrywania **własnej, skonfigurowanej płytki** używaj `scripts/build-flash.py` — przy pierwszym uruchomieniu sam robi ten setup, więc to jedyny skrypt, jakiego potrzebujesz (zob. *Budowanie i wgrywanie na urządzenie* niżej). `build.py` służy głównie do obrazu per-wariant i CI.

Robi tyle:

- Klonuje ESP-ADF v2.8 do `./esp-adf`, jeśli `ADF_PATH` nie jest ustawiony.
- Inicjalizuje submoduły ESP-ADF `components/esp-adf-libs` i `components/esp-sr` (czysty clone ich nie pobiera, bo to gotowe biblioteki binarne).
- Wrzuca definicję płytki AtlasCube do `esp-adf/components/audio_board/esp32_s3_atlascube/`.
- Patchuje `Kconfig.projbuild`, `CMakeLists.txt` i `component.mk` w `esp-adf/components/audio_board/`, żeby płytka się zarejestrowała.
- Aplikuje patch FreeRTOS (`idf_v5.5_freertos.patch`) na ESP-IDF — niezbędny dla `xTaskCreateRestrictedPinnedToCore`, bez tego task dekodera MP3 wywala się w runtime (`E AUDIO_THREAD: Not found right xTaskCreateRestrictedPinnedToCore`).

`sdkconfig.defaults` już zawiera `CONFIG_ESP32_S3_ATLASCUBE_BOARD=y`.

<details>
<summary>Kroki ręczne — co build.py automatyzuje, do podejrzenia / debugowania</summary>

Jakbyś chciał zrobić to z palca (albo debugujesz setup):

1. **Submoduły ESP-ADF:**
   ```bash
   git -C $ADF_PATH submodule update --init components/esp-adf-libs components/esp-sr
   ```
2. **Źródła płytki** — skopiuj albo zrób symlink:
   ```bat
   mklink /D %ADF_PATH%\components\audio_board\esp32_s3_atlascube <ścieżka-do-repo>\components\audio_board\esp32_s3_atlascube
   ```
3. **Rejestracja w `esp-adf/components/audio_board/Kconfig.projbuild`** (w środku choice'a `AUDIO_BOARD`):
   ```kconfig
   config ESP32_S3_ATLASCUBE_BOARD
       bool "ESP32-S3-AtlasCube"
   ```
4. **Rejestracja w `esp-adf/components/audio_board/CMakeLists.txt`** (przed `register_component()`):
   ```cmake
   if (CONFIG_ESP32_S3_ATLASCUBE_BOARD)
       message(STATUS "Current board name is " CONFIG_ESP32_S3_ATLASCUBE_BOARD)
       list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32_s3_atlascube)
       set(COMPONENT_SRCS
           ./esp32_s3_atlascube/board.c
           ./esp32_s3_atlascube/board_pins_config.c
       )
   endif()
   ```
5. **Rejestracja w `esp-adf/components/audio_board/component.mk`** (stary build na GNU Make):
   ```makefile
   ifdef CONFIG_ESP32_S3_ATLASCUBE_BOARD
   COMPONENT_ADD_INCLUDEDIRS += ./esp32_s3_atlascube
   COMPONENT_SRCDIRS += ./esp32_s3_atlascube
   endif
   ```
6. **Patch FreeRTOS na ESP-IDF:**
   ```bash
   git -C $IDF_PATH apply --ignore-whitespace $ADF_PATH/idf_patches/idf_v5.5_freertos.patch
   ```

</details>

**Wybór wariantu sprzętowego**

Aktywny wariant siedzi w [`main/include/defines.h`](main/include/defines.h) — trzy niezależne grupy `#define`: `DISPLAY_*`, `UI_PROFILE_*`, `TOUCH_*`. `scripts/build.py <wariant>` przełącza je za Ciebie; ręcznie — odkomentuj dokładnie jeden wpis w każdej grupie.

Po ręcznej zmianie wariantu puść `idf.py fullclean`, żeby `sdkconfig` wygenerował się od nowa dla nowej kombinacji (`build.py` robi to automatycznie).

**Build i flash ręcznie**

Gdy wariant i patche są na miejscu (`scripts/build.py --skip-build` robi sam setup), działa standardowy flow ESP-IDF:

```bash
idf.py build
idf.py flash
```

> **Edycja plików boardu przy iteracji zwykłym `idf.py`** (np. przycisk build
> wtyczki ESP-IDF w VS Code): `idf.py` buduje kopię wewnątrz Twojego klona
> ESP-ADF, więc zmiany w repo w `components/audio_board/esp32_s3_atlascube/` nie
> zadziałają, dopóki nie puścisz ponownie `scripts/build.py --skip-build`. Żeby
> edycje były na bieżąco, zastąp kopię w ADF junctionem (bez uprawnień admina):
>
> ```powershell
> $dest = "$env:ADF_PATH\components\audio_board\esp32_s3_atlascube"
> Remove-Item -Recurse -Force $dest
> New-Item -ItemType Junction -Path $dest -Target "<repo>\components\audio_board\esp32_s3_atlascube"
> ```
>
> `build.py` wykrywa istniejący symlink/junction i go nie nadpisuje.

**Budowanie i wgrywanie na urządzenie**

`scripts/build-flash.py` to skrypt all-in-one dla usera: ustaw sprzęt w `defines.h` i odpal — przy pierwszym uruchomieniu sam skonfiguruje ESP-ADF (bez osobnego kroku), skompresuje web UI, zbuduje i wgra na podłączoną płytkę, pytając ile urządzenia nadpisać:

```bash
python scripts/build-flash.py -p COM5
```

| Zakres | Wgrywa | Zachowuje |
|---|---|---|
| Sam firmware | slot aplikacji | web UI + ustawienia |
| Firmware + Web UI | aplikacja + partycja `www` | ustawienia |
| Wszystko (factory) | aplikacja + `www` + `config` | — (resetuje ustawienia do domyślnych) |

Układ flasha dzieli dawną partycję storage na `www` (edytowalne web UI) i `config` (JSON ustawień), więc ponowne wgranie kodu albo UI nigdy nie kasuje ustawień — dopiero pełny flash (factory) seeduje domyślne. Flaga `--scope fw|ui|all` pomija pytanie (albo `--scope build` — tylko kompilacja, bez flashowania), `--monitor` otwiera monitor szeregowy po wgraniu.

Żeby poprawić web UI bez flashowania, edytuj pliki na żywo w przeglądarce (edytor plików na urządzeniu albo wbudowana strona setupu) — zapisują się prosto na partycję `www` po HTTP.

> Zmieniasz wariant sprzętowy w `defines.h`? `build-flash.py` wykrywa nieaktualny `sdkconfig` (stare definicje wyświetlacza/dotyku zostają, bo zmienione `sdkconfig.defaults` nie są ponownie aplikowane) i proponuje go usunąć dla czystego buildu. Flaga `--clean` wymusza to bez pytania.

**Pojedynczy scalony obraz (do dystrybucji)**

`build.py` tworzy go automatycznie jako `build/AtlasCube-<wariant>.bin`. Skleja bootloader, partition table, aplikację i oba obrazy SPIFFS (`www` + `config`) w jeden plik wgrywany od offsetu `0x0` przez `esptool` albo web flasher. Ręcznie:

```bash
python spiffs_image/tools/compress_web.py
idf.py build
idf.py merge-bin -o AtlasCube.bin
```

Flash:

```bash
esptool.py write_flash 0x0 AtlasCube.bin
```

### Własne czcionki

Czcionki leżą w [`components/ui/fonts/`](components/ui/fonts/) jako tablice C dla LVGL. Rozmiary nie są żadnym standardem — dobierane są pod panel. `_NN` w nazwie to `--size` (wysokość linii w px); duże pliki `_72/_80/_96` zawierają **tylko cyfry** (`--range 0x30-0x3A` plus jedna ikona), a pliki `_NN_pl` mają pełen polski zestaw znaków.

Aby dodać nową czcionkę (np. większą `montserrat_120`):

1. **Wygeneruj** plik `.c` w [lv_font_conv](https://lvgl.io/tools/fontconverter) (dokładne `Opts:` znajdziesz w nagłówku każdego pliku). Dla czcionki zegara tylko z cyframi:
   ```bash
   lv_font_conv --font Montserrat-Medium.ttf --range 0x30-0x3A \
     --font FontAwesome5-Solid+Brands+Regular.woff --range 0xF0F3 \
     --size 120 --bpp 4 --format lvgl --no-compress -o lv_font_montserrat_120.c
   ```
   Wrzuć plik do [`components/ui/fonts/`](components/ui/fonts/).
2. **Skompiluj** — dopisz nazwę pliku do listy źródeł w [`components/ui/CMakeLists.txt`](components/ui/CMakeLists.txt).
3. **Zadeklaruj** — dodaj `LV_FONT_DECLARE(lv_font_montserrat_120);` w [`ui_fonts.h`](components/ui/fonts/ui_fonts.h).
4. **Zarejestruj** — dopisz `{ "montserrat_120", &lv_font_montserrat_120 },` do tabeli w [`ui_fonts.c`](components/ui/fonts/ui_fonts.c). Id pojawi się wtedy automatycznie w listach czcionek w web UI i jest zapisywane w profilu UI.

Uwaga: glif jest niższy niż nominalny rozmiar (≈72 % wartości `--size` dla cyfr), więc żeby uzyskać cyfrę o wysokości `X` px wybierz `--size ≈ X / 0.72`. Mapowanie czcionek na pola ekranów opisuje [`docs/layout_editor.md`](docs/layout_editor.md#font-registry).

---

## Web UI

Dostępne pod IP urządzenia lub `<nazwa>.local` (tryb STA), albo pod `192.168.4.1` (tryb AP).

| Strona | Ścieżka |
|---|---|
| Radio / now playing | `/` |
| Odtwarzacz muzyki SD | `/sd-player.html` |
| Ustawienia | `/settings.html` |
| Playlista | `/playlist.html` |
| Wydarzenia | `/events.html` |
| Korektor | `/eq.html` |
| Edytor layoutu | `/layout.html` |
| Edytor plików SPIFFS | `/spiffs-editor.html` |
| Edytor plików SD | `/sd-editor.html` |
| Menedżer plików (SPIFFS / SD) | `/manager.html` |
| Widgety MQTT | `/mqtt.html` |

Endpoint WebSocket: `ws://<ip-urzadzenia>/ws` — wypycha zmiany stanu (głośność, utwór, stan radia) na żywo.

Wersja działającego firmware (z `git describe`) jest pokazywana w nagłówku web UI, na stronie konfiguracji Wi-Fi oraz — razem z adresem IP urządzenia — na **ekranie powitalnym (splash)** przez kilka sekund w trybie STA (przełączane w Ustawienia → Display). Szybki sposób na potwierdzenie, co się wgrało i jak dostać się do urządzenia.

---

## MQTT

W urządzeniu siedzi klient MQTT, który łączy się z lokalnym brokerem (np. Mosquitto) w sieci LAN. Konfiguracja w **Ustawienia → MQTT** w web UI: host, port, login/hasło, client ID, base topic. Po zapisaniu klient łączy się od nowa w locie — restart nie jest potrzebny.

- **Przełącznik kompilacji**: `CONFIG_MQTT_ENABLE` (menuconfig → *MQTT configuration*). Domyślnie `y`; ustaw `n` jeśli chcesz wyrzucić cały komponent z firmware.
- **Połączenie**: zwykłe TCP (tylko LAN, bez TLS), QoS 0, automatyczne wznawianie (ogarnia `esp-mqtt`).
- **Will / status online**: po połączeniu urządzenie wystawia `online` (retained) na `<base_topic>/status`, a broker dostarcza `offline` (LWT, retained) przy nagłym zerwaniu.
- **Format payloadu**: zwykły tekst na hierarchicznych topicach (styl Tasmota/HA) — łatwo z `mosquitto_pub`, łatwo do Home Assistanta przez `command_topic`/`state_topic` w YAML-u.

### Mapa topiców

Wszystkie topice radia mają prefix `<base_topic>/` (domyślnie `atlascube/`). `client_id` MQTT to osobna sprawa po stronie brokera i w nazwach topiców się nie pojawia.

| Suffix topiku | Kierunek | Payload | Uwagi |
|---|---|---|---|
| `cmd/play` | subscribe | cokolwiek | wznawia aktualnie wybraną stację |
| `cmd/stop` | subscribe | cokolwiek | |
| `cmd/next` / `cmd/prev` | subscribe | cokolwiek | przewija po playliście w kółko |
| `cmd/volume` | subscribe | `0`–`100` | przycinane do zakresu |
| `cmd/station` | subscribe | indeks stacji | liczone od 0 |
| `state/playing` | publish (retain) | `playing` \| `stopped` \| `buffering` \| `error` | |
| `state/volume` | publish (retain) | `0`–`100` | |
| `state/station_index` | publish (retain) | indeks stacji | |
| `state/station` | publish (retain) | nazwa stacji | z wpisu playlisty |
| `state/title` | publish (retain) | tytuł ICY | "" gdy zatrzymane |
| `status` | publish (retain) + LWT | `online` \| `offline` | LWT dorzuca `offline` jeśli urządzenie odpadnie |

### Ekran widgetów

Osobny ekran (swipe w prawo z zegara) mieści **do 6 widgetów** w siatce. Każde gniazdo konfigurujesz niezależnie z `/mqtt.html` (link z Ustawienia → MQTT). Ustaw *Type* na `None` żeby wyłączyć dany slot.

**Typy widgetów**

- **Toggle** — tap publikuje `ON`/`OFF` na cmd topiku; wygląd ciągnie ze state topiku, więc UI pozostaje zsynchronizowane, nawet jak coś przełączy HA, fizyczny przycisk czy automatyzacja.
- **Slider** — `min` / `max` / `step` do ustawienia; publikuje liczbę na cmd topiku i śledzi state topik.
- **Label** — tylko do podglądu; wyświetla ostatnią wartość ze state topiku, z opcjonalnym suffixem `unit` (`°C`, `%`, …).

**Wspólne pola**

- **Title** — krótki napis nad widgetem.
- **Command topic** — publikowany przy interakcji (toggle/slider). Przykład (Tasmota): `cmnd/livingroom/POWER`. Przykład (zigbee2mqtt łyka czysty tekst na `/set`): `zigbee2mqtt/<nazwa>/set`.
- **State topic** — subscribe przy połączeniu i ponownym łączeniu; steruje wartością widgetu.
- **JSON path** — jeśli ustawione, wyciąga pojedyncze pole z JSON-a (działa w obie strony):
  - *Przychodzący*: np. `state` wyłuska `"ON"` z payloadu zigbee2mqtt `{"state":"ON", ...}`.
  - *Wychodzący*: cmd jest opakowany jako `{"<path>": <value>}` zamiast surowego tekstu — przydatne dla urządzeń, które chcą JSON-a (zigbee2mqtt: `{"brightness":128}`).
  - Puste pole = czysty tekst w obie strony.
- Parser tekstu rozumie `ON`/`OFF`/`on`/`off`/`true`/`false`/`1`/`0` dla booli i gołe liczby dla sliderów.

### Przykłady

Podgląd wszystkiego, co urządzenie publikuje:

```bash
mosquitto_sub -h 192.168.1.10 -v -t 'atlascube/#'
```

Sterowanie radiem:

```bash
mosquitto_pub -h 192.168.1.10 -t atlascube/cmd/play
mosquitto_pub -h 192.168.1.10 -t atlascube/cmd/volume  -m 30
mosquitto_pub -h 192.168.1.10 -t atlascube/cmd/station -m 2
```

Minimalny YAML do Home Assistanta:

```yaml
mqtt:
  switch:
    - name: AtlasCube Radio
      command_topic: atlascube/cmd/play
      payload_off:   stopped       # do wyłączania użyj cmd/stop; albo rozbij na dwa switche
      state_topic:   atlascube/state/playing
      payload_on:    playing
  number:
    - name: AtlasCube Volume
      command_topic: atlascube/cmd/volume
      state_topic:   atlascube/state/volume
      min: 0
      max: 100
  sensor:
    - name: AtlasCube Title
      state_topic: atlascube/state/title
```

> MQTT Discovery (auto-rejestracja w HA) jeszcze nie działa — encje deklaruje się ręcznie, jak wyżej.

**Edytor plików**

`/spiffs-editor.html` to edytor w przeglądarce do plików web UI — HTML/CSS/JS i innych tekstowych zasobów. Listuje pliki z partycji `www`, edytuje z podświetlaniem składni i zapisuje z powrotem przez HTTP — bez reflashowania (HTML/CSS/JS są ponownie gzipowane na urządzeniu). Przydatne do dłubania w layoucie albo web UI na urządzeniu, które już działa u kogoś. JSON-y ustawień leżą na osobnej partycji `config` i edytuje się je przez własne ekrany (Ustawienia, Wydarzenia, MQTT, …).

---

## Aktualizacje OTA

Aktualizacja firmware przez Wi-Fi z **Ustawienia → Tools** — bez kabla USB, bez esptool. Strona pokazuje bieżącą wersję, przyjmuje obraz firmware, streamuje go do urządzenia i restartuje do nowego. Postęp jest pokazywany też na ekranie urządzenia.

**Układ dwuslotowy.** OTA korzysta z dwóch partycji aplikacji (`ota_0` / `ota_1`, zob. [`partitions16MB.csv`](partitions16MB.csv)), żeby zapisać nowy obraz do nieaktywnego slotu i przełączyć bootloader z rollbackiem. Gdy nie ma nieaktywnego slotu, endpoint zwraca `501`.

**Który plik:** wgrywasz **samą aplikację** `build/atlascube.bin` (~2,3 MB) — *nie* scalony `AtlasCube-<wariant>.bin`, który zawiera też bootloader, tablicę partycji oraz partycje `www`/`config` i służy do pełnego wgrania przez USB od `0x0`. Upewnij się, że obraz pasuje do Twojego wariantu wyświetlacza — wgranie binarki innego wariantu zepsuje UI.

**Przejście na layout OTA:** przestawienie istniejącego urządzenia 16 MB na układ partycji OTA to jednorazowy pełny reflash przez USB (zmiana tablicy partycji nie przejdzie przez samo OTA). Potem każda kolejna aktualizacja idzie już przez web.

**Bezpieczeństwo:**
- Pierwszy bajt jest sprawdzany pod kątem magic obrazu ESP (`0xE9`) zanim cokolwiek zostanie zapisane, a `esp_ota_end` waliduje cały obraz (suma kontrolna) przed przełączeniem partycji bootowania.
- Urządzenie zatrzymuje odtwarzanie na czas zapisu, żeby zwolnić RAM i uniknąć kontencji na flash/SPI.
- **Najpierw backup:** przycisk *Export running firmware* (`GET /api/ota/backup`) pobiera aktywny slot jako `atlascube-<wersja>.bin` — re-flashowalny snapshot, który można wgrać z powrotem, żeby ręcznie cofnąć aktualizację.

> OTA niesie **wyłącznie aplikację** — nie tyka partycji `www` (web UI) ani `config` (ustawienia), więc Twoje UI i ustawienia przeżywają aktualizację. Gdy nowy firmware niesie też nowe web UI, zaktualizuj je osobno: edytorem plików w przeglądarce (`/spiffs-editor.html`) albo wbudowaną stroną setupu, ewentualnie pełnym reflashem od `0x0`.

---

## Dokumentacja projektu

Notatki architektury i decyzji projektowych w [`docs/`](docs/):

- [`audio_pipeline.md`](docs/audio_pipeline.md) — pipeline streamingu, DSP, tuning TCP, przypisanie tasków do rdzeni
- [`events.md`](docs/events.md) — system przypomnień i wydarzeń
- [`layout_editor.md`](docs/layout_editor.md) — personalizacja layoutu UI
- [`navigation.md`](docs/navigation.md) — mapa ekranów: pierścień domowy, sterowanie (enkoder/dotyk), jak edytować
- [`display_drivers.md`](docs/display_drivers.md) — kruczki driverów wyświetlaczy (parzyste granice w QSPI AMOLED, mutex SPI, budżet bufora LVGL vs wewnętrzny DRAM)

---

## Roadmap

- **Obudowa** — drukowana 3D, w trakcie projektowania; firmware aktualnie chodzi na gołej płytce deweloperskiej
- **Więcej zawartości z karty SD** — karta microSD napędza już fotoramkę, nagrania powiadomień głosowych i odtwarzacz muzyki; rozszerzenie m.in. na logo stacji
- **Webowy edytor melodii** — narzędzie w przeglądarce do komponowania własnych melodii na buzzer

---

## Licencja

MIT
