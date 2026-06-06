<script setup lang="ts">
import { computed } from 'vue';
import { scopeStore } from '../stores/scope';

const selCh = computed(() => scopeStore.state.selectedChannel);
const hasSelection = computed(() => selCh.value >= 0);
const ui = computed(() => hasSelection.value ? scopeStore.channelUI[selCh.value] : null);
const color = computed(() => ui.value?.color ?? '#666');

// Scale presets: 1-2-5 sequence across a wide range
const SCALE_STEPS = [
  0.001, 0.002, 0.005,
  0.01, 0.02, 0.05,
  0.1, 0.2, 0.5,
  1, 2, 5,
  10, 20, 50,
  100, 200, 500,
  1000, 2000, 5000,
  10000, 20000, 50000,
];

const scaleIndex = computed({
  get() {
    if (!ui.value) return 9; // default = 1
    // Find closest step
    let best = 0;
    let bestDist = Infinity;
    for (let i = 0; i < SCALE_STEPS.length; i++) {
      const dist = Math.abs(Math.log(SCALE_STEPS[i]) - Math.log(ui.value.vScale));
      if (dist < bestDist) { bestDist = dist; best = i; }
    }
    return best;
  },
  set(idx: number) {
    if (ui.value) {
      ui.value.vScale = SCALE_STEPS[idx];
    }
  },
});

const scaleLabel = computed(() => {
  if (!ui.value) return '---';
  const s = ui.value.vScale;
  if (s >= 1000) return (s / 1000).toFixed(0) + 'k/div';
  if (s >= 1) return s.toFixed(s >= 10 ? 0 : s >= 1 ? 1 : 2) + '/div';
  if (s >= 0.001) return (s * 1000).toFixed(0) + 'm/div';
  return s.toExponential(1) + '/div';
});

function onOffsetChange(e: Event) {
  if (ui.value) {
    ui.value.vOffset = Number((e.target as HTMLInputElement).value) / 100;
  }
}

function onScaleWheel(e: WheelEvent) {
  e.preventDefault();
  if (!hasSelection.value) return;
  const dir = e.deltaY < 0 ? 1 : -1; // scroll up = increase index = coarser scale
  const newIdx = Math.max(0, Math.min(SCALE_STEPS.length - 1, scaleIndex.value + dir));
  scaleIndex.value = newIdx;
}

function onOffsetWheel(e: WheelEvent) {
  e.preventDefault();
  if (!ui.value) return;
  const step = e.deltaY < 0 ? 0.1 : -0.1; // scroll up = move trace up
  ui.value.vOffset = Math.max(-5, Math.min(5, ui.value.vOffset + step));
}

function onAutoScale() {
  if (!ui.value || selCh.value < 0) return;
  const s = scopeStore.state.samples.find(s => s.channel === selCh.value);
  if (!s || s.data.length === 0) return;
  // Find data range
  let min = Infinity, max = -Infinity;
  for (let i = 0; i < s.data.length; i++) {
    if (s.data[i] < min) min = s.data[i];
    if (s.data[i] > max) max = s.data[i];
  }
  let range = max - min;
  if (range === 0) range = Math.abs(max) || 1;
  const target = range / 8;
  const exp = Math.floor(Math.log10(target));
  const base = Math.pow(10, exp);
  const norm = target / base;
  let scale: number;
  if (norm <= 1) scale = base;
  else if (norm <= 2) scale = 2 * base;
  else if (norm <= 5) scale = 5 * base;
  else scale = 10 * base;
  ui.value.vScale = scale || 1;
  const mid = (min + max) / 2;
  ui.value.vOffset = -(mid / ui.value.vScale);
}
</script>

<template>
  <div class="vert-strip" :class="{ disabled: !hasSelection }">
    <div class="strip-header">
      <span class="strip-color" :style="{ background: color }"></span>
      <button v-if="hasSelection" class="btn-auto" @click="onAutoScale" title="Auto-fit scale">A</button>
    </div>
    <div class="strip-slider">
      <span class="strip-label">Scale</span>
      <input
        type="range" :min="0" :max="SCALE_STEPS.length - 1" step="1"
        :value="scaleIndex"
        @input="scaleIndex = Number(($event.target as HTMLInputElement).value)"
        @wheel.prevent="onScaleWheel"
        :disabled="!hasSelection"
        class="vslider"
        orient="vertical"
      />
      <span class="strip-value">{{ scaleLabel }}</span>
    </div>
    <div class="strip-slider">
      <span class="strip-label">Pos</span>
      <input
        type="range" min="-500" max="500" step="1"
        :value="Math.round((ui?.vOffset ?? 0) * 100)"
        @input="onOffsetChange"
        @wheel.prevent="onOffsetWheel"
        :disabled="!hasSelection"
        class="vslider"
        orient="vertical"
      />
      <span class="strip-value">{{ ui ? ui.vOffset.toFixed(1) : '---' }}</span>
    </div>
  </div>
</template>

<style scoped>
.vert-strip {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
  padding: 4px 2px;
  width: 48px;
  flex-shrink: 0;
  border-left: 1px solid #333;
  border-right: 1px solid #333;
  background: #151515;
}

.vert-strip.disabled {
  opacity: 0.35;
}

.strip-header {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 3px;
}

.strip-color {
  width: 12px;
  height: 12px;
  border-radius: 2px;
}

.btn-auto {
  background: #333;
  color: #ccc;
  border: 1px solid #555;
  border-radius: 3px;
  cursor: pointer;
  padding: 1px 5px;
  font-size: 9px;
  font-weight: 700;
}

.btn-auto:hover {
  background: #444;
}

.strip-slider {
  display: flex;
  flex-direction: column;
  align-items: center;
  flex: 1;
  min-height: 0;
}

.strip-label {
  font-size: 9px;
  color: #888;
  white-space: nowrap;
}

.vslider {
  -webkit-appearance: slider-vertical;
  appearance: slider-vertical;
  writing-mode: vertical-lr;
  direction: rtl; /* top = max, bottom = min */
  flex: 1;
  width: 20px;
  min-height: 60px;
  accent-color: #4af;
  cursor: pointer;
}

.vslider:disabled {
  cursor: not-allowed;
}

.strip-value {
  font-size: 9px;
  font-family: monospace;
  color: #aaa;
  white-space: nowrap;
  text-align: center;
  max-width: 46px;
  overflow: hidden;
  text-overflow: ellipsis;
}
</style>
