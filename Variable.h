#ifndef VARIABLES_H
#define VARIABLES_H

volatile bool flagAlarmShiftLever = false;

// =================== KONFIGURASI WAKTU =====================
const unsigned long NEUTRAL_MIN_MS      = 5000;   // 5 detik minimal netral (buat abuse/cancel)
const unsigned long ABUSE_STABLE_MS     = 1000;   // 2 detik gear tujuan stabil untuk V4 "2"

// =================== INPUT GEAR ============================
// Di project asli, ini diisi dari CAN: TransCurrentGear = rxBuf[3] - 125;
// lalu currentGearRaw = TransCurrentGear (dibatasi -1..3)
int currentGearRaw = 0;    // -1 (R), 0 (N), 1..3 (D)

// Normalisasi gear ke -1 (R), 0 (N), +1 (D)
int normalizeGear(int g) {
  if (g < 0)  return -1;   // R
  if (g > 0)  return +1;   // semua 1,2,3 jadi D
  return 0;                // N
}

// =================== RIWAYAT GEAR (ARRAY) ==================
const int HISTORY_SIZE = 5;

struct GearState {
  int gearNorm;              // -1 = R, 0 = N, +1 = D
  unsigned long timestamp;   // millis() saat state tercatat
};

GearState gearHistory[HISTORY_SIZE];
int historyCount = 0;

// =================== STATE LOGIKA ==========================

// Netral & shiftlever
volatile bool neutralActive          = false;
unsigned long neutralStartMs = 0;

volatile bool shiftReadyFlag         = false;   // event: netral >= 5 detik (boleh pindah shiftlever)
volatile bool shiftReadyStatus       = false;   // status (opsional kalau mau dipakai)

// Abuse
volatile bool abuseWarningFlag       = false;   // event: abuse terdeteksi (untuk V4 "1")
volatile bool abuseActive            = false;   // sedang dalam event abuse (antara V4 "1" & "2")
int  abuseTargetGear        = 0;       // gearNorm tujuan (R atau D)
volatile bool abuseStableTimerActive = false;   // timer stabil untuk V4 "2"
unsigned long abuseStableStartMs = 0;  // kapan mulai stabil di gear tujuan

// Gear norm sekarang & sebelumnya
int currentGearNorm         = 0;
int lastGearNorm            = 0;

int countingAlarmShiftLever = 0;


// Konfigurasi Watchdog Timer
#define WDT_TIMEOUT 10  // Timeout dalam detik
#define WDT_TIMEOUT_MS (WDT_TIMEOUT * 1000)

#endif