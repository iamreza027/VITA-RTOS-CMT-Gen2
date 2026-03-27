// ===================================================================
//  LOGIKA TRANSMISI ABUSE
// ===================================================================
// Tambah entry baru ke history (array geser kiri, data baru di belakang)
void addGearToHistory(int gearNorm, unsigned long t) {
  if (historyCount < HISTORY_SIZE) {
    gearHistory[historyCount].gearNorm = gearNorm;
    gearHistory[historyCount].timestamp = t;
    historyCount++;
  } else {
    // geser ke kiri
    for (int i = 1; i < HISTORY_SIZE; i++) {
      gearHistory[i - 1] = gearHistory[i];
    }
    // masukkan di slot terakhir
    gearHistory[HISTORY_SIZE - 1].gearNorm = gearNorm;
    gearHistory[HISTORY_SIZE - 1].timestamp = t;
  }
}
// ----------------- DETEKSI DARI HISTORY ---------------------
// Fungsi bantu: ambil index dari belakang
// getFromEnd(0) => elemen terakhir
// getFromEnd(1) => elemen sebelum terakhir, dst.

bool getHistoryFromEnd(int offset, GearState &out) {
  if (historyCount == 0) return false;
  int idx = historyCount - 1 - offset;
  if (idx < 0) return false;
  out = gearHistory[idx];
  return true;
}
// Cek pattern R -> N -> R atau D -> N -> D dan netralDuration < 5 detik

bool isCancelPattern(unsigned long &neutralDurationOut) {
  GearState last, mid, first;
  if (!getHistoryFromEnd(0, last)) return false;   // state ke-3 (paling baru)
  if (!getHistoryFromEnd(1, mid)) return false;    // state ke-2
  if (!getHistoryFromEnd(2, first)) return false;  // state ke-1

  // pattern: first -> mid -> last
  // Cancelling jika:
  //   (R -> N -> R) atau (D -> N -> D)
  if (mid.gearNorm != 0) return false;  // tengah harus N

  // R -> N -> R
  if (first.gearNorm == -1 && last.gearNorm == -1) {
    neutralDurationOut = last.timestamp - mid.timestamp;  // lama di N
    return true;
  }

  // D -> N -> D
  if (first.gearNorm == +1 && last.gearNorm == +1) {
    neutralDurationOut = last.timestamp - mid.timestamp;
    return true;
  }

  return false;
}
// Cek apakah terjadi abuse D <-> R dengan netral < 5 detik

bool isAbusePattern(unsigned long &neutralDurationOut) {
  GearState last, mid, first;

  // 1) Kasus tanpa netral: langsung D -> R atau R -> D
  if (getHistoryFromEnd(0, last) && getHistoryFromEnd(1, first)) {
    if (first.gearNorm != 0 && last.gearNorm != 0 && first.gearNorm != last.gearNorm) {
      // D (1) -> R(-1) atau R(-1) -> D(1)
      // Tidak ada netral di tengah. Kita anggap neutralDuration = 0.
      neutralDurationOut = 0;
      return true;
    }
  }

  // 2) Kasus dengan netral singkat: D -> N -> R atau R -> N -> D
  if (!getHistoryFromEnd(0, last) || !getHistoryFromEnd(1, mid) || !getHistoryFromEnd(2, first)) {
    return false;
  }

  if (mid.gearNorm != 0) return false;  // tengah harus N

  // D -> N -> R
  if (first.gearNorm == +1 && last.gearNorm == -1) {
    neutralDurationOut = last.timestamp - mid.timestamp;
    return true;
  }

  // R -> N -> D
  if (first.gearNorm == -1 && last.gearNorm == +1) {
    neutralDurationOut = last.timestamp - mid.timestamp;
    return true;
  }

  return false;
}
// ---------------- LOGIKA UTAMA (dipanggil tiap loop) ----------------

