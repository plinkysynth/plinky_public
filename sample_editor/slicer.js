const dpr = window.devicePixelRatio || 1;


let customNode = null;
const eqcanvas = document.getElementById('eqCanvas');
const eqctx = eqcanvas.getContext('2d');
const sampleRate = 32000; // Sample rate
const TAU = 2 * Math.PI;
const butterq1 = 0.54119610;
const butterq2 = 1.3065630;
let audioContext;
let audioSource = null;
let analyserNode = null;
let isPlaying = false;
let brightnessArray;
let imageData;
let frequencyData;
let frequencyDatapeak;
let frequencyDatahold;

const biquad_params = [
    { b0: 1, b1: 0, b2: 0, a1: 0, a2: 0 },
    { b0: 1, b1: 0, b2: 0, a1: 0, a2: 0 },
    { b0: 1, b1: 0, b2: 0, a1: 0, a2: 0 },
    { b0: 1, b1: 0, b2: 0, a1: 0, a2: 0 },
    { b0: 1, b1: 0, b2: 0, a1: 0, a2: 0 },
    { b0: 1, b1: 0, b2: 0, a1: 0, a2: 0 },
    { b0: 1, b1: 0, b2: 0, a1: 0, a2: 0 },
    { b0: 1, b1: 0, b2: 0, a1: 0, a2: 0 },
];
const default_eq = [
    { type: 'highpass', freq: 40, gain: 0, q: butterq1 },
    { type: 'lowshelf', freq: 200, gain: 0, q: 1 },
    { type: 'peaking', freq: 500, gain: 0, q: 0.5 },
    { type: 'peaking', freq: 2000, gain: 0, q: 0.5 },
    { type: 'highshelf', freq: 6000, gain: 0, q: 1 },
    { type: 'lowpass', freq: 14000, gain: 0, q: butterq1 },
    { type: 'highpass', freq: 40, gain: 0, q: butterq2 },
    { type: 'lowpass', freq: 14000, gain: 0, q: butterq2 }
];
const default_volenv = [
    { x: 0.0, y: 1.0, power: 10000 },
    { x: 1.0, y: 0.0, power: 1 }
];
const default_volenv_wt = [
    { x: 0.0, y: 1.0, power: 1 },
    { x: 1.0, y: 1.0, power: 1 }
];


const wfcanvas = document.getElementById('audioCanvas');
const wfctx = wfcanvas.getContext('2d');
const fftcanvas = document.getElementById('fftCanvas');
const fftctx = fftcanvas.getContext('2d');
const playButton = document.getElementById('playButton');

function db2lin(db) {
    return Math.pow(10, db / 20);
}

function midiToFreq(midiValue) {
    return 440 * Math.pow(2, (midiValue - 69) / 12);
}

function freqToMidi(freq) {
    return (69 + 12 * Math.log2(freq / 440));
}

function midiToNoteName(midiValue) {
    midiValue = Math.round(midiValue);
    const notes = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
    const octave = Math.floor(midiValue / 12) - 1;
    const note = notes[midiValue % 12];
    return `${note}${octave}`;
}

const sliceListElement = document.getElementById('sliceList');

function make_state(wt, cs) {
    return {
    currentSlice: 0,
    slices: JSON.parse("[]"),
    filename_list: JSON.parse("[]"),
    the_audio : new Float32Array(),
    first_sample : 0,
    zoom : 64,
    pitched : false,
    wavetablemode : wt,
    contiguous_slices : cs,
    };
}
const tab_states = [];
let selectedTab = 0;

for (let i = 0; i < 9; i++) {
    tab_states.push(make_state(i==8, i<8));
}
let G = tab_states[selectedTab];
function selectTab(index) {
    //if (index === selectedTab) return;
    console.log('---------- SELECT TAB', index);
    G = tab_states[index];
    const tabs = document.querySelectorAll('.tab');
    tabs[selectedTab + 1].classList.remove('selected'); // offset for burger menu
    tabs[selectedTab + 1].querySelector('canvas').style.opacity = 0.25;
    selectedTab = index;
    tabs[selectedTab + 1].querySelector('canvas').style.opacity = 1;
    tabs[selectedTab + 1].classList.add('selected');
    updateSliceList();
    updateAudioDisplay();
    
    
}


function get_default_volenv() {
    return G.wavetablemode ? default_volenv_wt : default_volenv;
}

const default_slice = {
    name: "noname",
    start: 0,
    end: 1,
    gain: 0,
    eq: JSON.parse(JSON.stringify(default_eq)),
    volenv: JSON.parse(JSON.stringify(get_default_volenv())),
    reversed: false,
    base_pitch: 48,
    };

function enforceSliceConstraints( delete_empty_slices=false) {
    let deleted = false;
    if (!G.contiguous_slices) return false;
    while (G.slices.length > 8) {
        // find the shortest slice and delete it
        let shortest = 0;
        for (let i = 1; i < G.slices.length; i++) {
            if (G.slices[i].end - G.slices[i].start < G.slices[shortest].end - G.slices[shortest].start)
                shortest = i;
        }
        if (shortest < G.currentSlice) G.currentSlice--;
        G.slices.splice(shortest, 1);
        deleted=true;
    }
    if (G.currentSlice < G.slices.length-1) {
        if (G.slices[G.currentSlice].end > G.slices[G.currentSlice+1].end)
            G.slices[G.currentSlice].end = G.slices[G.currentSlice+1].end;
        G.slices[G.currentSlice+1].start = G.slices[G.currentSlice].end;
    }
    const start = G.slices.length > 0 ? G.slices[0].start : 0;
    let maxlen = Math.min(G.the_audio.length, 2*1024*1024 + start);
    for (let i = 0; i<G.slices.length; i++) {
        if (G.slices[i].start < 0) G.slices[i].start = 0;
        if (G.slices[i].end > maxlen) G.slices[i].end = maxlen;
        if (i<G.slices.length-1) G.slices[i].end = G.slices[i+1].start;
        if (i>0 && G.slices.start < G.slices[i-1].end) G.slices[i].start = G.slices[i-1].end;
        if (G.slices[i].start >= G.slices[i].end) {
            G.slices[i].start=G.slices[i].end;
        }
    }
    if (delete_empty_slices) {
        for (let i = G.slices.length-1; i>=0; ) {
            if (G.slices[i].start >= G.slices[i].end) {
                G.slices.splice(i, 1);
                deleted=true;
                if (G.currentSlice > i) G.currentSlice--;
            } else {
                i--;
            }
        }
    }
    if (deleted)
        updateSliceList();
    return deleted;
}

function updateSliceList() {
    sliceListElement.innerHTML = '';
    enforceSliceConstraints();
    if (G.currentSlice >= G.slices.length) G.currentSlice = 0;
    G.slices.forEach((slice, index) => {
        console.log(slice);
        const listItem = document.createElement('li');
        listItem.innerHTML = `<span class="color-chip" style="background-color: hsl(${slice.hue}, 100%, 50%)"></span>${slice.name}`;
        listItem.onclick = () => selectSlice(index);
        listItem.setAttribute('draggable', 'true');
        listItem.ondragstart = (event) => onDragStart(event, index);
        listItem.ondragover = (event) => event.preventDefault();
        listItem.ondrop = (event) => onDrop(event, index);
        sliceListElement.appendChild(listItem);
    });
    // Reselect the current slice after reordering
    selectSlice(G.currentSlice);
    if (G.slices.length > 0 || G.the_audio.length > 0) {
        document.querySelectorAll('.editor-container').forEach(editor => editor.style.display = 'block');
        document.getElementById('help').style.display = 'none';
    } else {
        document.querySelectorAll('.editor-container').forEach(editor => editor.style.display = 'none');
        document.getElementById('help').style.display = 'block';
    }
}


