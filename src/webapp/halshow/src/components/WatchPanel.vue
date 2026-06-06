<script setup lang="ts">
import { ref } from 'vue';
import { halshowStore } from '../stores/halshow';

const editingName = ref('');
const editValue = ref('');
const editError = ref('');

function getWatchValue(name: string): string {
  const item = halshowStore.state.watchValues.find(v => v.name === name);
  return item?.value ?? '—';
}

function getWatchType(name: string): string {
  const item = halshowStore.state.watchValues.find(v => v.name === name);
  return item?.type ?? '';
}

function getWatchDir(name: string): string {
  const item = halshowStore.state.watchValues.find(v => v.name === name);
  return item?.dir ?? '';
}

function canSet(name: string): boolean {
  const item = halshowStore.state.watchValues.find(v => v.name === name);
  if (!item) return false;
  // Whitelist: only allow setting known-settable items
  if (item.kind === 'pin' && (item.dir === 'IN' || item.dir === 'IO') && !item.linked) return true;
  if (item.kind === 'param' && item.dir === 'RW') return true;
  if (item.kind === 'signal' && !item.linked) return true;
  return false;
}

function isBitType(name: string): boolean {
  return getWatchType(name) === 'bit';
}

function startEdit(name: string) {
  editingName.value = name;
  editValue.value = getWatchValue(name);
  editError.value = '';
}

async function submitEdit() {
  if (!editingName.value) return;
  const result = await halshowStore.setWatchValue(editingName.value, editValue.value);
  if (result.success) {
    editingName.value = '';
    editError.value = '';
  } else {
    editError.value = result.error ?? 'Failed';
  }
}

function cancelEdit() {
  editingName.value = '';
  editError.value = '';
}
</script>

<template>
  <div class="watch-panel">
    <div class="watch-header">
      <span>Watch List ({{ halshowStore.state.watchList.length }} items)</span>
      <button v-if="halshowStore.state.watchList.length > 0" @click="halshowStore.clearWatch()">Clear All</button>
    </div>

    <div v-if="halshowStore.state.watchList.length === 0" class="empty">
      No items being watched.
      <br /><br />
      Double-click items in the tree or click "+ Watch" in the Show tab to add.
    </div>

    <table v-else class="watch-table">
      <colgroup>
        <col />
        <col style="width: 140px" />
        <col style="width: 50px" />
        <col style="width: 40px" />
        <col style="width: 40px" />
        <col style="width: 30px" />
      </colgroup>
      <thead>
        <tr>
          <th>Name</th>
          <th>Value</th>
          <th>Type</th>
          <th>Dir</th>
          <th></th>
          <th></th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="name in halshowStore.state.watchList" :key="name">
          <td class="name">{{ name }}</td>
          <td class="value">{{ getWatchValue(name) }}</td>
          <td class="type">{{ getWatchType(name) }}</td>
          <td class="dir">{{ getWatchDir(name) }}</td>
          <td class="set-col">
            <button v-if="canSet(name)" class="set-btn" @click="startEdit(name)">Set</button>
          </td>
          <td class="remove">
            <button @click="halshowStore.removeFromWatch(name)">×</button>
          </td>
        </tr>
      </tbody>
    </table>

    <!-- Set Value dialog -->
    <div v-if="editingName" class="edit-overlay" @click.self="cancelEdit">
      <div class="edit-box">
        <div class="edit-title">Set {{ editingName }}</div>
        <template v-if="isBitType(editingName)">
          <div class="bit-buttons">
            <button :class="{ active: editValue === 'TRUE' }" @click="editValue = 'TRUE'">TRUE</button>
            <button :class="{ active: editValue === 'FALSE' }" @click="editValue = 'FALSE'">FALSE</button>
          </div>
        </template>
        <template v-else>
          <input
            v-model="editValue"
            class="edit-input"
            @keydown.enter="submitEdit"
            @keydown.escape="cancelEdit"
            autofocus
          />
        </template>
        <div v-if="editError" class="edit-error">{{ editError }}</div>
        <div class="edit-actions">
          <button class="apply-btn" @click="submitEdit">Apply</button>
          <button class="cancel-btn" @click="cancelEdit">Cancel</button>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.watch-panel {
  font-size: 12px;
  position: relative;
}

