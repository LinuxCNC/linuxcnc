<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue';
import LadderView from './components/LadderView.vue';
import EditToolbar from './components/EditToolbar.vue';
import VarSpy from './components/VarSpy.vue';
import ComParamsPanel from './components/ComParamsPanel.vue';
import RequestsPanel from './components/RequestsPanel.vue';
import StatusPanel from './components/StatusPanel.vue';
import { ladderStore } from './stores/ladder';
import { modbusStore } from './stores/modbus';
import { ClassicladderClient, LadderState, type Status } from './generated/classicladder_client';

type Tab = 'ladder' | 'variables' | 'modbus-params' | 'modbus-requests' | 'modbus-status';
const activeTab = ref<Tab>('ladder');
const clStatus = ref<Status | null>(null);
const statusClient = new ClassicladderClient(window.location.origin);
let statusTimer: ReturnType<typeof setInterval> | null = null;

async function refreshStatus() {
  try { clStatus.value = await statusClient.getStatus(); } catch {}
}

onMounted(() => {
  ladderStore.fetchProgram();
  modbusStore.fetchAll();
  modbusStore.startPolling();
  refreshStatus();
  statusTimer = setInterval(refreshStatus, 2000);
});

onUnmounted(() => {
  modbusStore.stopPolling();
  if (statusTimer) clearInterval(statusTimer);
});

async function toggleRun() {
  if (clStatus.value?.state === LadderState.RUN) {
    await ladderStore.setState(LadderState.STOP);
  } else {
    await ladderStore.setState(LadderState.RUN);
  }
  setTimeout(refreshStatus, 200);
}
</script>

<template>
  <div class="app">
    <header class="header">
      <h1>Classic Ladder</h1>
      <div class="header-actions">
        <div class="status-indicator" v-if="clStatus">
          <span :class="clStatus.state === 2 ? 'dot-green' : 'dot-red'"></span>
          {{ clStatus.state === 2 ? 'Running' : 'Stopped' }}
        </div>
        <button class="btn btn-sm" @click="toggleRun">
          {{ clStatus?.state === 2 ? 'Stop' : 'Run' }}
        </button>
        <button class="btn btn-sm" @click="ladderStore.fetchProgram()">Reload</button>
      </div>
    </header>

    <nav class="tabs">
      <button :class="{ active: activeTab === 'ladder' }" @click="activeTab = 'ladder'">
        Ladder
      </button>
      <button :class="{ active: activeTab === 'variables' }" @click="activeTab = 'variables'">
        Variables
      </button>
      <button :class="{ active: activeTab === 'modbus-params' }" @click="activeTab = 'modbus-params'">
        Modbus Config
      </button>
      <button :class="{ active: activeTab === 'modbus-requests' }" @click="activeTab = 'modbus-requests'">
        Modbus I/O
      </button>
      <button :class="{ active: activeTab === 'modbus-status' }" @click="activeTab = 'modbus-status'">
        Modbus Status
      </button>
    </nav>

    <div class="error-bar" v-if="ladderStore.state.error || modbusStore.state.error">
      {{ ladderStore.state.error || modbusStore.state.error }}
      <button @click="ladderStore.state.error = ''; modbusStore.state.error = ''">✕</button>
    </div>

    <main class="content">
      <template v-if="activeTab === 'ladder'">
        <EditToolbar />
        <LadderView />
      </template>
      <VarSpy v-else-if="activeTab === 'variables'" />
      <ComParamsPanel v-else-if="activeTab === 'modbus-params'" />
      <RequestsPanel v-else-if="activeTab === 'modbus-requests'" />
      <StatusPanel v-else-if="activeTab === 'modbus-status'" />
    </main>
  </div>
</template>

<style>
* {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  font-size: 14px;
  background: #1e1e2e;
  color: #cdd6f4;
}

.app {
  max-width: 900px;
  margin: 0 auto;
  padding: 16px;
}

.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 16px;
}

.header h1 {
  font-size: 18px;
  font-weight: 600;
}

.header-actions {
  display: flex;
  align-items: center;
  gap: 8px;
}

.btn-sm {
  padding: 4px 10px !important;
  font-size: 12px !important;
}

.status-indicator {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 13px;
}

.dot-green, .dot-red {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  display: inline-block;
}
.dot-green { background: #a6e3a1; }
.dot-red { background: #f38ba8; }

.tabs {
  display: flex;
  gap: 2px;
  margin-bottom: 16px;
  border-bottom: 1px solid #45475a;
}

.tabs button {
  background: none;
  border: none;
  color: #a6adc8;
  padding: 8px 16px;
  cursor: pointer;
  border-bottom: 2px solid transparent;
  font-size: 13px;
}

.tabs button.active {
  color: #cdd6f4;
  border-bottom-color: #89b4fa;
}

.tabs button:hover {
  color: #cdd6f4;
}

.error-bar {
  background: #f38ba8;
  color: #1e1e2e;
  padding: 8px 12px;
  border-radius: 4px;
  margin-bottom: 12px;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.error-bar button {
  background: none;
  border: none;
  font-size: 16px;
  cursor: pointer;
  color: #1e1e2e;
}

.content {
  background: #313244;
  border-radius: 8px;
  padding: 16px;
}

/* Form elements */
label {
  display: block;
  font-size: 12px;
  color: #a6adc8;
  margin-bottom: 4px;
}

input, select {
  background: #1e1e2e;
  border: 1px solid #45475a;
  color: #cdd6f4;
  padding: 6px 10px;
  border-radius: 4px;
  font-size: 13px;
  width: 100%;
}

input:focus, select:focus {
  outline: none;
  border-color: #89b4fa;
}

button.btn {
  background: #89b4fa;
  color: #1e1e2e;
  border: none;
  padding: 8px 16px;
  border-radius: 4px;
  cursor: pointer;
  font-size: 13px;
  font-weight: 500;
}

button.btn:hover {
  background: #b4d0fb;
}

button.btn-danger {
  background: #f38ba8;
}

button.btn-danger:hover {
  background: #f5a3b8;
}
</style>