function deleteSlice(index, deletewav) {
    if (index >= G.slices.length) return;
    const start = G.slices[index].start;
    const end = G.slices[index].end;
    G.slices.splice(index, 1);
    if (deletewav) {
        let new_audio = new Float32Array(G.the_audio.length - (end - start));
        new_audio.set(G.the_audio.slice(0, start));
        new_audio.set(G.the_audio.slice(end), start);
        G.the_audio = new_audio;
        function fix_pos(x) {
            if (x < start) return x;
            if (x >= end) return x - (end - start);
            return start;
        }
        for (let i = G.filename_list.length - 1; i >= 0; i--) {
            G.filename_list[i].start = fix_pos(G.filename_list[i].start);
            G.filename_list[i].end = fix_pos(G.filename_list[i].end);
            if (G.filename_list[i].start >= G.filename_list[i].end) {
                G.filename_list.splice(i, 1);
            }
        }
        for (let i = G.slices.length - 1; i >= 0; i--) {
            G.slices[i].start = fix_pos(G.slices[i].start);
            G.slices[i].end = fix_pos(G.slices[i].end);
            if (G.slices[i].start >= G.slices[i].end) {
                G.slices.splice(i, 1);
            }
        }
    }
    updateSliceList();
}

function ensureSliceVisible(start, end, forcezoom = false) {
    console.log("ensureSliceVisible", start, end);
    // lets move the slice to be within the display
    const minzoom = Math.max(0.1, (end - start) / wfcanvas.width);
    if (G.zoom < minzoom || forcezoom)
        G.zoom = minzoom;
    if (start < G.first_sample) {
        G.first_sample = start;
    }
    const last_sample = G.first_sample + wfcanvas.width * G.zoom;
    if (end > last_sample) {
        G.first_sample += end - last_sample;
    }
}

function selectSlice(index) {
    G.currentSlice = index;
    const slice = G.currentSlice < G.slices.length ? G.slices[G.currentSlice] : default_slice;
    document.getElementById('sliceName').value = slice.name;
    document.getElementById('gainSlider').value = slice.gain;
    document.getElementById('gainValue').innerText = slice.gain;
    
    document.getElementById('basePitchSlider').value = slice.base_pitch;
    document.getElementById('basePitchValue').innerText = midiToNoteName(slice.base_pitch);
    
    document.documentElement.style.setProperty('--highlight-hue', index * 45);
    if (index < G.slices.length) {
        document.querySelector(`#sliceList li:nth-child(${index + 1})`).scrollIntoView();
        document.querySelectorAll('.slice-list li').forEach((li, liIndex) => {
            li.classList.toggle('selected', liIndex === index);
            li.style.color = liIndex === index ? 'white' : 'black';
        });
    }
    updateExtraSlider();
    document.getElementById('reversedCheckbox').checked = slice.reversed;
    setupBiquads();
    if (G.currentSlice < G.slices.length)
        ensureSliceVisible(slice.start, slice.end);
    updateAudioDisplay();
}

function updateExtraSlider() {
    document.getElementById('basePitchSlider').style.display = 'none';
    document.getElementById('basePitchLabel').style.display = 'none';
    if (G.pitched) {
        document.getElementById('basePitchSlider').style.display = 'block';
        document.getElementById('basePitchLabel').style.display = 'block';
        document.getElementById('basePitchValue').innerText = midiToNoteName(document.getElementById('basePitchSlider').value);
    }
}

///////////////////////// EQ

console.log("hello from eq", eqcanvas.width, eqcanvas.height);
let nearest_filter = -1; // The nearest filter to the mouse (-1 means none)
let is_dragging = false; // Whether we are currently dragging a filter

function x2freq(x) {
    return 16 * Math.pow(10, (x / eqcanvas.width) * 3); // log scale 20Hz to 20kHz
}

function freq2x(freq) {
    return Math.log10(freq / 16) * (eqcanvas.width / 3);
}

function gain2y(gain) {
    return (24 - gain) * (eqcanvas.height / 60);
}

function setupBiquad(filter, index) {
    const freq = clamp((TAU / sampleRate) * filter.freq, (TAU / sampleRate) * 10, Math.PI - 0.05);
    const sn = Math.sin(freq), cs = Math.cos(freq);
    let alpha, A, b0, b1, b2, a0, a1, a2;

    if (filter.type === 'highpass' || filter.type === 'lowpass') {
        alpha = sn / (2 * filter.q);
        if (filter.type === 'lowpass') {
            b1 = 1 - cs;
            b0 = b2 = b1 / 2;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
        } else if (filter.type === 'highpass') {
            b1 = -(1 + cs);
            b0 = b2 = -b1 / 2;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
        }
    } else if (filter.type === 'peaking') {
        A = Math.pow(10, filter.gain / 40);
        alpha = sn * Math.sinh((0.69314718055994 * 0.5 * filter.q * freq) / sn);
        b0 = 1 + alpha * A;
        b1 = -2 * cs;
        b2 = 1 - alpha * A;
        a0 = 1 + alpha / A;
        a1 = b1;
        a2 = 1 - alpha / A;
    } else if (filter.type === 'lowshelf' || filter.type === 'highshelf') {
        A = Math.pow(10, filter.gain / 20);
        alpha = sn * Math.sqrt((A + 1) * (1 / filter.q - 1) + 2 * A);
        if (filter.type === 'lowshelf') {
            b0 = A * ((A + 1) - (A - 1) * cs + alpha);
            b1 = 2 * A * ((A - 1) - (A + 1) * cs);
            b2 = A * ((A + 1) - (A - 1) * cs - alpha);
            a0 = (A + 1) + (A - 1) * cs + alpha;
            a1 = -2 * ((A - 1) + (A + 1) * cs);
            a2 = (A + 1) + (A - 1) * cs - alpha;
        } else if (filter.type === 'highshelf') {
            b0 = A * ((A + 1) + (A - 1) * cs + alpha);
            b1 = -2 * A * ((A - 1) + (A + 1) * cs);
            b2 = A * ((A + 1) + (A - 1) * cs - alpha);
            a0 = (A + 1) - (A - 1) * cs + alpha;
            a1 = 2 * ((A - 1) - (A + 1) * cs);
            a2 = (A + 1) - (A - 1) * cs - alpha;
        }
    }
    const bqp = biquad_params[index];
    bqp.b0 = b0 / a0;
    bqp.b1 = b1 / a0;
    bqp.b2 = b2 / a0;
    bqp.a1 = a1 / a0;
    bqp.a2 = a2 / a0;
    if (customNode) {
        customNode.parameters.get('b0_' + index).setValueAtTime(bqp.b0, 0);
        customNode.parameters.get('b1_' + index).setValueAtTime(bqp.b1, 0);
        customNode.parameters.get('b2_' + index).setValueAtTime(bqp.b2, 0);
        customNode.parameters.get('a1_' + index).setValueAtTime(bqp.a1, 0);
        customNode.parameters.get('a2_' + index).setValueAtTime(bqp.a2, 0);
    }
}

function setupBiquads() {
    if (G.currentSlice >= G.slices.length) return;
    slice = G.slices[G.currentSlice];
    for (let i = 0; i < 8; ++i) {
        setupBiquad(slice.eq[i], i);
    }
}

function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
}

