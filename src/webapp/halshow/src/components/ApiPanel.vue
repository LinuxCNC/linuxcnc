<script setup lang="ts">
import { halshowStore, type ApiInfo } from '../stores/halshow';
import { computed } from 'vue';

// Build tree: instance → apis[]
const apiTree = computed(() => {
  const map = new Map<string, ApiInfo[]>();
  for (const api of halshowStore.state.apiRegistry) {
    const list = map.get(api.instance) || [];
    list.push(api);
    map.set(api.instance, list);
  }
  return Array.from(map.entries()).sort((a, b) => a[0].localeCompare(b[0]));
});

const selectedApi = computed(() => halshowStore.state.selectedApi);

function selectApi(api: ApiInfo) {
  halshowStore.selectApi(api);
}

function isSelected(api: ApiInfo): boolean {
  const s = selectedApi.value;
  return s !== null && s.api_name === api.api_name && s.instance === api.instance;
}
</script>

<template>
  <div class="api-panel">
    <div class="api-layout">
      <!-- Left: tree -->
      <div class="api-tree">
        <div v-if="apiTree.length === 0" class="empty">
          <span v-if="!halshowStore.state.connected">Connecting...</span>
          <span v-else>No APIs registered</span>
        </div>
        <div v-for="[instance, apis] in apiTree" :key="instance" class="instance-group">
          <div class="instance-name">{{ instance }}</div>
          <div
            v-for="api in apis"
            :key="api.api_name + ':' + api.instance"
            class="api-leaf"
            :class="{ selected: isSelected(api) }"
            @click="selectApi(api)"
          >
            <span class="api-name">{{ api.api_name }}</span>
            <span class="api-badge" v-if="api.rest">REST</span>
            <span class="api-badge ws" v-if="api.watches && api.watches.length">WS</span>
            <span class="api-version">v{{ api.version }}</span>
          </div>
        </div>
      </div>

      <!-- Right: detail -->
      <div class="api-detail">
        <div v-if="!selectedApi" class="empty">
          Select an API from the tree to view details.
        </div>
        <template v-else>
          <div class="detail-header">{{ selectedApi.api_name }} @ {{ selectedApi.instance }}</div>

          <!-- Consumers (always first) -->
          <div class="detail-section">
            <div class="section-title">Consumers</div>
            <div class="consumer-list" v-if="selectedApi.consumers && selectedApi.consumers.length">
              <span v-for="c in selectedApi.consumers" :key="c" class="consumer-tag">{{ c }}</span>
            </div>
            <div v-else class="empty-note">No recorded consumers</div>
          </div>

          <!-- REST endpoints -->
          <div class="detail-section" v-if="selectedApi.functions && selectedApi.functions.length">
            <div class="section-title">Functions</div>
            <table class="func-table">
              <thead>
                <tr><th>Name</th><th>Method</th><th>Path</th></tr>
              </thead>
              <tbody>
                <tr v-for="fn in selectedApi.functions" :key="fn.name">
                  <td class="mono">{{ fn.name }}</td>
                  <td><span class="method-badge" v-if="fn.method">{{ fn.method }}</span></td>
                  <td class="mono path">
                    <a v-if="fn.method === 'GET' && fn.path" :href="fn.path" target="_blank">{{ fn.path }}</a>
                    <span v-else>{{ fn.path || '—' }}</span>
                  </td>
                </tr>
              </tbody>
            </table>
          </div>

          <!-- WebSocket watches -->
          <div class="detail-section" v-if="selectedApi.watches && selectedApi.watches.length">
            <div class="section-title">WebSocket Watches</div>
            <table class="func-table">
              <thead>
                <tr><th>Name</th><th>Default Rate</th></tr>
              </thead>
              <tbody>
                <tr v-for="w in selectedApi.watches" :key="w.name">
                  <td class="mono">{{ w.name }}</td>
                  <td>{{ w.default_rate_ms }}ms</td>
                </tr>
              </tbody>
            </table>
          </div>

          <!-- WebSocket commands -->
          <div class="detail-section" v-if="selectedApi.commands && selectedApi.commands.length">
            <div class="section-title">WebSocket Commands</div>
            <div class="command-list">
              <span v-for="cmd in selectedApi.commands" :key="cmd" class="command-tag">{{ cmd }}</span>
            </div>
          </div>
        </template>
      </div>
    </div>
  </div>
</template>

<style scoped>
.api-panel {
  display: flex;
  flex: 1;
  min-height: 0;
  overflow: hidden;
}

.api-layout {
  display: flex;
  flex: 1;
  min-height: 0;
}

.api-tree {
  width: 300px;
  flex-shrink: 0;
  overflow-y: auto;
  border-right: 1px solid #333;
  background: #151515;
  padding: 4px 0;
  font-size: 12px;
}

.api-detail {
  flex: 1;
  overflow-y: auto;
  min-height: 0;
  background: #1a1a1a;
  padding: 12px;
}

.instance-group {
  margin-bottom: 4px;
}

.instance-name {
  padding: 3px 8px;
  color: #4af;
  font-weight: 600;
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.api-leaf {
  padding: 3px 8px 3px 20px;
  cursor: pointer;
  display: flex;
  align-items: center;
  gap: 6px;
}

.api-leaf:hover {
  background: #222;
}

.api-leaf.selected {
  background: #2a3a4a;
}

.api-name {
  color: #ccc;
}

.api-badge {
  background: #2a4a2a;
  color: #8f8;
  font-size: 9px;
  padding: 1px 4px;
  border-radius: 3px;
  font-weight: 600;
}

.api-badge.ws {
  background: #2a3a4a;
  color: #8cf;
}

.api-version {
  color: #666;
  font-size: 10px;
  margin-left: auto;
}

.detail-header {
  color: #4af;
  font-size: 14px;
  font-weight: 600;
  margin-bottom: 12px;
  padding-bottom: 6px;
  border-bottom: 1px solid #333;
}

.detail-section {
  margin-bottom: 16px;
}

.section-title {
  color: #999;
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  margin-bottom: 6px;
}

.consumer-list, .command-list {
  display: flex;
  flex-wrap: wrap;
  gap: 4px;
}

.consumer-tag {
  background: #2a2a3a;
  color: #aaf;
  padding: 2px 8px;
  border-radius: 3px;
  font-size: 12px;
}

.command-tag {
  background: #3a2a2a;
  color: #faa;
  padding: 2px 8px;
  border-radius: 3px;
  font-size: 12px;
  font-family: monospace;
}

.func-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 12px;
}

.func-table th {
  text-align: left;
  color: #888;
  font-weight: normal;
  padding: 2px 8px 2px 0;
  border-bottom: 1px solid #333;
}

.func-table td {
  padding: 3px 8px 3px 0;
  border-bottom: 1px solid #222;
}

.mono {
  font-family: monospace;
  color: #ccc;
}

.path {
  color: #8c8;
}

.method-badge {
  background: #333;
  color: #fc8;
  padding: 1px 4px;
  border-radius: 2px;
  font-size: 10px;
  font-weight: 600;
}

.empty {
  color: #666;
  padding: 12px;
  font-style: italic;
}

.empty-note {
  color: #555;
  font-style: italic;
  font-size: 12px;
}
</style>
