<script setup lang="ts">
import { computed } from 'vue';
import { scopeStore } from '../stores/scope';
import { ScopeState, TrigEdge } from '../generated/halscope_client';

const stateLabel = computed(() => {
  const labels: Record<number, string> = {
    [ScopeState.IDLE]: 'Idle',
    [ScopeState.INIT]: 'Init',
    [ScopeState.PRE_TRIG]: 'Pre-trigger',
    [ScopeState.TRIG_WAIT]: 'Waiting',
    [ScopeState.POST_TRIG]: 'Capturing',
    [ScopeState.DONE]: 'Done',
    [ScopeState.RESET]: 'Reset',
  };
  return labels[scopeStore.state.status.state] ?? 'Unknown';
});

const stateClass = computed(() => {
  const s = scopeStore.state.status.state;
  if (s === ScopeState.DONE) return 'state-done';
  if (s === ScopeState.TRIG_WAIT || s === ScopeState.PRE_TRIG) return 'state-waiting';
  if (s === ScopeState.POST_TRIG) return 'state-capturing';
  return '';
});

const isRunning = computed(() => scopeStore.isCapturing());

const canArm = computed(() => {
  const s = scopeStore.state.status.state;
  const hasChannels = scopeStore.state.status.channels.some(c => c.enabled);
  return (s === ScopeState.IDLE || s === ScopeState.DONE) && hasChannels;
});

const canForce = computed(() => {
  const s = scopeStore.state.status.state;
  return s === ScopeState.PRE_TRIG || s === ScopeState.TRIG_WAIT;
});

const hasSamples = computed(() => scopeStore.state.samples.length > 0);

const trigChannels = computed(() =>
  scopeStore.state.status.channels.filter(c => c.enabled)
);

const scaleLabel = computed(() => {
  const ds = scopeStore.calcDispScale();
  if (ds === 0) return '----';
  return scopeStore.formatTimeValue(ds) + '/div';
});

const recInfo = computed(() => {
  const sp = scopeStore.getSamplePeriod();
  if (sp === 0) return '----';
  const rl = scopeStore.state.status.recLen;
  const freq = 1 / sp;
  let fStr: string;
  if (freq >= 1e6) fStr = (freq / 1e6).toFixed(1) + ' MHz';
  else if (freq >= 1e3) fStr = (freq / 1e3).toFixed(1) + ' kHz';
  else fStr = freq.toFixed(0) + ' Hz';
  return `${rl} @ ${fStr}`;
});

function onRun() {
  scopeStore.run();
}

function onStop() {
  scopeStore.stop();
}

function onSingle() {
  scopeStore.arm();
}

function onThreadChange(e: Event) {
  scopeStore.setSelectedThread((e.target as HTMLSelectElement).value);
  scopeStore.applyConfig();
}

function onCaptureConfigChange() {
  scopeStore.applyConfig();
}

function onMaxChannelsChange(e: Event) {
  const newMax = Number((e.target as HTMLSelectElement).value);
  // Check if any active channel has index >= newMax
  const activeChannels = scopeStore.state.status.channels.filter(c => c.enabled);
  const blocked = activeChannels.some(c => c.channel >= newMax);
  if (blocked) {
    scopeStore.state.error = `Cannot reduce to ${newMax} channels — remove channels ${newMax}+ first`;
    // Reset the select to the current value
    (e.target as HTMLSelectElement).value = String(scopeStore.captureConfig.maxChannels);
    return;
  }
  scopeStore.captureConfig.maxChannels = newMax;
  scopeStore.applyConfig();
}

function onTrigConfigChange() {
  scopeStore.applyConfig();
}

function onAutoTrigChange() {
  // Auto trigger can be changed even during capture
  scopeStore.setTrigger();
}

function onZoomChange(e: Event) {
  scopeStore.setHorizZoom(Number((e.target as HTMLInputElement).value));
}