function GainDB(freq, filter) {
    const omega = freq * TAU / (2.0 * sampleRate);
    const s2 = Math.sin(omega);
    const phi = 4.0 * s2 * s2;
    const As = 1.0 + filter.a1 + filter.a2;
    const Bs = filter.b0 + filter.b1 + filter.b2;
    const num = Bs * Bs + (filter.b0 * filter.b2 * phi - (filter.b1 * (filter.b0 + filter.b2) + 4.0 * filter.b0 * filter.b2)) * phi;
    const den = As * As + (filter.a2 * phi - (filter.a1 * (1.0 + filter.a2) + 4.0 * filter.a2)) * phi;
    const magSquared = num / den;
    return 10 * Math.log10(magSquared);
}

function drawEQCurve() {
    eqctx.clearRect(0, 0, eqcanvas.width, eqcanvas.height);
    if (G.currentSlice >= G.slices.length) return;
    const slice = G.slices[G.currentSlice];
    const accent_col = `hsl(${G.currentSlice * 45}, 70%, 50%)`;
    const inv_accent_col = `hsl(${G.currentSlice * 45}, 70%, 50%)`;
    const inv_accent_col_fill = `hsl(${G.currentSlice * 45}, 70%, 50%, 0.2)`;
    eqctx.font = (10 * dpr) + 'px Arial';
    eqctx.fillStyle = 'lightgray';
    eqctx.lineWidth = 1;
    for (gain = -24; gain < 24; gain += 6) {
        eqctx.strokeStyle = (gain == 0) ? 'black' : ((gain / 6) & 1) ? 'lightgray' : 'gray';
        let y = gain2y(gain);
        drawHorizontalLine(y);
        if (!((gain / 6) & 1)) eqctx.fillText(gain, 2, y - 2);
    }
    // Frequency markers
    for (let freq = 10; freq <= 20000; freq *= 10) {
        let x1 = freq2x(freq);
        let x5 = freq2x(freq * 5);
        eqctx.fillText(freq >= 1000 ? freq / 1000 + 'k' : freq, x1 + 2, eqcanvas.height - 2);
        eqctx.fillText(freq >= 200 ? freq / 200 + 'k' : freq * 5, x5 + 2, eqcanvas.height - 2);
        eqctx.strokeStyle = 'gray';
        drawVerticalLine(x1);
        drawVerticalLine(x5);
        eqctx.strokeStyle = 'lightgray';
        for (let mul = 2; mul < 10; mul++)
            if (mul != 5) drawVerticalLine(freq2x(freq * mul));
    }

    // Draw EQ curve
    eqctx.beginPath();
    for (let x = 0; x <= eqcanvas.width; x++) {
        const freq = x2freq(x);
        let totalGain = 0;
        for (const filter of biquad_params) {
            totalGain += GainDB(freq, filter);
        }
        const y = gain2y(totalGain);
        if (x === 0) {
            eqctx.moveTo(x, y);
        } else {
            eqctx.lineTo(x, y);
        }
    }
    eqctx.strokeStyle = 'black';
    eqctx.lineWidth = 2.5;
    eqctx.stroke();

    // Highlight the vertical lines for the low pass and high pass filters
    eqctx.strokeStyle = accent_col;
    eqctx.lineWidth = (nearest_filter == 0) ? 5 : 2;
    drawVerticalLine(freq2x(slice.eq[0].freq));
    eqctx.lineWidth = (nearest_filter == 5) ? 5 : 2;
    drawVerticalLine(freq2x(slice.eq[5].freq));
    eqctx.lineWidth = 1;

    if (G.pitched) {
        eqctx.fillStyle = inv_accent_col_fill;
        eqctx.font = (60 * dpr) + 'px Arial';
        eqctx.fillText(midiToNoteName(slice.base_pitch), 4, eqcanvas.height - 4);
        const freq = midiToFreq(slice.base_pitch);
        const x1 = freq2x(freq / 2);
        const x2 = freq2x(freq * 2);
        eqctx.fillRect(x1, 0, x2 - x1, eqcanvas.height);
        eqctx.strokeStyle = inv_accent_col;
        eqctx.lineWidth = (nearest_filter == -99) ? 5 : 2;
        drawVerticalLine(freq2x(freq));

    }


    // Draw circles for mid filters, larger if selected or highlighted
    for (let i = 1; i <= 4; i++) {
        const filter = slice.eq[i];
        const x = freq2x(filter.freq), y = gain2y(filter.gain);
        const isHighlighted = (i === nearest_filter);
        eqctx.beginPath();
        eqctx.arc(x, y, isHighlighted ? 10 : 7, 0, 2 * Math.PI);
        eqctx.fillStyle = accent_col;
        eqctx.fill();
    }
}
function drawHorizontalLine(y) {
    eqctx.beginPath();
    eqctx.moveTo(0, y);
    eqctx.lineTo(eqcanvas.width, y);
    eqctx.stroke();
}
function drawVerticalLine(x) {
    eqctx.beginPath();
    eqctx.moveTo(x, 0);
    eqctx.lineTo(x, eqcanvas.height);
    eqctx.stroke();
}
function is_zero_crossing(sample) {
    sample = sample | 0;
    return (sample >= 0 && sample < G.the_audio.length - 1 && G.the_audio[sample] < 0 && G.the_audio[sample + 1] >= 0);
}

function find_best_snap_point_near(sample) {
    const bestsample = sample | 0;
    const snap_range = 4 * dpr * G.zoom;
    // look for a positive zero crossing
    for (let radius = 0; radius < snap_range; ++radius) {
        if (is_zero_crossing(bestsample + radius))
            return bestsample + radius;
        if (is_zero_crossing(bestsample - radius))
            return bestsample - radius;
    }
    return bestsample;
}

eqcanvas.addEventListener('pointermove', (event) => {
    if (G.currentSlice >= G.slices.length) return;
    const slice = G.slices[G.currentSlice];
    const mouseX = (event.offsetX) * dpr;
    const mouseY = (event.offsetY) * dpr;

    if (!is_dragging) {
        // Find the closest filter within 10 pixels
        nearest_filter = -1;
        let minDistance = 10 * dpr;
        for (let i = 0; i < 6; i++) {
            const filter = slice.eq[i];
            const x = freq2x(filter.freq), y = gain2y(filter.gain);
            let yscale = (i == 0 || i == 5) ? 0 : 1;
            const dist = Math.sqrt(Math.pow(x - mouseX, 2) + yscale * Math.pow(y - mouseY, 2));
            if (dist < minDistance) {
                nearest_filter = i;
                minDistance = dist;
            }
        }
        if (G.pitched) {
            const freq = midiToFreq(slice.base_pitch);
            const x = freq2x(freq);
            const dist = Math.abs(x - mouseX);
            if (dist < minDistance) {
                nearest_filter = -99;
                minDistance = dist;
            }
        }
        if (nearest_filter == -1) {
            const x0 = freq2x(slice.eq[0].freq);
            const x5 = freq2x(slice.eq[5].freq);
            if (mouseX < x0) nearest_filter = 0;
            if (mouseX > x5) nearest_filter = 5;
        }
        drawEQCurve(); // Redraw to update highlighting
    } else if (nearest_filter !== -1) {
        // Adjust the frequency and gain of the selected filter
        const filter = nearest_filter >= 0 ? slice.eq[nearest_filter] : null;
        const deltaX = (event.movementX / eqcanvas.width) * 3 * dpr; // Logarithmic scale factor for X axis
        const deltaY = -event.movementY * (60 / eqcanvas.height) * dpr; // Linear scale for Y axis
        if (nearest_filter == -99) {
            let freq = clamp(midiToFreq(slice.base_pitch) * Math.pow(10, deltaX), 16, 20000);
            slice.base_pitch = freqToMidi(freq);
            document.getElementById('basePitchValue').innerText = midiToNoteName(slice.base_pitch);
            document.getElementById('basePitchSlider').value = slice.base_pitch;

        } else {
            filter.freq = Math.max(17
                , Math.min(15000, filter.freq * Math.pow(10, deltaX))); // Clamp frequency between 20Hz and 20kHz
            filter.gain = Math.max(-36, Math.min(24, filter.gain + deltaY)); // Clamp gain between -60dB and +12dB
            setupBiquad(filter, nearest_filter); // Recompute coefficients
            if (nearest_filter == 0) { slice.eq[6].freq = filter.freq; setupBiquad(slice.eq[6], 6); } // Update highpass2
            if (nearest_filter == 5) { slice.eq[7].freq = filter.freq; setupBiquad(slice.eq[7], 7); } // Update lowpass2
        }
        drawEQCurve(); // Redraw curve
    }
});

