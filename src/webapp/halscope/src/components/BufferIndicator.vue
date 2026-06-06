<script setup lang="ts">
/**
 * Buffer position indicator — exact port of scope_horiz.c: refresh_pos_disp().
 *
 * Canvas-drawn:
 *  - Small box (4px tall): the full record buffer, filled to current sample
 *  - Vertical line: trigger point
 *  - Large box: current chart view window (10 divs × disp_scale)
 *
 * Auto-scales so both record and view are always visible.
 * Always interactive: drag to pan, scroll to zoom, double-click to reset.
 */
import { ref, watch, onMounted, onBeforeUnmount, nextTick } from 'vue';
import { scopeStore } from '../stores/scope';

const canvas = ref<HTMLCanvasElement>();
let resizeObs: ResizeObserver | null = null;

// Drag state
let dragging = false;
let dragStartX = 0;
let dragOrigPos = 0;

/**
 * Compute all time-domain coordinates — exact port of refresh_pos_disp().
 * Everything in seconds, trigger point = time 0.
 */
function timeCoords() {
  const dw = scopeStore.calcDisplayWindow();
  const sp = dw.samplePeriod;

  // Record boundaries relative to trigger
  const recStart = -dw.preTrig * sp;
  const recEnd = (dw.recLen - dw.preTrig) * sp;
  const recCurr = recStart + scopeStore.state.status.samples * sp;

  // Display window from calcDisplayWindow (already trigger-relative)
  const dispStart = dw.screenStartTime;
  const dispEnd = dw.screenEndTime;

  // Auto-scale: range covers whichever is larger
  const min = Math.min(recStart, dispStart);
  const max = Math.max(recEnd, dispEnd);
  const span = max - min || 1;

  return { recStart, recEnd, recCurr, dispStart, dispEnd, min, span };
}

function draw() {
  const c = canvas.value;
  if (!c) return;
  const ctx = c.getContext('2d');
  if (!ctx) return;

  const dpr = window.devicePixelRatio || 1;
  const rect = c.getBoundingClientRect();
  const w = rect.width;
  const h = rect.height;
  c.width = w * dpr;
  c.height = h * dpr;
  ctx.scale(dpr, dpr);
  ctx.clearRect(0, 0, w, h);

  const tc = timeCoords();
  const scale = (w - 1) / tc.span;

  // Y geometry (matches original C code)
  const midY = Math.floor((h - 1) / 2);
  const boxHalf = Math.max(Math.floor(midY / 2), 3);
  const trigHalf = midY;

  // Pixel X positions
  const trigX   = Math.round(scale * (0 - tc.min)) + 0.5;
  const recL    = Math.round(scale * (tc.recStart - tc.min)) + 0.5;
  const recR    = Math.round(scale * (tc.recEnd - tc.min)) + 0.5;
  const recCurX = Math.round(scale * (tc.recCurr - tc.min));
  const boxL    = Math.round(scale * (tc.dispStart - tc.min));
  const boxR    = Math.round(scale * (tc.dispEnd - tc.min));

  // 1) Record bar fill (captured portion)
  if (scopeStore.state.status.samples > 0) {
    ctx.fillStyle = '#2a4a2a';
    ctx.fillRect(recL, midY - 2, recCurX - recL, 5);
  }

  // 2) Record bar outline (small box, ~4px tall)
  ctx.strokeStyle = '#999';
  ctx.lineWidth = 1;
  ctx.strokeRect(recL, midY - 2.5, recR - recL, 5);

  // 3) Trigger vertical line (full height)
  ctx.strokeStyle = '#f84';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(trigX, midY - trigHalf);
  ctx.lineTo(trigX, midY + trigHalf);
  ctx.stroke();

  // 4) View window box (large box)
  ctx.strokeStyle = '#4af';
  ctx.lineWidth = 1.5;
  ctx.strokeRect(boxL, midY - boxHalf, boxR - boxL, boxHalf * 2);
  ctx.fillStyle = 'rgba(68, 170, 255, 0.06)';
  ctx.fillRect(boxL, midY - boxHalf, boxR - boxL, boxHalf * 2);
}

// --- Interaction ---
// Drag: moves posSetting (like original horiz_motion + middle_drag)
// Scroll: changes zoomSetting (like original change_zoom)

function onMouseDown(e: MouseEvent) {
  dragging = true;
  dragStartX = e.clientX;
  dragOrigPos = scopeStore.state.posSetting;
  e.preventDefault();
  window.addEventListener('mousemove', onMouseMove);
  window.addEventListener('mouseup', onMouseUp);
}

function onMouseMove(e: MouseEvent) {
  if (!dragging) return;
  const c = canvas.value;
  if (!c) return;
  const rect = c.getBoundingClientRect();
  const tc = timeCoords();
  const scale = (rect.width - 1) / tc.span;
  const recDuration = tc.recEnd - tc.recStart;

  // Pixel delta → time delta → position fraction delta
  const dtSeconds = (e.clientX - dragStartX) / scale;
  const dPos = recDuration > 0 ? dtSeconds / recDuration : 0;
  scopeStore.setHorizPos(dragOrigPos + dPos);
}

function onMouseUp() {
  dragging = false;
  window.removeEventListener('mousemove', onMouseMove);
  window.removeEventListener('mouseup', onMouseUp);
}

function onWheel(e: WheelEvent) {
  e.preventDefault();
  // Zoom in/out by one step (like original change_zoom)
  const dir = e.deltaY < 0 ? 1 : -1;
  scopeStore.setHorizZoom(scopeStore.state.zoomSetting + dir);
}

function onDblClick() {
  scopeStore.setHorizZoom(1);
  scopeStore.setHorizPos(0.5);
}

watch(
  () => [
    scopeStore.state.status.state,
    scopeStore.state.status.samples,
    scopeStore.state.status.recLen,
    scopeStore.state.status.preTrig,
    scopeStore.state.zoomSetting,
    scopeStore.state.posSetting,
  ],
  () => draw(),
);

onMounted(() => {
  nextTick(() => draw());
  resizeObs = new ResizeObserver(() => draw());
  if (canvas.value) resizeObs.observe(canvas.value);
});

onBeforeUnmount(() => {
  resizeObs?.disconnect();
  window.removeEventListener('mousemove', onMouseMove);
  window.removeEventListener('mouseup', onMouseUp);
});
</script>

<template>
  <canvas
    ref="canvas"
    class="buffer-bar"
    @mousedown="onMouseDown"
    @wheel.prevent="onWheel"
    @dblclick="onDblClick"
  ></canvas>
</template>

<style scoped>
.buffer-bar {
  width: 100%;
  height: 24px;
  flex-shrink: 0;
  cursor: grab;
  background: #111;
  border: 1px solid #333;
  border-radius: 2px;
}

.buffer-bar:active {
  cursor: grabbing;
}
</style>