void updateTransmissionLogic() {
  unsigned long now = millis();

  // Normalisasi gear sekarang
  currentGearNorm = normalizeGear(currentGearRaw);

  // Jika gear berubah dari terakhir, update history
  if (currentGearNorm != lastGearNorm) {
    addGearToHistory(currentGearNorm, now);
    lastGearNorm = currentGearNorm;

    // --- Masuk netral ---
    if (currentGearNorm == 0) {
      neutralActive = true;
      neutralStartMs = now;
      // belum ada shiftReadyFlag di sini, tunggu 5 detik
    }

    // --- Keluar dari netral ---
    if (currentGearNorm != 0 && neutralActive) {
      unsigned long dur = now - neutralStartMs;

      // 1) Cek apakah ini pattern cancel (R->N->R atau D->N->D)
      unsigned long cancelDur = 0;
      if (isCancelPattern(cancelDur) && cancelDur < NEUTRAL_MIN_MS) {
        // Cancel: reset netral, tidak boleh shift, tidak abuse dari sisi netral
        neutralActive = false;
        shiftReadyFlag = false;
        // shiftReadyStatus boleh tetap (misal untuk history), optional
        // Tidak memicu alarm apapun
      } else {
        // Keluar netral ke arah lain, netralActive akan dicek di bawah (waktu)
        neutralActive = false;
        // tidak reset shiftReadyStatus; jika sudah sempat >=5 detik, tetap true
      }
    }

    // --- Cek ABUSE berdasarkan history ---
    unsigned long neutralDurAbuse = 0;
    if (isAbusePattern(neutralDurAbuse)) {
      if (neutralDurAbuse < NEUTRAL_MIN_MS) {
        // Ini Abuse: D <-> R dengan netral < 5 detik
        abuseWarningFlag = true;  // event satu kali, bisa dipakai untuk suara / log
        // abuseActive = true;
      }
    }
  }

  // Jika saat ini di netral dan menghitung, cek apakah sudah >= 5 detik
  if (neutralActive == true) {
    if (TimerAlarmShiftLever.isReady() == true) {
      countingAlarmShiftLever++;
      if (countingAlarmShiftLever >= 2 && flagAlarmShiftLever == true) {
        flagAlarmShiftLever = false;
        // Baru pertama kali mencapai 5 detik netral
        shiftReadyStatus = true;  // status: sekarang resmi boleh pindah
        shiftReadyFlag = true;    // event: alarm “boleh pindah shiftlever”
        // SuaraShiftlever = 1;
        countingAlarmShiftLever = 0;
      }
      TimerAlarmShiftLever.reset();
    }
  } else {
    countingAlarmShiftLever = 0;
    flagAlarmShiftLever = true;
  }

  if (abuseActive == true) {
    // Jika gear sekarang sudah berada di gear tujuan (R atau D)
    if (currentGearNorm == abuseTargetGear) {

      if (!abuseStableTimerActive) {
        abuseStableStartMs = now;
        abuseStableTimerActive = true;
      }

      if (abuseStableTimerActive && (now - abuseStableStartMs >= ABUSE_STABLE_MS)) {
        // Di sini kita anggap event abuse sudah selesai dan gear tujuan stabil
        // ---> KIRIM V4 "2" DI SINI <---
        // Contoh panggil fungsi log / sensor di project asli:
        // RecordSensor(xNoUnit, IDcardCurrent, "V4", "2", String(Speed), String(Total), xTgl, xJam, xSite);
        // SuaraAbuse = 0;  // kalau kamu matikan suara di sini
        EventItem item;

        strcpy(item.event, "V4");
        strcpy(item.kodeST, "2");
        item.valueSensor = (int)speed;

        fillEventTime(&item);

        xQueueSend(eventQueue, &item, 0);

        // Reset state abuse
        abuseActive = false;
        abuseStableTimerActive = false;
      }

    } else {
      // Gear berubah lagi, timer stabil di-reset
      abuseStableTimerActive = false;
    }
  }
}