// Handle mouse down
eqcanvas.addEventListener('pointerdown', (event) => {
    if (nearest_filter !== -1) {
        is_dragging = true;
        // capture the mouse for the drag
        event.preventDefault();
        eqcanvas.setPointerCapture(event.pointerId);
    }
});

eqcanvas.addEventListener('pointerup', (event) => {
    is_dragging = false;
    eqcanvas.releasePointerCapture(event.pointerId);
});

///////////////////////// WAVEFORM

console.log("hello from waveform.js", wfcanvas.width, wfcanvas.height)
let pointerDownTime = 0;
let pointerDownX = 0;
let isDragging = false;
let nearestPoint = -1;
let pointToLeft = -1;
let hoveredItem = -1;
let hoveredSlice = -1;
let hoveredStart = 0;
let hoveredEnd = 0;
let hoveredMouseSample = 0;

function drawCurve(points, startx, endx, fillit) {
    if (G.currentSlice >= G.slices.length) return;
    const width = endx - startx;
    // Draw the curve by interpolating between points
    wfctx.beginPath();
    wfctx.moveTo(startx, wfcanvas.height * (1 - points[0].y));

    for (let i = 0; i < points.length - 1; i++) {
        const p1 = points[i];
        const p2 = points[i + 1];
        let x1 = p1.x * width + startx;
        if (x1 >= wfcanvas.width) break;
        const x2 = p2.x * width + startx + 0.001;
        if (x2 < 0) continue;
        if (nearestPoint == -1 && pointToLeft == i) {
            const y1 = wfcanvas.height * (1 - p1.y) * 0.5;
            const y2 = wfcanvas.height * (1 - p2.y) * 0.5;
            wfctx.fillRect(x1, Math.min(y1, y2), x2 - x1, Math.abs(y2 - y1));
        }
        wfctx.lineWidth = (nearestPoint === -1 && pointToLeft === i) ? 4 : 2;
        wfctx.beginPath();
        wfctx.moveTo(x1, wfcanvas.height * (1 - p1.y) * 0.5);
        const dx = dpr * 2;
        for (let x = Math.max(0, x1); x < wfcanvas.width + dx; x += dx) {
            let t = (x - x1) / (x2 - x1);
            if (t > 1) t = 1;
            const y = p1.y + (p2.y - p1.y) * Math.pow(t, p1.power);
            const canvasY = wfcanvas.height * (1 - y) * 0.5;
            wfctx.lineTo(x, canvasY);
            if (t == 1) break;
        }
        wfctx.strokeStyle = 'black';
        wfctx.stroke();
    }

    for (let i = 0; i < points.length; i++) {
        const point = points[i];
        const x = point.x * width + startx;
        const y = wfcanvas.height * (1 - point.y) * 0.5;
        wfctx.beginPath();
        wfctx.arc(x, y, i === nearestPoint ? 10 : 7, 0, 2 * Math.PI);
        wfctx.fillStyle = fillit;
        wfctx.fill();
    }
}

