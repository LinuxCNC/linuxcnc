<script setup lang="ts">
import { halshowStore } from '../stores/halshow';
import type { PinInfo } from '../generated/halcmd_client';

function getNodePins(): PinInfo[] {
  return halshowStore.state.nodeOverviewPins;
}

function addToWatch(name: string) {
  halshowStore.addToWatch(name);
}

function addAllToWatch() {
  halshowStore.addAllNodePinsToWatch();
}

function allWatched(): boolean {
  const pins = getNodePins();
  return pins.length > 0 && pins.every(p => halshowStore.isWatched(p.name));
}
</script>

<template>
  <div class="node-overview">
    <div class="overview-header">
      <span>{{ halshowStore.state.selectedNode?.fullPath }} ({{ getNodePins().length }} items)</span>
      <button v-if="getNodePins().length > 0" :class="{ watched: allWatched() }" @click="addAllToWatch">{{ allWatched() ? '✓ All Watched' : '+ Watch All' }}</button>
    </div>

    <div v-if="getNodePins().length === 0" class="empty">
      No pins under this node.
    </div>

    <table v-else class="overview-table">
      <thead>
        <tr>
          <th>Name</th>
          <th>Value</th>
          <th>Type</th>
          <th>Dir</th>
          <th>Signal</th>
          <th></th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="pin in getNodePins()" :key="pin.name">
          <td class="name">{{ pin.name }}</td>
          <td class="value">{{ pin.value }}</td>
          <td class="type">{{ pin.type }}</td>
          <td class="dir">{{ pin.dir ?? '' }}</td>
          <td class="signal">{{ pin.linked ? pin.signal : '' }}</td>
          <td class="action">
            <button :class="{ watched: halshowStore.isWatched(pin.name) }" @click="addToWatch(pin.name)" title="Add to watch">{{ halshowStore.isWatched(pin.name) ? '✓' : '+W' }}</button>
          </td>
        </tr>
      </tbody>
    </table>
  </div>
</template>

<style scoped>
.node-overview {
  font-size: 12px;
}

.overview-header {
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

.overview-header button {
  background: #222;
  color: #999;
  border: 1px solid #444;
  border-radius: 3px;
  padding: 2px 8px;
  font-size: 11px;
  cursor: pointer;
}

.overview-header button:hover {
  background: #1a3a2a;
  color: #4f4;
  border-color: #484;
}

.empty {
  color: #666;
  font-style: italic;
  padding: 12px 0;
}

.overview-table {
  width: 100%;
  border-collapse: collapse;
}

.overview-table th {
  text-align: left;
  color: #888;
  padding: 4px 6px;
  border-bottom: 1px solid #333;
  font-weight: normal;
}

.overview-table td {
  padding: 3px 6px;
  border-bottom: 1px solid #222;
}

.overview-table .name {
  font-family: monospace;
  color: #ccc;
  max-width: 200px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.overview-table .value {
  font-family: monospace;
  color: #4f4;
  font-weight: 600;
}

.overview-table .type {
  color: #888;
}

.overview-table .dir {
  color: #888;
}

.overview-table .signal {
  font-family: monospace;
  color: #fa4;
}

.overview-table .action button {
  background: none;
  border: none;
  color: #666;
  cursor: pointer;
  font-size: 11px;
  padding: 0 4px;
}

.overview-table .action button:hover {
  color: #4af;
}

.overview-table .action button.watched {
  color: #4f4;
}

.overview-header button.watched {
  background: #1a3a2a;
  color: #4f4;
  border-color: #484;
}
</style>
