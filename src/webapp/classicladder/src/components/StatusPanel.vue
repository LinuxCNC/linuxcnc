<script setup lang="ts">
import { modbusStore } from '../stores/modbus';
</script>

<template>
  <div class="status-panel" v-if="modbusStore.state.status">
    <div class="stat-grid">
      <div class="stat">
        <span class="stat-label">State</span>
        <span :class="modbusStore.state.status.running ? 'val-ok' : 'val-warn'">
          {{ modbusStore.state.status.running ? 'Running' : 'Stopped' }}
        </span>
      </div>
      <div class="stat">
        <span class="stat-label">Current Request</span>
        <span class="val">{{ modbusStore.state.status.currentReq }}</span>
      </div>
      <div class="stat">
        <span class="stat-label">Frames Sent</span>
        <span class="val">{{ modbusStore.state.status.frameCount }}</span>
      </div>
      <div class="stat">
        <span class="stat-label">Errors</span>
        <span :class="modbusStore.state.status.errorCount > 0 ? 'val-warn' : 'val'">
          {{ modbusStore.state.status.errorCount }}
        </span>
      </div>
      <div class="stat">
        <span class="stat-label">Slave Port</span>
        <span class="val">
          {{ modbusStore.state.status.slavePort > 0 ? modbusStore.state.status.slavePort : 'Disabled' }}
        </span>
      </div>
    </div>

    <div class="actions">
      <button class="btn" @click="modbusStore.refreshStatus()">Refresh Now</button>
    </div>
  </div>
  <div v-else class="loading">Loading status...</div>
</template>

<style scoped>
.stat-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(160px, 1fr));
  gap: 16px;
}

.stat {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.stat-label {
  font-size: 11px;
  text-transform: uppercase;
  color: #a6adc8;
}

.val {
  font-size: 20px;
  font-weight: 600;
}

.val-ok {
  font-size: 20px;
  font-weight: 600;
  color: #a6e3a1;
}

.val-warn {
  font-size: 20px;
  font-weight: 600;
  color: #f38ba8;
}

.actions {
  margin-top: 20px;
}

.loading {
  color: #a6adc8;
  text-align: center;
  padding: 24px;
}
</style>