let mouseSample = 0;
let mouseX = 0;
function do_mouse_move(event, mousedown=false) {
    let slice = G.currentSlice < G.slices.length ? G.slices[G.currentSlice] : default_slice;
    const points = slice.volenv;
    const startx = (slice.start - G.first_sample) / G.zoom;
    const endx = (slice.end - G.first_sample) / G.zoom;
    const width = endx - startx;
    mouseX = event.offsetX * dpr;
    const mousePos = {
        x: (mouseX - startx) / width,
        y: 1 - ((event.offsetY) * dpr) / wfcanvas.height * 2
    };
    const newMouseSample = ((event.offsetX) * dpr) * G.zoom + G.first_sample;
    //let dmouseSample = newMouseSample - mouseSample;
    mouseSample = newMouseSample;
    //console.log(mouseSample);
    if (!isDragging) {
        let prevnearest = nearestPoint;
        let prevpointToLeft = pointToLeft;
        let prevhovered = hoveredItem;
        let prevhoveredSlice = hoveredSlice;
        nearestPoint = -1;
        pointToLeft = -1;
        hoveredItem = -1;
        hoveredSlice = -1;
        let minDist = 10 * dpr / width;
        if (mousePos.y >= -1 && mousePos.y <= 1) {
            if (mousePos.y >= 0) {
                for (let i = 0; i < points.length; i++) {
                    const dist = Math.abs(points[i].x - mousePos.x);
                    const ydist = Math.abs(points[i].y - mousePos.y);
                    if (dist < minDist && ydist < 0.1) {
                        minDist = dist;
                        nearestPoint = i;
                        hoveredItem = 1; // edit point
                    }
                    if (points[i].x < mousePos.x && mousePos.x >= 0 && mousePos.x <= 1) {
                        pointToLeft = i;
                    }
                }
            }
            if (nearestPoint == -1) {
                if (mousePos.x >= -minDist && mousePos.x < minDist) hoveredItem = 2; // start point
                else if (mousePos.x >= 1 - minDist && mousePos.x < 1 + minDist) hoveredItem = 3; // end point
                else if (pointToLeft >= 0) {
                    const p1 = slice.volenv[pointToLeft];
                    const p2 = slice.volenv[pointToLeft + 1];
                    const t = (mousePos.x - p1.x) / (p2.x - p1.x);
                    const y = p1.y + (p2.y - p1.y) * Math.pow(t, p1.power);
                    //const miny = Math.min(p1.y, p2.y) + 5 / wfcanvas.height;
                    //const maxy = Math.max(p1.y, p2.y) - 5 / wfcanvas.height;
                    if (Math.abs(y - mousePos.y) < 10 / 256)
                        hoveredItem = 4; // curve
                }
            }
            hoveredMouseSample = mouseSample;
            hoveredStart = slice.start;
            hoveredEnd = slice.end;
            if (hoveredItem == -1 && mousePos.x >= 0 && mousePos.x <= 1)
                hoveredItem = 5; // drag whole slice
            if (hoveredItem != 4) pointToLeft = -1;
            if (hoveredItem != 1) nearestPoint = -1;
            if (hoveredItem == -1) {
                // look to see if we are over any of the slices
                for (let j = ((G.currentSlice >= G.slices.length) ? 0 : 1); j < G.slices.length; j++) {
                    const i = (G.currentSlice + j) % G.slices.length;
                    const otherslice = G.slices[i];
                    if (mouseSample >= otherslice.start && mouseSample < otherslice.end) {
                        hoveredSlice = i;
                        hoveredItem = 6;
                        if (mousedown) {
                            if (mouseSample < otherslice.start + G.zoom * 10) {
                                console.log("!!!2");
                                hoveredItem = 2;
                                G.currentSlice = i;
                            } else if (mouseSample > otherslice.end - G.zoom * 10) {
                                console.log("!!!3");
                                hoveredItem = 3;
                                G.currentSlice = i;
                            }
                        }
                        break;
                    }
                }
            }
        }
        //console.log(nearestPoint, pointToLeft, hoveredItem, mousePos.x, mousePos.y);
        if (prevnearest !== nearestPoint || prevpointToLeft !== pointToLeft || prevhovered !== hoveredItem || prevhoveredSlice !== hoveredSlice) {
            console.log("redrawing", nearestPoint, pointToLeft, hoveredItem, hoveredSlice);
            renderAudio();
        }
    } else {
        // dragging!
        if (hoveredItem === 6 || hoveredItem === -1) {
            const dragdist = event.offsetX - pointerDownX;
            if (Math.abs(dragdist) > 5) {
                let name = "noname";
                for (let i = 0; i < G.filename_list.length; i++) {
                    if (G.filename_list[i].start <= mouseSample && G.filename_list[i].end >= mouseSample) {
                        name = G.filename_list[i].name;
                        break;
                    }
                }
                let newslice = {
                    name: name,
                    start: find_best_snap_point_near(Math.min(hoveredMouseSample, mouseSample)),
                    end: find_best_snap_point_near(Math.max(hoveredMouseSample, mouseSample)),
                    gain: 0,
                    eq: JSON.parse(JSON.stringify(default_eq)),
                    volenv: JSON.parse(JSON.stringify(get_default_volenv())),
                    reversed: false,
                    base_pitch: 48,
                };
                if (!G.contiguous_slices || G.slices.length == 0) {
                    // make a slice the old way 
                    G.slices.push(newslice);
                    G.currentSlice = G.slices.length - 1;    
                    hoveredItem = (dragdist < 0) ? 2 : 3;
                } else if (G.slices.length >= 8) {
                    // dont to anything
                } else {
                    let i=0;
                    for (;i<G.slices.length;i++) {
                        if (mouseSample <= G.slices[i].start) {
                            break;
                        }
                    }
                    // insert a slice before slice i
                    if (i<G.slices.length) {
                        newslice.end=G.slices[i].start;
                        hoveredItem=2;
                    } else { 
                        newslice.start=G.slices[i-1].end;
                        hoveredItem=3;
                    }
                    G.slices.splice(i, 0, newslice);
                    G.currentSlice = i;
                }
                updateSliceList();
                slice = G.slices[G.currentSlice];
            }

        }
        function clampslice() {
            slice.end = Math.max(0, Math.min(G.the_audio.length, slice.end));
            slice.start = Math.max(0, Math.min(slice.end, slice.start));
            enforceSliceConstraints();
        }
        clampslice();
        if (hoveredItem === 5) {
            slice.start = find_best_snap_point_near(Math.max(0, Math.min(slice.end - 1, hoveredStart + mouseSample - hoveredMouseSample)));
            slice.end = find_best_snap_point_near(Math.max(0, Math.max(slice.start + 1, hoveredEnd + mouseSample - hoveredMouseSample)));
            clampslice();
            updateAudioDisplay();
        }
        if (hoveredItem === 2) {
            slice.start = find_best_snap_point_near(Math.max(0, Math.min(slice.end - 1, mouseSample)));
            clampslice();
            updateAudioDisplay();
        }
        else if (hoveredItem === 3) {
            slice.end = find_best_snap_point_near(Math.max(0, Math.max(slice.start + 1, mouseSample)));
            clampslice();
            updateAudioDisplay();
        }
        else if (hoveredItem === 1) {
            // Update the coordinates of the nearest point
            // if (nearestPoint === 0)
            //     slice.start = Math.max(0, Math.min(slice.end - 1, mouseSample));
            // else if (nearestPoint === points.length - 1)
            //     slice.end = Math.max(0, Math.max(slice.start + 1, mouseSample));
            // else 
            if (nearestPoint > 0 && nearestPoint < points.length - 1) {
                points[nearestPoint].x = Math.max(points[nearestPoint - 1].x, Math.min(mousePos.x, points[nearestPoint + 1].x));
            }
            points[nearestPoint].y = Math.max(0, Math.min(mousePos.y, 1));
            updateAudioDisplay();
        } else if (hoveredItem == 4 && pointToLeft !== -1 && pointToLeft < points.length - 1) {
            // Update the power of the point to the left
            const p1 = points[pointToLeft];
            const p2 = points[pointToLeft + 1];
            if (p1.x < p2.x) {
                const eps = 0.0001;
                const t = Math.min(1 - eps, Math.max(eps, (mousePos.x - p1.x) / (p2.x - p1.x)));
                const miny = Math.min(p1.y, p2.y) + 1 / wfcanvas.height;
                const maxy = Math.max(p1.y, p2.y) - 1 / wfcanvas.height;
                const clampedY = Math.max(miny, Math.min(mousePos.y, maxy));
                p1.power = Math.log((clampedY - p1.y) / (p2.y - p1.y)) / Math.log(t);
                if (mousePos.y > maxy + 0.1 || mousePos.y < miny - 0.1) {
                    // you've slammed into the vertical limits hard, maybe you want to create a new point?
                    const newPoint = {
                        x: mousePos.x,
                        y: mousePos.y,
                        power: points[pointToLeft].power
                    };
                    points.splice(pointToLeft + 1, 0, newPoint);
                    nearestPoint = pointToLeft + 1;
                    hoveredItem = 1;
                    pointToLeft = -1;
                }
                updateAudioDisplay();
            }
        }
    }
}
wfcanvas.addEventListener('pointermove', (event) => {
    do_mouse_move(event);
});
let isFirefox = navigator.userAgent.toLowerCase().indexOf('firefox') > -1;
if (isFirefox)
    document.getElementById('help').innerHTML = 'Please click here before dropping files.';
document.getElementById('help').addEventListener('click', makeAudioContext);

wfcanvas.addEventListener('pointerdown', (event) => {
    console.log("pointerdown");
    makeAudioContext();
    event.preventDefault();
    wfcanvas.setPointerCapture(event.pointerId);
    do_mouse_move(event, true);
    isDragging = true;
    pointerDownTime = Date.now();
    pointerDownX = event.offsetX;
});

wfcanvas.addEventListener('pointerup', (event) => {
    event.preventDefault();
    wfcanvas.releasePointerCapture(event.pointerId);
    isDragging = false;
    if (hoveredItem == 6) {
        selectSlice(hoveredSlice);
    }
    enforceSliceConstraints();
    //console.log("pointer up", G.slices.length, G.slices[0].start, G.slices[0].end, G.slices[1].start, G.slices[1].end);

});

