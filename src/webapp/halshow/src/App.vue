<script setup lang="ts">
import HalTree from './components/HalTree.vue';
import DetailPanel from './components/DetailPanel.vue';
import NodeOverview from './components/NodeOverview.vue';
import WatchPanel from './components/WatchPanel.vue';
import HalcmdPanel from './components/HalcmdPanel.vue';
import ApiPanel from './components/ApiPanel.vue';
import StatusBar from './components/StatusBar.vue';
import { halshowStore } from './stores/halshow';
import type { TreeCategory } from './stores/halshow';

const categories: { id: TreeCategory; label: string }[] = [
  { id: 'pins', label: 'Pins' },
  { id: 'params', label: 'Params' },
  { id: 'signals', label: 'Signals' },
  { id: 'components', label: 'Components' },
  { id: 'functions', label: 'Functions' },
  { id: 'threads', label: 'Threads' },
  { id: 'api', label: 'API' },
];

function onFilterInput(e: Event) {
  halshowStore.setFilter((e.target as HTMLInputElement).value);
}
</script>

<template>
  <div class="app">
    <div class="toolbar">
      <div class="category-tabs">
        <button
          v-for="cat in categories"
          :key="cat.id"
          :class="{ active: halshowStore.state.treeCategory === cat.id }"
          @click="halshowStore.setCategory(cat.id)"
        >{{ cat.label }}</button>
      </div>
      <input
        class="filter-input"
        type="text"
        placeholder="Filter..."
        :value="halshowStore.state.treeFilter"
        @input="onFilterInput"
      />
      <button class="refresh-btn" @click="halshowStore.refresh()">Refresh</button>
    </div>
    <div class="main-area" v-if="halshowStore.state.treeCategory === 'api'">
      <ApiPanel />
    </div>
    <div class="main-area" v-else>
      <div class="tree-panel">
        <HalTree />
      </div>
      <div class="right-panel">
        <div class="tab-bar">
          <button
            :class="{ active: halshowStore.state.activeTab === 'show' }"
            @click="halshowStore.setActiveTab('show')"
          >Show</button>
          <button
            :class="{ active: halshowStore.state.activeTab === 'watch' }"
            @click="halshowStore.setActiveTab('watch')"
          >Watch</button>
          <button
            :class="{ active: halshowStore.state.activeTab === 'cmd' }"
            @click="halshowStore.setActiveTab('cmd')"
          >Cmd</button>
        </div>
        <div class="tab-content">
          <template v-if="halshowStore.state.activeTab === 'show'">
            <NodeOverview v-if="halshowStore.state.selectedNode && !halshowStore.state.selectedNode.isLeaf" />
            <DetailPanel v-else />
          </template>
          <WatchPanel v-if="halshowStore.state.activeTab === 'watch'" />
          <HalcmdPanel v-if="halshowStore.state.activeTab === 'cmd'" />
        </div>
      </div>
    </div>
    <StatusBar />
  </div>
</template>

<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

html, body, #app {
  width: 100%;
  height: 100%;
  background: #111;
  color: #ccc;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
  font-size: 13px;
}

.app {
  display: flex;
  flex-direction: column;
  height: 100vh;
}

.toolbar {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 6px 8px;
  background: #1a1a1a;
  border-bottom: 1px solid #333;
}

.category-tabs {
  display: flex;
  gap: 2px;
}

.category-tabs button {
  background: #222;
  color: #999;
  border: 1px solid #333;
  border-radius: 3px;
  padding: 4px 10px;
  font-size: 12px;
  cursor: pointer;
}

.category-tabs button.active {
  background: #2a4a6a;
  color: #fff;
  border-color: #4a8abf;
}

.category-tabs button:hover:not(.active) {
  background: #2a2a2a;
  color: #ccc;
}

.filter-input {
  flex: 1;
  max-width: 250px;
  background: #222;
  border: 1px solid #333;
  border-radius: 3px;
  padding: 4px 8px;
  color: #ccc;
  font-size: 12px;
}

.filter-input:focus {
  outline: none;
  border-color: #4a8abf;
}

.refresh-btn {
  background: #222;
  color: #999;
  border: 1px solid #333;
  border-radius: 3px;
  padding: 4px 10px;
  font-size: 12px;
  cursor: pointer;
}

.refresh-btn:hover {
  background: #2a2a2a;
  color: #ccc;
}

.main-area {
  flex: 1;
  display: flex;
  overflow: hidden;
}

.tree-panel {
  width: 300px;
  flex-shrink: 0;
  border-right: 1px solid #333;
  overflow-y: auto;
}

.right-panel {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.tab-bar {
  display: flex;
  gap: 2px;
  padding: 6px 8px;
  background: #1a1a1a;
  border-bottom: 1px solid #333;
}

.tab-bar button {
  background: #222;
  color: #999;
  border: 1px solid #333;
  border-radius: 3px;
  padding: 4px 12px;
  font-size: 12px;
  cursor: pointer;
}

.tab-bar button.active {
  background: #2a4a6a;
  color: #fff;
  border-color: #4a8abf;
}

.tab-bar button:hover:not(.active) {
  background: #2a2a2a;
  color: #ccc;
}

.tab-content {
  flex: 1;
  overflow-y: auto;
  padding: 8px;
}
</style>
