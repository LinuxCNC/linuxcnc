<script setup lang="ts">
import { computed } from 'vue';
import ScopeToolbar from './components/ScopeToolbar.vue';
import ScopeChart from './components/ScopeChart.vue';
import ChannelPanel from './components/ChannelPanel.vue';
import BufferIndicator from './components/BufferIndicator.vue';
import VerticalControls from './components/VerticalControls.vue';
import { scopeStore } from './stores/scope';

function formatReal(v: number): string {
  const abs = Math.abs(v);
  if (abs >= 1000) return (v / 1000).toFixed(2) + 'k';
  if (abs >= 1) return v.toFixed(3);
  if (abs >= 0.001) return (v * 1000).toFixed(2) + 'm';
  if (abs >= 0.000001) return (v * 1e6).toFixed(1) + 'µ';
  if (v === 0) return '0';
  return v.toExponential(2);
}

const dragInfo = computed(() => {
  const st = scopeStore.state;
  if (!st.isDragging) return '';
  const parts: string[] = [];
  if (st.dragDeltaTime !== null) {
    parts.push('Δt=' + scopeStore.formatTimeValue(st.dragDeltaTime));
  }
  if (st.dragDeltaValue !== null) {
    parts.push('Δy=' + formatReal(st.dragDeltaValue));
  }
  if (st.dragStartTime !== null && st.dragStartValue !== null) {
    parts.push('from ' + scopeStore.formatTimeValue(st.dragStartTime) + ' / ' + formatReal(st.dragStartValue));
  }
  return parts.join('   ');
});
</script>

<template>
  <div class="app">
    <ScopeToolbar />
    <div class="main-area">
      <div class="chart-area">
        <BufferIndicator />
        <ScopeChart />
        <div class="cursor-bar">{{ dragInfo || '\u00A0' }}</div>
      </div>
      <VerticalControls />
      <div class="side-panel">
        <ChannelPanel />
      </div>
    </div>
  </div>
</template>

<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

html, body, #app {
  width: 100%;
  height: 100%;
  background: #111;
  color: #ccc;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
  font-size: 13px;
}

.app {
  display: flex;
  flex-direction: column;
  height: 100vh;
}

.main-area {
  flex: 1;
  display: flex;
  overflow: hidden;
}

.chart-area {
  flex: 1;
  padding: 8px;
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.side-panel {
  width: 280px;
  flex-shrink: 0;
  padding: 8px;
  border-left: 1px solid #333;
  overflow-y: auto;
}

.cursor-bar {
  background: #1a1a1a;
  border: 1px solid #333;
  border-radius: 3px;
  padding: 2px 8px;
  font-family: monospace;
  font-size: 12px;
  color: #4af;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  flex-shrink: 0;
}
</style>
