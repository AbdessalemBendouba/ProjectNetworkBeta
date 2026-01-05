#ifndef WEB_CODE_H
#define WEB_CODE_H

#include <Arduino.h>

// ===================================================================================
// ===                                THE WEB PAGE                                 ===
// ===================================================================================

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Advanced Diagnostics</title>
  <meta charset="UTF-8">
  <style>
    /* Dark Sci-Fi Theme */
    body { font-family: 'Segoe UI', sans-serif; background: #0b0c10; color: #c5c6c7; padding: 20px; }
    h2 { text-align: center; color: #66fcf1; margin-bottom: 5px; text-transform: uppercase; letter-spacing: 2px; }
    h4 { text-align: center; color: #45a29e; margin-top: 0; font-weight: normal; }
    
    /* Layout */
    .container { 
        display: grid; 
        grid-template-columns: 1fr 1fr; 
        gap: 15px; margin-bottom: 20px; 
    }
    .box { 
        background: #1f2833; 
        padding: 10px; 
        border-radius: 8px; 
        border: 1px solid #45a29e; 
        box-shadow: 0 0 10px rgba(69, 162, 158, 0.2);
    }
    canvas { width: 100%; height: 220px; background: #000; display: block; border-radius: 4px; }
    .label { text-align: center; margin: 0 0 8px 0; font-size: 14px; font-weight: bold; letter-spacing: 1px;}

    /* Status Bar */
    #statusBox {
        background: #1f2833; padding: 15px; border-radius: 6px;
        text-align: center; font-size: 1.6em; font-weight: bold;
        border: 2px solid #45a29e; margin-bottom: 20px; 
        text-transform: uppercase; color: #fff;
        text-shadow: 0 0 5px rgba(255,255,255,0.3);
    }

    /* Buttons */
    .controls { text-align: center; padding: 20px; background: #1f2833; border-radius: 8px; border: 1px solid #333; }
    button {
        padding: 12px 25px; cursor: pointer; background: #2c3e50; color: white;
        border: 1px solid #45a29e; border-radius: 4px; font-size: 14px; margin: 8px;
        font-weight: bold; transition: 0.2s; text-transform: uppercase;
    }
    button:hover { background: #45a29e; color: #000; box-shadow: 0 0 15px #66fcf1; }
    button:disabled { opacity: 0.3; cursor: not-allowed; filter: grayscale(100%); }

    /* Text Colors */
    .vib-text { color: #00ffcc; text-shadow: 0 0 5px #00ffcc; }
    .cur-text { color: #ffff00; text-shadow: 0 0 5px #ffff00; }
  </style>
</head>
<body>

  <h2>ESP32 Machine Health AI</h2>
  <h4>Real-Time Vibration & Current Analysis</h4>
  
  <div id="statusBox">SYSTEM UNCALIBRATED</div>

  <div class="container">
    <!-- VIBRATION -->
    <div class="box" style="border-left: 5px solid #00ffcc;">
        <p class="label vib-text">VIBRATION: TIME WAVEFORM</p>
        <canvas id="vibTime"></canvas>
    </div>
    <div class="box" style="border-left: 5px solid #00ffcc;">
        <p class="label vib-text">VIBRATION: SPECTRUM (FFT)</p>
        <canvas id="vibFft"></canvas>
    </div>

    <!-- CURRENT -->
    <div class="box" style="border-left: 5px solid #ffff00;">
        <p class="label cur-text">DC CURRENT: LOAD MONITOR</p>
        <canvas id="curTime"></canvas>
    </div>
    <div class="box" style="border-left: 5px solid #ffff00;">
        <p class="label cur-text">DC CURRENT: ARCING/RIPPLE DETECTOR</p>
        <canvas id="curFft"></canvas>
    </div>
  </div>

  <div class="controls">
    <button id="btnLearn" onclick="startLearning()">🧠 START AI CALIBRATION</button>
    <br><br>
    <span style="color:#aaa; font-weight:bold;">SIMULATE FAULTS:</span>
    <button onclick="simVibFault()">💥 Vib Spike</button>
    <button onclick="simCurJam()">🛑 Motor JAM</button>
    <button onclick="simCurDry()">🍃 Dry Run</button>
    <button onclick="simCurArc()">⚡ Brush Arcing</button>
  </div>

<script>
    // ==========================================
    // ===       AI CONFIGURATION ENGINE      ===
    // ==========================================
    const LEARN_FRAMES = 50; // Learning duration (~10 sec)
    
    // --- Vibration Settings ---
    const VIB_MASK_MARGIN = 1.4; // +40% tolerance
    const VIB_NOISE_FLOOR = 2.0; // Minimum sensitivity

    // --- Current Settings (The "Fantastic" Update) ---
    const CUR_UPPER_PERCENT = 0.30; // +30% = Overload
    const CUR_LOWER_PERCENT = 0.50; // -50% = Underload
    const CUR_NOISE_BUFFER  = 100.0; // Minimum gap for limits (prevents false alarms on 0)
    const CUR_IDLE_CUTOFF   = 50.0;  // Ignore values below this (Motor OFF)
    
    // Ripple (FFT) Settings
    const CUR_RIPPLE_MARGIN = 1.6; // +60% tolerance for arcing
    const CUR_FFT_SKIP_BINS = 5;   // IGNORE DC Component (Huge Spike at start)

    // --- System State ---
    let learningCounter = 0;
    let isLearning = false;
    let isCalibrated = false;

    // --- Learned Models ---
    let vibMask = []; 
    let vibMaxRms = 0;
    let curBaselineMean = 0; 
    let curRippleMask = [];  

    // --- Math Helpers ---
    const calcMean = d => d.reduce((a,b)=>a+b,0)/d.length;
    const calcRms = d => Math.sqrt(d.reduce((a,b)=>a+b*b,0)/d.length);

    // ==========================================
    // ===        ADVANCED CHARTING           ===
    // ==========================================
    function drawChart(id, data, color, type, limitUpper, limitLower) {
        const cvs = document.getElementById(id);
        const ctx = cvs.getContext('2d');
        
        // Responsive Resize
        if (cvs.width !== cvs.clientWidth) { cvs.width = cvs.clientWidth; cvs.height = cvs.clientHeight; }
        else ctx.clearRect(0, 0, cvs.width, cvs.height);

        const w = cvs.width; const h = cvs.height;
        ctx.lineWidth = 2;

        // --- Data Slicing for Current FFT ---
        // We hide the DC component (first 5 bins) so the noise is visible
        let displayData = data;
        let displayLimit = limitUpper;
        
        if (type === 'curFft') {
            displayData = data.slice(CUR_FFT_SKIP_BINS);
            if (displayLimit && Array.isArray(displayLimit)) {
                displayLimit = displayLimit.slice(CUR_FFT_SKIP_BINS);
            }
        }

        // --- Scaling Logic ---
        let max = Math.max(...displayData);
        let min = Math.min(...displayData);
        
        // Force charts to include limit lines (prevents cutting off red lines)
        if (displayLimit !== undefined) {
            if (Array.isArray(displayLimit)) {
                 let maxLim = Math.max(...displayLimit);
                 if (maxLim > max) max = maxLim;
            } else {
                 if (displayLimit > max) max = displayLimit;
                 if (limitLower < min) min = limitLower;
            }
        }
        
        // Visual polish: prevent flat line glitch
        if (max - min < 10) { max += 5; min -= 5; }
        let range = max - min; if(range === 0) range = 1;

        // --- Drawing ---
        ctx.strokeStyle = color; ctx.fillStyle = color; ctx.beginPath();

        if (type.includes('Fft')) {
            // Bar Chart
            let barW = w / displayData.length;
            for(let i=0; i<displayData.length; i++) {
                let barH = ((displayData[i] - min) / range) * (h - 20);
                ctx.fillRect(i*barW, h - barH, barW, barH);
            }
            // Mask Line
            if (isCalibrated && displayLimit) {
                ctx.strokeStyle = '#ff3333'; ctx.beginPath();
                for(let i=0; i<displayLimit.length; i++) {
                    let y = h - ((displayLimit[i] - min)/range * (h-20));
                    if(i==0) ctx.moveTo(0,y); else ctx.lineTo(i*barW, y);
                }
                ctx.stroke();
            }
        } else {
            // Line Chart
            let step = w / (displayData.length - 1);
            for(let i=0; i<displayData.length; i++) {
                let y = h - (((displayData[i] - min) / range) * (h - 20)) - 10;
                if(i==0) ctx.moveTo(0,y); else ctx.lineTo(i*step, y);
            }
            ctx.stroke();

            // Limits (Current Time only)
            if (type === 'curTime' && isCalibrated) {
                // Red Upper
                let yUp = h - (((limitUpper - min) / range) * (h - 20)) - 10;
                ctx.strokeStyle = '#ff3333'; ctx.setLineDash([5, 5]); ctx.beginPath();
                ctx.moveTo(0, yUp); ctx.lineTo(w, yUp); ctx.stroke();
                
                // Blue Lower
                let yLow = h - (((limitLower - min) / range) * (h - 20)) - 10;
                ctx.strokeStyle = '#3388ff'; ctx.beginPath();
                ctx.moveTo(0, yLow); ctx.lineTo(w, yLow); ctx.stroke();
                ctx.setLineDash([]);
            }
        }
    }

    // ==========================================
    // ===        MAIN SYSTEM LOGIC           ===
    // ==========================================
    function updateSystem(type, time, fft) {
        const statusBox = document.getElementById('statusBox');
        
        // --- 1. LEARNING ---
        if (isLearning) {
            learningCounter++;
            statusBox.innerHTML = `CALIBRATING... ${Math.round((learningCounter/LEARN_FRAMES)*100)}%`;
            statusBox.style.background = "#2c3e50";
            statusBox.style.color = "#66fcf1";

            if (type === 'vib') {
                let rms = calcRms(fft);
                if (rms > vibMaxRms) vibMaxRms = rms;
                if (vibMask.length === 0) vibMask = new Array(fft.length).fill(0);
                for(let i=0; i<fft.length; i++) if (fft[i] > vibMask[i]) vibMask[i] = fft[i];
            }
            if (type === 'cur') {
                let mean = calcMean(time);
                if (curBaselineMean === 0) curBaselineMean = mean;
                else curBaselineMean = (curBaselineMean * 0.9) + (mean * 0.1);

                if (curRippleMask.length === 0) curRippleMask = new Array(fft.length).fill(0);
                for(let i=0; i<fft.length; i++) if (fft[i] > curRippleMask[i]) curRippleMask[i] = fft[i];
            }

            if (learningCounter >= LEARN_FRAMES * 2) finalizeLearning(); // *2 because 2 sensor types
            
            // Draw raw during learn
            if(type==='vib') drawChart('vibTime', time, '#0fc', 'vibTime');
            if(type==='cur') drawChart('curTime', time, '#ff0', 'curTime');
            return;
        }

        // --- 2. MONITORING ---
        if (!isCalibrated) return;

        let anomaly = null;

        // >>> VIBRATION CHECK <<<
        if (type === 'vib') {
            let rms = calcRms(fft);
            if (rms > Math.max(vibMaxRms * VIB_MASK_MARGIN, VIB_NOISE_FLOOR)) anomaly = "VIB: HIGH ENERGY";
            if (!anomaly) {
                for(let i=2; i<fft.length; i++) {
                     let limit = Math.max(vibMask[i] * VIB_MASK_MARGIN, VIB_NOISE_FLOOR);
                     if (fft[i] > limit) { anomaly = `VIB: FREQ SPIKE (${i*2}Hz)`; break; }
                }
            }
            let limitArr = vibMask.map(v => Math.max(v * VIB_MASK_MARGIN, VIB_NOISE_FLOOR));
            drawChart('vibTime', time, '#0fc', 'vibTime');
            drawChart('vibFft', fft, '#0ff', 'vibFft', limitArr);
        }

        // >>> CURRENT CHECK (Robust) <<<
        if (type === 'cur') {
            let currentMean = calcMean(time);
            
            // Calculate Absolute Limits based on Baseline
            let upperLimit = curBaselineMean + Math.max(Math.abs(curBaselineMean) * CUR_UPPER_PERCENT, CUR_NOISE_BUFFER);
            let lowerLimit = curBaselineMean - Math.max(Math.abs(curBaselineMean) * CUR_LOWER_PERCENT, CUR_NOISE_BUFFER);
            let rippleLimits = curRippleMask.map(v => Math.max(v * CUR_RIPPLE_MARGIN, 2.0));

            // Logic: Is motor actually running?
            if (Math.abs(currentMean) < CUR_IDLE_CUTOFF && Math.abs(curBaselineMean) < CUR_IDLE_CUTOFF) {
                // Motor is OFF (or sensor disconnected) - No Faults
            } else {
                // Motor is Running - Check Faults
                if (currentMean > upperLimit) anomaly = "MOTOR FAULT: JAM / OVERLOAD";
                else if (currentMean < lowerLimit) anomaly = "MOTOR FAULT: DRY RUN / BROKEN BELT";
                else {
                    // Check Spectrum (Skip first bins)
                    for(let i=CUR_FFT_SKIP_BINS; i<fft.length; i++) {
                        if (fft[i] > rippleLimits[i]) { 
                            console.log(i);
                            anomaly = "MOTOR FAULT: BRUSH ARCING / NOISE"; 
                            break; 
                        }
                    }
                }
            }

            drawChart('curTime', time, '#ff0', 'curTime', upperLimit, lowerLimit);
            drawChart('curFft', fft, '#f0f', 'curFft', rippleLimits);
        }

        // --- 3. STATUS UPDATE ---
        if (anomaly) {
            statusBox.innerHTML = `⚠️ ${anomaly}`;
            statusBox.style.background = "#880000"; // Alarm Red
            statusBox.style.color = "#fff";
            statusBox.style.border = "2px solid red";
            statusBox.style.boxShadow = "0 0 20px red";
            return;
        } 
        // Only return to green if the CURRENT packet type matches the display
        // This is a simple latch prevention
        statusBox.innerHTML = "SYSTEM NORMAL";
        statusBox.style.background = "#1f2833";
        statusBox.style.color = "#0f0";
        statusBox.style.border = "2px solid #0f0";
        statusBox.style.boxShadow = "none";
        
    }
    

    // ==========================================
    // ===            SIMULATIONS             ===
    // ==========================================
    function startLearning() {
        learningCounter = 0; vibMaxRms = 0; curBaselineMean = 0; vibMask = []; curRippleMask = [];
        isLearning = true; isCalibrated = false;
        document.getElementById('btnLearn').disabled = true;
    }
    function finalizeLearning() {
        isLearning = false; isCalibrated = true;
        document.getElementById('btnLearn').disabled = false;
    }

    function simVibFault() {
        if(!isCalibrated) return alert("Calibrate first!");
        let t = Array(256).fill(0).map(x=>Math.random()*2);
        let f = vibMask.map(x=>x*0.5); 
        f[50] = (vibMask[50] * VIB_MASK_MARGIN) + 50; // Massive Spike
        updateSystem('vib', t, f);
    }
    
    function simCurJam() {
        if(!isCalibrated) return alert("Calibrate first!");
        // Force value ABOVE upper limit guaranteed
        let jamLevel = curBaselineMean + Math.max(curBaselineMean * 0.5, CUR_NOISE_BUFFER + 50);
        let t = Array(256).fill(jamLevel).map(x=>x+(Math.random()*10));
        updateSystem('cur', t, curRippleMask);
    }

    function simCurDry() {
        if(!isCalibrated) return alert("Calibrate first!");
        // Force value BELOW lower limit guaranteed
        let dryLevel = curBaselineMean - Math.max(curBaselineMean * 0.6, CUR_NOISE_BUFFER + 50);
        let t = Array(256).fill(dryLevel).map(x=>x+(Math.random()*10));
        updateSystem('cur', t, curRippleMask);
    }

    function simCurArc() {
        if(!isCalibrated) return alert("Calibrate first!");
        let t = Array(256).fill(curBaselineMean).map(x=>x+(Math.random()));
        let f = curRippleMask.map(x=>x);
        // Spike at index 50 (well past the skip bin)
        f[50] = (curRippleMask[50] * CUR_RIPPLE_MARGIN) + 100; 
        updateSystem('cur', t, f);
    }

    // --- SSE Connection ---
    if (!!window.EventSource) {
        var source = new EventSource('/events');
        source.addEventListener('update', function(e) {
            try { var d = JSON.parse(e.data); updateSystem(d.type, d.time, d.fft); } catch (err) {}
        }, false);
    }
</script>
</body></html>
)rawliteral";

#endif