function onZoomWheel(e: WheelEvent) {
  e.preventDefault();
  const dir = e.deltaY < 0 ? 1 : -1;
  scopeStore.setHorizZoom(scopeStore.state.zoomSetting + dir);
}

function onPosChange(e: Event) {
  scopeStore.setHorizPos(Number((e.target as HTMLInputElement).value) / 1000);
}

function onPosWheel(e: WheelEvent) {
  e.preventDefault();
  const step = e.deltaY < 0 ? 0.02 : -0.02;
  scopeStore.setHorizPos(scopeStore.state.posSetting + step);
}
</script>

<template>
  <div class="toolbar">
    <!-- Row 1: Run controls + state + capture config -->
    <div class="toolbar-row">
      <div class="toolbar-group">
        <span v-if="scopeStore.state.connected" class="connected-badge">●</span>
        <span v-else class="disconnected-badge">○</span>
        <span class="state-badge" :class="stateClass">{{ stateLabel }}</span>
      </div>

      <div class="toolbar-group">
        <button class="btn btn-run" @click="onRun" :disabled="!canArm">▶ Run</button>
        <button class="btn" @click="onSingle" :disabled="!canArm">⎍ Single</button>
        <button class="btn btn-stop" @click="onStop" :disabled="!isRunning">■ Stop</button>
        <button class="btn" @click="scopeStore.forceTrigger()" :disabled="!canForce">⚡ Force</button>
        <button class="btn btn-reset" @click="scopeStore.fullReset()" :disabled="isRunning">⟲ Reset</button>
        <button class="btn btn-file" @click="scopeStore.saveCapture()" :disabled="!hasSamples">💾 Save</button>
        <button class="btn btn-file" @click="scopeStore.loadCapture()">📂 Load</button>
      </div>

      <div class="toolbar-group config-group">
        <label>
          Thread
          <select
            :value="scopeStore.state.selectedThread"
            :disabled="isRunning"
            @change="onThreadChange"
          >
            <option v-for="t in scopeStore.state.threads" :key="t.name" :value="t.name">
              {{ t.name }} ({{ (t.periodNs / 1000).toFixed(0) }}µs)
            </option>
          </select>
        </label>
        <label>
          Ch/Samples
          <select
            :value="scopeStore.captureConfig.maxChannels"
            :disabled="isRunning"
            @change="onMaxChannelsChange"
          >
            <option
              v-for="opt in scopeStore.state.status.channelOptions"
              :key="opt.maxChannels"
              :value="opt.maxChannels"
            >
              {{ opt.maxChannels }} ch — {{ opt.recLen }} samples
            </option>
          </select>
        </label>
        <label>
          Mult
          <input type="number" v-model.number="scopeStore.captureConfig.samplePeriodMult"
            :disabled="isRunning" @change="onCaptureConfigChange"
            min="1" max="1000" />
        </label>
      </div>

      <div class="toolbar-group config-group">
        <label>
          Trig
          <select v-model.number="scopeStore.triggerConfig.channel"
            :disabled="isRunning" @change="onTrigConfigChange">
            <option v-for="ch in trigChannels" :key="ch.channel" :value="ch.channel">
              {{ ch.pinName || `Ch ${ch.channel}` }}
            </option>
          </select>
        </label>
        <label>
          Lvl
          <input type="number" v-model.number="scopeStore.triggerConfig.level"
            :disabled="isRunning" @change="onTrigConfigChange"
            step="0.1" />
        </label>
        <label>
          <select v-model.number="scopeStore.triggerConfig.edge"
            :disabled="isRunning" @change="onTrigConfigChange">
            <option :value="TrigEdge.RISING">↑</option>
            <option :value="TrigEdge.FALLING">↓</option>
          </select>
        </label>
        <label class="checkbox-label">
          <input type="checkbox" v-model="scopeStore.triggerConfig.autoTrig"
            @change="onAutoTrigChange" /> Auto
        </label>
      </div>

      <div v-if="scopeStore.state.error" class="error-bar" :title="scopeStore.state.error">
        {{ scopeStore.state.error }}
      </div>
    </div>

    <!-- Row 2: Horizontal controls (scale display, zoom, pos) -->
    <div class="toolbar-row horiz-row">
      <span class="scale-display">{{ scaleLabel }}</span>
      <label class="slider-label">
        Zoom
        <input
          type="range" min="1" max="9" step="1"
          :value="scopeStore.state.zoomSetting"
          @input="onZoomChange"
          @wheel.prevent="onZoomWheel"
          class="slider"
        />
      </label>
      <label class="slider-label">
        Pos
        <input
          type="range" min="0" max="1000" step="1"
          :value="Math.round(scopeStore.state.posSetting * 1000)"
          @input="onPosChange"
          @wheel.prevent="onPosWheel"
          class="slider"
        />
      </label>
      <span class="rec-info">{{ recInfo }}</span>
    </div>
  </div>