.watch-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 8px;
  padding-bottom: 4px;
  border-bottom: 1px solid #333;
  color: #4af;
  font-size: 13px;
  font-weight: 600;
}

.watch-header button {
  background: #222;
  color: #999;
  border: 1px solid #444;
  border-radius: 3px;
  padding: 2px 8px;
  font-size: 11px;
  cursor: pointer;
}

.watch-header button:hover {
  background: #3a1a1a;
  color: #f88;
  border-color: #844;
}

.empty {
  color: #666;
  font-style: italic;
  padding: 12px 0;
}

.watch-table {
  width: 100%;
  border-collapse: collapse;
  table-layout: fixed;
}

.watch-table th {
  text-align: left;
  color: #888;
  padding: 4px 6px;
  border-bottom: 1px solid #333;
  font-weight: normal;
}

.watch-table td {
  padding: 3px 6px;
  border-bottom: 1px solid #222;
}

.watch-table .name {
  font-family: monospace;
  color: #ccc;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.watch-table .value {
  font-family: monospace;
  color: #4f4;
  font-weight: 600;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.watch-table .type {
  color: #888;
}

.watch-table .dir {
  color: #888;
}

.watch-table .set-col {
  width: 40px;
}

.set-btn {
  background: #1a2a3a;
  color: #4af;
  border: 1px solid #346;
  border-radius: 3px;
  padding: 1px 6px;
  font-size: 10px;
  cursor: pointer;
}

.set-btn:hover {
  background: #2a4a6a;
  border-color: #4a8abf;
}

.watch-table .remove button {
  background: none;
  border: none;
  color: #666;
  cursor: pointer;
  font-size: 14px;
  padding: 0 4px;
}

.watch-table .remove button:hover {
  color: #f44;
}

/* Edit dialog */
.edit-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.6);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 100;
}

.edit-box {
  background: #1a1a1a;
  border: 1px solid #444;
  border-radius: 6px;
  padding: 16px;
  min-width: 280px;
}

.edit-title {
  font-size: 13px;
  font-weight: 600;
  color: #4af;
  margin-bottom: 12px;
  font-family: monospace;
  word-break: break-all;
}

.bit-buttons {
  display: flex;
  gap: 8px;
  margin-bottom: 12px;
}

.bit-buttons button {
  flex: 1;
  padding: 8px;
  background: #222;
  color: #999;
  border: 1px solid #444;
  border-radius: 4px;
  font-size: 13px;
  font-weight: 600;
  cursor: pointer;
}

.bit-buttons button.active {
  background: #2a4a6a;
  color: #fff;
  border-color: #4a8abf;
}

.bit-buttons button:hover:not(.active) {
  background: #2a2a2a;
  color: #ccc;
}

.edit-input {
  width: 100%;
  background: #222;
  border: 1px solid #555;
  border-radius: 3px;
  padding: 6px 8px;
  color: #fff;
  font-family: monospace;
  font-size: 13px;
  margin-bottom: 12px;
}

.edit-input:focus {
  outline: none;
  border-color: #4a8abf;
}

.edit-error {
  color: #f66;
  font-size: 11px;
  margin-bottom: 8px;
}

.edit-actions {
  display: flex;
  gap: 8px;
  justify-content: flex-end;
}

.apply-btn {
  background: #2a4a6a;
  color: #fff;
  border: 1px solid #4a8abf;
  border-radius: 3px;
  padding: 4px 12px;
  font-size: 12px;
  cursor: pointer;
}

.apply-btn:hover {
  background: #3a5a7a;
}

.cancel-btn {
  background: #222;
  color: #999;
  border: 1px solid #444;
  border-radius: 3px;
  padding: 4px 12px;
  font-size: 12px;
  cursor: pointer;
}

.cancel-btn:hover {
  background: #2a2a2a;
  color: #ccc;
}
</style>
