#include "analyzer.h"
#include <ArduinoJson.h>
#include <math.h>
#include <vector>
#include "debug_log.h"

// Hardcoded fallback segment layout (standard profile values).
// Used when a named profile cannot be found in cfg.seg_profiles.
static const SegSample kFallbackSegs[7] = {
  {0.25f, 0.05f, 0.50f, 0.08f}, // a top
  {0.78f, 0.18f, 0.10f, 0.26f}, // b top-right
  {0.78f, 0.56f, 0.10f, 0.26f}, // c bottom-right
  {0.25f, 0.88f, 0.50f, 0.08f}, // d bottom
  {0.12f, 0.56f, 0.10f, 0.26f}, // e bottom-left
  {0.12f, 0.18f, 0.10f, 0.26f}, // f top-left
  {0.25f, 0.46f, 0.50f, 0.08f}, // g middle
};

static const SegSample* findProfile(const std::vector<SevenSegProfile> &profiles, const String &name) {
  for (const auto &p : profiles) {
    if (p.name == name) return p.segs;
  }
  // Fallback: try "standard"
  for (const auto &p : profiles) {
    if (p.name == "standard") return p.segs;
  }
  return kFallbackSegs;
}

static int clampi(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static int avgGray(camera_fb_t *fb, int x, int y, int w, int h) {
  if (!fb || fb->format != PIXFORMAT_GRAYSCALE) return -1;

  x = clampi(x, 0, fb->width - 1);
  y = clampi(y, 0, fb->height - 1);
  w = clampi(w, 1, fb->width - x);
  h = clampi(h, 1, fb->height - y);

  uint32_t sum = 0;
  uint32_t count = 0;

  for (int yy = y; yy < y + h; yy++) {
    int row = yy * fb->width;
    for (int xx = x; xx < x + w; xx++) {
      sum += fb->buf[row + xx];
      count++;
    }
  }

  return count ? (int)(sum / count) : 0;
}

struct Rect {
  int x;
  int y;
  int w;
  int h;
};

static Rect relRect(const Rect &base, float rx, float ry, float rw, float rh) {
  Rect r;
  r.x = base.x + (int)(base.w * rx);
  r.y = base.y + (int)(base.h * ry);
  r.w = (int)(base.w * rw);
  r.h = (int)(base.h * rh);
  if (r.w < 1) r.w = 1;
  if (r.h < 1) r.h = 1;
  return r;
}

static bool segmentIsOn(int avg, int threshold, bool invertLogic) {
  return invertLogic ? (avg < threshold) : (avg > threshold);
}

// Returns 1=on, 0=off, -1=uncertain (within ±margin of threshold).
// With margin=0 always returns 0 or 1 (never -1).
// Preserves original segmentIsOn semantics: normal → ON if avg>threshold, inverted → ON if avg<threshold.
static int segmentStateConfident(int avg, int threshold, int margin, bool invertLogic) {
  if (!invertLogic) {
    if (avg > threshold + margin) return 1;
    if (avg < threshold - margin) return 0;
    return margin > 0 ? -1 : (avg > threshold ? 1 : 0);
  } else {
    if (avg < threshold - margin) return 1;
    if (avg > threshold + margin) return 0;
    return margin > 0 ? -1 : (avg < threshold ? 1 : 0);
  }
}

// Computes min and max pixel brightness in a region.
static void minMaxGray(camera_fb_t *fb, int x, int y, int w, int h, int &minV, int &maxV) {
  x = clampi(x, 0, fb->width - 1);
  y = clampi(y, 0, fb->height - 1);
  w = clampi(w, 1, fb->width - x);
  h = clampi(h, 1, fb->height - y);
  minV = 255; maxV = 0;
  for (int yy = y; yy < y + h; yy++) {
    const int row = yy * fb->width;
    for (int xx = x; xx < x + w; xx++) {
      const uint8_t v = fb->buf[row + xx];
      if (v < minV) minV = v;
      if (v > maxV) maxV = v;
    }
  }
}

// Linearly maps avg from [minV,maxV] to [0,255].
static int stretchAvg(int avg, int minV, int maxV) {
  if (maxV <= minV) return avg;
  return clampi((avg - minV) * 255 / (maxV - minV), 0, 255);
}

// Otsu-Schwellwert: findet automatisch den optimalen Trennwert aus dem Histogramm
// der angegebenen ROI-Region. O(W×H) für Histogramm + O(256) für Otsu.
// Funktioniert für bimodale Verteilungen (Segment-Pixel vs. Hintergrund-Pixel).
static int otsuThreshold(camera_fb_t *fb, int x, int y, int w, int h) {
  if (!fb || fb->format != PIXFORMAT_GRAYSCALE) return 128;

  x = clampi(x, 0, fb->width - 1);
  y = clampi(y, 0, fb->height - 1);
  w = clampi(w, 1, fb->width - x);
  h = clampi(h, 1, fb->height - y);

  int hist[256] = {};
  const int total = w * h;

  for (int yy = y; yy < y + h; yy++) {
    const int row = yy * fb->width;
    for (int xx = x; xx < x + w; xx++) {
      hist[fb->buf[row + xx]]++;
    }
  }

  float sum = 0;
  for (int i = 0; i < 256; i++) sum += (float)i * hist[i];

  float sumB = 0, wB = 0, maxVar = 0;
  int threshold = 128;

  for (int t = 0; t < 256; t++) {
    wB += hist[t];
    if (wB == 0) continue;
    const float wF = (float)total - wB;
    if (wF == 0) break;
    sumB += (float)t * hist[t];
    const float mB = sumB / wB;
    const float mF = (sum - sumB) / wF;
    const float var = wB * wF * (mB - mF) * (mB - mF);
    if (var > maxVar) { maxVar = var; threshold = t; }
  }

  return threshold;
}

// Minimum Gap zwischen den sortierten Segment-Avgs damit ein Digit als nicht-leer gilt.
// Blanke Stellen haben kaum Kontrast zwischen den Segmentflächen → gap_max klein.
static const int MIN_SEGMENT_GAP = 25;

// Berechnet den Threshold aus dem "balancierten" Gap zwischen den sortierten Segment-Avgs.
// Score = gap_size / (1 + |left_count - right_count|): bevorzugt Splits nahe der Mitte
// gegenüber einseitigen Splits, auch wenn der Absolut-Gap kleiner ist.
// Beispiel: [59,77,81,87,119,178,185] → Split zwischen 87 und 119 (score=16) schlägt
// Split zwischen 119 und 178 (score=14.75), obwohl letzterer größer ist.
// Gibt den maximalen Absolut-Gap zurück (für Blank-Erkennung).
static int gapThreshold(const int avgs[7], int &maxGapOut) {
  int sorted[7];
  for (int i = 0; i < 7; i++) sorted[i] = avgs[i];
  // Insertion sort für 7 Elemente
  for (int i = 1; i < 7; i++) {
    int key = sorted[i], j = i - 1;
    while (j >= 0 && sorted[j] > key) { sorted[j+1] = sorted[j]; j--; }
    sorted[j+1] = key;
  }
  int maxGap = 0;
  float bestScore = -1.0f;
  int bestIdx = 2; // Fallback: Median-Split
  for (int i = 0; i < 6; i++) {
    int gap = sorted[i+1] - sorted[i];
    if (gap > maxGap) maxGap = gap;
    int leftCount = i + 1;
    int rightCount = 6 - i;
    float score = (float)gap / (1.0f + abs(leftCount - rightCount));
    if (score > bestScore) { bestScore = score; bestIdx = i; }
  }
  maxGapOut = maxGap;
  return (sorted[bestIdx] + sorted[bestIdx+1]) / 2;
}

static uint8_t readDigit7Seg(camera_fb_t *fb, const Rect &digit, int threshold, bool invertLogic, const SegSample segs[7], JsonObject dbg, bool stretch, int margin) {
  Rect sr[7];
  for (int i = 0; i < 7; i++) sr[i] = relRect(digit, segs[i].rx, segs[i].ry, segs[i].rw, segs[i].rh);

  int minV = 0, maxV = 255;
  if (stretch) {
    minMaxGray(fb, digit.x, digit.y, digit.w, digit.h, minV, maxV);
    dbg["stretch_min"] = minV;
    dbg["stretch_max"] = maxV;
  }

  static const char* const kSegKeys[7]    = {"a","b","c","d","e","f","g"};
  static const char* const kAvgKeys[7]    = {"a_avg","b_avg","c_avg","d_avg","e_avg","f_avg","g_avg"};

  int avgs[7];
  for (int i = 0; i < 7; i++) {
    avgs[i] = avgGray(fb, sr[i].x, sr[i].y, sr[i].w, sr[i].h);
    if (stretch) avgs[i] = stretchAvg(avgs[i], minV, maxV);
  }

  // Bei auto_threshold (threshold=-1): Gap-Threshold aus den 7 Segment-Avgs.
  bool useGap = (threshold < 0);
  int gapMax = 0;
  if (useGap) {
    threshold = gapThreshold(avgs, gapMax);
    dbg["gap_threshold"] = threshold;
    dbg["gap_max"] = gapMax;
    // Zu wenig Kontrast → blanke Stelle, sofort als ungültig markieren
    if (gapMax < MIN_SEGMENT_GAP) {
      dbg["blank"] = true;
      dbg["bits"] = 0xFF;
      return 0xFF;
    }
  }

  uint8_t bits = 0;
  bool uncertain = false;
  int states[7];
  for (int i = 0; i < 7; i++) {
    int s = segmentStateConfident(avgs[i], threshold, margin, invertLogic);
    if (s < 0) { uncertain = true; s = 0; }
    bits |= (uint8_t)s << i;
    states[i] = s;
  }
  for (int i = 0; i < 7; i++) dbg[kSegKeys[i]] = states[i];
  for (int i = 0; i < 7; i++) dbg[kAvgKeys[i]] = avgs[i];

  dbg["threshold"] = threshold;
  dbg["invert_logic"] = invertLogic;
  if (uncertain) dbg["uncertain"] = true;
  if (uncertain) bits = 0xFF; // mark as invalid
  dbg["bits"] = bits;
  return bits;
}

static uint8_t readDigit7SegNoDebug(camera_fb_t *fb, const Rect &digit, int threshold, bool invertLogic, const SegSample segs[7], bool stretch, int margin) {
  int minV = 0, maxV = 255;
  if (stretch) minMaxGray(fb, digit.x, digit.y, digit.w, digit.h, minV, maxV);

  int avgs[7];
  for (int i = 0; i < 7; i++) {
    Rect r = relRect(digit, segs[i].rx, segs[i].ry, segs[i].rw, segs[i].rh);
    avgs[i] = avgGray(fb, r.x, r.y, r.w, r.h);
    if (stretch) avgs[i] = stretchAvg(avgs[i], minV, maxV);
  }

  if (threshold < 0) {
    int maxGap = 0;
    threshold = gapThreshold(avgs, maxGap);
    if (maxGap < MIN_SEGMENT_GAP) return 0xFF; // blank
  }

  uint8_t bits = 0;
  for (int i = 0; i < 7; i++) {
    int s = segmentStateConfident(avgs[i], threshold, margin, invertLogic);
    if (s < 0) return 0xFF; // uncertain → invalid
    bits |= (uint8_t)s << i;
  }
  return bits;
}

static int bitsToDigit(uint8_t bits) {
  switch (bits) {
    case 0x3F: return 0;
    case 0x06: return 1;
    case 0x5B: return 2;
    case 0x4F: return 3;
    case 0x66: return 4;
    case 0x6D: return 5;
    case 0x7D: return 6;
    case 0x07: return 7;
    case 0x7F: return 8;
    case 0x6F: return 9;
    default: return -1;
  }
}

// Wie bitsToDigit, aber mit Hamming-1 Toleranz: wenn kein exakter Match,
// suche das Digit das sich in genau 1 Bit unterscheidet.
// Verhindert Fehler durch einen einzelnen Grenzwert-Segment-Messfehler.
static int bitsToDigitFuzzy(uint8_t bits) {
  int d = bitsToDigit(bits);
  if (d >= 0) return d;
  static const uint8_t kPatterns[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
  };
  for (int i = 0; i <= 9; i++) {
    if (__builtin_popcount(bits ^ kPatterns[i]) == 1) return i;
  }
  return -1;
}

static char bitsToSevenSegChar(uint8_t bits) {
  switch (bits) {
    case 0x3F: return '0';
    case 0x06: return '1';
    case 0x5B: return '2';
    case 0x4F: return '3';
    case 0x66: return '4';
    case 0x6D: return '5';
    case 0x7D: return '6';
    case 0x07: return '7';
    case 0x7F: return '8';
    case 0x6F: return '9';

    // zusätzliche Buchstaben für Diehl Fehlerseite
    // Segmentreihenfolge: a,b,c,d,e,f,g -> Bits 0..6
    // A = a,b,c,e,f,g
    case 0x77: return 'A';
    // E = a,d,e,f,g
    case 0x79: return 'E';
    // H = b,c,e,f,g
    case 0x76: return 'H';

    // falls leer / nichts erkennbar
    case 0x00: return '\0';

    default: return '?';
  }
}

static Roi* findRoiById(AppConfig &cfg, const String &roiId) {
  for (auto &roi : cfg.rois) {
    if (roi.id == roiId) return &roi;
  }
  return nullptr;
}

static bool computeSevenSegGeometry(const Roi &roi, int &digits, int &gap, int &digitW);
static int  digitXPos(const Roi &roi, int digitW, int i);

static bool evaluateSevenSegStringNoDebug(camera_fb_t *fb, const Roi &roi, const SegSample segs[7], String &outText) {
  int digits = 0;
  int gap = 0;
  int digitW = 0;
  if (!computeSevenSegGeometry(roi, digits, gap, digitW)) {
    outText = "";
    LOGD("string sevenseg roi=%s invalid geometry w=%d h=%d digits=%d gap=%d\n",
         roi.id.c_str(), roi.w, roi.h, roi.digits, roi.digit_gap_px);
    return false;
  }

  outText = "";
  bool anyValid = false;

  for (int i = 0; i < digits; i++) {
    Rect dr{
      digitXPos(roi, digitW, i),
      roi.y,
      digitW,
      roi.h
    };

    const int threshold = roi.auto_threshold
      ? -1  // -1 = Gap-Threshold direkt aus Segment-Avgs berechnen
      : roi.threshold;

    uint8_t bits = readDigit7SegNoDebug(fb, dr, threshold, roi.invert_logic, segs, roi.stretch_contrast, roi.confidence_margin);
    char c = bitsToSevenSegChar(bits);

    LOGA("string sevenseg roi=%s digit=%d bits=0x%02X char=%c\n",
         roi.id.c_str(), i, bits, c ? c : '_');

    if (c == '?') {
      return false;
    }

    if (c != '\0') {
      outText += c;
      anyValid = true;
    }
  }

  return anyValid;
}


static const int MIN_SEVENSEG_DIGIT_WIDTH_PX = 8;
static const int MIN_SEVENSEG_DIGIT_HEIGHT_PX = 20;

static bool computeSevenSegGeometry(const Roi &roi, int &digits, int &gap, int &digitW) {
  digits = roi.digits;
  gap = roi.digit_gap_px;

  if (digits < 1) {
    return false;
  }

  if (gap < 0) {
    return false;
  }

  // Sum all gaps: use roi.gaps[i] if fully specified, else uniform digit_gap_px
  bool hasIndividual = ((int)roi.gaps.size() == digits - 1);
  int totalGap = 0;
  for (int i = 0; i < digits - 1; i++) {
    totalGap += hasIndividual ? roi.gaps[i] : gap;
  }

  int available = roi.w - totalGap;
  digitW = available / digits;

  if (digitW < MIN_SEVENSEG_DIGIT_WIDTH_PX) {
    return false;
  }

  if (roi.h < MIN_SEVENSEG_DIGIT_HEIGHT_PX) {
    return false;
  }

  return true;
}

// Returns the x position of digit i, using individual gaps if configured.
static int digitXPos(const Roi &roi, int digitW, int i) {
  bool hasIndividual = ((int)roi.gaps.size() == roi.digits - 1);
  int x = roi.x;
  for (int j = 0; j < i; j++) {
    x += digitW;
    x += hasIndividual ? roi.gaps[j] : roi.digit_gap_px;
  }
  return x;
}

static double applyDecimalPlaces(int rawValue, const Roi &roi) {
  if (roi.decimal_places <= 0) {
    return rawValue;
  }

  double scale = 1.0;
  for (int i = 0; i < roi.decimal_places; i++) {
    scale *= 10.0;
  }

  return ((double)rawValue) / scale;
}

static bool evaluateSevenSegDebug(camera_fb_t *fb, const Roi &roi, const SegSample segs[7], JsonObject outObj, double &outValue) {
  // Globaler Threshold nur als Fallback/Log; bei auto_threshold wird pro Stelle berechnet.
  const int globalThreshold = roi.auto_threshold
    ? otsuThreshold(fb, roi.x, roi.y, roi.w, roi.h)
    : roi.threshold;
  if (roi.auto_threshold) outObj["otsu_threshold"] = globalThreshold;

  int digits = 0;
  int gap = 0;
  int digitW = 0;
  if (!computeSevenSegGeometry(roi, digits, gap, digitW)) {
    outValue = -1.0;
    outObj["geometry_ok"] = false;
    outObj["min_digit_w"] = MIN_SEVENSEG_DIGIT_WIDTH_PX;
    outObj["min_digit_h"] = MIN_SEVENSEG_DIGIT_HEIGHT_PX;
    outObj["roi_w"] = roi.w;
    outObj["roi_h"] = roi.h;
    outObj["digits"] = roi.digits;
    outObj["digit_gap_px"] = roi.digit_gap_px;
    LOGD("sevenseg roi=%s invalid geometry w=%d h=%d digits=%d gap=%d\n",
         roi.id.c_str(), roi.w, roi.h, roi.digits, roi.digit_gap_px);
    return false;
  }
  outObj["geometry_ok"] = true;
  outObj["digit_w"] = digitW;

  std::vector<int> decoded((size_t)digits, -1);
  bool started = false;
  bool anyValid = false;
  bool allValid = true;

  for (int i = 0; i < digits; i++) {
    Rect dr{
      digitXPos(roi, digitW, i),
      roi.y,
      digitW,
      roi.h
    };

    // Per-digit Otsu: jede Stelle hat eigenen Kontrastbereich → eigener Threshold.
    // Verhindert dass eine helle/dunkle Stelle den Threshold für alle anderen kippt.
    const int threshold = roi.auto_threshold
      ? -1  // -1 = Gap-Threshold direkt aus Segment-Avgs berechnen
      : roi.threshold;

    String key = "d" + String(i);
    JsonObject digitDbg = outObj[key].to<JsonObject>();
    JsonObject segDbg = digitDbg["segments"].to<JsonObject>();

    uint8_t bits = readDigit7Seg(fb, dr, threshold, roi.invert_logic, segs, segDbg, roi.stretch_contrast, roi.confidence_margin);
    int d = roi.auto_threshold ? bitsToDigitFuzzy(bits) : bitsToDigit(bits);
    decoded[i] = d;
    digitDbg["value"] = d;

    LOGA("sevenseg roi=%s digit=%d bits=0x%02X value=%d\n",
         roi.id.c_str(), i, bits, d);

    if (d >= 0) {
      started = true;
      anyValid = true;
    } else {
      if (started) allValid = false;
    }
  }

  if (!anyValid || !allValid) {
    outValue = -1.0;
    return false;
  }

  int value = 0;
  for (int i = 0; i < digits; i++) {
    if (decoded[i] >= 0) {
      value = value * 10 + decoded[i];
    }
  }

  outValue = applyDecimalPlaces(value, roi);
  return true;
}

static bool evaluateSevenSegNoDebug(camera_fb_t *fb, const Roi &roi, const SegSample segs[7], double &outValue) {
  int digits = 0;
  int gap = 0;
  int digitW = 0;
  if (!computeSevenSegGeometry(roi, digits, gap, digitW)) {
    outValue = -1.0;
    LOGD("mqtt sevenseg roi=%s invalid geometry w=%d h=%d digits=%d gap=%d\n",
         roi.id.c_str(), roi.w, roi.h, roi.digits, roi.digit_gap_px);
    return false;
  }

  std::vector<int> decoded((size_t)digits, -1);
  bool started = false;
  bool anyValid = false;
  bool allValid = true;

  for (int i = 0; i < digits; i++) {
    Rect dr{
      digitXPos(roi, digitW, i),
      roi.y,
      digitW,
      roi.h
    };

    // Per-digit Otsu: jede Stelle bekommt eigenen Threshold.
    const int threshold = roi.auto_threshold
      ? -1  // -1 = Gap-Threshold direkt aus Segment-Avgs berechnen
      : roi.threshold;

    uint8_t bits = readDigit7SegNoDebug(fb, dr, threshold, roi.invert_logic, segs, roi.stretch_contrast, roi.confidence_margin);
    int d = roi.auto_threshold ? bitsToDigitFuzzy(bits) : bitsToDigit(bits);
    decoded[i] = d;

    LOGA("mqtt sevenseg roi=%s digit=%d bits=0x%02X value=%d\n",
         roi.id.c_str(), i, bits, d);

    if (d >= 0) {
      started = true;
      anyValid = true;
    } else {
      if (started) allValid = false;
    }
  }

  if (!anyValid || !allValid) {
    outValue = -1.0;
    return false;
  }

  int value = 0;
  for (int i = 0; i < digits; i++) {
    if (decoded[i] >= 0) {
      value = value * 10 + decoded[i];
    }
  }

  outValue = applyDecimalPlaces(value, roi);
  return true;
}

bool evaluateStateToJson(camera_fb_t *fb, AppConfig &cfg, JsonDocument &doc) {
  doc.clear();
  doc["valid"] = true;

  for (auto &roi : cfg.rois) {
    if (roi.type == "symbol") {
      int avg = avgGray(fb, roi.x, roi.y, roi.w, roi.h);
      int eval = roi.invert_logic ? (255 - avg) : avg;
      bool state = roi.last_state;

      if (roi.auto_threshold) {
        const int otsu = otsuThreshold(fb, roi.x, roi.y, roi.w, roi.h);
        state = (eval >= otsu);
      } else {
        if (eval >= roi.threshold_on) state = true;
        else if (eval <= roi.threshold_off) state = false;
      }

      roi.last_state = state;
      doc[roi.id] = state;

      LOGA("mqtt symbol roi=%s avg=%d state=%s\n",
           roi.id.c_str(), avg, state ? "ON" : "OFF");
    } else if (roi.type == "sevenseg") {
      const SegSample *segs = findProfile(cfg.seg_profiles, roi.seg_profile);
      double value = -1.0;
      bool ok = evaluateSevenSegNoDebug(fb, roi, segs, value);

      if (!ok) {
        doc["valid"] = false;
        doc[roi.id] = -1;
        // Helligkeits-Diagnose: zeigt ob ROI falsch positioniert oder Threshold falsch ist.
        // avg_gray nahe 0 → ROI zu dunkel / Belichtung noch nicht eingeschwungen.
        // avg_gray weit über threshold → Threshold zu niedrig (alle Segmente als ON erkannt).
        int roiAvg = avgGray(fb, roi.x, roi.y, roi.w, roi.h);
        LOGE("sevenseg roi=%s invalid: avg_gray=%d threshold=%d x=%d y=%d w=%d h=%d\n",
             roi.id.c_str(), roiAvg, roi.threshold, roi.x, roi.y, roi.w, roi.h);
      } else {
        if (roi.decimal_places > 0) {
          char buf[32];
          snprintf(buf, sizeof(buf), "%.*f", roi.decimal_places, value);
          doc[roi.id] = String(buf);
          LOGD("mqtt sevenseg roi=%s value=%s\n", roi.id.c_str(), buf);
        } else {
          int intValue = (int)round(value);
          doc[roi.id] = intValue;
          LOGD("mqtt sevenseg roi=%s value=%d\n", roi.id.c_str(), intValue);
        }
      }
    }
  }

  return true;
}

String evaluateCurrent(camera_fb_t *fb, AppConfig &cfg) {
  JsonDocument doc;
  doc["ok"] = true;

  JsonObject result = doc["result"].to<JsonObject>();
  result["valid"] = true;

  JsonArray symbols = doc["debug"]["symbols"].to<JsonArray>();
  JsonArray sevensegs = doc["debug"]["sevenseg"].to<JsonArray>();

  for (auto &roi : cfg.rois) {
    if (roi.type == "symbol") {
      int avg = avgGray(fb, roi.x, roi.y, roi.w, roi.h);
      int eval = roi.invert_logic ? (255 - avg) : avg;
      bool state = roi.last_state;

      if (roi.auto_threshold) {
        const int otsu = otsuThreshold(fb, roi.x, roi.y, roi.w, roi.h);
        state = (eval >= otsu);
      } else {
        if (eval >= roi.threshold_on) state = true;
        else if (eval <= roi.threshold_off) state = false;
      }

      roi.last_state = state;
      result[roi.id] = state;

      LOGA("symbol roi=%s avg=%d on=%d off=%d state=%s\n",
           roi.id.c_str(),
           avg,
           roi.threshold_on,
           roi.threshold_off,
           state ? "ON" : "OFF");

      JsonObject o = symbols.add<JsonObject>();
      o["id"] = roi.id;
      o["avg"] = avg;
      o["state"] = state;
      o["threshold_on"] = roi.threshold_on;
      o["threshold_off"] = roi.threshold_off;
      o["invert_logic"] = roi.invert_logic;
    } else if (roi.type == "sevenseg") {
      JsonObject o = sevensegs.add<JsonObject>();
      o["id"] = roi.id;
      o["digits"] = roi.digits;
      o["decimal_places"] = roi.decimal_places;
      o["seg_profile"] = roi.seg_profile;

      const SegSample *segs = findProfile(cfg.seg_profiles, roi.seg_profile);
      double value = -1.0;
      bool ok = evaluateSevenSegDebug(fb, roi, segs, o, value);

      if (!ok) {
        result["valid"] = false;
        result[roi.id] = -1;
        LOGD("sevenseg roi=%s invalid result\n", roi.id.c_str());
      } else {
        if (roi.decimal_places > 0) {
          char buf[32];
          snprintf(buf, sizeof(buf), "%.*f", roi.decimal_places, value);
          result[roi.id] = String(buf);
          LOGD("sevenseg roi=%s value=%s\n", roi.id.c_str(), buf);
        } else {
          int intValue = (int)round(value);
          result[roi.id] = intValue;
          LOGD("sevenseg roi=%s value=%d\n", roi.id.c_str(), intValue);
        }
      }
    }
  }

  String out;
  serializeJson(doc, out);
  return out;
}

String evaluateStateOnly(camera_fb_t *fb, AppConfig &cfg) {
  JsonDocument doc;
  evaluateStateToJson(fb, cfg, doc);

  String out;
  serializeJson(doc, out);
  return out;
}

bool readSevenSegIntById(camera_fb_t *fb, AppConfig &cfg, const String &roiId, int &value) {
  Roi *roi = findRoiById(cfg, roiId);
  if (!roi) return false;
  if (roi->type != "sevenseg") return false;

  const SegSample *segs = findProfile(cfg.seg_profiles, roi->seg_profile);
  double outValue = -1.0;
  if (!evaluateSevenSegNoDebug(fb, *roi, segs, outValue)) return false;

  value = (int)round(outValue);
  return true;
}

bool readSevenSegStringById(camera_fb_t *fb, AppConfig &cfg, const String &roiId, String &value) {
  Roi *roi = findRoiById(cfg, roiId);
  if (!roi) return false;
  if (roi->type != "sevenseg") return false;

  const SegSample *segs = findProfile(cfg.seg_profiles, roi->seg_profile);
  return evaluateSevenSegStringNoDebug(fb, *roi, segs, value);
}

bool readSevenSegCharById(camera_fb_t *fb, AppConfig &cfg, const String &roiId, char &value) {
  String text;
  if (!readSevenSegStringById(fb, cfg, roiId, text)) return false;
  if (text.length() < 1) return false;

  value = text.charAt(0);
  return true;
}