wfcanvas.addEventListener('dblclick', (event) => {
    if (G.currentSlice >= G.slices.length) return;
    const slice = G.currentSlice < G.slices.length ? G.slices[G.currentSlice] : default_slice;
    const points = slice.volenv;
    console.log("dblclick");
    const startx = (slice.start - G.first_sample) / G.zoom;
    const endx = (slice.end - G.first_sample) / G.zoom;
    const width = endx - startx;
    const rect = wfcanvas.getBoundingClientRect();
    const mousePos = {
        x: ((event.clientX - rect.left) * dpr - startx) / width,
        y: 1 - ((event.clientY - rect.top) * dpr) / wfcanvas.height * 2
    };
    const splitSample = find_best_snap_point_near(mouseSample);
    if (hoveredItem == 5 && splitSample > slice.start && splitSample < slice.end) {
        // split the current spliace
        const newSlice = JSON.parse(JSON.stringify(slice));
        newSlice.start = splitSample;
        G.slices.splice(G.currentSlice + 1, 0, newSlice);
        slice.end = splitSample;
        if (newSlice.end - newSlice.start > slice.end - slice.start)
            G.currentSlice++;
        updateSliceList();
        updateAudioDisplay();
        return;
    }
    if (hoveredItem == 1 && nearestPoint !== -1 && nearestPoint !== 0 && nearestPoint !== points.length - 1) {
        console.log("delete point");
        // Delete the nearest point if it is not the first or last point
        points.splice(nearestPoint, 1);
        nearestPoint = -1;
    } else if (nearestPoint === -1 && pointToLeft !== -1) {
        // Insert a new point after pointToLeft
        const newPoint = {
            x: mousePos.x,
            y: mousePos.y,
            power: points[pointToLeft].power
        };
        points.splice(pointToLeft + 1, 0, newPoint);
    }
    console.log("end dblclick");
    updateAudioDisplay();
});


document.addEventListener('dragover', (event) => {
    event.preventDefault();
});

document.addEventListener('drop', async (event) => {
    console.log("drop");
    makeAudioContext();
    event.preventDefault();
    if (event.dataTransfer.items) {
        const old_audio_length = G.the_audio.length;
        G.currentSlice = G.slices.length;
        const files = [];
        for (let item of event.dataTransfer.items) {
            if (item.kind === 'file') {
                const file = item.getAsFile();
                if (file)
                    files.push(file);
            }
        }
        for (let file of files) {
            console.log("ok im going to do it", file);
            await processAudioFile(file);
        }
        const new_audio_length = G.the_audio.length;
        ensureSliceVisible(old_audio_length, new_audio_length, old_audio_length==0);
        updateSliceList();
        updateAudioDisplay();
    }
});
let lastwheelevent = 0;
wfcanvas.addEventListener('wheel', (event) => {
    lastwheelevent = event.timeStamp;
    event.preventDefault();
    if (event.deltaY !== 0) {
        // Zoom adjustment
        mouseX = event.offsetX * dpr;
        mouseSample = G.first_sample + mouseX * G.zoom;
        const max_zoom = G.the_audio.length / wfcanvas.width * 5; // let them zoom out until only 20% of the viewport is filled
        const was_bigger = G.zoom > max_zoom;
        G.zoom *= Math.pow(1.1, -event.deltaY / 100);
        if (!was_bigger)
            G.zoom = Math.min(max_zoom, G.zoom);
        G.zoom = Math.max(0.1, G.zoom);
        G.first_sample = mouseSample - mouseX * G.zoom;
    } else if (event.deltaX !== 0) {
        G.first_sample += event.deltaX * G.zoom;
    }
    const margin = 10 * dpr * G.zoom;
    const minfs = -wfcanvas.width * G.zoom + margin;
    const maxfs = G.the_audio.length - margin;
    G.first_sample = Math.max(minfs, Math.min(G.first_sample, maxfs));

    computeBrightnessArray();
    renderAudio();
});

function makeAudioContext() {
    if (!audioContext) {
        if (isFirefox)
            document.getElementById('help').innerHTML = 'Now drop one or more audio files here to edit them.';
        audioContext = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: sampleRate });
    }
}

async function processAudioFile(file) {
    console.log("processAudioFile");
    if (isFirefox && !audioContext)
        return;
    makeAudioContext();
    const arrayBuffer = await file.arrayBuffer();
    let audioBuffer;
    try {
        audioBuffer = await audioContext.decodeAudioData(arrayBuffer);        
    } catch (error) {
        // not an audio file?
        console.log("error decoding audio data", error);
        return;
    } 
    const monoAudio = mixDownToMono(audioBuffer);
    const resampledAudio = G.wavetablemode ? await resampleAudio(monoAudio, audioBuffer.sampleRate, sampleRate) : monoAudio;
    for (let i = 0; i < resampledAudio.length; i++) {
        //resampledAudio[i] = Math.atanh(Math.max(-1, Math.min(1, resampledAudio[i])) * 0.97) / 0.97; // undo the soft clipper :) this actually makes the full range +-2
        if (isNaN(resampledAudio[i])) {
            resampledAudio[i] = 0;
            console.log("nan at", i);
        }
    }
    G.filename_list.push({ name: file.name, start: G.the_audio.length, end: G.the_audio.length + resampledAudio.length });
    const new_audio = new Float32Array(G.the_audio.length + resampledAudio.length);
    new_audio.set(G.the_audio);
    const start = G.the_audio.length;
    const end = new_audio.length;
    new_audio.set(resampledAudio, start);
    G.the_audio = new_audio;
    //let bpm = sampleRate / resampledAudio.length * 60;
    //while (bpm < 90) bpm *= 2;
    //while (bpm > 180) bpm /= 2;
    if (1) G.slices.push(
        {
            name: file.name,
            start: start, end: end,
            gain: 0,
            eq: JSON.parse(JSON.stringify(default_eq)),
            volenv: JSON.parse(JSON.stringify(get_default_volenv())),
            reversed: false,
            base_pitch: 48,
        },
    );

}

function updateAudioDisplay() {
    computeBrightnessArray();
    if (!isPlaying && !anim_requested) { // if playing, it renders anyway.
        anim_requested = true;
        requestAnimationFrame(renderAudio);
    }
}

function mixDownToMono(audioBuffer) {
    const numberOfChannels = audioBuffer.numberOfChannels;
    const length = audioBuffer.length;
    const mono = new Float32Array(length);
    for (let i = 0; i < numberOfChannels; i++) {
        const channelData = audioBuffer.getChannelData(i);
        for (let j = 0; j < length; j++) {
            mono[j] += channelData[j] / numberOfChannels;
        }
    }
    return mono;
}

async function resampleAudio(audioData, originalSampleRate, targetSampleRate) {
    if (originalSampleRate === targetSampleRate) {
        return audioData;
    }

    const ratio = targetSampleRate / originalSampleRate;
    const targetLength = Math.round(audioData.length * ratio);
    const resampledAudio = new Float32Array(targetLength);

    const offlineContext = new OfflineAudioContext(1, targetLength, targetSampleRate);
    const buffer = offlineContext.createBuffer(1, audioData.length, originalSampleRate);
    buffer.copyToChannel(audioData, 0);

    const source = offlineContext.createBufferSource();
    source.buffer = buffer;
    source.connect(offlineContext.destination);
    source.start(0);

    const renderedBuffer = await offlineContext.startRendering();
    renderedBuffer.copyFromChannel(resampledAudio, 0);

    return resampledAudio;
}

