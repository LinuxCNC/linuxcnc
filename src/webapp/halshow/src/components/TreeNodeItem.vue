<script setup lang="ts">
import { halshowStore, type TreeNode } from '../stores/halshow';

const props = defineProps<{
  node: TreeNode;
  depth: number;
}>();

function onToggle(e: Event) {
  e.stopPropagation();
  halshowStore.toggleNode(props.node);
}

function onClick() {
  halshowStore.selectNode(props.node);
}

function onDblClick() {
  if (props.node.isLeaf) {
    halshowStore.addToWatch(props.node.fullPath);
  }
}
</script>

<template>
  <div class="tree-node">
    <div
      class="node-row"
      :class="{
        selected: halshowStore.state.selectedNode?.fullPath === node.fullPath,
        leaf: node.isLeaf,
      }"
      :style="{ paddingLeft: (depth * 16 + 6) + 'px' }"
      @click="onClick"
      @dblclick="onDblClick"
    >
      <span v-if="!node.isLeaf" class="expand-icon" @click="onToggle">{{ node.expanded ? '▾' : '▸' }}</span>
      <span v-else class="leaf-icon">•</span>
      <span class="node-name">{{ node.name }}</span>
    </div>
    <template v-if="!node.isLeaf && node.expanded">
      <TreeNodeItem
        v-for="child in node.children"
        :key="child.fullPath"
        :node="child"
        :depth="depth + 1"
      />
    </template>
  </div>
</template>

<script lang="ts">
export default { name: 'TreeNodeItem' };
</script>

<style scoped>
.node-row {
  display: flex;
  align-items: center;
  gap: 4px;
  padding: 2px 6px;
  cursor: pointer;
  white-space: nowrap;
}

.node-row:hover {
  background: #1a2a3a;
}

.node-row.selected {
  background: #2a4a6a;
  color: #fff;
}

.expand-icon {
  width: 16px;
  font-size: 10px;
  color: #888;
  flex-shrink: 0;
  cursor: pointer;
  text-align: center;
  padding: 2px;
}

.expand-icon:hover {
  color: #fff;
}

.leaf-icon {
  width: 12px;
  font-size: 8px;
  color: #555;
  flex-shrink: 0;
}

.node-name {
  font-family: monospace;
  font-size: 12px;
}
</style>
