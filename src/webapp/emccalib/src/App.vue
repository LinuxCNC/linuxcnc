<script setup lang="ts">
import { calibStore } from './stores/calib';
import TunableTable from './components/TunableTable.vue';

const state = calibStore.state;
</script>

<template>
  <div class="app">
    <header class="toolbar">
      <h1>EMC Calibration</h1>
      <div class="toolbar-actions">
        <button @click="calibStore.loadTunables()" :disabled="state.loading">
          Refresh
        </button>
        <button @click="calibStore.saveAll()" :disabled="state.saving || state.sections.length === 0" class="btn-save">
          Save to INI
        </button>
      </div>
    </header>

    <div v-if="state.error" class="error-bar">{{ state.error }}</div>
    <div v-if="state.saveMessage" class="success-bar">{{ state.saveMessage }}</div>

    <div v-if="state.loading && state.sections.length === 0" class="loading">
      Loading tunables...
    </div>

    <div v-else-if="state.sections.length === 0" class="empty">
      No tunable parameters discovered. Ensure HAL files use <code>setp pin [SECTION]KEY</code> syntax.
    </div>

    <template v-else>
      <nav class="tabs">
        <button
          v-for="section in state.sections"
          :key="section.name"
          :class="['tab', { active: state.activeTab === section.name }]"
          @click="calibStore.setActiveTab(section.name)"
        >
          {{ section.name }}
        </button>
      </nav>

      <div class="tab-content">
        <TunableTable
          v-for="section in state.sections"
          v-show="state.activeTab === section.name"
          :key="section.name"
          :section="section"
        />
      </div>
    </template>
  </div>
</template>

<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  background: #1e1e1e;
  color: #d4d4d4;
}

.app {
  display: flex;
  flex-direction: column;
  height: 100vh;
}

.toolbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 8px 16px;
  background: #2d2d2d;
  border-bottom: 1px solid #3e3e3e;
}

.toolbar h1 {
  font-size: 16px;
  font-weight: 600;
}

.toolbar-actions {
  display: flex;
  gap: 8px;
}

button {
  padding: 4px 12px;
  background: #3c3c3c;
  color: #d4d4d4;
  border: 1px solid #555;
  border-radius: 3px;
  cursor: pointer;
  font-size: 13px;
}

button:hover:not(:disabled) {
  background: #4c4c4c;
}

button:disabled {
  opacity: 0.5;
  cursor: default;
}

.btn-save {
  background: #0e639c;
  border-color: #1177bb;
}

.btn-save:hover:not(:disabled) {
  background: #1177bb;
}

.error-bar {
  padding: 6px 16px;
  background: #5a1d1d;
  color: #f48771;
  font-size: 13px;
}

.success-bar {
  padding: 6px 16px;
  background: #1d3a1d;
  color: #89d185;
  font-size: 13px;
}

.loading, .empty {
  padding: 32px;
  text-align: center;
  color: #888;
}

.tabs {
  display: flex;
  gap: 0;
  background: #252526;
  border-bottom: 1px solid #3e3e3e;
  overflow-x: auto;
}

.tab {
  padding: 8px 16px;
  border: none;
  border-bottom: 2px solid transparent;
  background: transparent;
  color: #888;
  font-size: 13px;
  white-space: nowrap;
  border-radius: 0;
}

.tab:hover {
  color: #d4d4d4;
  background: transparent;
}

.tab.active {
  color: #d4d4d4;
  border-bottom-color: #0e639c;
}

.tab-content {
  flex: 1;
  overflow-y: auto;
  padding: 16px;
}
</style>
