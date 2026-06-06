<script setup lang="ts">
import { computed } from 'vue';
import LadderRung from './LadderRung.vue';
import { ladderStore } from '../stores/ladder';

const usedRungs = computed(() => {
  if (!ladderStore.state.program) return [];
  const section = ladderStore.state.program.sections[ladderStore.state.activeSection];
  if (!section || section.language !== 0) return []; // Only ladder sections

  // Walk linked list of rungs
  const rungs = ladderStore.state.program.rungs;
  const result: { rung: typeof rungs[0]; index: number }[] = [];
  let idx = section.firstRung;
  const visited = new Set<number>();
  while (idx >= 0 && idx < rungs.length && !visited.has(idx)) {
    visited.add(idx);
    if (rungs[idx].used) {
      result.push({ rung: rungs[idx], index: idx });
    }
    idx = rungs[idx].nextRung;
    if (idx === section.firstRung) break; // circular
  }
  return result;
});

const sections = computed(() => {
  if (!ladderStore.state.program) return [];
  return ladderStore.state.program.sections
    .map((s, i) => ({ ...s, index: i }))
    .filter(s => s.used);
});

function onCellClick(rungIdx: number, row: number, col: number) {
  ladderStore.selectCell(rungIdx, row, col);
}
</script>

<template>
  <div class="ladder-view">
    <!-- Section tabs -->
    <div class="section-tabs" v-if="sections.length > 0">
      <button
        v-for="sec in sections"
        :key="sec.index"
        :class="{ active: ladderStore.state.activeSection === sec.index }"
        @click="ladderStore.setActiveSection(sec.index)"
      >
        {{ sec.name || `Section ${sec.index}` }}
        <span class="lang-badge">{{ sec.language === 0 ? 'LD' : 'SFC' }}</span>
      </button>
    </div>

    <!-- Ladder content -->
    <div class="rungs-container" v-if="usedRungs.length > 0">
      <LadderRung
        v-for="entry in usedRungs"
        :key="entry.index"
        :rung="entry.rung"
        :rung-index="entry.index"
        :symbols="ladderStore.state.symbolMap"
        @cell-click="(r, c) => onCellClick(entry.index, r, c)"
      />
    </div>
    <div v-else class="empty-state">
      <p v-if="!ladderStore.state.program">Loading program...</p>
      <p v-else>No rungs in this section.</p>
    </div>
  </div>
</template>

<style scoped>
.ladder-view {
  display: flex;
  flex-direction: column;
  height: 100%;
}

.section-tabs {
  display: flex;
  gap: 2px;
  padding: 0 0 8px;
  border-bottom: 1px solid #45475a;
  margin-bottom: 8px;
  flex-wrap: wrap;
}

.section-tabs button {
  background: #1e1e2e;
  border: 1px solid #45475a;
  color: #a6adc8;
  padding: 4px 12px;
  border-radius: 4px;
  cursor: pointer;
  font-size: 12px;
  display: flex;
  align-items: center;
  gap: 6px;
}

.section-tabs button.active {
  background: #313244;
  color: #cdd6f4;
  border-color: #89b4fa;
}

.lang-badge {
  font-size: 9px;
  padding: 1px 4px;
  border-radius: 3px;
  background: #45475a;
  color: #cba6f7;
  font-weight: 600;
}

.rungs-container {
  flex: 1;
  overflow-y: auto;
  padding-right: 4px;
}

.empty-state {
  display: flex;
  align-items: center;
  justify-content: center;
  height: 200px;
  color: #a6adc8;
}
</style>
