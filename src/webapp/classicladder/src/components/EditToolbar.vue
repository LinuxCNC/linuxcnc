<script setup lang="ts">
import { ladderStore } from '../stores/ladder';

const contactTypes = [
  { type: 1, label: '| |', title: 'Normally Open Contact' },
  { type: 2, label: '|/|', title: 'Normally Closed Contact' },
  { type: 3, label: '|P|', title: 'Rising Edge Contact' },
  { type: 4, label: '|N|', title: 'Falling Edge Contact' },
];

const coilTypes = [
  { type: 50, label: '( )', title: 'Normal Coil' },
  { type: 51, label: '(/)', title: 'Negated Coil' },
  { type: 52, label: '(S)', title: 'Set Coil' },
  { type: 53, label: '(R)', title: 'Reset Coil' },
];

const otherTypes = [
  { type: 9, label: '───', title: 'Connection' },
  { type: 10, label: 'TMR', title: 'Timer' },
  { type: 11, label: 'MON', title: 'Monostable' },
  { type: 12, label: 'CTR', title: 'Counter' },
  { type: 13, label: 'IEC', title: 'Timer IEC' },
  { type: 20, label: 'CMP', title: 'Compare' },
  { type: 60, label: 'OPR', title: 'Operate' },
];

function selectTool(type: number) {
  ladderStore.setEditTool(type);
}

function deleteTool() {
  ladderStore.setEditTool(0); // ELE_FREE
}
</script>

<template>
  <div class="edit-toolbar">
    <div class="tool-group">
      <span class="group-label">Contacts</span>
      <button
        v-for="t in contactTypes"
        :key="t.type"
        :class="{ active: ladderStore.state.editTool === t.type }"
        :title="t.title"
        @click="selectTool(t.type)"
      >{{ t.label }}</button>
    </div>
    <div class="tool-group">
      <span class="group-label">Coils</span>
      <button
        v-for="t in coilTypes"
        :key="t.type"
        :class="{ active: ladderStore.state.editTool === t.type }"
        :title="t.title"
        @click="selectTool(t.type)"
      >{{ t.label }}</button>
    </div>
    <div class="tool-group">
      <span class="group-label">Blocks</span>
      <button
        v-for="t in otherTypes"
        :key="t.type"
        :class="{ active: ladderStore.state.editTool === t.type }"
        :title="t.title"
        @click="selectTool(t.type)"
      >{{ t.label }}</button>
    </div>
    <div class="tool-group">
      <button class="delete-btn" :class="{ active: ladderStore.state.editTool === 0 }" title="Delete" @click="deleteTool()">✕</button>
      <button class="action-btn" title="Toggle top connection" @click="ladderStore.toggleTopConnection()">┬</button>
    </div>
    <div class="tool-group right">
      <button class="action-btn save" @click="ladderStore.saveProgram()" :disabled="!ladderStore.state.dirty">
        Save
      </button>
    </div>
  </div>
</template>

<style scoped>
.edit-toolbar {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 8px;
  background: #1e1e2e;
  border: 1px solid #45475a;
  border-radius: 4px;
  margin-bottom: 8px;
  flex-wrap: wrap;
}

.tool-group {
  display: flex;
  align-items: center;
  gap: 2px;
}

.tool-group.right {
  margin-left: auto;
}

.group-label {
  font-size: 9px;
  color: #a6adc8;
  text-transform: uppercase;
  margin-right: 4px;
}

.tool-group button {
  background: #313244;
  border: 1px solid #45475a;
  color: #cdd6f4;
  padding: 4px 8px;
  border-radius: 3px;
  cursor: pointer;
  font-family: monospace;
  font-size: 12px;
  font-weight: 700;
  min-width: 32px;
  text-align: center;
}

.tool-group button:hover {
  background: #45475a;
}

.tool-group button.active {
  background: #89b4fa;
  color: #1e1e2e;
  border-color: #89b4fa;
}

.delete-btn {
  color: #f38ba8 !important;
}

.delete-btn.active {
  background: #f38ba8 !important;
  color: #1e1e2e !important;
  border-color: #f38ba8 !important;
}

.action-btn.save {
  background: #a6e3a1;
  color: #1e1e2e;
  border-color: #a6e3a1;
  font-family: inherit;
}

.action-btn.save:disabled {
  opacity: 0.4;
  cursor: default;
}
</style>
