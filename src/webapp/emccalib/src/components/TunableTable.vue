<script setup lang="ts">
import { calibStore } from '../stores/calib';
import type { TunableSection, TunableItem } from '../generated/emccalib_client';

defineProps<{
  section: TunableSection;
}>();

function getEditValue(item: TunableItem): string {
  return calibStore.getEdit(item.section, item.key) ?? '';
}

function onInput(item: TunableItem, event: Event) {
  const target = event.target as HTMLInputElement;
  calibStore.setEdit(item.section, item.key, target.value);
}

function hasEdit(item: TunableItem): boolean {
  return calibStore.getEdit(item.section, item.key) !== undefined;
}

function isModified(item: TunableItem): boolean {
  return item.value !== item.ini_value;
}
</script>

<template>
  <table class="tunable-table">
    <thead>
      <tr>
        <th class="col-key">INI Parameter</th>
        <th class="col-pin">HAL Pin</th>
        <th class="col-ini">INI Value</th>
        <th class="col-current">Current</th>
        <th class="col-edit">New Value</th>
        <th class="col-actions">Actions</th>
      </tr>
    </thead>
    <tbody>
      <tr v-for="item in section.items" :key="item.key" :class="{ modified: isModified(item) }">
        <td class="col-key">{{ item.key }}</td>
        <td class="col-pin mono">{{ item.hal_pin }}</td>
        <td class="col-ini mono">{{ item.ini_value }}</td>
        <td class="col-current mono" :class="{ changed: isModified(item) }">
          {{ item.value }}
        </td>
        <td class="col-edit">
          <input
            type="text"
            :value="getEditValue(item)"
            :placeholder="String(item.value)"
            @input="onInput(item, $event)"
            @keyup.enter="calibStore.testValue(item)"
          />
        </td>
        <td class="col-actions">
          <div class="action-btns">
            <button @click="calibStore.testValue(item)" :disabled="!hasEdit(item)" title="Apply to HAL (live)">
              Test
            </button>
            <button @click="calibStore.revertValue(item)" :disabled="!isModified(item)" title="Revert to INI value">
              Revert
            </button>
          </div>
        </td>
      </tr>
    </tbody>
  </table>
</template>

<style scoped>
.tunable-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 13px;
}

th {
  text-align: left;
  padding: 6px 10px;
  background: #2d2d2d;
  border-bottom: 1px solid #3e3e3e;
  font-weight: 600;
  color: #888;
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

td {
  padding: 4px 10px;
  border-bottom: 1px solid #2d2d2d;
}

tr:hover {
  background: #2a2d2e;
}

tr.modified td {
  background: #1d2d1d;
}

.mono {
  font-family: 'Consolas', 'Courier New', monospace;
}

.changed {
  color: #dcdcaa;
  font-weight: 600;
}

.col-key { width: 15%; }
.col-pin { width: 25%; color: #9cdcfe; }
.col-ini { width: 12%; }
.col-current { width: 12%; }
.col-edit { width: 20%; }
.col-actions {
  width: 16%;
  white-space: nowrap;
}

.action-btns {
  display: flex;
  gap: 4px;
  align-items: center;
}

input {
  width: 100%;
  padding: 3px 6px;
  background: #3c3c3c;
  color: #d4d4d4;
  border: 1px solid #555;
  border-radius: 2px;
  font-family: 'Consolas', 'Courier New', monospace;
  font-size: 13px;
}

input:focus {
  outline: none;
  border-color: #0e639c;
}

.col-actions button {
  padding: 2px 8px;
  font-size: 12px;
}
</style>