function computeBrightnessArray() {
    
    // also update minimap
    const minimap = new Uint8Array(2048);
    const start = G.slices.length > 0 ? G.slices[0].start : 0;
    const end = Math.min(G.the_audio.length, start + 2 * 1024 * 1024);
    let gain=1;
    let curslice = -1;
    let next_slice_start = G.slices.length > 0 ? G.slices[0].start : end;
    let cur_slice_end = end;
    for (let i=start;i<end;i+=1024) {
        if (i>=next_slice_start) {
            curslice++;
            next_slice_start = (curslice < G.slices.length - 1) ? G.slices[curslice+1].start : end;
            cur_slice_end = (curslice < G.slices.length) ? G.slices[curslice].end : end;
            gain = db2lin(G.slices[curslice].gain);
        }
        if (i>=cur_slice_end) 
            gain=1;
        let max = 0;
        for (let j=0;j<1024;j++) {
            max = Math.max(max, Math.abs(G.the_audio[i+j] * gain));
        }
        minimap[(i-start)/1024] = (max*16)|0;
    }
    tab_states[selectedTab].minimap = minimap;
    renderMiniCanvas(selectedTab, minimap);
    
    imageData = null;
    brightnessArray = null;
    default_slice.start = 0;
    default_slice.end = 1;
    const slice = G.currentSlice < G.slices.length ? G.slices[G.currentSlice] : default_slice;
    const points = slice.volenv;
    const pregain = db2lin(slice.gain);
    const width = wfcanvas.width;
    const height = wfcanvas.height;
    brightnessArray = new Uint8ClampedArray(width * height * 4);
    const N = Math.max(1, G.the_audio.length);
    const W = width;
    const H = height;
    const startx = (slice.start - G.first_sample) / G.zoom;
    const endx = (slice.end - G.first_sample) / G.zoom;
    const swidth = Math.max(endx - startx, 0.00001);
    let curpoint = 0;
    for (let x = 0; x < W; x++) {
        let sampleIndex = G.first_sample + x * G.zoom;
        let postgain = null;
        let thezoom = G.zoom / 100;
        if (x >= startx && x < endx) {
            const fracx = (x - startx) / swidth;
            while (curpoint < points.length - 2 && points[curpoint + 1].x < fracx) curpoint++;
            const p1 = points[curpoint];
            const p2 = points[curpoint + 1];
            const t = (fracx - p1.x) / (p2.x - p1.x);
            postgain = p1.y + (p2.y - p1.y) * Math.pow(t, p1.power);
            if (slice.reversed) {
                thezoom = -thezoom;
                sampleIndex = slice.end - (sampleIndex - slice.start);
            }
        }
        for (let f = 0; f < 100; f++, sampleIndex += thezoom) {
            if (sampleIndex < N - 1 && sampleIndex >= 0) {
                const sample0 = G.the_audio[sampleIndex | 0];
                const sample1 = G.the_audio[(sampleIndex + 1) | 0];
                let sample = sample0 + (sample1 - sample0) * (sampleIndex - (sampleIndex | 0));
                if (postgain != null) {
                    sample = /*Math.tanh*/(sample * pregain) * postgain;
                } else {
                    sample = /*Math.tanh*/(sample);
                }
                if (sample < -1) sample = -1;
                else if (sample>1) sample = 1;
                const yf = ((1 - sample) / 2) * (H - 1);
                const y = yf | 0;
                const index = (y * W + x) * 4;
                const i = (yf - y) * 16;
                brightnessArray[index + 3] += 16 - i; // alpha
                brightnessArray[index + 3 + W * 4] += i; // alpha
            }
        }
    }
    for (let i = 3; i < W * H * 4; i += 4) {
        brightnessArray[i] = (brightnessArray[i] * 255 / (brightnessArray[i] + 16)) | 0;
    }
    imageData = new ImageData(brightnessArray, width, height);
}
let anim_requested = false;
function renderAudio(playheadPosition = null) {
    anim_requested = false;
    drawEQCurve();
    const width = wfcanvas.width;

    if (!imageData)
        computeBrightnessArray();
    if (!imageData)
        return;
    const height = wfcanvas.height;
    wfctx.putImageData(imageData, 0, 0);
    const slice = G.currentSlice < G.slices.length ? G.slices[G.currentSlice] : default_slice;
    const points = slice.volenv;
    wfctx.globalCompositeOperation = 'darken';
    wfctx.fillStyle = "lightgray";
    wfctx.strokeStyle = "gray";
    wfctx.lineWidth = 0.5;
    wfctx.font = (8 * dpr) + 'px Arial';
    for (let i = 0; i < G.filename_list.length; i++) {
        const start = G.filename_list[i].start;
        const end = G.filename_list[i].end;
        const name = G.filename_list[i].name;
        const startx = (start - G.first_sample) / G.zoom;
        // wfctx.moveTo(startx, 0);
        // wfctx.lineTo(startx, height);
        // wfctx.stroke();
        wfctx.fillText(name, startx + 4, 12 * dpr);
    }
    wfctx.strokeStyle = "black";
    wfctx.lineWidth = 3;
    for (let slicei = 0; slicei < G.slices.length; slicei++) {
    
        const otherslice = G.slices[slicei];
        const alpha = 0.1; // (hoveredItem == 6 && hoveredSlice == slicei) ? 0.25 : 0.1;
        wfctx.fillStyle = `hsl(${slicei * 45}, 5%, 60%, ${alpha})`;
        const startx = (otherslice.start - G.first_sample) / G.zoom;
        const endx = (otherslice.end - G.first_sample) / G.zoom;
        if (G.pitched) {
            wfctx.font = (60 * dpr) + 'px Arial';
            wfctx.fillText(midiToNoteName(otherslice.base_pitch), startx + 4, height - 4);
        }

        if (slicei == G.currentSlice) continue;
        wfctx.fillRect(startx, 0, endx - startx, height);
        wfctx.moveTo(startx, 0);
        wfctx.lineTo(startx, height);
        wfctx.moveTo(endx, 0);
        wfctx.lineTo(endx, height);
        wfctx.stroke();
    }

    wfctx.fillStyle = `hsl(${G.currentSlice * 45}, 70%, 50%, 0.1)`;
    wfctx.strokeStyle = `hsl(${G.currentSlice * 45}, 70%, 50%)`;
    fullcol = `hsl(${G.currentSlice * 45}, 70%, 50%)`;
    const startx = (slice.start - G.first_sample) / G.zoom;
    const endx = (slice.end - G.first_sample) / G.zoom;
    wfctx.fillRect(startx, 0, endx - startx, height);
    // if (slice.type == 'drumloop') {
    //     const dx = (sampleRate / slice.bpm * 60) / G.zoom;
    //     if (dx > 2) for (let x = startx; x < endx; x += dx) {
    //         const newx = Math.min(endx, x + dx * 0.5);
    //         wfctx.fillRect(x, 0, newx - x, height);
    //     }
    // }
    // else 
    wfctx.lineWidth = (hoveredItem == 2) ? 4 : 2;
    wfctx.beginPath();
    wfctx.moveTo(startx, 0);
    wfctx.lineTo(startx, height);
    wfctx.stroke();
    wfctx.lineWidth = (hoveredItem == 3) ? 4 : 2;
    wfctx.beginPath();
    wfctx.moveTo(endx, 0);
    wfctx.lineTo(endx, height);
    wfctx.stroke();
    wfctx.lineWidth = 1;
    //wfctx.strokeRect(startx, -5, endx - startx, height + 10);
    wfctx.globalCompositeOperation = 'source-over';
    drawCurve(points, startx, endx, fullcol);
    if (isPlaying && analyserNode) {
        renderFrequencyAnalysis();
    }

    if (playheadPosition !== null && isPlaying) {
        wfctx.lineWidth = 1;
        wfctx.strokeStyle = fullcol;
        wfctx.beginPath();
        wfctx.moveTo(playheadPosition, 0);
        wfctx.lineTo(playheadPosition, height);
        wfctx.stroke();
    }
}

