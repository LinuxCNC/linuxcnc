<script setup lang="ts">
import { ref, computed, watch } from 'vue';
import { scopeStore } from '../stores/scope';
import { HalType } from '../generated/halscope_client';

const pinSearch = ref('');
const showPinList = ref(false);
const kindFilter = ref(''); // '', 'pin', 'sig', 'param'

const enabledChannels = computed(() =>
  scopeStore.state.status.channels.filter(c => c.enabled)
);

const canAddChannel = computed(() => {
  const maxCh = scopeStore.state.status.maxChannels;
  return enabledChannels.value.length < maxCh && nextFreeChannel.value >= 0;
});

const nextFreeChannel = computed(() => {
  const used = new Set(enabledChannels.value.map(c => c.channel));
  const maxCh = scopeStore.state.status.maxChannels;
  for (let i = 0; i < maxCh; i++) {
    if (!used.has(i)) return i;
  }
  return -1;
});

function halTypeName(t: HalType): string {
  const names: Record<number, string> = {
    [HalType.BIT]: 'bit',
    [HalType.FLOAT]: 'float',
    [HalType.S32]: 's32',
    [HalType.U32]: 'u32',
    [HalType.S64]: 's64',
    [HalType.U64]: 'u64',
  };
  return names[t] ?? '?';
}

async function openPinBrowser() {
  showPinList.value = true;
  pinSearch.value = '';
  kindFilter.value = '';
  await doSearch();
}

async function doSearch() {
  const text = pinSearch.value.trim();
  const pattern = text ? `*${text}*` : '*';
  const kind = kindFilter.value || undefined;
  await scopeStore.searchPins(pattern, kind);
}

async function selectPin(pin: string) {
  const ch = nextFreeChannel.value;
  if (ch < 0) return;
  await scopeStore.addChannel(pin, ch);
  showPinList.value = false;
  pinSearch.value = '';
}

async function onRemoveChannel(channel: number) {
  await scopeStore.removeChannel(channel);
}

// Debounce search
let searchTimeout: ReturnType<typeof setTimeout>;
watch(pinSearch, () => {
  clearTimeout(searchTimeout);
  searchTimeout = setTimeout(doSearch, 250);
});

// Re-search when kind filter changes
watch(kindFilter, () => doSearch());
</script>

<template>
  <div class="channel-panel">
    <div class="panel-header">
      <span>Channels</span>
      <button class="btn btn-sm" @click="openPinBrowser" :disabled="!canAddChannel">
        + Add
      </button>
    </div>

    <!-- Active channels -->
    <div class="channel-list">
      <div
        v-for="ch in enabledChannels"
        :key="ch.channel"
        class="channel-item"
        :class="{ selected: scopeStore.state.selectedChannel === ch.channel }"
        @click="scopeStore.setSelectedChannel(ch.channel)"
      >
        <span
          class="channel-color"
          :style="{ background: scopeStore.channelUI[ch.channel].color }"
        ></span>
        <span class="channel-name" :title="ch.pinName">
          {{ ch.pinName }}
        </span>
        <span class="channel-type">{{ halTypeName(ch.dataType) }}</span>
        <label class="channel-vis">
          <input
            type="checkbox"
            :checked="scopeStore.channelUI[ch.channel].visible"
            @change="scopeStore.channelUI[ch.channel].visible = ($event.target as HTMLInputElement).checked"
          />
        </label>
        <button class="btn-icon" @click="onRemoveChannel(ch.channel)" title="Remove">✕</button>
      </div>
      <div v-if="enabledChannels.length === 0" class="empty-hint">
        No channels configured
      </div>
    </div>

    <!-- Pin browser dialog -->
    <div v-if="showPinList" class="pin-browser">
      <div class="pin-browser-header">
        <input
          v-model="pinSearch"
          placeholder="Search pins, signals, params..."
          class="pin-search"
          autofocus
        />
        <button class="btn-icon" @click="showPinList = false">✕</button>
      </div>
      <div class="kind-filter">
        <label class="kind-btn" :class="{ active: kindFilter === '' }">
          <input type="radio" v-model="kindFilter" value="" /> All
        </label>
        <label class="kind-btn" :class="{ active: kindFilter === 'pin' }">
          <input type="radio" v-model="kindFilter" value="pin" /> Pins
        </label>
        <label class="kind-btn" :class="{ active: kindFilter === 'sig' }">
          <input type="radio" v-model="kindFilter" value="sig" /> Signals
        </label>
        <label class="kind-btn" :class="{ active: kindFilter === 'param' }">
          <input type="radio" v-model="kindFilter" value="param" /> Params
        </label>
      </div>
      <div class="pin-list">
        <div
          v-for="pin in scopeStore.state.pins"
          :key="pin"
          class="pin-item"
          @click="selectPin(pin)"
        >
          {{ pin }}
        </div>
        <div v-if="!scopeStore.state.pins?.length" class="empty-hint">
          No matching HAL objects
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.channel-panel {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.panel-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-weight: 600;
  font-size: 13px;
  padding: 4px 0;
  border-bottom: 1px solid #333;
}

.channel-list {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.channel-item {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 3px 4px;
  font-size: 12px;
  border-radius: 3px;
}

.channel-item:hover {
  background: #222;
}

.channel-item.selected {
  background: #1a2a3a;
  outline: 1px solid #4af;
}

.channel-color {
  width: 10px;
  height: 10px;
  border-radius: 2px;
  flex-shrink: 0;
}

.channel-name {
  flex: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-family: monospace;
}

.channel-type {
  color: #888;
  font-size: 11px;
  flex-shrink: 0;
}

.channel-vis {
  flex-shrink: 0;
}

.btn-icon {
  background: none;
  border: none;
  color: #888;
  cursor: pointer;
  padding: 0 2px;
  font-size: 12px;
}

.btn-icon:hover {
  color: #ff4444;
}

.btn {
  background: #333;
  color: #ccc;
  border: 1px solid #555;
  border-radius: 3px;
  cursor: pointer;
  padding: 2px 8px;
  font-size: 12px;
}

.btn:hover {
  background: #444;
}

.btn:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.btn-sm {
  font-size: 11px;
  padding: 1px 6px;
}

.pin-browser {
  border: 1px solid #555;
  border-radius: 4px;
  background: #1a1a1a;
  max-height: 300px;
  display: flex;
  flex-direction: column;
  margin-top: 4px;
}

.pin-browser-header {
  display: flex;
  gap: 4px;
  padding: 4px;
  border-bottom: 1px solid #333;
}

.kind-filter {
  display: flex;
  gap: 2px;
  padding: 3px 4px;
  border-bottom: 1px solid #333;
}

.kind-btn {
  display: inline-flex;
  align-items: center;
  gap: 2px;
  padding: 2px 6px;
  border-radius: 3px;
  font-size: 11px;
  color: #999;
  cursor: pointer;
}

.kind-btn input[type="radio"] {
  display: none;
}

.kind-btn.active {
  background: #333;
  color: #eee;
}

.kind-btn:hover {
  background: #2a2a2a;
}

.pin-search {
  flex: 1;
  background: #222;
  border: 1px solid #444;
  border-radius: 3px;
  color: #ccc;
  padding: 3px 6px;
  font-size: 12px;
  font-family: monospace;
}

.pin-list {
  overflow-y: auto;
  max-height: 250px;
}

.pin-item {
  padding: 3px 8px;
  font-size: 12px;
  font-family: monospace;
  cursor: pointer;
}

.pin-item:hover {
  background: #333;
}

.empty-hint {
  color: #666;
  font-size: 12px;
  padding: 8px;
  text-align: center;
}
</style>
