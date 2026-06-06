# 🕋 Smart IoT Al-Amanah: Sistem Keamanan Kotak Amal Masjid Pintar Berbasis ESP32 & GPS Tracking

Proyek ini adalah sistem keamanan aktif dan pelacakan lokasi (*real-time tracking*) untuk kotak amal masjid pintar berbasis **Internet of Things (IoT)** menggunakan mikrokontroler **ESP32**. Sistem ini dirancang untuk mencegah tindakan pencurian dan pembongkaran paksa kotak amal dengan fitur manajemen daya portabel mandiri serta algoritma *Failsafe Offline Mode*.

---

## 🚀 Fitur Utama

- **Deteksi Guncangan Akurat:** Memanfaatkan Sensor Getar SW-420 dengan sistem *Hold Time* 2 detik untuk menghindari *false alarm*.
- **Deteksi Pembongkaran Pintu:** Memanfaatkan *Magnetic Door Sensor* (Saklar Buluh) untuk mendeteksi pembukaan laci/pintu kotak secara ilegal.
- **Pelacakan Lokasi (GPS Tracking):** Menggunakan modul GPS NEO-6M untuk mendapatkan titik koordinat bumi (*latitude* & *longitude*) secara presisi.
- **Notifikasi Blynk IoT:** Mengirimkan *push notification* darurat dan menampilkan visualisasi pemetaan posisi kotak amal langsung ke smartphone pengguna secara *real-time*.
- **Failsafe Offline Mode (Timeout 20 Detik):** Jika sirkuit kehilangan koneksi internet/WiFi saat dinyalakan, sistem tidak akan macet (*crash*). Setelah 20 detik, alat otomatis beralih ke Mode Offline sehingga alarm suara lokal (*buzzer*) dan layar LCD tetap bekerja 100% melindungi kotak amal.
- **Manajemen Daya Portabel:** Ditenagai baterai isi ulang 18650 yang dipantau langsung lewat modul *Mini Voltmeter* (angka voltase) dan *Lithium Battery Fuel Gauge Display* (bar LED fisik).
- **WiFi Smart Portal:** Menggunakan `WiFiManager` sehingga pengurus masjid dapat mengatur ulang atau mengganti nama WiFi/Hotspot dengan mudah melalui tombol reset fisik tanpa perlu memprogram ulang sirkuit.

---

## 🛠️ Pin Mapping (Konfigurasi Hardware ESP32)

Perangkat ini dirakit kokoh menggunakan **Expansion Board ESP32** untuk mencegah kabel kendur. Berikut adalah konfigurasi pin yang digunakan pada ESP32 (30-Pin):

| Nama Komponen | Tipe Pin | Pin ESP32 | Deskripsi |
| :--- | :---: | :---: | :--- |
| **Sensor Getar SW-420** | Input Digital | `GPIO 34` | Mendeteksi guncangan/getaran fisis |
| **Magnetic Door Sensor** | Input Digital | `GPIO 13` | Mendeteksi status pintu kotak amal |
| **Tombol Reset WiFi** | Input Digital | `GPIO 25` | Ditekan 5 detik untuk menghapus memori WiFi |
| **Buzzer Aktif 5V** | Output Digital | `GPIO 27` | Sirine alarm suara peringatan |
| **GPS NEO-6M (TX)** | Hardware Serial | `GPIO 32` | Terhubung ke pin RX GPS |
| **GPS NEO-6M (RX)** | Hardware Serial | `GPIO 33` | Terhubung ke pin TX GPS |
| **Layar LCD 16x2 I2C (SDA)**| Komunikasi I2C | `GPIO 21` | Jalur data layar informasi |
| **Layar LCD 16x2 I2C (SCL)**| Komunikasi I2C | `GPIO 22` | Jalur *clock* layar informasi |

---

## 📦 Pustaka (Libraries) yang Dibutuhkan

Sebelum melakukan *compile* program di Arduino IDE, pastikan Anda telah menginstal beberapa pustaka berikut melalui *Library Manager*:

1. `WiFi.h` (Bawaan core ESP32)
2. `WiFiManager` oleh tzapu (v2.0.16-rc.2 atau versi terbaru)
3. `TinyGPS++` oleh Mikal Hart
4. `BlynkSimpleEsp32.h` (Pustaka resmi Blynk)
5. `LiquidCrystal_I2C` oleh Frank de Brabander

---

## ⚙️ Cara Penggunaan & Alur Kerja Sistem

1. **Inisialisasi Awal (Booting):**
   Saat pertama kali dinyalakan, sistem akan menampilkan pesan `"NET SEARCHING..."` pada layar LCD dan membuka portal WiFi pintar selama 20 detik.
2. **Konfigurasi WiFi Pertama Kali:**
   Jika alat belum pernah terhubung ke WiFi, buka smartphone Anda, cari jaringan WiFi bernama **"Smart-IoT-Box"**, hubungkan, lalu masukkan SSID dan *password* WiFi masjid setempat melalui halaman web portal yang muncul.
3. **Kondisi Aman (Standby):**
   Jika koneksi internet berhasil, sistem terhubung ke server Blynk. Layar LCD akan menampilkan pesan secara bergantian antara status keamanan, angka tegangan baterai, serta jumlah satelit GPS yang terkunci.
4. **Respons Kondisi Bahaya:**
   - **Guncangan:** *Buzzer* berbunyi putus-putus cepat (`bip-bip-bip`), LCD menampilkan `"!! WARNING !!"`, dan status dikirim ke aplikasi Blynk.
   - **Pintu Dibuka:** *Buzzer* menjerit panjang tanpa putus, LCD menampilkan `"DOOR UNLOCKED"`, Blynk langsung memunculkan *push notification* darurat ke HP pengurus, dan widget peta memperbarui lokasi kotak amal secara *real-time*.

---

## 👤 Identitas Pengembang

Proyek ini dirancang dan dikembangkan sebagai bagian dari Penelitian Laporan Tugas Akhir program studi **Teknik Informatika**, Fakultas Sains dan Teknologi, **Universitas Nahdlatul Ulama Sunan Giri (UNUGIRI) Bojonegoro**.

- **Nama:** M. Yoga Adi Saputra
- **NIM:** 241101067
- **Tahun Riset:** 2026
