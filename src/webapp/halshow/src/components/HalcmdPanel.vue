<script setup lang="ts">
import { ref, nextTick } from 'vue';
import { halshowStore } from '../stores/halshow';

const input = ref('');
const historyRef = ref<HTMLElement>();

function scrollToBottom() {
  nextTick(() => {
    if (historyRef.value) {
      historyRef.value.scrollTop = historyRef.value.scrollHeight;
    }
  });
}

async function execute() {
  const cmd = input.value.trim();
  if (!cmd) return;
  input.value = '';
  await halshowStore.executeHalcmd(cmd);
  scrollToBottom();
}
</script>

<template>
  <div class="halcmd-panel">
    <div class="history" ref="historyRef">
      <div
        v-for="(entry, idx) in halshowStore.state.cmdHistory"
        :key="idx"
        class="history-entry"
      >
        <div class="cmd-line">halcmd&gt; {{ entry.cmd }}</div>
        <div v-if="entry.output" class="cmd-output">{{ entry.output }}</div>
        <div v-if="entry.error" class="cmd-error">{{ entry.error }}</div>
      </div>
    </div>
    <div class="input-row">
      <span class="prompt">halcmd&gt;</span>
      <input
        v-model="input"
        class="cmd-input"
        type="text"
        placeholder="Enter command..."
        @keydown.enter="execute"
        autofocus
      />
      <button class="clear-btn" @click="halshowStore.clearCmdHistory()" title="Clear output">Clear</button>
    </div>
  </div>
</template>

<style scoped>
.halcmd-panel {
  display: flex;
  flex-direction: column;
  height: 100%;
  font-family: monospace;
  font-size: 12px;
}

.history {
  flex: 1;
  overflow-y: auto;
  padding: 4px 8px;
}

.history-entry {
  margin-bottom: 4px;
}

.cmd-line {
  color: #4af;
}

.cmd-output {
  color: #ccc;
  white-space: pre-wrap;
  padding-left: 8px;
}

.cmd-error {
  color: #f66;
  white-space: pre-wrap;
  padding-left: 8px;
}

.input-row {
  display: flex;
  align-items: center;
  gap: 4px;
  padding: 6px 8px;
  border-top: 1px solid #333;
  background: #1a1a1a;
}

.prompt {
  color: #4af;
  flex-shrink: 0;
}

.clear-btn {
  background: #222;
  color: #999;
  border: 1px solid #444;
  border-radius: 3px;
  padding: 4px 8px;
  font-size: 11px;
  cursor: pointer;
  flex-shrink: 0;
}

.clear-btn:hover {
  background: #3a1a1a;
  color: #f88;
  border-color: #844;
}

.cmd-input {
  flex: 1;
  background: #222;
  border: 1px solid #333;
  border-radius: 3px;
  padding: 4px 8px;
  color: #fff;
  font-family: monospace;
  font-size: 12px;
}

.cmd-input:focus {
  outline: none;
  border-color: #4a8abf;
}
</style>
