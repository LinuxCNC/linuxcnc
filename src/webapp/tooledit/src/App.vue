<script setup lang="ts">
import ToolTable from './components/ToolTable.vue';
import { toolStore } from './stores/tools';
import { onMounted } from 'vue';

onMounted(() => {
  toolStore.loadTools();
});
</script>

<template>
  <div class="app">
    <header class="toolbar">
      <h1>Tool Editor</h1>
      <div class="actions">
        <button @click="toolStore.reloadTable()">Reload</button>
      </div>
    </header>
    <div v-if="toolStore.state.error" class="error-bar">
      {{ toolStore.state.error }}
      <button @click="toolStore.state.error = null">✕</button>
    </div>
    <div v-if="toolStore.state.loading" class="loading">Loading...</div>
    <ToolTable v-else />
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
  font-size: 13px;
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
  border-bottom: 1px solid #404040;
  flex-shrink: 0;
}

.toolbar h1 {
  font-size: 16px;
  font-weight: 500;
}

.actions {
  display: flex;
  gap: 8px;
}

button {
  padding: 4px 12px;
  border: 1px solid #555;
  border-radius: 3px;
  background: #3c3c3c;
  color: #d4d4d4;
  cursor: pointer;
  font-size: 12px;
}

button:hover {
  background: #4a4a4a;
}

button:disabled {
  opacity: 0.5;
  cursor: default;
}

.error-bar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 6px 16px;
  background: #5a1d1d;
  color: #f48771;
  font-size: 12px;
  flex-shrink: 0;
}

.error-bar button {
  border: none;
  background: none;
  color: #f48771;
  font-size: 14px;
}

.loading {
  padding: 32px;
  text-align: center;
  color: #888;
}
</style>
