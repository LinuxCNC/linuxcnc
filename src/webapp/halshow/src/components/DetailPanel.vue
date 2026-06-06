<script setup lang="ts">
import { ref } from 'vue';
import { halshowStore } from '../stores/halshow';
import type { PinInfo, ParamInfo, SignalInfo } from '../generated/halcmd_client';

const editValue = ref('');
const editing = ref(false);
const setResult = ref('');

function startEdit(value: string) {
  editValue.value = value;
  editing.value = true;
  setResult.value = '';
}

async function submitValue() {
  const item = halshowStore.state.selectedItem;
  const kind = halshowStore.state.selectedItemKind;
  if (!item || !kind) return;

  let itemKind: 'pin' | 'param' | 'signal';
  if (kind === 'pins') itemKind = 'pin';
  else if (kind === 'params') itemKind = 'param';
  else if (kind === 'signals') itemKind = 'signal';
  else return;

  const result = await halshowStore.setValue(item.name, editValue.value, itemKind);
  if (result.success) {
    setResult.value = 'OK';
    editing.value = false;
    // Re-fetch to update displayed value
    if (halshowStore.state.selectedNode) {
      halshowStore.selectNode(halshowStore.state.selectedNode);
    }
  } else {
    setResult.value = result.error ?? 'Failed';
  }
}

function cancelEdit() {
  editing.value = false;
  setResult.value = '';
}

async function doUnlink() {
  const item = halshowStore.state.selectedItem as PinInfo;
  if (!item) return;
  const result = await halshowStore.unlinkPin(item.name);
  setResult.value = result.success ? 'Unlinked' : (result.error ?? 'Failed');
  if (result.success && halshowStore.state.selectedNode) {
    halshowStore.selectNode(halshowStore.state.selectedNode);
  }
}

function addToWatch() {
  const item = halshowStore.state.selectedItem;
  if (item) {
    halshowStore.addToWatch(item.name);
  }
}

function isItemWatched(): boolean {
  const item = halshowStore.state.selectedItem;
  return item ? halshowStore.isWatched(item.name) : false;
}
</script>

