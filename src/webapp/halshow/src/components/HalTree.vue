<script setup lang="ts">
import { halshowStore } from '../stores/halshow';
import TreeNodeItem from './TreeNodeItem.vue';
</script>

<template>
  <div class="hal-tree">
    <div v-if="halshowStore.state.treeNodes.length === 0" class="empty">
      <span v-if="!halshowStore.state.connected">Connecting...</span>
      <span v-else-if="halshowStore.state.error">{{ halshowStore.state.error }}</span>
      <span v-else>No items</span>
    </div>
    <TreeNodeItem
      v-for="node in halshowStore.state.treeNodes"
      :key="node.fullPath"
      :node="node"
      :depth="0"
    />
  </div>
</template>

<style scoped>
.hal-tree {
  padding: 4px 0;
  font-size: 12px;
}

.empty {
  padding: 12px;
  color: #666;
  font-style: italic;
}
</style>