void CheckOverspeed() {

  float speed = CAN_getSpeed();

  int ON = OverspeedMuatanON;
  int OFF = OverspeedMuatanOFF;

  bool overspeed;

  // hysteresis
  if (!overspeedActive)
    overspeed = speed > ON;
  else
    overspeed = speed > OFF;

  // update audio state
  requestAudio(AUDIO_OVER_SPEED, overspeed);

  // ===================== RISING EDGE (ON) =====================
  if (overspeed) {
    if (!overspeedActive) {

      overspeedActive = true;

      playAudio(AUDIO_OVER_SPEED);

      EventItem item;

      strcpy(item.event, "V6");
      strcpy(item.kodeST, "1");  // ON
      item.valueSensor = (int)speed;

      fillEventTime(&item);
      xQueueSend(eventQueue, &item, 0);

      Serial.println("OVERSPEED ON");
    }
  }

  // ===================== FALLING EDGE (OFF) =====================
  else {
    if (overspeedActive) {  // penting: cek sebelumnya aktif

      overspeedActive = false;

      EventItem item;

      strcpy(item.event, "V6");
      strcpy(item.kodeST, "2");  // OFF
      item.valueSensor = (int)speed;

      fillEventTime(&item);
      xQueueSend(eventQueue, &item, 0);

      Serial.println("OVERSPEED OFF");
    }
  }
}
void CheckCostingNetral() {

  float speed = CAN_getSpeed();
  int gear = CAN_getCurrentGear();

  bool coastingCondition = (gear == 0 && speed > 5);

  // ================= TIMER =================
  if (coastingCondition) {
    if (neutralStartTime == 0)
      neutralStartTime = millis();
  } else {
    neutralStartTime = 0;
  }

  unsigned long duration = (neutralStartTime == 0) ? 0 : (millis() - neutralStartTime);

  bool coastingTriggered = (duration >= 3000);

  // ================= AUDIO =================
  requestAudio(AUDIO_COASTING_GEAR, coastingTriggered);

  // ================= ON EVENT =================
  if (coastingTriggered) {
    if (!costingNeutralActive) {

      costingNeutralActive = true;

      playAudio(AUDIO_COASTING_GEAR);

      EventItem item;

      strcpy(item.event, "V10");
      strcpy(item.kodeST, "1");
      item.valueSensor = (int)speed;

      fillEventTime(&item);
      xQueueSend(eventQueue, &item, 0);

      Serial.println("COASTING NETRAL ON");
    }
  }

  // ================= OFF EVENT =================
  else {
    if (costingNeutralActive) {

      costingNeutralActive = false;

      EventItem item;

      strcpy(item.event, "V10");
      strcpy(item.kodeST, "2");
      item.valueSensor = (int)speed;

      fillEventTime(&item);
      xQueueSend(eventQueue, &item, 0);

      Serial.println("COASTING NETRAL OFF");
    }
  }
}

void CheckSlipStall() {

  int rpm = CAN_getRPM();
  int gear = CAN_getCurrentGear();

  bool SlipStallCondition = (gear == 1 && rpm > );

  // ================= TIMER =================
  if (coastingCondition) {
    if (neutralStartTime == 0)
      neutralStartTime = millis();
  } else {
    neutralStartTime = 0;
  }

  unsigned long duration = (neutralStartTime == 0) ? 0 : (millis() - neutralStartTime);

  bool coastingTriggered = (duration >= 3000);

  // ================= AUDIO =================
  requestAudio(AUDIO_COASTING_GEAR, coastingTriggered);

  // ================= ON EVENT =================
  if (coastingTriggered) {
    if (!costingNeutralActive) {

      costingNeutralActive = true;

      playAudio(AUDIO_COASTING_GEAR);

      EventItem item;

      strcpy(item.event, "V10");
      strcpy(item.kodeST, "1");
      item.valueSensor = (int)speed;

      fillEventTime(&item);
      xQueueSend(eventQueue, &item, 0);

      Serial.println("COASTING NETRAL ON");
    }
  }

  // ================= OFF EVENT =================
  else {
    if (costingNeutralActive) {

      costingNeutralActive = false;

      EventItem item;

      strcpy(item.event, "V10");
      strcpy(item.kodeST, "2");
      item.valueSensor = (int)speed;

      fillEventTime(&item);
      xQueueSend(eventQueue, &item, 0);

      Serial.println("COASTING NETRAL OFF");
    }
  }
}