</template>

<style scoped>
.toolbar {
  display: flex;
  flex-direction: column;
  background: #1a1a1a;
  border-bottom: 1px solid #333;
  font-size: 12px;
}

.toolbar-row {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 12px;
  padding: 4px 8px;
}

.horiz-row {
  border-top: 1px solid #282828;
  padding: 3px 8px;
}

.toolbar-group {
  display: flex;
  align-items: center;
  gap: 6px;
}

.config-group label {
  display: flex;
  align-items: center;
  gap: 3px;
  color: #999;
}

.config-group input[type="number"] {
  width: 55px;
  background: #222;
  border: 1px solid #444;
  border-radius: 3px;
  color: #ccc;
  padding: 2px 4px;
  font-size: 11px;
}

.config-group select {
  background: #222;
  border: 1px solid #444;
  border-radius: 3px;
  color: #ccc;
  padding: 2px 4px;
  font-size: 11px;
  max-width: 140px;
}

.slider-label {
  display: flex;
  align-items: center;
  gap: 4px;
  color: #999;
  font-size: 11px;
}

.slider {
  width: 120px;
  height: 4px;
  accent-color: #4af;
  cursor: pointer;
}

.scale-display {
  color: #4af;
  font-weight: 600;
  font-size: 12px;
  min-width: 80px;
}

.rec-info {
  color: #888;
  font-size: 11px;
}

.checkbox-label {
  cursor: pointer;
  user-select: none;
  color: #999;
}

.btn {
  background: #333;
  color: #ccc;
  border: 1px solid #555;
  border-radius: 3px;
  cursor: pointer;
  padding: 3px 10px;
  font-size: 12px;
  white-space: nowrap;
}

.btn:hover { background: #444; }
.btn:disabled { opacity: 0.4; cursor: not-allowed; }

.btn-run { color: #4f4; border-color: #4a4; }
.btn-run:hover { background: #243; }
.btn-stop { color: #f44; border-color: #a44; }
.btn-stop:hover { background: #422; }
.btn-reset { color: #fa4; border-color: #a84; }
.btn-reset:hover { background: #432; }
.btn-file { color: #aaf; border-color: #88a; }
.btn-file:hover { background: #234; }

.connected-badge {
  color: #4f4;
  font-size: 12px;
}

.disconnected-badge {
  color: #f84;
  font-size: 12px;
}

.state-badge {
  display: inline-block;
  width: 80px;
  text-align: center;
  padding: 2px 0;
  border-radius: 3px;
  background: #333;
  font-weight: 600;
}

.state-done { color: #4f4; background: #1a2a1a; }
.state-waiting { color: #ff4; background: #2a2a1a; }
.state-capturing { color: #4af; background: #1a2a3a; }

.error-bar {
  color: #f44;
  font-size: 11px;
  padding: 2px 4px;
  max-width: 300px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
</style>