<template>
  <div class="detail-panel">
    <div v-if="!halshowStore.state.selectedItem" class="empty">
      Select an item from the tree to view details.
      <br /><br />
      Double-click to add to Watch list.
    </div>

    <!-- Pin detail -->
    <template v-else-if="halshowStore.state.selectedItemKind === 'pins'">
      <div class="detail-header">Pin</div>
      <table class="detail-table">
        <tr><td class="label">Name</td><td class="value mono">{{ (halshowStore.state.selectedItem as PinInfo).name }}</td></tr>
        <tr><td class="label">Type</td><td class="value">{{ (halshowStore.state.selectedItem as PinInfo).type }}</td></tr>
        <tr><td class="label">Direction</td><td class="value">{{ (halshowStore.state.selectedItem as PinInfo).dir }}</td></tr>
        <tr><td class="label">Value</td><td class="value mono">{{ (halshowStore.state.selectedItem as PinInfo).value }}</td></tr>
        <tr><td class="label">Owner</td><td class="value">{{ (halshowStore.state.selectedItem as PinInfo).owner }}</td></tr>
        <tr><td class="label">Linked</td><td class="value">{{ (halshowStore.state.selectedItem as PinInfo).linked ? 'Yes → ' + (halshowStore.state.selectedItem as PinInfo).signal : 'No' }}</td></tr>
        <tr v-if="(halshowStore.state.selectedItem as PinInfo).alias"><td class="label">Alias</td><td class="value mono">{{ (halshowStore.state.selectedItem as PinInfo).alias }}</td></tr>
      </table>
      <div class="actions">
        <button @click="startEdit((halshowStore.state.selectedItem as PinInfo).value)">Set Value</button>
        <button v-if="(halshowStore.state.selectedItem as PinInfo).linked" @click="doUnlink">Unlink</button>
        <button :class="{ watched: isItemWatched() }" @click="addToWatch">{{ isItemWatched() ? '✓ Watched' : '+ Watch' }}</button>
      </div>
    </template>

    <!-- Param detail -->
    <template v-else-if="halshowStore.state.selectedItemKind === 'params'">
      <div class="detail-header">Parameter</div>
      <table class="detail-table">
        <tr><td class="label">Name</td><td class="value mono">{{ (halshowStore.state.selectedItem as ParamInfo).name }}</td></tr>
        <tr><td class="label">Type</td><td class="value">{{ (halshowStore.state.selectedItem as ParamInfo).type }}</td></tr>
        <tr><td class="label">Direction</td><td class="value">{{ (halshowStore.state.selectedItem as ParamInfo).dir }}</td></tr>
        <tr><td class="label">Value</td><td class="value mono">{{ (halshowStore.state.selectedItem as ParamInfo).value }}</td></tr>
        <tr><td class="label">Owner</td><td class="value">{{ (halshowStore.state.selectedItem as ParamInfo).owner }}</td></tr>
      </table>
      <div class="actions">
        <button v-if="(halshowStore.state.selectedItem as ParamInfo).dir === 'RW'" @click="startEdit((halshowStore.state.selectedItem as ParamInfo).value)">Set Value</button>
        <button :class="{ watched: isItemWatched() }" @click="addToWatch">{{ isItemWatched() ? '✓ Watched' : '+ Watch' }}</button>
      </div>
    </template>

    <!-- Signal detail -->
    <template v-else-if="halshowStore.state.selectedItemKind === 'signals'">
      <div class="detail-header">Signal</div>
      <table class="detail-table">
        <tr><td class="label">Name</td><td class="value mono">{{ (halshowStore.state.selectedItem as SignalInfo).name }}</td></tr>
        <tr><td class="label">Type</td><td class="value">{{ (halshowStore.state.selectedItem as SignalInfo).type }}</td></tr>
        <tr><td class="label">Value</td><td class="value mono">{{ (halshowStore.state.selectedItem as SignalInfo).value }}</td></tr>
        <tr v-if="(halshowStore.state.selectedItem as SignalInfo).writers.length">
          <td class="label">Writers</td>
          <td class="value mono">{{ (halshowStore.state.selectedItem as SignalInfo).writers.join(', ') }}</td>
        </tr>
        <tr v-if="(halshowStore.state.selectedItem as SignalInfo).readers.length">
          <td class="label">Readers</td>
          <td class="value mono">{{ (halshowStore.state.selectedItem as SignalInfo).readers.join(', ') }}</td>
        </tr>
        <tr v-if="(halshowStore.state.selectedItem as SignalInfo).bidirs.length">
          <td class="label">Bidirs</td>
          <td class="value mono">{{ (halshowStore.state.selectedItem as SignalInfo).bidirs.join(', ') }}</td>
        </tr>
      </table>
      <div class="actions">
        <button @click="startEdit((halshowStore.state.selectedItem as SignalInfo).value)">Set Value</button>
        <button :class="{ watched: isItemWatched() }" @click="addToWatch">{{ isItemWatched() ? '✓ Watched' : '+ Watch' }}</button>
      </div>
    </template>

    <!-- Component detail -->
    <template v-else-if="halshowStore.state.selectedItemKind === 'components'">
      <div class="detail-header">Component</div>
      <table class="detail-table">
        <tr><td class="label">Name</td><td class="value mono">{{ halshowStore.state.selectedItem.name }}</td></tr>
        <tr><td class="label">ID</td><td class="value">{{ (halshowStore.state.selectedItem as any).id }}</td></tr>
        <tr><td class="label">Type</td><td class="value">{{ (halshowStore.state.selectedItem as any).type }}</td></tr>
        <tr><td class="label">State</td><td class="value">{{ (halshowStore.state.selectedItem as any).state }}</td></tr>
        <tr v-if="(halshowStore.state.selectedItem as any).pid"><td class="label">PID</td><td class="value">{{ (halshowStore.state.selectedItem as any).pid }}</td></tr>
      </table>
    </template>

    <!-- Function detail -->
    <template v-else-if="halshowStore.state.selectedItemKind === 'functions'">
      <div class="detail-header">Function</div>
      <table class="detail-table">
        <tr><td class="label">Name</td><td class="value mono">{{ halshowStore.state.selectedItem.name }}</td></tr>
        <tr><td class="label">Owner</td><td class="value">{{ (halshowStore.state.selectedItem as any).owner }}</td></tr>
        <tr><td class="label">Users</td><td class="value">{{ (halshowStore.state.selectedItem as any).users }}</td></tr>
        <tr><td class="label">Runtime</td><td class="value">{{ (halshowStore.state.selectedItem as any).runtime }} ns</td></tr>
        <tr><td class="label">FP</td><td class="value">{{ (halshowStore.state.selectedItem as any).fp ? 'Yes' : 'No' }}</td></tr>
      </table>
    </template>

    <!-- Thread detail -->
    <template v-else-if="halshowStore.state.selectedItemKind === 'threads'">
      <div class="detail-header">Thread</div>
      <table class="detail-table">
        <tr><td class="label">Name</td><td class="value mono">{{ halshowStore.state.selectedItem.name }}</td></tr>
        <tr><td class="label">Period</td><td class="value">{{ (halshowStore.state.selectedItem as any).period }} ns</td></tr>
        <tr><td class="label">FP</td><td class="value">{{ (halshowStore.state.selectedItem as any).fp ? 'Yes' : 'No' }}</td></tr>
        <tr v-if="(halshowStore.state.selectedItem as any).functions?.length">
          <td class="label">Functions</td>
          <td class="value mono">{{ (halshowStore.state.selectedItem as any).functions.join(', ') }}</td>
        </tr>
      </table>
    </template>

    <!-- Set Value dialog -->
    <div v-if="editing" class="edit-overlay">
      <div class="edit-box">
        <label>New value:</label>
        <input
          v-model="editValue"
          class="edit-input"
          @keydown.enter="submitValue"
          @keydown.escape="cancelEdit"
          autofocus
        />
        <div class="edit-actions">
          <button @click="submitValue">Set</button>
          <button @click="cancelEdit">Cancel</button>
        </div>
      </div>
    </div>

    <div v-if="setResult" class="result-msg" :class="{ error: setResult !== 'OK' && setResult !== 'Unlinked' }">
      {{ setResult }}
    </div>
  </div>