function renderFrequencyAnalysis() {
    analyserNode.getFloatFrequencyData(frequencyData);
    const width = fftcanvas.width;
    const height = fftcanvas.height;
    const numBins = frequencyData.length;
    fftctx.clearRect(0, 0, width, height);
    if (!G.slices.length) return;
    const slice = G.slices[G.currentSlice];
    fftctx.fillStyle = `hsl(${G.currentSlice * 45}, 70%, 50%, 0.1)`;
    fftctx.strokeStyle = `hsl(${G.currentSlice * 45}, 70%, 50%, 0.8)`;
    fftctx.beginPath();
    fftctx.moveTo(0, height);
    for (let i = 0; i < numBins; i++) {
        if (frequencyData[i] > frequencyDatapeak[i]) {
            frequencyDatapeak[i] = frequencyData[i];
            frequencyDatahold[i] = 100;
        } else {
            frequencyDatahold[i]--;
            if (frequencyDatahold[i] < 0) {
                frequencyDatapeak[i] = frequencyData[i];
            }
        }
        const freq = (i / numBins) * 16000;
        const x = freq2x(freq);
        const magnitude = gain2y(frequencyData[i] + 30); // Adjust magnitude to fit in fftcanvas
        fftctx.lineTo(x, magnitude);
    }
    fftctx.lineTo(width, height);
    fftctx.stroke();
    fftctx.closePath();
    fftctx.fill();
    fftctx.moveTo(0, height);
    for (let i = 0; i < numBins; i++) {
        const freq = (i / numBins) * 16000;
        const x = freq2x(freq);
        const magnitude = gain2y(frequencyDatapeak[i] + 30); // Adjust magnitude to fit in fftcanvas
        fftctx.lineTo(x, magnitude);
    }
    fftctx.stroke();
}

function sinc(x) {
    return x === 0 ? 1 : Math.sin(Math.PI * x) / (Math.PI * x);
}

function generateSincCoefficients(numTaps, cutoff) {
    const coefficients = new Float32Array(numTaps);
    const center = (numTaps - 1) / 2;

    let sum = 0;
    for (let i = 0; i < numTaps; i++) {
        coefficients[i] = sinc((i - center) * cutoff);
        sum += coefficients[i];
    }

    // Normalize coefficients
    for (let i = 0; i < numTaps; i++) {
        coefficients[i] /= sum;
    }

    return coefficients;
}

const sinc_fir = generateSincCoefficients(16, 0.5);
console.log(sinc_fir);
function decimate2x(input) {
    const numTaps = sinc_fir.length;
    const sincCoefficients = sinc_fir;
    const outputLength = input.length / 2;
    const output = new Float32Array(outputLength);
    for (let i = 0; i < outputLength; i++) {
        let sum = 0;
        for (let j = 0; j < numTaps; j++) {
            const idx = (2 * i - j + input.length) % input.length; // Wrap index
            sum += input[idx] * sincCoefficients[j];
        }
        output[i] = sum;
    }
    return output;
}

function resample_slice(audio_slice, new_length) {
    const resampled = new Float32Array(new_length);
    for (let i = 0; i < new_length; i++) {
        const t = i / new_length;
        const sampleIndex = t * (audio_slice.length);
        const sample0 = audio_slice[sampleIndex | 0];
        const sample1 = audio_slice[((sampleIndex + 1) | 0) % audio_slice.length];
        resampled[i] = sample0 + (sample1 - sample0) * (sampleIndex - (sampleIndex | 0));
    }
    return resampled;
}

function decimate_2x(audio_slice) {
    const resampled = new Float32Array(audio_slice.length / 2);
    for (let i = 0; i < resampled.length; i++) {
        resampled[i] = audio_slice[i * 2];
    }
    return resampled;
}

function playAudio() {
    makeAudioContext();
    console.log('playAudio');
    if (!G.the_audio.length || !audioContext) return;
    if (isPlaying) {
        audioSource.stop();
        isPlaying = false;
        if (playButton) playButton.textContent = 'Play';
        //fftcanvas.style.display = 'none';
        return;
    }
    if (G.currentSlice >= G.slices.length) return;
    (async () => {
        if (!customNode) {
            console.log("LOADING WORKLET");
            await audioContext.audioWorklet.addModule('processor.js');
            customNode = new AudioWorkletNode(audioContext, 'custom-processor', {
                parameterData: {
                    gain: 1,
                }
            });
        }
        // make a slice of the_audio
        const slice = G.slices[G.currentSlice];
        const volenv = slice.volenv;
        let audio_slice = G.the_audio.slice(slice.start | 0, slice.end | 0);
        if (slice.reversed)
            audio_slice = new Float32Array(audio_slice).reverse();
        if (G.wavetablemode) {
            console.log("resampling");
            audio_slice = decimate_2x(decimate_2x(decimate_2x(resample_slice(audio_slice, 2048))));
            const new_audio_slice = new Float32Array(audio_slice.length * 256);
            for (let i = 0; i < 256; i++) {
                new_audio_slice.set(audio_slice, i * audio_slice.length);
            }
            audio_slice = new_audio_slice;
        }
        const audioBuffer = audioContext.createBuffer(1, audio_slice.length, sampleRate);
        audioBuffer.copyToChannel(audio_slice, 0);
        audioSource = audioContext.createBufferSource();
        audioSource.buffer = audioBuffer;
        console.log("samplerate", audioContext.sampleRate);
        analyserNode = audioContext.createAnalyser({ fftSize: 4096, minDecibels: -90, maxDecibels: -20, smoothingTimeConstant: 0.8 });
        analyserNode.fftSize = 4096;
        frequencyData = new Float32Array(analyserNode.frequencyBinCount);
        frequencyDatapeak = new Float32Array(analyserNode.frequencyBinCount);
        frequencyDatahold = new Int32Array(analyserNode.frequencyBinCount);
        gainvalue = db2lin(slice.gain);
        console.log("gainvalue", gainvalue);
        if (customNode) {
            customNode.disconnect();
            customNode.port.postMessage({ type: 'volumeEnvelope', dx: 1 / (slice.end - slice.start), envelope: slice.volenv, wavetablemode: G.wavetablemode });
            audioSource.connect(customNode);
            customNode.connect(analyserNode);
            setupBiquads();
            customNode.parameters.get('gain').setValueAtTime(gainvalue, 0);
        } else {
            audioSource.connect(analyserNode);
        }
        analyserNode.connect(audioContext.destination);

        audioSource.start();

        isPlaying = true;
        if (playButton) playButton.textContent = 'Stop';
        //fftcanvas.style.display = 'block';

        const startTime = audioContext.currentTime;
        const duration = audioBuffer.duration;

        function updatePlayhead() {
            if (!isPlaying) return;
            const currentTime = audioContext.currentTime - startTime;
            if (currentTime >= duration) {
                isPlaying = false;
                if (playButton) playButton.textContent = 'Play';
                //fftcanvas.style.display = 'none';
                return;
            }
            const playheadSample = currentTime * sampleRate + slice.start; // Current sample based on time and sample rate
            let playheadPosition = Math.floor((playheadSample - G.first_sample) / G.zoom);
            // const scrollx = wfcanvas.width * 0.75;
            // if (playheadPosition > scrollx && lastwheelevent < Date.now() - 1000) {
            //     playheadPosition = scrollx;
            //     G.first_sample = playheadSample - playheadPosition * G.zoom;
            //     if (G.first_sample < 0) {
            //         G.first_sample = 0;
            //         playheadPosition = Math.floor((playheadSample - G.first_sample) / G.zoom);
            //     }
            //     imageData = null;
            // }
            renderAudio(playheadPosition);
            anim_requested = true;
            requestAnimationFrame(updatePlayhead);
        }
        updatePlayhead();
        audioSource.onended = () => {
            isPlaying = false;
            if (playButton) playButton.textContent = 'Play';
            //fftcanvas.style.display = 'none';
            renderAudio();
        };

    })();

}
if (playButton) playButton.addEventListener('click', playAudio);



