# Download on Telegram:
## [🗲 Telegram](https://t.me/k5robby69)

# Robzyl K1 Firmware Documentation
**USB is deactivated by default**: press any key but PTT when switch ON to enable USB**

**CHIRP Driver included**: `Chirp_Robzyl_K1.py`  
This driver also allows 
* **customization of the 24 bands on channels 1000 to 1024.**
* **Scanlist Monitor setting**

**Main Menu** accessible in MR/VFO mode:

## Keyboard Shortcuts

| Key | Short Press | Long Press | F+ |
|--------|-------|------|----|
| **MENU** | Menu | - | - |
| **EXIT** | Exit | - | - |
| **< >** | ↑↓ Keys | - | Squelch ↑↓ |
| **1** | - | Band Plan | Band Plan |
| **2** | - | VFO A/B | - |
| **3** | - | VFO/MR | - |
| **4** | - | Frequency Copy | Frequency Copy |
| **5** | - | **Robzyl Spectrum** | - |
| **6** | - | Power ↑↓ | Power ↑↓ |
| **7** | - | VOX N/A | - |
| **8** | Rev Freq | Backlight Max/Min | - |
| **9** | - | Favorite Channel | Backlight Max/Min |
| **\*** | - | Scan VFO/MR | CTCSS/DCS Scan |
| **F** | Function | Keypad Lock | - |

---

## Robzyl Spectrum

| Mode | Description |
|---------|-------------|
| **BAND** | 24 configurable bands |
| **SCAN-LIST** | 20 scan-lists based on your memory channels |
| **RANGE** | Spectrum within start/end frequency limits |
| **FREQUENCY** | Spectrum centered on the VFO frequency |

### Practical Use

**Launch Spectrum**: `F+5` from VFO mode.

![Main View](https://github.com/user-attachments/assets/f28463e6-5542-4c1d-ace4-b2ab3a44c4ac)

* **Top line**: DSxx (Dynamic Squelch), Modulation, Listen BW, Step (or A+XXXX/AFC during listening).
* **Second line**: Current frequency and scan-list or band (depending on mode).
* **Bottom line**: Current span and center frequency peak.

**Key Mapping:**

| Key | Function |
|-----|----------|
| **1** | Activate backlight |
| **2** | Simplified screen (scanner) |
| **3** | Select listening bandwidth |
| **4** | Selection menu (single/multiple SL or BD) |
| **5** | Access Settings (1/4 to navigate, </> to change values) |
| **6** | Toggle BAND, SCAN-LIST, RANGE, FREQUENCY modes |
| **7** | Save main settings |
| **8** | Toggle BIG/CLASSIC frequency display |
| **9** | Select modulation |
| **0** | Access reception history |
| **M** | Enter Still Mode (monitoring and register access) |
| **PTT** | Skip current frequency |
| **SIDE KEY 1** | Toggle Normal -> FL (Freq Lock) -> M (Monitor) |
| **SIDE KEY 2** | Blacklist current frequency |
| ***/F** | Adjust Dynamic Squelch (Uxx) |
| **< >** | Navigate SL, bands, or frequency |

---

## Settings Menu

![Settings Menu](https://github.com/user-attachments/assets/be9c4ea0-72ca-4fc9-85d7-323c3553bb9f)

* **RSSI Delay**: RSSI capture time in ms.
* **SpectrumDelay**: Hang time after signal drops below squelch.
* **Max Listen Time**: Maximum listening time for a received signal.
* **Fstart/Fstop**: Set start/stop frequencies (RG mode).
* **Step**: Set frequency stepping.
* **ListenBW**: Set listening bandwidth.
* **Modulation**: FM/AM/USB.
* **RX_Backlight_ON**: Enable backlight during spectrum reception.
* **PowerSave**: Increases LCD refresh delay.
* **Noislvl_OFF**: Noise floor adjustment to avoid false triggers.
* **Popups**: Message display duration.
* **Record Trig**: Save history when Dynamic Squelch is OFF.
* **Key Unlocked**: Auto keypad lock.
* **GlitchMax**: Noise rejection level.
* **SoundBoostON**: Higher volume (risk of distortion).
* **Clear History**: Erase EEPROM history.
* **Reset Default**: Reset spectrum parameters and registers.

---

## Operating Modes

### 1. Simplified View
![Simplified View](https://github.com/user-attachments/assets/3594c6c6-728a-4ffc-88ac-5495ab352456)
Summary: Ambient Temp, RSSI level, Frequency, and Channel/Band name.

### 2. Still Mode (Monitoring)
![Still Mode](https://github.com/user-attachments/assets/a51c16cc-7d03-49ff-99f2-9cfc6ff8f52b)
Launched with **M** during active listening. Advanced users can modify registers here.
* **< >**: Change frequency by step.
* ***/F**: Change step size.

### 3. Frequency History
![Frequency History](https://github.com/user-attachments/assets/b0ca73b5-b64a-4b85-bcc2-ee6ffac59bb0)
Dynamic list of received signals. Navigate to listen (auto-engages Freq Lock).
* **SK1**: Toggle FL / Monitor.
* **< >**: Navigate history (FL mode).
* **2**: Save entry to first available memory slot.
* **5**: Scan history entries.
* **7**: Save history to EEPROM.

### 4. ScanLists (SL Mode)
![ScanList Selection](https://github.com/user-attachments/assets/722c1eeb-30ce-4433-9f66-1ccab36be85a)
* **Function**: Loads memories assigned to scanlists into the spectrum.
* **Menu [4]**: Select SLs with **^/v**. **[5]** for exclusive, **[4]** for multi-select.

### 5. Predefined Bands (BAND Mode)
![Band Selection](https://github.com/user-attachments/assets/75c924b3-139a-4d04-8fde-337045eba6d0)
* **Function**: Analyzes preset bands (PMR, CB, AERO, HAM, etc.).

---

## Chirp Configuration (Bands 1000-1024)
![Chirp Config](https://github.com/user-attachments/assets/dbc68936-3d7a-478b-a290-6e30d5f67f77)
Memory channels **1000 to 1024** store custom band settings. For each, define:
* Start and End frequency (via offset).
* Band Name.
* Modulation & Frequency Step.

## [🗲 Youtube Channel](https://www.youtube.com/@robby_69400)

### 🙏 Many thanks to Zylka, Kolyan, Iggy, Toni, Yves and Francois