</template>

<style scoped>
.detail-panel {
  position: relative;
}

.empty {
  color: #666;
  font-style: italic;
  padding: 20px;
}

.detail-header {
  font-size: 14px;
  font-weight: 600;
  color: #4af;
  margin-bottom: 10px;
  padding-bottom: 4px;
  border-bottom: 1px solid #333;
}

.detail-table {
  width: 100%;
  border-collapse: collapse;
}

.detail-table td {
  padding: 3px 8px;
  vertical-align: top;
}

.detail-table .label {
  color: #888;
  width: 90px;
  white-space: nowrap;
}

.detail-table .value {
  color: #ccc;
  word-break: break-all;
}

.detail-table .value.mono {
  font-family: monospace;
  font-size: 12px;
}

.actions {
  margin-top: 12px;
  display: flex;
  gap: 6px;
}

.actions button {
  background: #222;
  color: #ccc;
  border: 1px solid #444;
  border-radius: 3px;
  padding: 4px 10px;
  font-size: 12px;
  cursor: pointer;
}

.actions button:hover {
  background: #2a4a6a;
  border-color: #4a8abf;
}

.actions button.watched {
  background: #1a3a2a;
  color: #4f4;
  border-color: #484;
}

.edit-overlay {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.7);
  display: flex;
  align-items: flex-start;
  justify-content: center;
  padding-top: 40px;
}

.edit-box {
  background: #1a1a1a;
  border: 1px solid #444;
  border-radius: 4px;
  padding: 12px;
  min-width: 250px;
}

.edit-box label {
  display: block;
  font-size: 12px;
  color: #888;
  margin-bottom: 6px;
}

.edit-input {
  width: 100%;
  background: #222;
  border: 1px solid #555;
  border-radius: 3px;
  padding: 5px 8px;
  color: #fff;
  font-family: monospace;
  font-size: 13px;
  margin-bottom: 8px;
}

.edit-input:focus {
  outline: none;
  border-color: #4a8abf;
}

.edit-actions {
  display: flex;
  gap: 6px;
  justify-content: flex-end;
}

.edit-actions button {
  background: #222;
  color: #ccc;
  border: 1px solid #444;
  border-radius: 3px;
  padding: 4px 12px;
  font-size: 12px;
  cursor: pointer;
}

.edit-actions button:first-child {
  background: #2a4a6a;
  border-color: #4a8abf;
}

.edit-actions button:hover {
  background: #3a5a7a;
}

.result-msg {
  margin-top: 8px;
  padding: 4px 8px;
  border-radius: 3px;
  font-size: 12px;
  background: #1a3a1a;
  color: #4f4;
}

.result-msg.error {
  background: #3a1a1a;
  color: #f44;
}
</style>
